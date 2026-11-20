/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_exec.cpp Building a blueprint by posting ordinary game commands. */

#include "../stdafx.h"
#include "blueprint_exec.h"
#include "blueprint_plan.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../error.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../tunnelbridge_map.h"
#include "../track_func.h"
#include "../signal_func.h"
#include "../newgrf_station.h"
#include "../rail_cmd.h"
#include "../station_cmd.h"
#include "../terraform_cmd.h"
#include "../tunnelbridge_cmd.h"
#include "../strings_func.h"
#include "../network/network.h"
#include "../timer/timer.h"
#include "../timer/timer_window.h"

#include "table/strings.h"

#include <memory>

#include "../safeguards.h"

namespace Blueprint {

/** How long to wait in multiplayer for a batch to show up on the map before moving on. */
static constexpr std::chrono::milliseconds BATCH_TIMEOUT{3000};

/** Present signals bitmask for a signal item. */
static uint8_t SignalMask(const Item &it)
{
	Trackdir td = TrackToTrackdir(it.track);
	uint8_t mask = 0;
	if (it.sig_dirs & 1) mask |= SignalAlongTrackdir(td);
	if (it.sig_dirs & 2) mask |= SignalAlongTrackdir(ReverseTrackdir(td));
	return mask;
}

/** Use the stored station spec if this game has it, else the default station. */
static std::pair<StationClassID, uint16_t> ResolveStationSpec(const Item &it)
{
	if (static_cast<uint>(it.spec_class) < StationClass::GetClassCount()) {
		const StationClass *cls = StationClass::Get(it.spec_class);
		if (it.spec_index < cls->GetSpecCount() && cls->GetSpec(it.spec_index) != nullptr) return {it.spec_class, it.spec_index};
	}
	return {STAT_CLASS_DFLT, 0};
}

/** Whether the effect of a step is visible on the map. */
static bool IsDone(const Step &s)
{
	const Item &it = *s.item;
	switch (s.kind) {
		case Step::Kind::Terraform:
			NOT_REACHED();

		case Step::Kind::Track:
			return IsPlainRailTile(s.tile) && GetTileOwner(s.tile) == _local_company && (GetTrackBits(s.tile) & it.tracks) == it.tracks;

		case Step::Kind::Signal:
			return IsPlainRailTile(s.tile) && HasSignalOnTrack(s.tile, it.track) &&
					(GetPresentSignals(s.tile) & SignalOnTrack(it.track)) == SignalMask(it) &&
					GetSignalType(s.tile, it.track) == it.sig_type;

		case Step::Kind::Depot:
			return IsRailDepotTile(s.tile) && GetTileOwner(s.tile) == _local_company && GetRailDepotDirection(s.tile) == it.dir;

		case Step::Kind::Bridge:
		case Step::Kind::Tunnel:
			return IsTileType(s.tile, MP_TUNNELBRIDGE) && GetTunnelBridgeTransportType(s.tile) == TRANSPORT_RAIL &&
					GetOtherTunnelBridgeEnd(s.tile) == s.other;

		case Step::Kind::Station: {
			auto [sx, sy] = it.Footprint();
			for (int y = 0; y < sy; y++) {
				for (int x = 0; x < sx; x++) {
					TileIndex t = TileAddXY(s.tile, x, y);
					if (!IsRailStationTile(t) || GetTileOwner(t) != _local_company) return false;
				}
			}
			return true;
		}
	}
	NOT_REACHED();
}

/** A paste in progress. */
struct Job {
	Blueprint bp;   ///< Own copy; steps point into its items.
	RailType rt;
	std::vector<Step> terraform; ///< One entry per corner to change; raise is the remaining difference at planning time.
	std::vector<int> targets;    ///< Target height per #terraform entry.
	std::vector<Step> build;
	std::vector<Step> signals;

	int terraform_passes_left = 0;
	enum class Phase : uint8_t { Terraform, Build, Signals, Done } phase = Phase::Terraform;
	bool waiting = false;        ///< A batch was posted and we wait for it to show up.
	std::chrono::milliseconds waited{0};

	uint failures = 0;
	TileIndex first_failure = INVALID_TILE;

	void Fail(TileIndex tile)
	{
		this->failures++;
		if (this->first_failure == INVALID_TILE) this->first_failure = tile;
	}

	/** Post one terraform pass: move every unfinished corner one level towards its target. */
	void PostTerraformPass()
	{
		for (size_t i = 0; i < this->terraform.size(); i++) {
			TileIndex tile = this->terraform[i].tile;
			int cur = TileHeight(tile);
			if (cur == this->targets[i]) continue;
			Command<CMD_TERRAFORM_LAND>::Post((StringID)0, tile, SLOPE_N, cur < this->targets[i]);
		}
	}

	bool TerraformPassDone() const
	{
		/* In single player commands execute immediately; in multiplayer we cannot know the
		 * intermediate heights exactly, so just wait for the timeout or the final shape. */
		if (!_networking) return true;
		for (size_t i = 0; i < this->terraform.size(); i++) {
			if (static_cast<int>(TileHeight(this->terraform[i].tile)) != this->targets[i]) return false;
		}
		return true;
	}

