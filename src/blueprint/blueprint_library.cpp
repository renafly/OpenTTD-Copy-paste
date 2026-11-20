/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_library.cpp JSON (de)serialisation and the on-disk template library. */

#include "../stdafx.h"
#include "blueprint_library.h"
#include "../debug.h"
#include "../fileio_func.h"
#include "../string_func.h"
#include "../core/format.hpp"

#include <filesystem>

#include "../safeguards.h"

namespace Blueprint {

static const char *const FILE_EXTENSION = ".json";

nlohmann::json ToJson(const Blueprint &bp)
{
	nlohmann::json j;
	j["format"] = FILE_FORMAT;
	j["name"] = bp.name;
	j["w"] = bp.w;
	j["h"] = bp.h;

	nlohmann::json items = nlohmann::json::array();
	for (const Item &it : bp.items) {
		nlohmann::json ji;
		ji["type"] = static_cast<int>(it.type);
		ji["dx"] = it.dx;
		ji["dy"] = it.dy;
		switch (it.type) {
			case ItemType::Track:
				ji["tracks"] = static_cast<int>(it.tracks);
				break;
			case ItemType::Signal:
				ji["track"] = static_cast<int>(it.track);
				ji["sig_type"] = static_cast<int>(it.sig_type);
				ji["sig_var"] = static_cast<int>(it.sig_var);
				ji["sig_dirs"] = it.sig_dirs;
				break;
			case ItemType::Depot:
				ji["dir"] = static_cast<int>(it.dir);
				break;
			case ItemType::Bridge:
				ji["bridge_type"] = it.bridge_type;
				[[fallthrough]];
			case ItemType::Tunnel:
				ji["dir"] = static_cast<int>(it.dir);
				ji["length"] = it.length;
				break;
			case ItemType::Station:
				ji["axis"] = static_cast<int>(it.axis);
				ji["platforms"] = it.platforms;
				ji["plat_len"] = it.plat_len;
				ji["spec_class"] = static_cast<int>(it.spec_class);
				ji["spec_index"] = it.spec_index;
				ji["station_group"] = it.station_group;
				break;
		}
		items.push_back(std::move(ji));
	}
	j["items"] = std::move(items);
	j["heights"] = bp.heights;
	return j;
}

/** Read an integer field and check it lies in [lo, hi]. Throws on missing/wrong type. */
template <typename T>
static T GetRanged(const nlohmann::json &j, const char *key, long long lo, long long hi, bool &ok)
{
	long long v = j.at(key).get<long long>();
	if (v < lo || v > hi) ok = false;
	return static_cast<T>(v);
}

std::optional<Blueprint> FromJson(const nlohmann::json &j)
{
	try {
		bool ok = true;
		int format = j.at("format").get<int>();
		if (format < 1 || format > FILE_FORMAT) return std::nullopt;

		Blueprint bp;
		bp.name = j.at("name").get<std::string>();
		bp.w = GetRanged<uint16_t>(j, "w", 1, 4096, ok);
		bp.h = GetRanged<uint16_t>(j, "h", 1, 4096, ok);
		if (!ok) return std::nullopt;

		for (const auto &ji : j.at("items")) {
			Item it{};
			it.type = GetRanged<ItemType>(ji, "type", 0, static_cast<int>(ItemType::Station), ok);
			it.dx = GetRanged<int16_t>(ji, "dx", 0, bp.w - 1, ok);
			it.dy = GetRanged<int16_t>(ji, "dy", 0, bp.h - 1, ok);
			if (!ok) return std::nullopt;
			switch (it.type) {
				case ItemType::Track:
					it.tracks = GetRanged<TrackBits>(ji, "tracks", 1, TRACK_BIT_ALL, ok);
					break;
				case ItemType::Signal:
					it.track = GetRanged<Track>(ji, "track", TRACK_BEGIN, TRACK_END - 1, ok);
					it.sig_type = GetRanged<SignalType>(ji, "sig_type", SIGTYPE_BLOCK, SIGTYPE_END - 1, ok);
					it.sig_var = GetRanged<SignalVariant>(ji, "sig_var", SIG_ELECTRIC, SIG_SEMAPHORE, ok);
					it.sig_dirs = GetRanged<uint8_t>(ji, "sig_dirs", 1, 3, ok);
					break;
				case ItemType::Depot:
					it.dir = GetRanged<DiagDirection>(ji, "dir", DIAGDIR_BEGIN, DIAGDIR_END - 1, ok);
					break;
				case ItemType::Bridge:
					it.bridge_type = GetRanged<BridgeType>(ji, "bridge_type", 0, 255, ok);
					[[fallthrough]];
				case ItemType::Tunnel:
					it.dir = GetRanged<DiagDirection>(ji, "dir", DIAGDIR_BEGIN, DIAGDIR_END - 1, ok);
					it.length = GetRanged<uint16_t>(ji, "length", 1, 4096, ok);
					break;
				case ItemType::Station:
					it.axis = GetRanged<Axis>(ji, "axis", AXIS_X, AXIS_Y, ok);
					it.platforms = GetRanged<uint8_t>(ji, "platforms", 1, 255, ok);
					it.plat_len = GetRanged<uint8_t>(ji, "plat_len", 1, 255, ok);
					it.spec_class = GetRanged<StationClassID>(ji, "spec_class", 0, UINT16_MAX, ok);
					it.spec_index = GetRanged<uint16_t>(ji, "spec_index", 0, UINT16_MAX, ok);
					it.station_group = GetRanged<uint16_t>(ji, "station_group", 0, UINT16_MAX, ok);
					break;
			}
			if (!ok) return std::nullopt;
			auto [sx, sy] = it.Footprint();
			if (it.dx + sx > bp.w || it.dy + sy > bp.h) return std::nullopt;
			bp.items.push_back(it);
		}

		for (const auto &h : j.at("heights")) {
			int v = h.get<int>();
			if (v < 0 || v > UINT8_MAX) return std::nullopt;
			bp.heights.push_back(static_cast<uint8_t>(v));
		}
		if (!bp.heights.empty() && bp.heights.size() != static_cast<size_t>(bp.w + 1) * (bp.h + 1)) return std::nullopt;

		return bp;
	} catch (const nlohmann::json::exception &) {
		return std::nullopt;
	}
}

std::string SanitiseFileName(std::string_view name)
{
	std::string res;
	res.reserve(name.size());
	for (char c : name) {
		bool bad = static_cast<unsigned char>(c) < 0x20 || std::string_view{"/\\:*?\"<>|"}.find(c) != std::string_view::npos;
		res.push_back(bad ? '_' : c);
	}
	/* Windows does not like trailing dots or spaces, and an empty name is no name. */
	while (!res.empty() && (res.back() == '.' || res.back() == ' ')) res.pop_back();
	if (res.empty()) res = "blueprint";
	return res;
}

Library::Library(std::string dir) : dir(std::move(dir))
{
	if (!this->dir.empty() && this->dir.back() != '/' && this->dir.back() != PATHSEPCHAR) this->dir += PATHSEPCHAR;
}

static std::optional<std::string> ReadWholeFile(const std::string &path)
{
	auto f = FileHandle::Open(path, "rb");
	if (!f.has_value()) return std::nullopt;
	std::string data;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), *f)) > 0) data.append(buf, n);
	return data;
}

