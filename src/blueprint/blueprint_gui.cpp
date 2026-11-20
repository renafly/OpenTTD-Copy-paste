/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint_gui.cpp Blueprint library window and placement tool. */

#include "../stdafx.h"
#include "blueprint_gui.h"
#include "blueprint.h"
#include "blueprint_capture.h"
#include "blueprint_exec.h"
#include "blueprint_library.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../widget_type.h"
#include "../gfx_func.h"
#include "../strings_func.h"
#include "../textbuf_gui.h"
#include "../tilehighlight_func.h"
#include "../viewport_func.h"
#include "../company_func.h"
#include "../error.h"
#include "../hotkeys.h"
#include "../tilearea_type.h"
#include "../map_func.h"
#include "../zoom_func.h"

#include "table/strings.h"
#include "../table/sprites.h"

#include "../safeguards.h"

namespace Blueprint {

/** Longest template name, in characters. */
static constexpr uint MAX_NAME_CHARS = 64;

static bool _include_terrain = true; ///< "Include terrain" toggle; kept for the session only.

struct BlueprintLibraryWindow : Window {
	enum class Mode : uint8_t { None, Copy, Place };
	enum class Query : uint8_t { None, NewName, Rename };

	Scrollbar *vscroll = nullptr;
	int selected = -1;          ///< Selected row in the library, or -1.
	Mode mode = Mode::None;
	Query query = Query::None;
	Blueprint pending;          ///< Captured blueprint waiting for its name.
	Blueprint placing;          ///< Rotated copy of the template being placed.
	int line_height = 0;

	BlueprintLibraryWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_BPL_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->OnInvalidateData();
	}

	void Close(int data = 0) override
	{
		if (this->mode != Mode::None) ResetObjectToPlace();
		this->Window::Close(data);
	}

	const std::vector<Blueprint> &Templates() const { return GetLibrary().All(); }

