/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_library.h JSON (de)serialisation and the on-disk template library. */

#ifndef BLUEPRINT_BLUEPRINT_LIBRARY_H
#define BLUEPRINT_BLUEPRINT_LIBRARY_H

#include "blueprint.h"
#include "../3rdparty/nlohmann/json.hpp"

namespace Blueprint {

/** Current template file format. Files with a higher number are ignored. */
static constexpr int FILE_FORMAT = 1;

nlohmann::json ToJson(const Blueprint &bp);
std::optional<Blueprint> FromJson(const nlohmann::json &j);
std::string SanitiseFileName(std::string_view name);

/** A folder of template files, one JSON file per template. */
class Library {
public:
	explicit Library(std::string dir);

	/** Re-read all templates from disk; invalid files are skipped. */
	void Reload();

	/** @return All templates, sorted by name (case-insensitive). */
	const std::vector<Blueprint> &All() const { return this->blueprints; }

	bool Save(Blueprint bp);
	bool Rename(size_t index, std::string_view new_name);
	bool Remove(size_t index);

	const std::string &Directory() const { return this->dir; }

private:
	std::string dir;
	std::vector<Blueprint> blueprints;
	std::vector<std::string> files; ///< File name (without directory) for each entry in #blueprints.

	std::string UniqueFileName(std::string_view name) const;
	bool WriteFile(const std::string &file, const Blueprint &bp) const;
	void Sort();
};

/** @return The library in the personal data folder ("blueprints" next to "save"). */
Library &GetLibrary();

} // namespace Blueprint

#endif /* BLUEPRINT_BLUEPRINT_LIBRARY_H */
