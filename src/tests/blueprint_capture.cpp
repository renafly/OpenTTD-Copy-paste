/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_capture.cpp Tests for capturing blueprints from the map. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../blueprint/blueprint_capture.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../rail_map.h"
#include "../clear_map.h"
#include "../track_func.h"

#include "../safeguards.h"

using namespace Blueprint;

static void FlatMap()
{
	Map::Allocate(64, 64);
	for (TileIndex t : Map::Iterate()) {
		MakeClear(t, CLEAR_GRASS, 3);
		SetTileHeight(t, 0);
	}
}

static const Item *Find(const Blueprint::Blueprint &bp, ItemType type)
{
	for (const Item &it : bp.items) {
		if (it.type == type) return &it;
	}
	return nullptr;
}

TEST_CASE("captures track and depot")
{
	FlatMap();
	MakeRailNormal(TileXY(10, 10), CompanyID::Begin(), TRACK_BIT_X, RAILTYPE_RAIL);
	MakeRailDepot(TileXY(11, 10), CompanyID::Begin(), DepotID::Begin(), DIAGDIR_SW, RAILTYPE_RAIL);

	Blueprint::Blueprint bp = Capture(TileXY(10, 10), TileXY(12, 11), CompanyID::Begin(), false);
	CHECK(bp.w == 3);
	CHECK(bp.h == 2);
	CHECK_FALSE(bp.HasTerrain());

	const Item *track = Find(bp, ItemType::Track);
	REQUIRE(track != nullptr);
	CHECK(track->dx == 0);
	CHECK(track->dy == 0);
	CHECK(track->tracks == TRACK_BIT_X);

	const Item *depot = Find(bp, ItemType::Depot);
	REQUIRE(depot != nullptr);
	CHECK(depot->dx == 1);
	CHECK(depot->dy == 0);
	CHECK(depot->dir == DIAGDIR_SW);
}

TEST_CASE("captures signal")
{
	FlatMap();
	TileIndex t = TileXY(5, 5);
	MakeRailNormal(t, CompanyID::Begin(), TRACK_BIT_X, RAILTYPE_RAIL);
	SetHasSignals(t, true);
	SetSignalType(t, TRACK_X, SIGTYPE_PBS_ONEWAY);
	SetSignalVariant(t, TRACK_X, SIG_ELECTRIC);
	SetPresentSignals(t, SignalAlongTrackdir(ReverseTrackdir(TrackToTrackdir(TRACK_X))));

	Blueprint::Blueprint bp = Capture(t, t, CompanyID::Begin(), false);
	const Item *sig = Find(bp, ItemType::Signal);
	REQUIRE(sig != nullptr);
	CHECK(sig->track == TRACK_X);
	CHECK(sig->sig_type == SIGTYPE_PBS_ONEWAY);
	CHECK(sig->sig_var == SIG_ELECTRIC);
	CHECK(sig->sig_dirs == 2);
	CHECK(Find(bp, ItemType::Track) != nullptr);
}

TEST_CASE("ignores other owners")
{
	FlatMap();
	MakeRailNormal(TileXY(3, 3), CompanyID{1}, TRACK_BIT_Y, RAILTYPE_RAIL);
	Blueprint::Blueprint bp = Capture(TileXY(3, 3), TileXY(4, 4), CompanyID::Begin(), false);
	CHECK(bp.items.empty());
}

TEST_CASE("relative heights")
{
	FlatMap();
	/* area (20,20)-(20,20): corners (20,20) (21,20) (20,21) (21,21) */
	SetTileHeight(TileXY(20, 20), 3);
	SetTileHeight(TileXY(21, 20), 4);
	SetTileHeight(TileXY(20, 21), 3);
	SetTileHeight(TileXY(21, 21), 4);

	Blueprint::Blueprint bp = Capture(TileXY(20, 20), TileXY(20, 20), CompanyID::Begin(), true);
	REQUIRE(bp.HasTerrain());
	CHECK(bp.heights == std::vector<uint8_t>{0, 1, 0, 1});
}

TEST_CASE("area corners in any order")
{
	FlatMap();
	MakeRailNormal(TileXY(10, 10), CompanyID::Begin(), TRACK_BIT_X, RAILTYPE_RAIL);
	Blueprint::Blueprint bp = Capture(TileXY(12, 11), TileXY(10, 10), CompanyID::Begin(), false);
	CHECK(bp.w == 3);
	CHECK(bp.h == 2);
	REQUIRE(bp.items.size() == 1);
	CHECK(bp.items[0].dx == 0);
}