void Library::Reload()
{
	this->blueprints.clear();
	this->files.clear();

	std::error_code ec;
	std::filesystem::path path = OTTD2FS(this->dir);
	if (!std::filesystem::is_directory(path, ec)) return;

	for (const auto &entry : std::filesystem::directory_iterator(path, ec)) {
		if (!entry.is_regular_file(ec)) continue;
		if (entry.path().extension() != FILE_EXTENSION) continue;

		std::string file = FS2OTTD(entry.path().filename().native());
		auto data = ReadWholeFile(this->dir + file);
		if (!data.has_value()) continue;

		std::optional<Blueprint> bp;
		try {
			bp = FromJson(nlohmann::json::parse(*data));
		} catch (const nlohmann::json::exception &) {
			bp = std::nullopt;
		}
		if (!bp.has_value()) {
			Debug(misc, 0, "Blueprint: skipping invalid template file '{}'", file);
			continue;
		}
		this->blueprints.push_back(std::move(*bp));
		this->files.push_back(std::move(file));
	}
	this->Sort();
}

void Library::Sort()
{
	std::vector<size_t> order(this->blueprints.size());
	for (size_t i = 0; i < order.size(); i++) order[i] = i;
	std::stable_sort(order.begin(), order.end(), [this](size_t a, size_t b) {
		return StrCompareIgnoreCase(this->blueprints[a].name, this->blueprints[b].name) < 0;
	});

	std::vector<Blueprint> bps;
	std::vector<std::string> fs;
	for (size_t i : order) {
		bps.push_back(std::move(this->blueprints[i]));
		fs.push_back(std::move(this->files[i]));
	}
	this->blueprints = std::move(bps);
	this->files = std::move(fs);
}

std::string Library::UniqueFileName(std::string_view name) const
{
	std::string base = SanitiseFileName(name);
	std::error_code ec;
	for (int i = 1;; i++) {
		std::string file = i == 1 ? base + FILE_EXTENSION : fmt::format("{} ({}){}", base, i, FILE_EXTENSION);
		if (!std::filesystem::exists(OTTD2FS(this->dir + file), ec)) return file;
	}
}

bool Library::WriteFile(const std::string &file, const Blueprint &bp) const
{
	std::error_code ec;
	std::filesystem::create_directories(OTTD2FS(this->dir), ec);

	auto f = FileHandle::Open(this->dir + file, "wb");
	if (!f.has_value()) {
		Debug(misc, 0, "Blueprint: could not write '{}{}'", this->dir, file);
		return false;
	}
	std::string data = ToJson(bp).dump(1, '\t');
	return fwrite(data.data(), 1, data.size(), *f) == data.size();
}

bool Library::Save(Blueprint bp)
{
	std::string file = this->UniqueFileName(bp.name);
	if (!this->WriteFile(file, bp)) return false;
	this->blueprints.push_back(std::move(bp));
	this->files.push_back(std::move(file));
	this->Sort();
	return true;
}

bool Library::Rename(size_t index, std::string_view new_name)
{
	if (index >= this->blueprints.size()) return false;
	Blueprint bp = this->blueprints[index];
	bp.name = std::string{new_name};

	std::string file = this->UniqueFileName(new_name);
	if (!this->WriteFile(file, bp)) return false;

	std::error_code ec;
	std::filesystem::remove(OTTD2FS(this->dir + this->files[index]), ec);

	this->blueprints[index] = std::move(bp);
	this->files[index] = std::move(file);
	this->Sort();
	return true;
}

bool Library::Remove(size_t index)
{
	if (index >= this->blueprints.size()) return false;
	std::error_code ec;
	if (!std::filesystem::remove(OTTD2FS(this->dir + this->files[index]), ec)) return false;
	this->blueprints.erase(this->blueprints.begin() + index);
	this->files.erase(this->files.begin() + index);
	return true;
}

Library &GetLibrary()
{
	static Library library = [] {
		Library lib(FioGetDirectory(SP_PERSONAL_DIR, BASE_DIR) + "blueprints");
		lib.Reload();
		return lib;
	}();
	return library;
}

} // namespace Blueprint
