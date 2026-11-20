/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_capture.h Reading rail infrastructure and terrain from the map into a blueprint. */

#ifndef BLUEPRINT_BLUEPRINT_CAPTURE_H
#define BLUEPRINT_BLUEPRINT_CAPTURE_H

#include "blueprint.h"
#include "../tile_type.h"
#include "../company_type.h"

namespace Blueprint {

/**
 * Capture the rail infrastructure of one company inside a rectangle.
 * @param a One corner tile of the area.
 * @param b The opposite corner tile of the area.
 * @param owner Only infrastructure owned by this company is captured.
 * @param with_terrain Whether to record corner heights.
 * @return The blueprint, with an empty name.
 */
Blueprint Capture(TileIndex a, TileIndex b, CompanyID owner, bool with_terrain);

} // namespace Blueprint

#endif /* BLUEPRINT_BLUEPRINT_CAPTURE_H */
