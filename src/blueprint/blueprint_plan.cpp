/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_plan.cpp Turning a blueprint into an ordered list of build steps. */

#include "../stdafx.h"
#include "blueprint_plan.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../settings_type.h"

#include "../safeguards.h"

namespace Blueprint {

/** @return Whether map coordinate (x, y) is on the map. Works on signed values so negative offsets never wrap. */
static bool OnMap(int64_t x, int64_t y)
{
	return x >= 0 && y >= 0 && x <= static_cast<int64_t>(Map::MaxX()) && y <= static_cast<int64_t>(Map::MaxY());
}

static Step::Kind KindOf(ItemType t)
{
	switch (t) {
		case ItemType::Track: return Step::Kind::Track;
		case ItemType::Signal: return Step::Kind::Signal;
		case ItemType::Depot: return Step::Kind::Depot;
		case ItemType::Bridge: return Step::Kind::Bridge;
		case ItemType::Tunnel: return Step::Kind::Tunnel;
		case ItemType::Station: return Step::Kind::Station;
	}
	NOT_REACHED();
}

static void PlanTerrain(const Blueprint &bp, int64_t ox, int64_t oy, std::vector<Step> &steps)
{
	const int limit = _settings_game.construction.map_height_limit;

	/* Base: the lowest existing corner under the footprint. */
	int base = INT32_MAX;
	for (int64_t cy = 0; cy <= bp.h; cy++) {
		for (int64_t cx = 0; cx <= bp.w; cx++) {
			if (!OnMap(ox + cx, oy + cy)) continue;
			base = std::min<int>(base, TileHeight(TileXY(static_cast<uint>(ox + cx), static_cast<uint>(oy + cy))));
		}
	}
	if (base == INT32_MAX) return;

	int template_min = *std::min_element(bp.heights.begin(), bp.heights.end());

	for (int64_t cy = 0; cy <= bp.h; cy++) {
		for (int64_t cx = 0; cx <= bp.w; cx++) {
			if (!OnMap(ox + cx, oy + cy)) continue;
			TileIndex tile = TileXY(static_cast<uint>(ox + cx), static_cast<uint>(oy + cy));
			int rel = bp.heights[static_cast<size_t>(cy * (bp.w + 1) + cx)] - template_min;
			int target = std::clamp(base + rel, 0, limit);
			int diff = target - static_cast<int>(TileHeight(tile));
			if (diff == 0) continue;
			Step s{Step::Kind::Terraform, tile};
			s.raise = static_cast<int8_t>(std::clamp(diff, -127, 127));
			steps.push_back(s);
		}
	}
}

std::vector<Step> PlanBuild(const Blueprint &bp, TileIndex origin, bool with_terrain)
{
	std::vector<Step> steps;
	const int64_t ox = TileX(origin);
	const int64_t oy = TileY(origin);

	if (with_terrain && bp.HasTerrain()) PlanTerrain(bp, ox, oy, steps);

	std::vector<Step> signals;
	for (const Item &it : bp.items) {
		const int64_t x = ox + it.dx;
		const int64_t y = oy + it.dy;
		auto [sx, sy] = it.Footprint();
		if (!OnMap(x, y) || !OnMap(x + sx - 1, y + sy - 1)) continue;

		Step s{KindOf(it.type), TileXY(static_cast<uint>(x), static_cast<uint>(y))};
		s.item = &it;

		if (it.type == ItemType::Bridge || it.type == ItemType::Tunnel) {
			TileIndexDiffC d = TileIndexDiffCByDiagDir(it.dir);
			const int64_t ex = x + static_cast<int64_t>(d.x) * it.length;
			const int64_t ey = y + static_cast<int64_t>(d.y) * it.length;
			if (!OnMap(ex, ey)) continue;
			s.other = TileXY(static_cast<uint>(ex), static_cast<uint>(ey));
		}

		if (it.type == ItemType::Signal) {
			signals.push_back(s);
		} else {
			steps.push_back(s);
		}
	}

	steps.insert(steps.end(), signals.begin(), signals.end());
	return steps;
}

} // namespace Blueprint
