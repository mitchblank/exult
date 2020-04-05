/*
Copyright (C) 2011-2013 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "SDL_events.h"

#include "ItemMenu_gump.h"

#include "exult.h"
#include "actors.h"
#include "gamewin.h"
#include "Gump_manager.h"
#include "Gump_button.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "Text_button.h"

using Itemmenu_button = CallbackTextButton<Itemmenu_gump>;
using Itemmenu_object = CallbackTextButton<Itemmenu_gump, Game_object*>;
using ObjectParams = Itemmenu_object::CallbackParams;

void Itemmenu_gump::select_object(Game_object* obj) {
	objectSelected = obj;
	auto it = objects.find(obj);
	assert(it != objects.cend());
	objectSelectedClickXY = it->second;
	close();
}

int clamp(int val, int low, int high) {
	assert(!(high < low));
	if (val < low) {
		return low;
	}
	if (high < val) {
		return high;
	}
	return val;
}

void Itemmenu_gump::fix_position(int num_elements) {
	int w = Game_window::get_instance()->get_width();
	int h = Game_window::get_instance()->get_height();
	int menu_height = clamp(num_elements * button_spacing_y, 0, h);
	x = clamp(x, 0, w - 100);
	y = clamp(y, 0, h - menu_height);
}

Itemmenu_gump::Itemmenu_gump(Game_object_map_xy *mobjxy, int cx, int cy)
	: Modal_gump(nullptr, cx, cy, EXULT_FLX_TRANSPARENTMENU_SHP, SF_EXULT_FLX) {
	objectSelected = nullptr;
	objectSelectedClickXY = {-1, -1};
	objectAction = no_action;
	//set_object_area(Rectangle(0, 0, 0, 0), -1, -1);//++++++ ???
	int btop = 0;
	int maxh = Game_window::get_instance()->get_height() - 2 * button_spacing_y;
	for (Game_object_map_xy::const_iterator it = mobjxy->begin();
	        it != mobjxy->end() && btop < maxh; it++, btop += button_spacing_y) {
		Game_object *o = it->first;
		objects[o] = it->second;
		buttons.push_back(std::make_unique<Itemmenu_object>(this, &Itemmenu_gump::select_object,
		                                                    ObjectParams{o}, o->get_name().c_str(), 10, btop, 59, 20));
	}
	buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::cancel_menu, "Cancel", 10, btop, 59, 20));
	fix_position(buttons.size());
}

Itemmenu_gump::Itemmenu_gump(Game_object *obj, int ox, int oy, int cx, int cy)
	: Modal_gump(nullptr, cx, cy, EXULT_FLX_TRANSPARENTMENU_SHP, SF_EXULT_FLX) {
	objectSelected = obj;
	objectAction = item_menu;
	objectSelectedClickXY = {ox, oy};
	int btop = 0;
	buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::set_use, "Use", 10, btop, 59, 20));
	btop += button_spacing_y;
	buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::set_pickup, "Pickup", 10, btop, 59, 20));
	btop += button_spacing_y;
	buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::set_move, "Move", 10, btop, 59, 20));
	btop += button_spacing_y;
	buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::cancel_menu, "Cancel", 10, btop, 59, 20));
	fix_position(buttons.size());
}

Itemmenu_gump::~Itemmenu_gump() {
	postCloseActions();
}

void Itemmenu_gump::paint() {
	Gump::paint();
	for (auto& btn : buttons) {
		btn->paint();
	}
	gwin->set_painted();
}

bool Itemmenu_gump::mouse_down(int mx, int my, int button) {
	// Only left and right buttons
	if (button != 1 && button != 3) {
		return false;
	}
	// We'll eat the mouse down if we've already got a button down
	if (pushed) {
		return true;
	}
	// First try checkmark
	pushed = Gump::on_button(mx, my);
	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}
	// On a button?
	if (pushed && !pushed->push(button)) {
		pushed = nullptr;
	}
	return button == 1 || pushed != nullptr;
}

bool Itemmenu_gump::mouse_up(int mx, int my, int button) {
	// Not Pushing a button?
	if (!pushed) {
		close();
		return false;
	}
	if (pushed->get_pushed() != button) {
		return button == 1;
	}
	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my))
		res = pushed->activate(button);
	pushed = nullptr;
	return res;
}

void Itemmenu_gump::postCloseActions() {
	if (!objectSelected) {
		return;
	}
	Game_window *gwin = Game_window::get_instance();
	switch (objectAction) {
	case use_item:
		objectSelected->activate();
		break;
	case pickup_item: {
		Main_actor *ava = gwin->get_main_actor();
		Tile_coord avaLoc = ava->get_tile();
		int avaX = (avaLoc.tx - gwin->get_scrolltx()) * c_tilesize;
		int avaY = (avaLoc.ty - gwin->get_scrollty()) * c_tilesize;
		auto tmpObj = gwin->find_object(avaX, avaY);
		if (tmpObj != ava) {
			// Avatar isn't in a good spot...
			// Let's give up for now :(
			break;
		}
		if (gwin->start_dragging(objectSelectedClickXY.x, objectSelectedClickXY.y)
		        && gwin->drag(avaX, avaY)) {
			gwin->drop_dragged(avaX, avaY, true);
		}
		break;
	}
	case move_item: {
		int tmpX, tmpY;
		if (Get_click(tmpX, tmpY, Mouse::greenselect, nullptr, true)
		        && gwin->start_dragging(objectSelectedClickXY.x, objectSelectedClickXY.y)
		        && gwin->drag(tmpX, tmpY)) {
			gwin->drop_dragged(tmpX, tmpY, true);
		}
		break;
	}
	case no_action: {
		// Make sure menu is visible on the screen
		// This will draw a selection menu for the object
		Itemmenu_gump itemgump(objectSelected, objectSelectedClickXY.x, objectSelectedClickXY.y, x, y);
		gwin->get_gump_man()->do_modal_gump(&itemgump, Mouse::hand);
		break;
	}
	case item_menu:
		break;
	}
}
