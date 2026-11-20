/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_exec.h Building a blueprint by posting ordinary game commands. */

#ifndef BLUEPRINT_BLUEPRINT_EXEC_H
#define BLUEPRINT_BLUEPRINT_EXEC_H

#include "blueprint.h"
#include "../tile_type.h"
#include "../rail_type.h"

namespace Blueprint {

/**
 * Build a blueprint for the local company.
 *
 * Work is posted in batches (terraform passes, then construction, then signals).
 * The next batch is only posted once the previous one is visible on the map, or
 * after a short timeout, so it also works in multiplayer, where commands are
 * executed later. Only standard commands without custom callbacks are used, so
 * official servers accept them. When done, one message reports how many parts
 * could not be built.
 *
 * @param bp The blueprint, already rotated as wanted.
 * @param origin Tile for the blueprint's (0, 0).
 * @param rt Rail type to build with.
 * @param with_terrain Whether to reproduce the stored terrain.
 * @return False if another paste is still in progress (nothing is done then).
 */
bool Execute(const Blueprint &bp, TileIndex origin, RailType rt, bool with_terrain);

/** @return Whether a paste is still being built. */
bool IsExecuting();

} // namespace Blueprint

#endif /* BLUEPRINT_BLUEPRINT_EXEC_H */
