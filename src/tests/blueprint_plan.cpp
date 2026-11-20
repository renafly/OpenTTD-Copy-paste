/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_plan.cpp Tests for turning blueprints into ordered build steps. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../blueprint/blueprint_plan.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../clear_map.h"
#include "../settings_type.h"

#include "../safeguards.h"

/* No "using namespace": MSVC finds Blueprint::Blueprint ambiguous with it. */
using Blueprint::Item;
using Blueprint::ItemType;
using Blueprint::Step;
using Blueprint::PlanBuild;

static void FlatMap(uint height)
{
	Map::Allocate(64, 64);
	_settings_game.construction.map_height_limit = 30;
	for (TileIndex t : Map::Iterate()) {
		MakeClear(t, CLEAR_GRASS, 3);
		SetTileHeight(t, height);
	}
}

static Blueprint::Blueprint TrackAndSignal(uint16_t w, uint16_t h)
{
	Blueprint::Blueprint bp;
	bp.w = w;
	bp.h = h;
	Item t{};
	t.type = ItemType::Track;
	t.tracks = TRACK_BIT_X;
	bp.items.push_back(t);
	Item s{};
	s.type = ItemType::Signal;
	s.track = TRACK_X;
	s.sig_dirs = 1;
	bp.items.push_back(s);
	Item d{};
	d.type = ItemType::Depot;
	d.dx = static_cast<int16_t>(w - 1);
	d.dy = static_cast<int16_t>(h - 1);
	bp.items.push_back(d);
	return bp;
}

TEST_CASE("signals come last")
{
	FlatMap(2);
	auto steps = PlanBuild(TrackAndSignal(2, 2), TileXY(10, 10), false);
	REQUIRE(steps.size() == 3);
	CHECK(steps.front().kind != Step::Kind::Signal);
	CHECK(steps.back().kind == Step::Kind::Signal);
	CHECK(steps[0].tile == TileXY(10, 10));
	CHECK(steps[1].tile == TileXY(11, 11));
}

TEST_CASE("terrain diff only")
{
	FlatMap(2);
	Blueprint::Blueprint bp = TrackAndSignal(1, 1);
	bp.heights = {0, 0, 0, 0};
	auto steps = PlanBuild(bp, TileXY(10, 10), true);
	for (const Step &s : steps) CHECK(s.kind != Step::Kind::Terraform);

	bp.heights = {0, 1, 0, 0}; // corner (1, 0)
	steps = PlanBuild(bp, TileXY(10, 10), true);
	REQUIRE(steps.size() >= 1);
	CHECK(steps[0].kind == Step::Kind::Terraform);
	CHECK(steps[0].tile == TileXY(11, 10));
	CHECK(steps[0].raise == 1);
	CHECK(steps[1].kind != Step::Kind::Terraform);
}

TEST_CASE("terrain lowers to the lowest point under the footprint")
{
	FlatMap(2);
	SetTileHeight(TileXY(11, 11), 3);
	Blueprint::Blueprint bp;
	bp.w = 1;
	bp.h = 1;
	bp.heights = {0, 0, 0, 0};
	auto steps = PlanBuild(bp, TileXY(10, 10), true);
	REQUIRE(steps.size() == 1);
	CHECK(steps[0].tile == TileXY(11, 11));
	CHECK(steps[0].raise == -1);
}

TEST_CASE("terrain clamped to map limits")
{
	FlatMap(29);
	Blueprint::Blueprint bp;
	bp.w = 1;
	bp.h = 1;
	bp.heights = {0, 5, 5, 5};
	auto steps = PlanBuild(bp, TileXY(10, 10), true);
	for (const Step &s : steps) {
		CHECK(TileHeight(s.tile) + s.raise <= 30);
	}
	CHECK(steps.size() == 3);
}

TEST_CASE("plan steps clipped at map edge")
{
	FlatMap(1);
	Blueprint::Blueprint bp;
	bp.w = 4;
	bp.h = 4;
	for (int16_t y = 0; y < 4; y++) {
		for (int16_t x = 0; x < 4; x++) {
			Item t{};
			t.type = ItemType::Track;
			t.dx = x;
			t.dy = y;
			t.tracks = TRACK_BIT_Y;
			bp.items.push_back(t);
		}
	}
	bp.heights.assign(25, 1);
	auto steps = PlanBuild(bp, TileXY(62, 62), true);
	CHECK_FALSE(steps.empty());
	for (const Step &s : steps) {
		CHECK(TileX(s.tile) <= Map::MaxX());
		CHECK(TileY(s.tile) <= Map::MaxY());
		CHECK(TileX(s.tile) >= 62);
		CHECK(TileY(s.tile) >= 62);
	}
}

TEST_CASE("terrain off")
{
	FlatMap(2);
	Blueprint::Blueprint bp = TrackAndSignal(1, 1);
	bp.heights = {0, 3, 0, 0};
	for (const Step &s : PlanBuild(bp, TileXY(10, 10), false)) CHECK(s.kind != Step::Kind::Terraform);
}

TEST_CASE("bridge with far head off map is dropped")
{
	FlatMap(1);
	Blueprint::Blueprint bp;
	bp.w = 6;
	bp.h = 1;
	Item b{};
	b.type = ItemType::Bridge;
	b.dir = DIAGDIR_SW;
	b.length = 5;
	bp.items.push_back(b);
	CHECK(PlanBuild(bp, TileXY(60, 10), false).empty());
	CHECK(PlanBuild(bp, TileXY(10, 10), false).size() == 1);
}