	bool HasSelection() const { return this->selected >= 0 && static_cast<size_t>(this->selected) < this->Templates().size(); }

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!this->HasSelection()) this->selected = this->Templates().empty() ? -1 : 0;
		this->vscroll->SetCount(this->Templates().size());
		bool can_build = _local_company != COMPANY_SPECTATOR && _game_mode == GM_NORMAL;
		this->SetWidgetDisabledState(WID_BPL_NEW, !can_build);
		this->SetWidgetDisabledState(WID_BPL_PLACE, !can_build || !this->HasSelection());
		this->SetWidgetDisabledState(WID_BPL_RENAME, !this->HasSelection());
		this->SetWidgetDisabledState(WID_BPL_DELETE, !this->HasSelection());
		this->SetWidgetDisabledState(WID_BPL_ROTATE, this->mode != Mode::Place);
		this->SetWidgetLoweredState(WID_BPL_NEW, this->mode == Mode::Copy);
		this->SetWidgetLoweredState(WID_BPL_PLACE, this->mode == Mode::Place);
		this->SetWidgetLoweredState(WID_BPL_TERRAIN, _include_terrain);
		this->SetDirty();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget != WID_BPL_LIST) return;
		this->line_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.matrix.Vertical();
		resize.height = this->line_height;
		fill.height = this->line_height;
		size.height = std::max<uint>(size.height, 8 * this->line_height + WidgetDimensions::scaled.framerect.Vertical());
		size.width = std::max<uint>(size.width, ScaleGUITrad(260));
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BPL_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_BPL_LIST) return;
		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
		const auto &all = this->Templates();
		if (all.empty()) {
			DrawStringMultiLine(tr, STR_BLUEPRINT_LIST_EMPTY);
			return;
		}
		auto [first, last] = this->vscroll->GetVisibleRangeIterators(all);
		for (auto it = first; it != last; ++it) {
			int row = static_cast<int>(std::distance(all.begin(), it));
			Rect line = tr.WithHeight(this->line_height);
			if (row == this->selected) GfxFillRect(line.left, line.top, line.right, line.bottom, PC_DARK_GREY);
			Rect text = line.Shrink(WidgetDimensions::scaled.matrix);
			StringID str = it->HasTerrain() ? STR_BLUEPRINT_LIST_ITEM_TERRAIN : STR_BLUEPRINT_LIST_ITEM;
			DrawString(text.left, text.right, text.top, GetString(str, it->name, it->w, it->h));
			tr.top += this->line_height;
		}
	}

	void StopMode()
	{
		if (this->mode == Mode::None) return;
		this->mode = Mode::None;
		ResetObjectToPlace();
	}

	void StartCopy()
	{
		this->StopMode();
		SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_RECT, this);
		this->mode = Mode::Copy;
		this->OnInvalidateData();
	}

	void StartPlace(size_t index)
	{
		if (index >= this->Templates().size()) return;
		if (_local_company == COMPANY_SPECTATOR || _game_mode != GM_NORMAL) return;
		this->StopMode();
		this->selected = static_cast<int>(index);
		this->placing = this->Templates()[index];
		SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_RECT, this);
		SetTileSelectSize(this->placing.w, this->placing.h);
		this->mode = Mode::Place;
		this->vscroll->ScrollTowards(this->selected);
		this->OnInvalidateData();
	}

	void Rotate()
	{
		if (this->mode != Mode::Place) return;
		this->placing = this->placing.Rotated();
		SetTileSelectSize(this->placing.w, this->placing.h);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, int click_count) override
	{
		switch (widget) {
			case WID_BPL_LIST: {
				auto row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_BPL_LIST, WidgetDimensions::scaled.framerect.top, this->line_height);
				if (row == Scrollbar::npos || static_cast<size_t>(row) >= this->Templates().size()) break;
				this->selected = static_cast<int>(row);
				if (click_count > 1) {
					this->StartPlace(row);
				} else if (this->mode == Mode::Place) {
					this->StartPlace(row);
				}
				this->OnInvalidateData();
				break;
			}

			case WID_BPL_NEW:
				if (this->mode == Mode::Copy) {
					this->StopMode();
				} else {
					this->StartCopy();
				}
				this->OnInvalidateData();
				break;

			case WID_BPL_PLACE:
				if (this->mode == Mode::Place) {
					this->StopMode();
				} else if (this->HasSelection()) {
					this->StartPlace(this->selected);
				}
				this->OnInvalidateData();
				break;

			case WID_BPL_ROTATE:
				this->Rotate();
				break;

			case WID_BPL_RENAME:
				if (!this->HasSelection()) break;
				this->query = Query::Rename;
				ShowQueryString(this->Templates()[this->selected].name, STR_BLUEPRINT_QUERY_RENAME, MAX_NAME_CHARS, this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				break;

			case WID_BPL_DELETE:
				if (!this->HasSelection()) break;
				ShowQuery(GetEncodedString(STR_BLUEPRINT_DELETE_CAPTION),
						GetEncodedString(STR_BLUEPRINT_DELETE_QUERY, this->Templates()[this->selected].name),
						this, DeleteCallback);
				break;

			case WID_BPL_TERRAIN:
				_include_terrain = !_include_terrain;
				this->OnInvalidateData();
				break;
		}
	}

	static void DeleteCallback(Window *w, bool confirmed)
	{
		auto *lw = dynamic_cast<BlueprintLibraryWindow *>(w);
		if (!confirmed || lw == nullptr || !lw->HasSelection()) return;
		if (lw->mode == Mode::Place) lw->StopMode();
		GetLibrary().Remove(lw->selected);
		if (lw->selected > 0 && static_cast<size_t>(lw->selected) >= GetLibrary().All().size()) lw->selected--;
		lw->OnInvalidateData();
	}

	void SelectByName(const std::string &name)
	{
		const auto &all = this->Templates();
		for (size_t i = 0; i < all.size(); i++) {
			if (all[i].name == name) {
				this->selected = static_cast<int>(i);
				this->vscroll->ScrollTowards(this->selected);
				return;
			}
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		Query q = this->query;
		this->query = Query::None;
		if (!str.has_value()) return;
		std::string name = *str;
		if (name.empty()) name = "Blueprint";

		if (q == Query::NewName) {
			this->pending.name = name;
			if (!GetLibrary().Save(this->pending)) {
				ShowErrorMessage(GetEncodedString(STR_BLUEPRINT_ERROR_SAVE), {}, WL_ERROR);
				return;
			}
			this->SelectByName(name);
		} else if (q == Query::Rename && this->HasSelection()) {
			if (!GetLibrary().Rename(this->selected, name)) {
				ShowErrorMessage(GetEncodedString(STR_BLUEPRINT_ERROR_SAVE), {}, WL_ERROR);
				return;
			}
			this->SelectByName(name);
		}
		this->OnInvalidateData();
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		switch (this->mode) {
			case Mode::Copy:
				VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_BUILD_OBJECT);
				break;

			case Mode::Place:
				if (IsExecuting()) {
					ShowErrorMessage(GetEncodedString(STR_BLUEPRINT_ERROR_BUSY), {}, WL_INFO);
					break;
				}
				Execute(this->placing, tile, GetRailToolbarRailType(), _include_terrain);
				break;

			case Mode::None:
				break;
		}
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, Point pt) override
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (pt.x == -1 || this->mode != Mode::Copy) return;

		TileArea area(start_tile, end_tile);
		if (area.w > MAX_COPY_SIDE || area.h > MAX_COPY_SIDE) {
			ShowErrorMessage(GetEncodedString(STR_BLUEPRINT_ERROR_TOO_LARGE, MAX_COPY_SIDE, MAX_COPY_SIDE), {}, WL_INFO);
			return;
		}

		/* Terrain is always recorded; the toggle decides whether it is used when placing. */
		this->pending = Capture(start_tile, end_tile, _local_company, true);
		if (this->pending.items.empty()) {
			ShowErrorMessage(GetEncodedString(STR_BLUEPRINT_ERROR_EMPTY), {}, WL_INFO);
			return;
		}

		this->StopMode();
		this->OnInvalidateData();
		this->query = Query::NewName;
		ShowQueryString({}, STR_BLUEPRINT_QUERY_NAME, MAX_NAME_CHARS, this, CS_ALPHANUMERAL, {QueryStringFlag::LengthIsInChars});
	}

	void OnPlaceObjectAbort() override
	{
		this->mode = Mode::None;
		this->OnInvalidateData();
	}

	EventState OnHotkey(int hotkey) override
	{
		if (hotkey == WID_BPL_ROTATE && this->mode == Mode::Place) {
			this->Rotate();
			return ES_HANDLED;
		}
		return this->Window::OnHotkey(hotkey);
	}

	static inline HotkeyList hotkeys{"blueprint_library", {
		Hotkey('R', "rotate", WID_BPL_ROTATE),
		Hotkey('N', "new_copy", WID_BPL_NEW),
	}};
};

