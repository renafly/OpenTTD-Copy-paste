/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_plan.h Turning a blueprint into an ordered list of build steps. */

#ifndef BLUEPRINT_BLUEPRINT_PLAN_H
#define BLUEPRINT_BLUEPRINT_PLAN_H

#include "blueprint.h"
#include "../tile_type.h"

namespace Blueprint {

/** One action needed to build a blueprint at a location. */
struct Step {
	enum class Kind : uint8_t { Terraform, Track, Depot, Bridge, Tunnel, Station, Signal };
	Kind kind;
	TileIndex tile;           ///< Target tile; for Terraform the tile whose north corner is the grid point.
	int8_t raise = 0;         ///< Terraform: levels to raise (positive) or lower (negative) that corner.
	const Item *item = nullptr; ///< Source item for every kind but Terraform.
	TileIndex other = INVALID_TILE; ///< Bridge/Tunnel: the other head.
};

/**
 * Plan the steps to build a blueprint with its north corner at \a origin.
 * Order: terraforming, then track/depots/bridges/tunnels/stations, then signals.
 * Items or corners that would fall outside the map are dropped.
 * @param bp The blueprint, already rotated as wanted. Must outlive the returned steps.
 * @param origin Tile for the blueprint's (0, 0).
 * @param with_terrain Whether to plan terraforming (ignored if the blueprint has no terrain).
 */
std::vector<Step> PlanBuild(const Blueprint &bp, TileIndex origin, bool with_terrain);

} // namespace Blueprint

#endif /* BLUEPRINT_BLUEPRINT_PLAN_H */