	void PostStep(const Step &s)
	{
		const Item &it = *s.item;
		switch (s.kind) {
			case Step::Kind::Terraform:
				NOT_REACHED();

			case Step::Kind::Track:
				for (Track t = TRACK_BEGIN; t < TRACK_END; t++) {
					if (!HasBit(it.tracks, t)) continue;
					if (IsPlainRailTile(s.tile) && HasBit(GetTrackBits(s.tile), t)) continue;
					Command<CMD_BUILD_SINGLE_RAIL>::Post((StringID)0, s.tile, this->rt, t, false);
				}
				break;

			case Step::Kind::Depot:
				Command<CMD_BUILD_TRAIN_DEPOT>::Post((StringID)0, s.tile, this->rt, it.dir);
				break;

			case Step::Kind::Bridge:
				Command<CMD_BUILD_BRIDGE>::Post((StringID)0, s.other, s.tile, TRANSPORT_RAIL, it.bridge_type, static_cast<uint8_t>(this->rt));
				break;

			case Step::Kind::Tunnel:
				Command<CMD_BUILD_TUNNEL>::Post((StringID)0, s.tile, TRANSPORT_RAIL, static_cast<uint8_t>(this->rt));
				break;

			case Step::Kind::Station: {
				auto [cls, idx] = ResolveStationSpec(it);
				Command<CMD_BUILD_RAIL_STATION>::Post((StringID)0, s.tile, this->rt, it.axis, it.platforms, it.plat_len, cls, idx, NEW_STATION, false);
				break;
			}

			case Step::Kind::Signal:
				Command<CMD_BUILD_SINGLE_SIGNAL>::Post((StringID)0, s.tile, it.track, it.sig_type, it.sig_var,
						false, false, false, SIGTYPE_BLOCK, SIGTYPE_LAST, 0, SignalMask(it));
				break;
		}
	}

	static bool AllDone(const std::vector<Step> &steps)
	{
		return std::ranges::all_of(steps, IsDone);
	}

	void CountFailures(const std::vector<Step> &steps)
	{
		for (const Step &s : steps) {
			if (!IsDone(s)) this->Fail(s.tile);
		}
	}

	/** Advance the job. @return true when finished. */
	bool Tick(std::chrono::milliseconds delta)
	{
		if (this->waiting) {
			this->waited += delta;
			bool ready;
			switch (this->phase) {
				case Phase::Terraform: ready = this->TerraformPassDone(); break;
				case Phase::Build: ready = !_networking || AllDone(this->build); break;
				case Phase::Signals: ready = !_networking || AllDone(this->signals); break;
				default: ready = true; break;
			}
			if (!ready && this->waited < BATCH_TIMEOUT) return false;
			this->waiting = false;
			this->waited = std::chrono::milliseconds{0};

			/* Batch finished: account for it and move to the next phase. */
			switch (this->phase) {
				case Phase::Terraform:
					if (--this->terraform_passes_left > 0) break;
					for (size_t i = 0; i < this->terraform.size(); i++) {
						if (static_cast<int>(TileHeight(this->terraform[i].tile)) != this->targets[i]) this->Fail(this->terraform[i].tile);
					}
					this->phase = Phase::Build;
					break;
				case Phase::Build:
					this->CountFailures(this->build);
					this->phase = Phase::Signals;
					break;
				case Phase::Signals:
					this->CountFailures(this->signals);
					this->phase = Phase::Done;
					break;
				default:
					break;
			}
		}

		switch (this->phase) {
			case Phase::Terraform:
				if (this->terraform_passes_left <= 0) {
					this->phase = Phase::Build;
					return this->Tick(std::chrono::milliseconds{0});
				}
				this->PostTerraformPass();
				break;
			case Phase::Build:
				for (const Step &s : this->build) this->PostStep(s);
				break;
			case Phase::Signals:
				for (const Step &s : this->signals) this->PostStep(s);
				break;
			case Phase::Done:
				return true;
		}
		this->waiting = true;
		return false;
	}

	void Report() const
	{
		if (this->failures == 0) return;
		int x = 0, y = 0;
		if (this->first_failure != INVALID_TILE) {
			x = TileX(this->first_failure) * TILE_SIZE;
			y = TileY(this->first_failure) * TILE_SIZE;
		}
		ShowErrorMessage(GetEncodedString(STR_BLUEPRINT_ERROR_PARTIAL), GetEncodedString(STR_BLUEPRINT_ERROR_PARTIAL_DETAIL, this->failures), WL_WARNING, x, y);
	}
};

static std::unique_ptr<Job> _job;

static const IntervalTimer<TimerWindow> _job_interval(std::chrono::milliseconds(100), [](auto) {
	if (_job == nullptr) return;
	/* Stop if the player left the game or switched company while we were building. */
	if (_local_company == COMPANY_SPECTATOR || _game_mode != GM_NORMAL) {
		_job.reset();
		return;
	}
	if (_job->Tick(std::chrono::milliseconds(100))) {
		_job->Report();
		_job.reset();
	}
});

bool IsExecuting()
{
	return _job != nullptr;
}

bool Execute(const Blueprint &bp, TileIndex origin, RailType rt, bool with_terrain)
{
	if (_job != nullptr) return false;

	auto job = std::make_unique<Job>();
	job->bp = bp;
	job->rt = rt;

	for (const Step &s : PlanBuild(job->bp, origin, with_terrain)) {
		switch (s.kind) {
			case Step::Kind::Terraform:
				job->terraform.push_back(s);
				job->targets.push_back(static_cast<int>(TileHeight(s.tile)) + s.raise);
				job->terraform_passes_left = std::max(job->terraform_passes_left, std::abs(static_cast<int>(s.raise)));
				break;
			case Step::Kind::Signal:
				job->signals.push_back(s);
				break;
			default:
				job->build.push_back(s);
				break;
		}
	}
	/* Allow a couple of extra passes for corners that neighbouring terraforming moved away from their target. */
	if (job->terraform_passes_left > 0) job->terraform_passes_left += 2;

	_job = std::move(job);
	/* Start right away; following batches run from the timer. */
	if (_job->Tick(std::chrono::milliseconds{0})) {
		_job->Report();
		_job.reset();
	}
	return true;
}

} // namespace Blueprint
