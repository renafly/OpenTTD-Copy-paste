/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file blueprint_capture.cpp Reading rail infrastructure and terrain from the map into a blueprint.
 *
 * Station block detection follows the CityMania client's BlueprintCopyArea
 * (citymania-org/cmclient, src/citymania/cm_blueprint.cpp, GPLv2).
 */

#include "../stdafx.h"
#include "blueprint_capture.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../tunnelbridge_map.h"
#include "../bridge_map.h"
#include "../tunnel_map.h"
#include "../track_func.h"
#include "../tilearea_type.h"
#include "../newgrf_station.h"

#include <map>
#include <set>

#include "../safeguards.h"

namespace Blueprint {

static void AddSignals(Blueprint &bp, TileIndex tile, int16_t dx, int16_t dy)
{
	TrackBits bits = GetTrackBits(tile);
	for (Track track = TRACK_BEGIN; track < TRACK_END; track++) {
		if (!HasBit(bits, track) || !HasSignalOnTrack(tile, track)) continue;
		Trackdir td = TrackToTrackdir(track);
		uint8_t dirs = 0;
		if (HasSignalOnTrackdir(tile, td)) dirs |= 1;
		if (HasSignalOnTrackdir(tile, ReverseTrackdir(td))) dirs |= 2;
		if (dirs == 0) continue;

		Item it{};
		it.type = ItemType::Signal;
		it.dx = dx;
		it.dy = dy;
		it.track = track;
		it.sig_type = GetSignalType(tile, track);
		it.sig_var = GetSignalVariant(tile, track);
		it.sig_dirs = dirs;
		bp.items.push_back(it);
	}
}

/** Check whether a tile belongs to the same rail station block as the reference. */
static bool IsSameStationBlockTile(TileIndex tile, StationID sid, Axis axis, CompanyID owner, const TileArea &area)
{
	return IsValidTile(tile) && area.Contains(tile) && IsRailStationTile(tile) && GetTileOwner(tile) == owner &&
			GetStationIndex(tile) == sid && GetRailStationAxis(tile) == axis;
}

static void AddStationBlocks(Blueprint &bp, const std::vector<TileIndex> &tiles, const TileArea &area, CompanyID owner)
{
	std::map<StationID, uint16_t> groups;
	std::set<TileIndex> done;

	for (TileIndex tile : tiles) {
		if (done.contains(tile)) continue;

		StationID sid = GetStationIndex(tile);
		Axis axis = GetRailStationAxis(tile);
		TileIndexDiff plat_delta = axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1);
		TileIndexDiff track_delta = axis == AXIS_X ? TileDiffXY(0, 1) : TileDiffXY(1, 0);

		/* Tiles are visited in x-then-y order, so this is the block's north corner. */
		uint8_t plat_len = 0;
		for (TileIndex t = tile; plat_len < UINT8_MAX && IsSameStationBlockTile(t, sid, axis, owner, area) && !done.contains(t); t = TileAdd(t, plat_delta)) {
			plat_len++;
		}

		uint8_t platforms = 1;
		for (TileIndex row = TileAdd(tile, track_delta); platforms < UINT8_MAX; row = TileAdd(row, track_delta)) {
			bool full = true;
			TileIndex t = row;
			for (uint8_t i = 0; i < plat_len; i++, t = TileAdd(t, plat_delta)) {
				if (!IsSameStationBlockTile(t, sid, axis, owner, area) || done.contains(t)) {
					full = false;
					break;
				}
			}
			if (!full) break;
			platforms++;
		}

		TileIndex row = tile;
		for (uint8_t p = 0; p < platforms; p++, row = TileAdd(row, track_delta)) {
			TileIndex t = row;
			for (uint8_t i = 0; i < plat_len; i++, t = TileAdd(t, plat_delta)) done.insert(t);
		}

		auto [it_group, inserted] = groups.try_emplace(sid, static_cast<uint16_t>(groups.size() + 1));
		(void)inserted;

		Item it{};
		it.type = ItemType::Station;
		it.dx = static_cast<int16_t>(TileX(tile) - TileX(area.tile));
		it.dy = static_cast<int16_t>(TileY(tile) - TileY(area.tile));
		it.axis = axis;
		it.platforms = platforms;
		it.plat_len = plat_len;
		it.station_group = it_group->second;
		if (const StationSpec *spec = GetStationSpec(tile); spec != nullptr) {
			it.spec_class = spec->class_index;
			it.spec_index = spec->index;
		}
		bp.items.push_back(it);
	}
}

Blueprint Capture(TileIndex a, TileIndex b, CompanyID owner, bool with_terrain)
{
	TileArea area(a, b);
	Blueprint bp;
	bp.w = area.w;
	bp.h = area.h;

	std::vector<TileIndex> station_tiles;

	for (TileIndex tile : area) {
		const int16_t dx = static_cast<int16_t>(TileX(tile) - TileX(area.tile));
		const int16_t dy = static_cast<int16_t>(TileY(tile) - TileY(area.tile));

		switch (GetTileType(tile)) {
			case MP_RAILWAY: {
				if (GetTileOwner(tile) != owner) break;
				if (IsRailDepot(tile)) {
					Item it{};
					it.type = ItemType::Depot;
					it.dx = dx;
					it.dy = dy;
					it.dir = GetRailDepotDirection(tile);
					bp.items.push_back(it);
					break;
				}
				Item it{};
				it.type = ItemType::Track;
				it.dx = dx;
				it.dy = dy;
				it.tracks = GetTrackBits(tile);
				bp.items.push_back(it);
				if (HasSignals(tile)) AddSignals(bp, tile, dx, dy);
				break;
			}

			case MP_TUNNELBRIDGE: {
				if (GetTunnelBridgeTransportType(tile) != TRANSPORT_RAIL || GetTileOwner(tile) != owner) break;
				TileIndex other = GetOtherTunnelBridgeEnd(tile);
				/* Store each bridge/tunnel once, from the head with the lower index, and only if both heads are inside. */
				if (!area.Contains(other) || other < tile) break;
				Item it{};
				it.type = IsTunnel(tile) ? ItemType::Tunnel : ItemType::Bridge;
				it.dx = dx;
				it.dy = dy;
				it.dir = GetTunnelBridgeDirection(tile);
				it.length = static_cast<uint16_t>(DistanceManhattan(tile, other));
				if (it.type == ItemType::Bridge) it.bridge_type = GetBridgeType(tile);
				bp.items.push_back(it);
				break;
			}

			case MP_STATION:
				if (IsRailStationTile(tile) && GetTileOwner(tile) == owner) station_tiles.push_back(tile);
				break;

			default:
				break;
		}
	}

	AddStationBlocks(bp, station_tiles, area, owner);

	if (with_terrain) {
		const uint x0 = TileX(area.tile);
		const uint y0 = TileY(area.tile);
		std::vector<uint> raw;
		raw.reserve(static_cast<size_t>(bp.w + 1) * (bp.h + 1));
		for (uint cy = 0; cy <= bp.h; cy++) {
			for (uint cx = 0; cx <= bp.w; cx++) {
				raw.push_back(TileHeightOutsideMap(x0 + cx, y0 + cy));
			}
		}
		uint min_h = *std::min_element(raw.begin(), raw.end());
		bp.heights.reserve(raw.size());
		for (uint v : raw) bp.heights.push_back(static_cast<uint8_t>(v - min_h));
	}

	return bp;
}

} // namespace Blueprint
