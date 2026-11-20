/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint.cpp Blueprint data model and rotation. */

#include "../stdafx.h"
#include "blueprint.h"
#include "../track_func.h"
#include "../direction_func.h"

#include "../safeguards.h"

namespace Blueprint {

/*
 * Rotation is 90 degrees clockwise on screen. In map coordinates the X axis
 * points south-west and the Y axis south-east, so a tile at (x, y) in a
 * w x h area moves to (y, w - 1 - x) and the area becomes h x w.
 * Diagonal directions turn NE -> SE -> SW -> NW -> NE.
 */

Track RotateTrack(Track track)
{
	static const Track rotated[TRACK_END] = {
		TRACK_Y,     // TRACK_X
		TRACK_X,     // TRACK_Y
		TRACK_RIGHT, // TRACK_UPPER
		TRACK_LEFT,  // TRACK_LOWER
		TRACK_UPPER, // TRACK_LEFT
		TRACK_LOWER, // TRACK_RIGHT
	};
	return rotated[track];
}

TrackBits RotateTrackBits(TrackBits bits)
{
	TrackBits res = TRACK_BIT_NONE;
	for (Track t = TRACK_BEGIN; t < TRACK_END; t++) {
		if (HasBit(bits, t)) res |= TrackToTrackBits(RotateTrack(t));
	}
	return res;
}

Trackdir RotateTrackdir(Trackdir td)
{
	Track rt = RotateTrack(TrackdirToTrack(td));
	DiagDirection exit = ChangeDiagDir(TrackdirToExitdir(td), DIAGDIRDIFF_90RIGHT);
	Trackdir cand = TrackToTrackdir(rt);
	return TrackdirToExitdir(cand) == exit ? cand : ReverseTrackdir(cand);
}

std::pair<int, int> Item::Footprint() const
{
	if (this->type != ItemType::Station) return {1, 1};
	return this->axis == AXIS_X ? std::pair<int, int>{this->plat_len, this->platforms} : std::pair<int, int>{this->platforms, this->plat_len};
}

Blueprint Blueprint::Rotated() const
{
	Blueprint res;
	res.name = this->name;
	res.w = this->h;
	res.h = this->w;
	res.items.reserve(this->items.size());

	for (const Item &it : this->items) {
		Item r = it;
		auto [sx, sy] = it.Footprint();
		/* The new origin is the rotated image of the footprint's far-x corner. */
		r.dx = it.dy;
		r.dy = static_cast<int16_t>(this->w - (it.dx + sx));
		(void)sy;

		switch (it.type) {
			case ItemType::Track:
				r.tracks = RotateTrackBits(it.tracks);
				break;

			case ItemType::Signal: {
				r.track = RotateTrack(it.track);
				Trackdir rd = RotateTrackdir(TrackToTrackdir(it.track));
				if (rd != TrackToTrackdir(r.track)) {
					/* Our "first" trackdir became the reverse one: swap the direction bits. */
					r.sig_dirs = static_cast<uint8_t>(((it.sig_dirs & 1) << 1) | ((it.sig_dirs >> 1) & 1));
				}
				break;
			}

			case ItemType::Depot:
			case ItemType::Bridge:
			case ItemType::Tunnel:
				r.dir = ChangeDiagDir(it.dir, DIAGDIRDIFF_90RIGHT);
				break;

			case ItemType::Station:
				r.axis = OtherAxis(it.axis);
				break;
		}
		res.items.push_back(r);
	}

	if (this->HasTerrain()) {
		const size_t cw = this->w + 1u; // corners per row before rotation
		const size_t ncw = res.w + 1u;  // corners per row after rotation
		res.heights.resize(this->heights.size());
		for (size_t cy = 0; cy <= this->h; cy++) {
			for (size_t cx = 0; cx <= this->w; cx++) {
				size_t nx = cy;
				size_t ny = this->w - cx;
				res.heights[ny * ncw + nx] = this->heights[cy * cw + cx];
			}
		}
	}

	return res;
}

} // namespace Blueprint
