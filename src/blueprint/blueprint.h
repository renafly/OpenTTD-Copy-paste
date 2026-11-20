/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file blueprint.h Data model for rail blueprints (copy/paste templates).
 *
 * Inspired by the CityMania client's blueprint tool (citymania-org/cmclient,
 * src/citymania/cm_blueprint.cpp, GPLv2).
 */

#ifndef BLUEPRINT_BLUEPRINT_H
#define BLUEPRINT_BLUEPRINT_H

#include "../track_type.h"
#include "../direction_type.h"
#include "../signal_type.h"
#include "../bridge_type.h"
#include "../newgrf_station.h"

namespace Blueprint {

/** Kind of thing stored in a blueprint item. Values are stored in template files; only append. */
enum class ItemType : uint8_t {
	Track = 0,   ///< Plain rail track bits on one tile.
	Signal = 1,  ///< Signals on one track of one tile.
	Depot = 2,   ///< Rail depot.
	Bridge = 3,  ///< Rail bridge, stored at one head.
	Tunnel = 4,  ///< Rail tunnel, stored at one entrance.
	Station = 5, ///< Rectangular block of rail station platforms.
};

/** One element of a blueprint, positioned relative to the blueprint's north corner. */
struct Item {
	ItemType type = ItemType::Track;
	int16_t dx = 0; ///< X offset from the origin tile.
	int16_t dy = 0; ///< Y offset from the origin tile.

	TrackBits tracks = TRACK_BIT_NONE; ///< Track: track bits on the tile.

	Track track = TRACK_X;                ///< Signal: track carrying the signal(s).
	SignalType sig_type = SIGTYPE_BLOCK;  ///< Signal: signal type.
	SignalVariant sig_var = SIG_ELECTRIC; ///< Signal: electric or semaphore.
	uint8_t sig_dirs = 0;                 ///< Signal: bit0 = signal along TrackToTrackdir(track), bit1 = along its reverse.

	DiagDirection dir = DIAGDIR_NE; ///< Depot: facing; Bridge/Tunnel: direction from this head towards the other.
	BridgeType bridge_type = 0;     ///< Bridge: bridge spec.
	uint16_t length = 0;            ///< Bridge/Tunnel: distance in tiles to the other head.

	Axis axis = AXIS_X;                         ///< Station: platform axis.
	uint8_t platforms = 0;                      ///< Station: number of parallel platforms.
	uint8_t plat_len = 0;                       ///< Station: platform length.
	StationClassID spec_class = STAT_CLASS_DFLT; ///< Station: NewGRF station class.
	uint16_t spec_index = 0;                    ///< Station: index within the class.
	uint16_t station_group = 0;                 ///< Station: blocks with the same group belonged to one station.

	/** Size of the item's rectangular footprint in tiles (1x1 for everything but stations). */
	std::pair<int, int> Footprint() const;

	bool operator==(const Item &other) const = default;
};

/** A named, rotatable template of rail infrastructure and optionally terrain. */
struct Blueprint {
	std::string name;
	uint16_t w = 0; ///< Width in tiles (X axis).
	uint16_t h = 0; ///< Height in tiles (Y axis).
	std::vector<Item> items;
	/** Corner heights, (w + 1) * (h + 1) values, row-major by y, relative to the lowest corner. Empty means no terrain. */
	std::vector<uint8_t> heights;

	bool HasTerrain() const { return !this->heights.empty(); }

	/** @return This blueprint rotated 90 degrees clockwise as seen on screen. */
	Blueprint Rotated() const;

	bool operator==(const Blueprint &other) const = default;
};

Track RotateTrack(Track track);
TrackBits RotateTrackBits(TrackBits bits);
Trackdir RotateTrackdir(Trackdir td);

} // namespace Blueprint

#endif /* BLUEPRINT_BLUEPRINT_H */
