/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_gui.h Blueprint library window and placement tool. */

#ifndef BLUEPRINT_BLUEPRINT_GUI_H
#define BLUEPRINT_BLUEPRINT_GUI_H

#include "../rail_type.h"
#include "../window_type.h"

/** Widgets of the blueprint library window. */
enum BlueprintLibraryWidgets : WidgetID {
	WID_BPL_CAPTION,   ///< Caption.
	WID_BPL_LIST,      ///< List of templates.
	WID_BPL_SCROLLBAR, ///< Scrollbar of the list.
	WID_BPL_NEW,       ///< Start copying a new template.
	WID_BPL_PLACE,     ///< Start placing the selected template.
	WID_BPL_ROTATE,    ///< Rotate the template being placed.
	WID_BPL_RENAME,    ///< Rename the selected template.
	WID_BPL_DELETE,    ///< Delete the selected template.
	WID_BPL_TERRAIN,   ///< Toggle terrain reproduction.
};

namespace Blueprint {

/** Largest area that can be copied, per side. */
static constexpr uint MAX_COPY_SIDE = 64;

/** Number of "place template N" hotkeys. */
static constexpr int NUM_PLACE_HOTKEYS = 10;

/** Open (or bring to front) the blueprint library window. */
Window *ShowLibraryWindow();

/**
 * Start placing a template from the library, as from a hotkey.
 * @param library_index Position in the library list (0-based).
 */
void StartPlacement(size_t library_index);

} // namespace Blueprint

/** @return The rail type selected in the rail construction toolbar (defined in rail_gui.cpp). */
RailType GetRailToolbarRailType();

#endif /* BLUEPRINT_BLUEPRINT_GUI_H */
