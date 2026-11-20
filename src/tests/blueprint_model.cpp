/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_model.cpp Tests for the blueprint data model and rotation. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../blueprint/blueprint.h"

#include "../safeguards.h"

/* No "using namespace": MSVC finds Blueprint::Blueprint ambiguous with it. */
using Blueprint::Item;
using Blueprint::ItemType;

static Item MakeTrack(int16_t dx, int16_t dy, TrackBits bits)
{
	Item it{};
	it.type = ItemType::Track;
	it.dx = dx;
	it.dy = dy;
	it.tracks = bits;
	return it;
}

TEST_CASE("rotate track bits")
{
	Blueprint::Blueprint bp;
	bp.w = 3;
	bp.h = 2;
	bp.items.push_back(MakeTrack(0, 0, TRACK_BIT_X));

	Blueprint::Blueprint r = bp.Rotated();
	CHECK(r.w == 2);
	CHECK(r.h == 3);
	REQUIRE(r.items.size() == 1);
	/* (dx, dy) -> (dy, w - 1 - dx) */
	CHECK(r.items[0].dx == 0);
	CHECK(r.items[0].dy == 2);
	CHECK(r.items[0].tracks == TRACK_BIT_Y);
}

TEST_CASE("rotate diagonal track bits")
{
	Blueprint::Blueprint bp;
	bp.w = 1;
	bp.h = 1;
	bp.items.push_back(MakeTrack(0, 0, TRACK_BIT_UPPER | TRACK_BIT_LEFT));
	Blueprint::Blueprint r = bp.Rotated();
	CHECK(r.items[0].tracks == (TRACK_BIT_RIGHT | TRACK_BIT_UPPER));
}

TEST_CASE("rotate height grid")
{
	Blueprint::Blueprint bp;
	bp.w = 2;
	bp.h = 1;
	/* corners: 3 wide (x = 0..2), 2 rows (y = 0..1), row-major by y */
	bp.heights = {0, 1, 2,
	              0, 1, 2};

	Blueprint::Blueprint r = bp.Rotated();
	CHECK(r.w == 1);
	CHECK(r.h == 2);
	REQUIRE(r.heights.size() == 6);
	/* corner (cx, cy) -> (cy, w - cx); new grid is 2 wide, 3 rows */
	CHECK(r.heights == std::vector<uint8_t>{2, 2,
	                                        1, 1,
	                                        0, 0});
}

TEST_CASE("rotate station footprint and axis")
{
	Blueprint::Blueprint bp;
	bp.w = 4;
	bp.h = 2;
	Item st{};
	st.type = ItemType::Station;
	st.dx = 0;
	st.dy = 0;
	st.axis = AXIS_X;
	st.platforms = 2;
	st.plat_len = 4;
	bp.items.push_back(st);

	Blueprint::Blueprint r = bp.Rotated();
	REQUIRE(r.items.size() == 1);
	CHECK(r.items[0].axis == AXIS_Y);
	/* footprint stays inside the rotated 2x4 rectangle */
	CHECK(r.items[0].dx == 0);
	CHECK(r.items[0].dy == 0);
}

TEST_CASE("rotate x4 is identity")
{
	Blueprint::Blueprint bp;
	bp.name = "all";
	bp.w = 5;
	bp.h = 3;
	bp.items.push_back(MakeTrack(1, 2, TRACK_BIT_X | TRACK_BIT_LOWER));

	Item sig{};
	sig.type = ItemType::Signal;
	sig.dx = 1;
	sig.dy = 2;
	sig.track = TRACK_LOWER;
	sig.sig_type = SIGTYPE_PBS_ONEWAY;
	sig.sig_var = SIG_SEMAPHORE;
	sig.sig_dirs = 2;
	bp.items.push_back(sig);

	Item depot{};
	depot.type = ItemType::Depot;
	depot.dx = 4;
	depot.dy = 0;
	depot.dir = DIAGDIR_SE;
	bp.items.push_back(depot);

	Item bridge{};
	bridge.type = ItemType::Bridge;
	bridge.dx = 0;
	bridge.dy = 1;
	bridge.dir = DIAGDIR_SW;
	bridge.length = 4;
	bridge.bridge_type = 3;
	bp.items.push_back(bridge);

	Item tunnel{};
	tunnel.type = ItemType::Tunnel;
	tunnel.dx = 2;
	tunnel.dy = 0;
	tunnel.dir = DIAGDIR_SE;
	tunnel.length = 2;
	bp.items.push_back(tunnel);

	Item st{};
	st.type = ItemType::Station;
	st.dx = 1;
	st.dy = 0;
	st.axis = AXIS_X;
	st.platforms = 1;
	st.plat_len = 3;
	bp.items.push_back(st);

	bp.heights.resize(6 * 4);
	for (size_t i = 0; i < bp.heights.size(); i++) bp.heights[i] = static_cast<uint8_t>(i % 5);

	Blueprint::Blueprint r = bp.Rotated().Rotated().Rotated().Rotated();
	CHECK(r == bp);

	/* and a single rotation must change something */
	CHECK_FALSE(bp.Rotated() == bp);
}

TEST_CASE("signal direction follows rotation")
{
	Blueprint::Blueprint bp;
	bp.w = 1;
	bp.h = 1;
	Item sig{};
	sig.type = ItemType::Signal;
	sig.track = TRACK_X;
	sig.sig_dirs = 1; // along TRACKDIR_X_NE: trains heading north-east see it
	bp.items.push_back(sig);

	Blueprint::Blueprint r = bp.Rotated();
	CHECK(r.items[0].track == TRACK_Y);
	/* NE rotates to SE, which is TRACKDIR_Y_SE = TrackToTrackdir(TRACK_Y) */
	CHECK(r.items[0].sig_dirs == 1);

	Blueprint::Blueprint r2 = r.Rotated();
	/* SE rotates to SW, the reverse of TRACKDIR_X_NE */
	CHECK(r2.items[0].sig_dirs == 2);
}