static constexpr std::initializer_list<NWidgetPart> _nested_blueprint_library_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_BPL_CAPTION), SetStringTip(STR_BLUEPRINT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BPL_LIST), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WID_BPL_SCROLLBAR), SetToolTip(STR_BLUEPRINT_LIST_TOOLTIP), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BPL_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WID_BPL_NEW), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_BLUEPRINT_NEW, STR_BLUEPRINT_NEW_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WID_BPL_PLACE), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_BLUEPRINT_PLACE, STR_BLUEPRINT_PLACE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_GREEN, WID_BPL_ROTATE), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_BLUEPRINT_ROTATE, STR_BLUEPRINT_ROTATE_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_GREEN, WID_BPL_RENAME), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_BLUEPRINT_RENAME, STR_BLUEPRINT_RENAME_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_GREEN, WID_BPL_DELETE), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_BLUEPRINT_DELETE, STR_BLUEPRINT_DELETE_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WID_BPL_TERRAIN), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_BLUEPRINT_TERRAIN, STR_BLUEPRINT_TERRAIN_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
};

static WindowDesc _blueprint_library_desc(
	WDP_AUTO, "blueprint_library", 0, 0,
	WC_BLUEPRINT_LIBRARY, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_blueprint_library_widgets,
	&BlueprintLibraryWindow::hotkeys
);

Window *ShowLibraryWindow()
{
	GetLibrary().Reload();
	Window *w = AllocateWindowDescFront<BlueprintLibraryWindow>(_blueprint_library_desc, 0);
	if (w == nullptr) {
		w = FindWindowById(WC_BLUEPRINT_LIBRARY, 0);
		if (w != nullptr) w->InvalidateData();
	}
	return w;
}

void StartPlacement(size_t library_index)
{
	Window *w = ShowLibraryWindow();
	auto *lw = dynamic_cast<BlueprintLibraryWindow *>(w);
	if (lw == nullptr) return;
	if (library_index >= GetLibrary().All().size()) {
		ShowErrorMessage(GetEncodedString(STR_BLUEPRINT_ERROR_NO_TEMPLATE, library_index + 1), {}, WL_INFO);
		return;
	}
	lw->StartPlace(library_index);
}

} // namespace Blueprint
