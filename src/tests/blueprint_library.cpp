/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_library.cpp Tests for blueprint JSON and the on-disk library. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../blueprint/blueprint_library.h"

#include <filesystem>
#include <fstream>
#include <random>

#include "../core/format.hpp"

#include "../safeguards.h"

/* No "using namespace": MSVC finds Blueprint::Blueprint ambiguous with it. */
using Blueprint::Item;
using Blueprint::ItemType;
using Blueprint::ToJson;
using Blueprint::FromJson;
using Blueprint::SanitiseFileName;
using Blueprint::Library;

namespace {

struct TempDir {
	std::filesystem::path path;
	TempDir()
	{
		std::random_device rd;
		this->path = std::filesystem::temp_directory_path() / fmt::format("bp_test_{}", rd());
		std::filesystem::create_directories(this->path);
	}
	~TempDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(this->path, ec);
	}
	std::string str() const { return this->path.string() + "/"; }
};

Blueprint::Blueprint MakeFull(std::string name)
{
	Blueprint::Blueprint bp;
	bp.name = std::move(name);
	bp.w = 3;
	bp.h = 2;

	Item t{};
	t.type = ItemType::Track;
	t.tracks = TRACK_BIT_X | TRACK_BIT_UPPER;
	bp.items.push_back(t);

	Item s{};
	s.type = ItemType::Signal;
	s.dx = 1;
	s.track = TRACK_Y;
	s.sig_type = SIGTYPE_PBS;
	s.sig_var = SIG_SEMAPHORE;
	s.sig_dirs = 3;
	bp.items.push_back(s);

	Item d{};
	d.type = ItemType::Depot;
	d.dx = 2;
	d.dy = 1;
	d.dir = DIAGDIR_NW;
	bp.items.push_back(d);

	Item b{};
	b.type = ItemType::Bridge;
	b.dir = DIAGDIR_SE;
	b.length = 2;
	b.bridge_type = 5;
	bp.items.push_back(b);

	Item tu{};
	tu.type = ItemType::Tunnel;
	tu.dx = 2;
	tu.dir = DIAGDIR_SW;
	tu.length = 7;
	bp.items.push_back(tu);

	Item st{};
	st.type = ItemType::Station;
	st.axis = AXIS_Y;
	st.platforms = 2;
	st.plat_len = 2;
	st.spec_class = static_cast<StationClassID>(1);
	st.spec_index = 4;
	st.station_group = 1;
	bp.items.push_back(st);

	bp.heights = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
	return bp;
}

} // namespace

TEST_CASE("json round trip")
{
	Blueprint::Blueprint bp = MakeFull("Junction");
	nlohmann::json j = ToJson(bp);
	CHECK(j["format"] == 1);
	auto back = FromJson(j);
	REQUIRE(back.has_value());
	CHECK(*back == bp);
}

TEST_CASE("future format rejected")
{
	nlohmann::json j = ToJson(MakeFull("x"));
	j["format"] = 2;
	CHECK_FALSE(FromJson(j).has_value());
}

TEST_CASE("height grid size mismatch rejected")
{
	nlohmann::json j = ToJson(MakeFull("x"));
	j["heights"].push_back(1);
	CHECK_FALSE(FromJson(j).has_value());
}

TEST_CASE("library skips invalid files")
{
	TempDir dir;
	{
		Library lib(dir.str());
		REQUIRE(lib.Save(MakeFull("Good")));
	}
	std::ofstream(dir.path / "bad.json") << "{";
	std::ofstream(dir.path / "other.txt") << "not a blueprint";

	Library lib(dir.str());
	lib.Reload();
	REQUIRE(lib.All().size() == 1);
	CHECK(lib.All()[0].name == "Good");
}

TEST_CASE("sanitised unique filenames")
{
	CHECK(SanitiseFileName("a/b") == "a_b");
	CHECK(SanitiseFileName("x\\y:z*?\"<>|") == "x_y_z______");

	TempDir dir;
	Library lib(dir.str());
	REQUIRE(lib.Save(MakeFull("a/b")));
	REQUIRE(lib.Save(MakeFull("a/b")));
	CHECK(std::filesystem::exists(dir.path / "a_b.json"));
	CHECK(std::filesystem::exists(dir.path / "a_b (2).json"));
	CHECK(lib.All().size() == 2);
}

TEST_CASE("persists across instances")
{
	TempDir dir;
	{
		Library lib(dir.str());
		REQUIRE(lib.Save(MakeFull("Keep me")));
	}
	Library lib(dir.str());
	lib.Reload();
	REQUIRE(lib.All().size() == 1);
	CHECK(lib.All()[0] == MakeFull("Keep me"));
}

TEST_CASE("rename and remove")
{
	TempDir dir;
	Library lib(dir.str());
	REQUIRE(lib.Save(MakeFull("b")));
	REQUIRE(lib.Save(MakeFull("A")));
	/* sorted case-insensitively */
	CHECK(lib.All()[0].name == "A");

	REQUIRE(lib.Rename(0, "c"));
	CHECK(lib.All()[0].name == "b");
	CHECK(lib.All()[1].name == "c");

	REQUIRE(lib.Remove(0));
	REQUIRE(lib.All().size() == 1);
	CHECK(lib.All()[0].name == "c");

	Library again(dir.str());
	again.Reload();
	REQUIRE(again.All().size() == 1);
	CHECK(again.All()[0].name == "c");
}
