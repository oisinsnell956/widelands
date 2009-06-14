/*
 * Copyright (C) 2002-2004, 2006-2009 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
/*
This file contains the options windows that are displayed when you click on
a building, plus the necessary hook function(s) in the class Building itself.

This is separated out because options windows should _never_ manipulate
buildings directly. Instead, they must send a player command through the Game
class.
*/


#include "constructionsite.h"
#include "game_debug_ui.h"
#include "graphic/graphic.h"
#include "graphic/rendertarget.h"
#include "i18n.h"
#include "wui/interactive_player.h"
#include "maphollowregion.h"
#include "militarysite.h"
#include "economy/request.h"
#include "soldier.h"
#include "trainingsite.h"
#include "tribe.h"
#include "logic/warehouse.h"
#include "waresdisplay.h"
#include "economy/wares_queue.h"

#include "ui_basic/box.h"
#include "ui_basic/button.h"
#include "ui_basic/listselect.h"
#include "ui_basic/progressbar.h"
#include "ui_basic/table.h"
#include "ui_basic/tabpanel.h"
#include "ui_basic/textarea.h"
#include "ui_basic/window.h"

#include "upcast.h"

#include <SDL_types.h>
#include <sys/types.h>

using Widelands::Building;
using Widelands::Building_Index;
using Widelands::ConstructionSite;
using Widelands::MilitarySite;
using Widelands::ProductionSite;
using Widelands::Soldier;
using Widelands::TrainingSite;
using Widelands::Warehouse;
using Widelands::atrAttack;
using Widelands::atrDefense;
using Widelands::atrEvade;
using Widelands::atrHP;

static char const * pic_ok                 = "pics/menu_okay.png";
static char const * pic_cancel             = "pics/menu_abort.png";
static char const * pic_debug              = "pics/menu_debug.png";

static char const * pic_bulldoze           = "pics/menu_bld_bulldoze.png";
static char const * pic_queue_background   = "pics/queue_background.png";

static char const * pic_list_worker        = "pics/menu_list_workers.png";

static char const * pic_priority_low       = "pics/low_priority_button.png";
static char const * pic_priority_normal    = "pics/normal_priority_button.png";
static char const * pic_priority_high      = "pics/high_priority_button.png";
static char const * pic_priority_low_on    = "pics/low_priority_on.png";
static char const * pic_priority_normal_on = "pics/normal_priority_on.png";
static char const * pic_priority_high_on   = "pics/high_priority_on.png";

static char const * pic_tab_military       = "pics/menu_tab_military.png";
static char const * pic_tab_training       = "pics/menu_tab_training.png";
static char const * pic_up_train           = "pics/menu_up_train.png";
static char const * pic_down_train         = "pics/menu_down_train.png";
static char const * pic_drop_soldier       = "pics/menu_drop_soldier.png";

/*
==============================================================================

Building UI IMPLEMENTATION

==============================================================================
*/

/*
===============
Building::show_options

Create the building's options window if necessary.
===============
*/
void Building::show_options(Interactive_Player * const plr)
{
	if (m_optionswindow)
		m_optionswindow->move_to_top();
	else
		create_options_window(*plr, m_optionswindow);
}

/*
===============
Building::hide_options

Force the destruction of the options window.
===============
*/
void Building::hide_options() {delete m_optionswindow;}


/*
==============================================================================

class BulldozeConfirm

==============================================================================
*/

/**
 * Confirm the bulldoze request for a building.
 * \todo move this into it's own set of files
 */
struct BulldozeConfirm : public UI::Window {
	BulldozeConfirm
		(Interactive_Base           * parent,
		 Building                   * building,
		 Widelands::PlayerImmovable * todestroy = 0);

	virtual void think();

private:
	void bulldoze();

private:
	Interactive_Base * m_iabase;
	Widelands::Object_Ptr m_building;
	Widelands::Object_Ptr m_todestroy;
};


/*
===============
BulldozeConfirm::BulldozeConfirm

Create the panels.
If todestroy is 0, the building will be destroyed when the user confirms it.
Otherwise, todestroy is destroyed when the user confirms it. This is useful to
confirm building destruction when the building's base flag is removed.
===============
*/
BulldozeConfirm::BulldozeConfirm
	(Interactive_Base           * parent,
	 Building                   * building,
	 Widelands::PlayerImmovable * todestroy)
	:
	UI::Window(parent, 0, 0, 200, 120, _("Destroy building?"))
{
	std::string text;

	center_to_parent();

	m_iabase = parent;
	m_building = building;

	if (!todestroy)
		m_todestroy = building;
	else
		m_todestroy = todestroy;

	text = _("Do you really want to destroy this ");
	text += building->descname();
	text += _("?");
	new UI::Textarea(this, 0, 0, 200, 74, text, Align_Center, true);

	new UI::Callback_Button<BulldozeConfirm>
		(this,
		 6, 80, 80, 34,
		 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
		 g_gr->get_picture(PicMod_Game, pic_ok),
		 &BulldozeConfirm::bulldoze, *this);

	(new UI::Callback_Button<BulldozeConfirm>
	 	(this,
	 	 114, 80, 80, 34,
	 	 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
	 	 g_gr->get_picture(PicMod_Game, pic_cancel),
	 	 &BulldozeConfirm::die, *this))
		->center_mouse();
}


/*
===============
Make sure the building still exists and can in fact be bulldozed.
===============
*/
void BulldozeConfirm::think()
{
	Widelands::Editor_Game_Base const & egbase = m_iabase->egbase();
	upcast(Building,        building,  m_building .get(egbase));
	upcast(Widelands::PlayerImmovable, todestroy, m_todestroy.get(egbase));

	if
		(!todestroy ||
		 !building  ||
		 !(building->get_playercaps() & (1 << Building::PCap_Bulldoze)))
		die();
}


/*
===============
BulldozeConfirm::bulldoze

Issue the CMD_BULLDOZE command for this building.
===============
*/
void BulldozeConfirm::bulldoze()
{
	Widelands::Editor_Game_Base & egbase = m_iabase->egbase();
	upcast(Building,        building,  m_building .get(egbase));
	upcast(Widelands::PlayerImmovable, todestroy, m_todestroy.get(egbase));

	if
		(todestroy &&
		 building &&
		 building->get_playercaps() & (1 << Building::PCap_Bulldoze))
	{
		if (upcast(Widelands::Game, game, &egbase)) {
			game->send_player_bulldoze
				(*todestroy,
				 get_key_state(SDLK_LCTRL) or get_key_state(SDLK_RCTRL));
			m_iabase->need_complete_redraw();
		} else { //  Editor
			todestroy->owner().bulldoze(*todestroy);
			m_iabase->need_complete_redraw();
		}
	}

	die();
}


/*
===============
show_bulldoze_confirm

Create a BulldozeConfirm window.
building is the building that the confirmation dialog displays.
todestroy is the immovable that will be bulldozed if the user confirms the
dialog.
===============
*/
void show_bulldoze_confirm
	(Interactive_Base           &       player,
	 Building                   &       building,
	 Widelands::PlayerImmovable * const todestroy)
{
	new BulldozeConfirm(&player, &building, todestroy);
}


/*
==============================================================================

class WaresQueueDisplay

==============================================================================
*/

/*
class WaresQueueDisplay
-----------------------
This passive class displays the status of a WaresQueue.
It updates itself automatically through think().
*/
struct WaresQueueDisplay : public UI::Panel {
	enum {
		CellWidth = WARE_MENU_PIC_WIDTH,
		Border = 4,
		Height = WARE_MENU_PIC_HEIGHT + 2 * Border,
	};

public:
	WaresQueueDisplay
		(UI::Panel             * parent,
		 int32_t x, int32_t y,
		 uint32_t                maxw,
		 Widelands::WaresQueue *,
		 Widelands::Game       *);
	~WaresQueueDisplay();

	virtual void think();
	virtual void draw(RenderTarget &);

private:
	void recalc_size();

private:
	Widelands::WaresQueue * m_queue;
	uint32_t         m_max_width;
	PictureID        m_icon;            //< Index to ware's picture
	PictureID        m_pic_background;

	uint32_t         m_cache_size;
	uint32_t         m_cache_filled;
	uint32_t         m_display_size;
};


/*
===============
WaresQueueDisplay::WaresQueueDisplay

Initialize the panel.
===============
*/
WaresQueueDisplay::WaresQueueDisplay
	(UI::Panel * const parent,
	 int32_t const x, int32_t const y, uint32_t const maxw,
	 Widelands::WaresQueue * const queue, Widelands::Game *)
:
UI::Panel(parent, x, y, 0, Height),
m_queue(queue),
m_max_width(maxw),
m_pic_background(g_gr->get_picture(PicMod_Game, pic_queue_background)),
m_cache_size(queue->get_size()),
m_cache_filled(queue->get_filled()),
m_display_size(0)
{
	const Widelands::Item_Ware_Descr & ware =
		*queue->owner().tribe().get_ware_descr(m_queue->get_ware());
	set_tooltip(ware.descname().c_str());

	m_icon = ware.icon();
	m_pic_background = g_gr->create_grayed_out_pic(m_icon);

	recalc_size();

	set_think(true);
}


WaresQueueDisplay::~WaresQueueDisplay() {
	g_gr->free_surface(m_pic_background);
}


/*
===============
WaresQueueDisplay::recalc_size

Recalculate the panel's size.
===============
*/
void WaresQueueDisplay::recalc_size()
{
	m_display_size = (m_max_width - 2 * Border) / CellWidth;

	m_cache_size = m_queue->get_size();

	if (m_cache_size < m_display_size)
		m_display_size = m_cache_size;

	set_size(m_display_size * CellWidth + 2 * Border, Height);
}


/*
===============
WaresQueueDisplay::think

Compare the current WaresQueue state with the cached state; update if necessary.
===============
*/
void WaresQueueDisplay::think()
{
	if (static_cast<uint32_t>(m_queue->get_size()) != m_cache_size)
		recalc_size();

	if (static_cast<uint32_t>(m_queue->get_filled()) != m_cache_filled)
		update(0, 0, get_w(), get_h());
}


/*
===============
WaresQueueDisplay::draw

Render the current WaresQueue state.
===============
*/
void WaresQueueDisplay::draw(RenderTarget & dst)
{
	if (!m_display_size)
		return;

	m_cache_filled = m_queue->get_filled();

	uint32_t nr_wares_to_draw = std::min(m_cache_filled, m_display_size);
	uint32_t nr_empty_to_draw = m_display_size - nr_wares_to_draw;
	Point point(Border, Border);
	for (; nr_wares_to_draw; --nr_wares_to_draw, point.x += CellWidth)
		dst.blit(point, m_icon);
	for (; nr_empty_to_draw; --nr_empty_to_draw, point.x += CellWidth)
		dst.blit(point, m_pic_background);
}

/*
=====

class Priority_Button

=====
*/

struct PriorityButtonInfo {
	UI::Button * button;
	PictureID picture_enabled;
	PictureID picture_disabled;

	PriorityButtonInfo() {}

	PriorityButtonInfo
		(UI::Button * const btn,
		 PictureID const pic_enabled, PictureID const pic_disabled)
		:
		button(btn), picture_enabled(pic_enabled), picture_disabled(pic_disabled)
	{}
};

struct PriorityButtonHelper : std::map<int32_t, PriorityButtonInfo> {
	PriorityButtonHelper
		(Widelands::Game     & g,
		 Building            * ps,
		 int32_t               ware_type,
		 Widelands::Ware_Index ware_index);

	void button_clicked (int32_t priority);
	void update_buttons ();

private:
	Widelands::Game     & m_game;
	Building            * m_building;
	int32_t               m_ware_type;
	Widelands::Ware_Index m_ware_index;
};


/*
==============================================================================

class Building_Window

==============================================================================
*/

/*
Baseclass providing common tools for building windows.
*/
struct Building_Window : public UI::Window {
	friend class TrainingSite_Window;
	friend class MilitarySite_Window;
	enum {
		Width = 4 * 34 //  4 normally sized buttons
	};

	Building_Window
		(Interactive_Player * parent, Building *, UI::Window * * registry);

	virtual ~Building_Window();

	Interactive_Player * get_player() {return m_player;}
	Building * get_building() {return m_building;}

	virtual void draw(RenderTarget &);
	virtual void think();

	UI::Panel * create_capsbuttons(UI::Panel * parent);

protected:
	void setup_capsbuttons();

	void act_bulldoze();
	void act_debug();
	void toggle_workarea();
	void act_start_stop();
	void act_enhance(Widelands::Building_Index);
	void act_drop_soldier(uint32_t);
	void act_change_soldier_capacity(int32_t);

	void create_ware_queue_panel
		(UI::Box *, Building *, Widelands::WaresQueue *);

	Interactive_Player * m_parent;
	std::list<PriorityButtonHelper> m_priority_helpers;
	UI::Callback_IDButton<PriorityButtonHelper, int32_t> *
	create_priority_button
		(UI::Box *, PriorityButtonHelper &,
		 int32_t priority, int32_t x, int32_t y, int32_t w, int32_t h,
		 const char * picture1, const char * picture2,
		 const std::string & tooltip);

private:
	UI::Window *                * m_registry;
	Interactive_Player          * m_player;
	Building                    * m_building;

	UI::Panel * m_capsbuttons; //  UI::Panel that contains capabilities buttons
	UI::Callback_Button<Building_Window> * m_toggle_workarea;

	//  capabilities that were last used in setting up the caps panel
	uint32_t m_capscache;

	Overlay_Manager::Job_Id m_workarea_job_id;
	PictureID workarea_cumulative_picid[NUMBER_OF_WORKAREA_PICS];
};


/*
===============
Building_Window::Building_Window

Create the window, add it to the registry.
===============
*/
Building_Window::Building_Window
	(Interactive_Player * const parent,
	 Building           * const building,
	 UI::Window *       * const registry)
	:
	UI::Window
		(parent,
		 0, 0, Width, 0,
		 building->info_string(parent->building_window_title_format()).c_str()),
m_workarea_job_id(Overlay_Manager::Job_Id::Null())
{
	m_parent = parent;

	m_registry = registry;
	delete *m_registry;
	*m_registry = this;

	m_player = parent;
	m_building = building;

	m_capsbuttons = 0;
	m_capscache = 0;

	move_to_mouse();

	set_think(true);

	char filename[] = "pics/workarea0cumulative.png";
	compile_assert(NUMBER_OF_WORKAREA_PICS <= 9);
	for (Workarea_Info::size_type i = 0; i < NUMBER_OF_WORKAREA_PICS; ++i) {
		++filename[13];
		workarea_cumulative_picid[i] = g_gr->get_picture(PicMod_Game, filename);
	}
}


Building_Window::~Building_Window()
{
	if (m_workarea_job_id)
		static_cast<Interactive_Player *>(get_parent())
		->egbase().map().overlay_manager().remove_overlay(m_workarea_job_id);
	*m_registry = 0;
}


/*
===============
Building_Window::draw

Draw a picture of the building in the background.
===============
*/
void Building_Window::draw(RenderTarget & dst)
{
	uint32_t anim = get_building()->get_ui_anim();

	dst.drawanim(Point(get_inner_w() / 2, get_inner_h() / 2), anim, 0, 0);

	// Draw all the panels etc. above the background
	UI::Window::draw(dst);
}


/*
===============
Building_Window::think

Check the capabilities and setup the capsbutton panel in case they've changed.
===============
*/
void Building_Window::think()
{
	if (m_capsbuttons) {
		if (get_building()->get_playercaps() != m_capscache)
			setup_capsbuttons();
	}

	UI::Window::think();
}


/*
===============
Building_Window::create_capsbuttons

Create the capsbuttons panel with the given parent window, set it up and return
it.
===============
*/
UI::Panel * Building_Window::create_capsbuttons(UI::Panel * const parent)
{
	delete m_capsbuttons;

	m_capsbuttons = new UI::Panel(parent, 0, 0, Width, 34);
	setup_capsbuttons();

	return m_capsbuttons;
}


/*
===============
Building_Window::setup_capsbuttons

Clear the capsbuttons panel and re-setup.
===============
*/
void Building_Window::setup_capsbuttons()
{
	int32_t x;

	assert(m_capsbuttons);

	m_capsbuttons->free_children();
	m_capscache = get_building()->get_playercaps();

	x = 0;

	if (upcast(ProductionSite const, productionsite, m_building))
		if (not dynamic_cast<MilitarySite const *>(productionsite)) {
			bool const is_stopped = productionsite->is_stopped();
			new UI::Callback_Button<Building_Window>
				(m_capsbuttons,
				 x, 0, 34, 34,
				 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
				 g_gr->get_picture
				 	(PicMod_Game,
				 	 (is_stopped ? "pics/continue.png" : "pics/stop.png")),
				 &Building_Window::act_start_stop, *this,
				 is_stopped ? _("Continue") : _("Stop"));
			x += 34;
		}

	if (m_capscache & 1 << Building::PCap_Enhancable) {
		std::set<Building_Index> const & enhancements =
			m_building->enhancements();
		Widelands::Tribe_Descr const & tribe = m_player->player().tribe();
		container_iterate_const(std::set<Building_Index>, enhancements, i)
			if (m_player->player().is_building_allowed(*i.current)) {
				Widelands::Building_Descr const & building =
					*tribe.get_building_descr(*i.current);
				char buffer[128];
				snprintf
					(buffer, sizeof(buffer),
					 _("Enhance to %s"), building.descname().c_str());
				new UI::Callback_IDButton
					<Building_Window, Widelands::Building_Index>
					(m_capsbuttons,
					 x, 0, 34, 34,
					 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
					 building.get_buildicon(),
					 &Building_Window::act_enhance, *this,
					 *i.current, //  button id = building id
					 buffer);
				x += 34;
			}
	}

	if (m_capscache & (1 << Building::PCap_Bulldoze)) {
		new UI::Callback_Button<Building_Window>
			(m_capsbuttons,
			 x, 0, 34, 34,
			 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
			 g_gr->get_picture(PicMod_Game, pic_bulldoze),
			 &Building_Window::act_bulldoze, *this,
			 _("Destroy"));
		x += 34;
	}

	if (m_building->descr().m_workarea_info.size()) {
		m_toggle_workarea = new UI::Callback_Button<Building_Window>
			(m_capsbuttons,
			 x, 0, 34, 34,
			 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
			 g_gr->get_picture(PicMod_Game,  "pics/workarea3cumulative.png"),
			 &Building_Window::toggle_workarea, *this,
			 _("Show workarea"));
		x += 34;
	}

	if (m_player->get_display_flag(Interactive_Base::dfDebug)) {
		new UI::Callback_Button<Building_Window>
			(m_capsbuttons,
			 x, 0, 34, 34,
			 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
			 g_gr->get_picture(PicMod_Game,  pic_debug),
			 &Building_Window::act_debug, *this,
			 _("Debug"));
		x += 34;
	}

	if (x == 0) {
		//  No capsbutton is in this window.
		//  Resize us, so that we do not take space.
		m_capsbuttons->set_inner_size(0, 0);
	}
}


/*
===============
Building_Window::act_bulldoze

Callback for bulldozing request
===============
*/
void Building_Window::act_bulldoze()
{
	new BulldozeConfirm(m_player, m_building);
}

void Building_Window::act_start_stop() {
	if (dynamic_cast<ProductionSite const *>(m_building))
		m_player->game().send_player_start_stop_building (*m_building);

	die();
}

/*
===============
Building_Window::act_bulldoze

Callback for bulldozing request
===============
*/
void Building_Window::act_enhance(Widelands::Building_Index const id)
{
	if
		(m_building
		 &&
		 m_building->get_playercaps() & (1 << Building::PCap_Enhancable))
		m_player->game().send_player_enhance_building (*m_building, id);

	die();
}

/*
===============
Building_Window::act_drop_soldier

Callback for bulldozing request
===============
*/
void Building_Window::act_drop_soldier(uint32_t serial) {
	if (m_building && (serial > 0))
		m_player->game().send_player_drop_soldier (*m_building, serial);
}

/*
===============
Do a modification at soldier capacity on military and training sites.
TODO: Check that building is a military or a training site.
===============
*/
void Building_Window::act_change_soldier_capacity(int32_t const value)
{
	if (m_building)
		m_player->game().send_player_change_soldier_capacity (*m_building, value);

}

/*
===============
Callback for debug window
===============
*/
void Building_Window::act_debug()
{
	show_field_debug
		(*m_player,
		 m_player->game().map().get_fcoords(m_building->get_position()));
}


void Building_Window::toggle_workarea() {
	Widelands::Map & map =
		static_cast<Interactive_Player *>(get_parent())->egbase().map();
	Overlay_Manager & overlay_manager = map.overlay_manager();
	if (m_workarea_job_id) {
		overlay_manager.remove_overlay(m_workarea_job_id);
		m_workarea_job_id = Overlay_Manager::Job_Id::Null();
		m_toggle_workarea->set_tooltip(_("Show workarea"));
	} else {
		m_workarea_job_id = overlay_manager.get_a_job_id();
		Widelands::HollowArea<> hollow_area
			(Widelands::Area<>(m_building->get_position(), 0), 0);
		const Workarea_Info & workarea_info =
			m_building->descr().m_workarea_info;
		Workarea_Info::const_iterator it = workarea_info.begin();
		for
			(Workarea_Info::size_type i =
			 std::min(workarea_info.size(), NUMBER_OF_WORKAREA_PICS);
			 i;
			 ++it)
		{
			--i;
			hollow_area.radius = it->first;
			Widelands::MapHollowRegion<> mr(map, hollow_area);
			do
				overlay_manager.register_overlay
					(mr.location(),
					 workarea_cumulative_picid[i],
					 0,
					 Point::invalid(),
					 m_workarea_job_id);
			while (mr.advance(map));
			hollow_area.hole_radius = hollow_area.radius;
		}
		m_toggle_workarea->set_tooltip(_("Hide workarea"));
	}
}

struct ProductionSite_Window : public Building_Window {
	ProductionSite_Window
		(Interactive_Player * parent, ProductionSite *, UI::Window * * registry);

	ProductionSite * get_productionsite() {
		return dynamic_cast<ProductionSite *>(get_building());
	}

	virtual void think();

private:
	UI::Window * * m_reg;
public:
	void list_worker_clicked();
protected:
	UI::Box * create_production_box(UI::Panel * ptr, ProductionSite * ps);
};



/*
==============================================================================

ConstructionSite UI IMPLEMENTATION

==============================================================================
*/

struct ConstructionSite_Window : public Building_Window {
	ConstructionSite_Window
		(Interactive_Player          * parent,
		 Widelands::ConstructionSite *,
		 UI::Window *                * registry);

	virtual void think();

private:
	UI::Progress_Bar * m_progress;
};


/*
===============
ConstructionSite_Window::ConstructionSite_Window

Create the window and its panels
===============
*/
ConstructionSite_Window::ConstructionSite_Window
	(Interactive_Player          * const parent,
	 Widelands::ConstructionSite * const cs,
	 UI::Window *                * const registry)
	: Building_Window(parent, cs, registry)
{
	UI::Box & box = *new UI::Box(this, 0, 0, UI::Box::Vertical);

	// Add the progress bar
	m_progress =
		new UI::Progress_Bar
			(&box,
			 0, 0,
			 UI::Progress_Bar::DefaultWidth, UI::Progress_Bar::DefaultHeight,
			 UI::Progress_Bar::Horizontal);
	m_progress->set_total(1 << 16);
	box.add(m_progress, UI::Box::AlignCenter);

	box.add_space(8);

	// Add the wares queue
	for (uint32_t i = 0; i < cs->get_nrwaresqueues(); ++i) {
		Building_Window::create_ware_queue_panel(&box, cs, cs->get_waresqueue(i));
	}

	box.add_space(8);

	// Add the caps button
	box.add(create_capsbuttons(&box), UI::Box::AlignCenter);

	fit_inner(box);
	move_inside_parent();
}


/*
===============
ConstructionSite_Window::think

Make sure the window is redrawn when necessary.
===============
*/
void ConstructionSite_Window::think()
{
	Building_Window::think();

	container_iterate(std::list<PriorityButtonHelper>, m_priority_helpers, i)
		i.current->update_buttons();

	m_progress->set_state
		(dynamic_cast<ConstructionSite &>(*get_building()).get_built_per64k());
}


/*
===============
ConstructionSite::create_options_window

Create the status window describing the construction site.
===============
*/
void ConstructionSite::create_options_window
	(Interactive_Player & plr, UI::Window * & registry)
{
	new ConstructionSite_Window(&plr, this, &registry);
}


/*
==============================================================================

Warehouse UI IMPLEMENTATION

==============================================================================
*/

struct Warehouse_Window : public Building_Window {
	Warehouse_Window
		(Interactive_Player * parent, Warehouse *, UI::Window * * registry);

	Warehouse * get_warehouse() {
		return dynamic_cast<Warehouse *>(get_building());
	}

	virtual void think();

private:
	void clicked_help      ();
	void clicked_switchpage();
	void clicked_goto      ();

private:
	WaresDisplay       * m_waresdisplay;
	Interactive_Player * m_parent;
	int32_t              m_curpage;
};

/*
===============
Open the window, create the window buttons and add to the registry.
===============
*/
Warehouse_Window::Warehouse_Window
	(Interactive_Player * const parent,
	 Warehouse          * const wh,
	 UI::Window *       * const registry)
	: Building_Window(parent, wh, registry)
{
	// Add wares display
	m_waresdisplay = new WaresDisplay(this, 0, 0, parent->player().tribe());
	m_waresdisplay->add_warelist
		(&get_warehouse()->get_wares(), WaresDisplay::WARE);

	set_inner_size(m_waresdisplay->get_w(), 0);

	int32_t const spacing = 5;
	int32_t const nr_buttons = 4; // one more, turn page button is bigger
	int32_t const button_w =
		(get_inner_w() - (nr_buttons + 1) * spacing) / nr_buttons;
	int32_t       posx = spacing;
	int32_t       posy = m_waresdisplay->get_h() + spacing;
	m_curpage = 0;


	new UI::Callback_Button<Warehouse_Window>
		(this,
		 posx, posy, button_w, 25,
		 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
		 g_gr->get_picture(PicMod_Game, "pics/menu_help.png"),
		 &Warehouse_Window::clicked_help, *this);

	posx += button_w + spacing;

	new UI::Callback_Button<Warehouse_Window>
		(this,
		 posx, posy, button_w * 2 + spacing, 25,
		 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
		 g_gr->get_picture(PicMod_Game, "pics/warehousewindow_switchpage.png"),
		 &Warehouse_Window::clicked_switchpage, *this);

	posx += button_w * 2 + 2 * spacing;

	new UI::Callback_Button<Warehouse_Window>
		(this,
		 posx, posy, button_w, 25,
		 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
		 g_gr->get_picture(PicMod_Game, "pics/menu_goto.png"),
		 &Warehouse_Window::clicked_goto, *this);

	posx += button_w + spacing;
	posy += 25 + spacing;

	// Add caps buttons
	posx = 0;
	UI::Panel * caps = create_capsbuttons(this);
	caps->set_pos(Point(spacing, posy));
	if (caps->get_h())
		posy += caps->get_h() + spacing;

	set_inner_size(get_inner_w(), posy);
	move_inside_parent();
}


/**
 * \todo Implement help
 */
void Warehouse_Window::clicked_help() {}


void Warehouse_Window::clicked_goto() {
	m_parent->move_view_to(get_warehouse()->get_position());
}


/*
 * Switch to the next page, that is, show
 * wares -> workers -> soldier
 */
void Warehouse_Window::clicked_switchpage() {
	if        (m_curpage == 0) {
		//  Showing wares, should show workers.
		m_waresdisplay->remove_all_warelists();
		m_waresdisplay->add_warelist
			(&get_warehouse()->get_workers(), WaresDisplay::WORKER);
		m_curpage = 1;
	} else if (m_curpage == 1) {
		//  Showing workers, should show soldiers
		//  TODO currently switches back to wares
		m_waresdisplay->remove_all_warelists();
		m_waresdisplay->add_warelist
			(&get_warehouse()->get_wares(), WaresDisplay::WARE);
		m_curpage = 0;
	}
}
/*
===============
Push the current wares status to the WaresDisplay.
===============
*/
void Warehouse_Window::think() {}


/*
===============
Warehouse::create_options_window

Create the warehouse information window
===============
*/
void Warehouse::create_options_window
	(Interactive_Player & plr, UI::Window * & registry)
{
	new Warehouse_Window(&plr, this, &registry);
}


/*
==============================================================================

ProductionSite UI IMPLEMENTATION

==============================================================================
*/

/*
 * ProductionSite_Window_ListWorkerWindow
 */
struct ProductionSite_Window_ListWorkerWindow : public UI::Window {
	ProductionSite_Window_ListWorkerWindow
		(Interactive_Player *, ProductionSite *);

	virtual void think();

private:
	void update();
	void fill_list();

	Widelands::Coords                     m_ps_location;
	ProductionSite                      * m_ps;
	Interactive_Player                  * m_parent;
	UI::Listselect<Widelands::Worker const *> * m_ls;
	UI::Textarea * m_type, * m_experience, * m_becomes;
};

/*
 * Constructor
 */
ProductionSite_Window_ListWorkerWindow::ProductionSite_Window_ListWorkerWindow
	(Interactive_Player * const parent, ProductionSite * const ps)
	:
	UI::Window(parent, 0, 0, 320, 125, _("Worker Listing"))
{
	m_ps          = ps;
	m_ps_location = ps->get_position();
	m_parent      = parent;

	int32_t const spacing = 5;
	int32_t       offsx   = spacing;
	int32_t       offsy   = 30;
	int32_t       posx    = offsx;
	int32_t       posy    = offsy;

	// listselect
	m_ls =
		new UI::Listselect<Widelands::Worker const *>
		(this,
		 posx, posy,
		 get_inner_w() / 2 - spacing, get_inner_h() - spacing - offsy);

	posx = get_inner_w() / 2 + spacing;
	new UI::Textarea(this, posx, posy, 150, 20, _("Type: "), Align_CenterLeft);
	m_type =
		new UI::Textarea(this, posx + 80, posy, 200, 20, "---", Align_CenterLeft);
	posy += 20 + spacing;

	//  experience
	new UI::Textarea
		(this, posx, posy, 150, 20, _("Experience: "), Align_CenterLeft);
	m_experience =
		new UI::Textarea(this, posx + 80, posy, 200, 20, "---", Align_CenterLeft);
	posy += 20 + spacing;

	// is working to become
	new UI::Textarea
		(this, posx, posy, 70, 20, _("Trying to become: "), Align_CenterLeft);
	posy += 20;
	m_becomes =
		new UI::Textarea(this, posx + 25, posy, 200, 20, "---", Align_CenterLeft);
	posy += 20 + spacing;

	center_to_parent();
	move_to_top();
}


void ProductionSite_Window_ListWorkerWindow::think() {
	Widelands::BaseImmovable const * const base_immovable =
		m_parent->egbase().map()[m_ps_location].get_immovable();
	if
		(not dynamic_cast<const Building *>(base_immovable)
		 or
		 dynamic_cast<const ConstructionSite *>(base_immovable))
	{
		// The Productionsite has been removed. Die quickly.
		die();
		return;
	}

	fill_list();
	UI::Window::think();
}

/*
 * fill list()
 */
void ProductionSite_Window_ListWorkerWindow::fill_list() {
	const uint32_t m_last_select = m_ls->selection_index();
	m_ls->clear();

	uint32_t const nr_working_positions = m_ps->descr().nr_working_positions();
	for (uint32_t i = 0; i < nr_working_positions; ++i)
		if
			(Widelands::Worker const * const worker =
			 	m_ps->working_positions()[i].worker)
			m_ls->add(worker->descname().c_str(), worker, worker->icon());
		else
			m_ls->add
				(m_ps->working_positions()[i].worker_request->is_open() ?
				 _("(vacant)") : _("(coming)"),
				 0);
	if (m_ls->size() > m_last_select) m_ls->select(m_last_select);
	else if (m_ls->size()) m_ls->select(m_ls->size() - 1);

	update();
}

/**
 * \brief Update worker info subwindow, following user selection
 */
void ProductionSite_Window_ListWorkerWindow::update()
{
	if (m_ls->has_selection() and m_ls->get_selected()) {
		Widelands::Worker      const & worker = *m_ls->get_selected();
		Widelands::Tribe_Descr const & tribe  = worker.tribe();

		m_type->set_text(worker.descname());

		if
			(worker.get_current_experience() != -1
			 and
			 worker.get_needed_experience () != -1)
		{
			// Fill upgrade status
			char buffer[7];
			snprintf
				(buffer, sizeof(buffer),
				 "%i/%i",
				 worker.get_current_experience(), worker.get_needed_experience());
			m_experience->set_text(buffer);

			// Get the descriptive name of the ongoing upgrade
			m_becomes->set_text
				(tribe.get_worker_descr(worker.becomes())->descname());

		} else {
			// Worker is not upgradeable
			m_experience->set_text("---");
			m_becomes->set_text("---");
		}
	} else {
		m_type      ->set_text("---");
		m_becomes   ->set_text("---");
		m_experience->set_text("---");
		m_becomes   ->set_text("---");
	}
}

PriorityButtonHelper::PriorityButtonHelper
	(Widelands::Game     &       g,
	 Building            *       building,
	 int32_t               const ware_type,
	 Widelands::Ware_Index const ware_index)
:
m_game(g),
m_building  (building),
m_ware_type (ware_type),
m_ware_index(ware_index)
{}

void PriorityButtonHelper::button_clicked (int32_t const priority) {
	m_game.send_player_set_ware_priority
		(*m_building, m_ware_type, m_ware_index, priority);
}

void PriorityButtonHelper::update_buttons () {
	int32_t const priority =
		m_building->get_priority(m_ware_type, m_ware_index, false);

	for (iterator it = begin(); it != end(); ++it) {
		bool const enable = it->first != priority;
		it->second.button->set_enabled(enable);
		it->second.button->set_pic
			(enable ? it->second.picture_enabled : it->second.picture_disabled);
	}
}

/*
===============
ProductionSite_Window::ProductionSite_Window

Create the window and its panels, add it to the registry.
===============
*/
ProductionSite_Window::ProductionSite_Window
	(Interactive_Player * const parent,
	 ProductionSite     * const ps,
	 UI::Window *       * const registry)
	: Building_Window(parent, ps, registry)
{
	m_reg = registry;

	UI::Box * prod_box = 0;
	if
		(not dynamic_cast<TrainingSite const *>(ps) and
		 not dynamic_cast<MilitarySite const *>(ps))
	{
		prod_box = create_production_box (this, ps);
		fit_inner(*prod_box);
		move_inside_parent();
	}
}

UI::Callback_IDButton<PriorityButtonHelper, int32_t> *
Building_Window::create_priority_button
	(UI::Box * box, PriorityButtonHelper & helper,
	 int32_t priority, int32_t x, int32_t, int32_t w, int32_t h,
	 char const * picture1, char const * picture2,
	 std::string const & button_tooltip)
{
	PictureID const pic_enabled  =
		g_gr->get_resized_picture
			(g_gr->get_picture(PicMod_Game,  picture1),
			 w, h, Graphic::ResizeMode_Clip);
	PictureID const pic_disabled =
		g_gr->get_resized_picture
			(g_gr->get_picture(PicMod_Game,  picture2),
			 w, h, Graphic::ResizeMode_Clip);
	UI::Callback_IDButton<PriorityButtonHelper, int32_t> * button =
		new UI::Callback_IDButton<PriorityButtonHelper, int32_t>
			(box,
			 x, 0, w, h,
			 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
			 pic_enabled,
			 &PriorityButtonHelper::button_clicked, helper, priority,
			 button_tooltip, true, true);
	helper[priority] = PriorityButtonInfo(button, pic_enabled, pic_disabled);
	return button;
}

void Building_Window::create_ware_queue_panel
	(UI::Box               * const box,
	 Building              * const ps,
	 Widelands::WaresQueue * const wq)
{
	const int32_t priority_buttons_width = WaresQueueDisplay::Height / 3;
	UI::Box * hbox = new UI::Box (box, 0, 0, UI::Box::Horizontal);
	WaresQueueDisplay * wqd =
		new WaresQueueDisplay
			(hbox, 0, 0,
			 get_w() - priority_buttons_width,
			 wq, m_parent->get_game());

	hbox->add(wqd, UI::Box::AlignTop);

	if (wq->get_ware()) {
		m_priority_helpers.push_back
			(PriorityButtonHelper
			 	(get_player()->game(),
			 	 ps,
			 	 Widelands::Request::WARE,
			 	 wq->get_ware()));

		PriorityButtonHelper & helper = m_priority_helpers.back();

		UI::Box * vbox = new UI::Box (hbox, 0, 0, UI::Box::Vertical);
		// Add priority buttons
		vbox->add
			(create_priority_button
			 	(vbox, helper, HIGH_PRIORITY, 0, 0,
			 	 priority_buttons_width,
			 	 priority_buttons_width,
			 	 pic_priority_high,
			 	 pic_priority_high_on,
			 	 _("Highest priority")),
			 UI::Box::AlignTop);
		vbox->add
			(create_priority_button
			 	(vbox, helper, DEFAULT_PRIORITY, 0, 0,
			 	 priority_buttons_width,
			 	 priority_buttons_width,
			 	 pic_priority_normal,
			 	 pic_priority_normal_on,
			 	 _("Normal priority")),
			 UI::Box::AlignTop);
		vbox->add
			(create_priority_button
			 	(vbox, helper, LOW_PRIORITY, 0, 0,
			 	 priority_buttons_width,
			 	 priority_buttons_width,
			 	 pic_priority_low,
			 	 pic_priority_low_on,
			 	 _("Lowest priority")),
			 UI::Box::AlignTop);

		hbox->add(vbox, UI::Box::AlignCenter);
		helper.update_buttons();
	}

	box->add(hbox, UI::Box::AlignLeft);
}

UI::Box * ProductionSite_Window::create_production_box
	(UI::Panel * const parent, ProductionSite * const ps)
{
	UI::Box * const box =
		new UI::Box
			(parent,
			 0, 0,
			 UI::Box::Vertical,
			 g_gr->get_xres() - 80,
			 g_gr->get_yres() - 80);

	// Add the wares queue
	std::vector<Widelands::WaresQueue *> const & warequeues = ps->warequeues();
	for (uint32_t i = 0; i < warequeues.size(); ++i) {
		Building_Window::create_ware_queue_panel (box, ps, warequeues[i]);
	}

	box->add_space(8);

	// Add caps buttons
	box->add(create_capsbuttons(box), UI::Box::AlignLeft);

	// Add list worker button
	box->add
		(new UI::Callback_Button<ProductionSite_Window>
		 	(box,
		 	 0, 0, 32, 32,
			 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
		 	 g_gr->get_picture(PicMod_Game,  pic_list_worker),
		 	 &ProductionSite_Window::list_worker_clicked, *this,
		 	 _("Show worker listing")),
		 UI::Box::AlignLeft);

	return box;
}


/*
 * List worker button has been clicked
 */
void ProductionSite_Window::list_worker_clicked() {
	assert(*m_reg == this);

	*m_reg =
		new ProductionSite_Window_ListWorkerWindow
			(m_parent, get_productionsite());
	die();
}

/*
===============
ProductionSite_Window::think

Make sure the window is redrawn when necessary.
===============
*/
void ProductionSite_Window::think()
{
	Building_Window::think();

	container_iterate(std::list<PriorityButtonHelper>, m_priority_helpers, i)
		i.current->update_buttons();
}


/*
===============
Create the production site information window.
===============
*/
void ProductionSite::create_options_window
	(Interactive_Player & plr, UI::Window * & registry)
{
	new ProductionSite_Window(&plr, this, &registry);
}


/*
==============================================================================

MilitarySite UI IMPLEMENTATION

==============================================================================
*/

struct MilitarySite_Window : public Building_Window {
	MilitarySite_Window
		(Interactive_Player & parent,
		 MilitarySite       &,
		 UI::Window *       & registry);

	MilitarySite * get_militarysite() {
		return dynamic_cast<MilitarySite *>(get_building());
	}

	Interactive_Player & iaplayer() const {
		return dynamic_cast<Interactive_Player &>(*get_parent());
	}

	virtual void think();
private:
	void update();
	void drop_button_clicked ();
	void soldier_capacity_up () {act_change_soldier_capacity (1);}
	void soldier_capacity_down() {act_change_soldier_capacity(-1);}

	UI::Window *                  & m_reg;
	Widelands::Coords               m_ms_location;
	UI::Box                         m_vbox;
	UI::Table<Soldier &>            m_table;
	UI::Callback_Button<MilitarySite_Window> m_drop_button;
	UI::Box                         m_bottom_box;
	UI::Panel                       m_capsbuttons;
	UI::Textarea                    m_capacity;
	UI::Callback_Button<MilitarySite_Window> m_capacity_down;
	UI::Callback_Button<MilitarySite_Window> m_capacity_up;
};


/*
===============
MilitarySite_Window::MilitarySite_Window

Create the window and its panels, add it to the registry.
===============
*/
MilitarySite_Window::MilitarySite_Window
	(Interactive_Player & parent,
	 MilitarySite       & ms,
	 UI::Window *       & registry)
	:
Building_Window(&parent, &ms, &registry),
m_reg          (registry),
m_ms_location  (ms.get_position()),
m_vbox         (this, 5, 5, UI::Box::Vertical),
m_table        (&m_vbox, 0, 0, 360, 200),
m_drop_button
	(&m_vbox,
	 0, 0, 360, 32,
	 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
	 g_gr->get_picture(PicMod_Game, pic_drop_soldier),
	 &MilitarySite_Window::drop_button_clicked, *this),
m_bottom_box   (&m_vbox, 0, 0, UI::Box::Horizontal),
m_capsbuttons  (&m_bottom_box, 0, 34, 34 * 7, 34),
m_capacity     (&m_bottom_box, 0, 0, _("Capacity"), Align_Right),
m_capacity_down
	(&m_bottom_box,
	 0, 0, 24, 24,
	 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
	 g_gr->get_picture(PicMod_Game, pic_down_train),
	 &MilitarySite_Window::soldier_capacity_down, *this),
m_capacity_up
	(&m_bottom_box,
	 0, 0, 24, 24,
	 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
	 g_gr->get_picture(PicMod_Game, pic_up_train),
	 &MilitarySite_Window::soldier_capacity_up,   *this)
{
	//  soldiers view
	m_table.add_column(100, _("Name"));
	m_table.add_column (40, _("HP"));
	m_table.add_column (40, _("AT"));
	m_table.add_column (40, _("DE"));
	m_table.add_column (40, _("EV"));
	m_table.add_column(100, _("Level")); // enough space for scrollbar
	m_vbox.add(&m_table, UI::Box::AlignCenter);

	m_vbox.add_space(8);

	// Add drop soldier button
	m_vbox.add(&m_drop_button, UI::Box::AlignCenter);


	//  Add the bottom row of buttons.

	m_bottom_box.add_space(5);

	// Add the caps button
	create_capsbuttons(&m_capsbuttons);
	m_bottom_box.add(&m_capsbuttons, UI::Box::AlignLeft);

	// Capacity buttons
	m_bottom_box.add(&m_capacity,      UI::Box::AlignRight);
	m_bottom_box.add_space(8);
	m_bottom_box.add(&m_capacity_down, UI::Box::AlignRight);
	m_bottom_box.add(&m_capacity_up,   UI::Box::AlignRight);
	m_capacity_down.set_repeating(true);
	m_capacity_up  .set_repeating(true);

	m_bottom_box.add_space(5);

	fit_inner(m_bottom_box);

	m_vbox.add_space(8);

	m_vbox.add(&m_bottom_box, UI::Box::AlignCenter);

	fit_inner(m_vbox);

	move_inside_parent();
}


/*
===============
MilitarySite_Window::think

Make sure the window is redrawn when necessary.
===============
*/
void MilitarySite_Window::think()
{
	Building_Window::think();

	Widelands::BaseImmovable const * const base_immovable =
		iaplayer().egbase().map()[m_ms_location].get_immovable();
	if
		(not dynamic_cast<const Building *>(base_immovable)
		 or
		 dynamic_cast<const ConstructionSite *>(base_immovable))
	{
		// The Site has been removed. Die quickly.
		die();
		return;
	}
	update();
}

/*
==============
Update the listselect, maybe there are new soldiers
=============
*/
void MilitarySite_Window::update() {
	std::vector<Soldier *> soldiers = get_militarysite()->presentSoldiers();

	if (soldiers.size() < m_table.size())
		m_table.clear();

	for (uint32_t i = 0; i < soldiers.size(); ++i) {
		Soldier & s = *soldiers[i];
		UI::Table<Soldier &>::Entry_Record * er = m_table.find(s);
		if (not er)                          er = &m_table.add(s);
		uint32_t const  hl = s.get_hp_level         ();
		uint32_t const mhl = s.get_max_hp_level     ();
		uint32_t const  al = s.get_attack_level     ();
		uint32_t const mal = s.get_max_attack_level ();
		uint32_t const  dl = s.get_defense_level    ();
		uint32_t const mdl = s.get_max_defense_level();
		uint32_t const  el = s.get_evade_level      ();
		uint32_t const mel = s.get_max_evade_level  ();
		er->set_string(0, s.descname().c_str());
		char buffer[sizeof("4294967295 / 4294967295")];
		sprintf(buffer,  "%u / %u", hl,                mhl);
		er->set_string(1, buffer);
		sprintf(buffer,  "%u / %u",      al,                 mal);
		er->set_string(2, buffer);
		sprintf(buffer,  "%u / %u",           dl,                  mdl);
		er->set_string(3, buffer);
		sprintf(buffer,  "%u / %u",                el,                   mel);
		er->set_string(4, buffer);
		sprintf(buffer, "%2u / %u", hl + al + dl + el, mhl + mal + mdl + mel);
		er->set_string(5, buffer);
	}
	m_table.sort();

	MilitarySite const & ms = dynamic_cast<MilitarySite &>(*get_building());
	uint32_t const capacity     = ms.   soldierCapacity();
	char buffer[200];
	snprintf(buffer, sizeof(buffer), _("Capacity: %2u"), capacity);
	m_capacity.set_text (buffer);
	uint32_t const capacity_min = ms.minSoldierCapacity();
	m_drop_button.set_enabled
		(m_table.has_selection() and capacity_min < m_table.size());
	m_capacity_down.set_enabled(capacity_min < capacity);
	m_capacity_up  .set_enabled(capacity < ms.maxSoldierCapacity());
}

void MilitarySite_Window::drop_button_clicked()
{
	assert(m_reg == this);
	assert(m_table.has_selection());
	act_drop_soldier(m_table.get_selected().serial());
}

/*
===============
MilitarySite::create_options_window

Create the production site information window.
===============
*/
void MilitarySite::create_options_window
	(Interactive_Player & plr, UI::Window * & registry)
{
	new MilitarySite_Window(plr, *this, registry);
}


/*
==============================================================================

TrainingSite UI IMPLEMENTATION

==============================================================================
*/

struct TrainingSite_Window : public ProductionSite_Window {
	TrainingSite_Window
		(Interactive_Player * parent,
		 TrainingSite       *,
		 UI::Window *       * registry);

	TrainingSite * get_trainingsite() {
		return dynamic_cast<TrainingSite *>(get_building());
	}

	virtual void think();
	void drop_button_clicked ();
	void soldier_capacity_up () {act_change_soldier_capacity (1);}
	void soldier_capacity_down() {act_change_soldier_capacity(-1);}
	UI::Box * create_military_box (UI::Panel *);
private:
	void update();

	void add_tab(char const * picname, UI::Panel * panel);

	Widelands::Coords      m_ms_location;
	UI::Window         * * m_reg;
	UI::Table<Soldier &> * m_table;
	UI::Textarea         * m_capacity;

	UI::Tab_Panel        * m_tabpanel;

};


/*
===============
Create the window and its panels, add it to the registry.
===============
*/
TrainingSite_Window::TrainingSite_Window
	(Interactive_Player * const parent,
	 TrainingSite       * const ms,
	 UI::Window *       * const registry)
	: ProductionSite_Window(parent, ms, registry)
{
	m_reg = registry;
	m_ms_location = ms->get_position ();

	m_tabpanel = new UI::Tab_Panel
		(this, 0, 0, g_gr->get_picture(PicMod_UI, "pics/but1.png"));
	m_tabpanel->set_snapparent(true);

	// Training Box (wares and buttons related to them)
	UI::Box * prod_box = create_production_box (m_tabpanel, ms);
	prod_box->resize();
	add_tab(pic_tab_training, prod_box);

	// Military Box (Soldiers and buttons related to them)
	// Training Box (wares and buttons related to them)
	UI::Box * train_box = create_military_box (m_tabpanel);
	train_box->resize();
	add_tab(pic_tab_military, train_box);

	m_tabpanel->resize();
	fit_inner (*m_tabpanel);
}


UI::Box * TrainingSite_Window::create_military_box (UI::Panel * const panel)
{
	UI::Box * sold_box = new UI::Box (panel, 0, 0, UI::Box::Vertical);

	//  soldiers view
	m_table = new UI::Table<Soldier &>(sold_box, 0, 0, 360, 200);
	m_table->add_column(100, _("Name"));
	m_table->add_column (40, _("HP"));
	m_table->add_column (40, _("AT"));
	m_table->add_column (40, _("DE"));
	m_table->add_column (40, _("EV"));
	m_table->add_column(100, _("Level")); // enough space for scrollbar
	sold_box->add (m_table, Align_Left);

	//  add drop soldier button
	sold_box->add
		(new UI::Callback_Button<TrainingSite_Window>
		 	(sold_box,
		 	 0, 0, 360, 32,
			 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
		 	 g_gr->get_picture(PicMod_Game, pic_drop_soldier),
		 	 &TrainingSite_Window::drop_button_clicked, *this),
		 Align_Left);

	//  add TrainingSite options and capacity buttons
	UI::Box * box = new UI::Box (sold_box, 0, 0, UI::Box::Horizontal);
	box->add
		(new UI::Textarea (box, 0, 11, _("Capacity"), Align_Left), Align_Left);
	box->add
		(new UI::Callback_Button<TrainingSite_Window>
		 	(box,
		 	 70, 4, 24, 24,
		 	 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
		 	 g_gr->get_picture(PicMod_Game, pic_down_train),
		 	 &TrainingSite_Window::soldier_capacity_down, *this),
		 Align_Top);

	m_capacity = new UI::Textarea (box, 0, 11, _("xx"), Align_Center);
	box->add (m_capacity, Align_Top);

	box->add
		(new UI::Callback_Button<TrainingSite_Window>
		 	(box,
		 	 118, 4, 24, 24,
		 	 g_gr->get_picture(PicMod_UI, "pics/but4.png"),
		 	 g_gr->get_picture(PicMod_Game, pic_up_train),
		 	 &TrainingSite_Window::soldier_capacity_up, *this),
		 Align_Top);
	sold_box->add (box, Align_Left);

	return sold_box;
}

/*
===============
Convenience function: Adds a new tab to the main tab panel
===============
*/
void TrainingSite_Window::add_tab(char const * picname, UI::Panel * panel)
{
	m_tabpanel->add(g_gr->get_picture(PicMod_Game,   picname), panel);
}


/*
===============
TrainingSite_Window::think

Make sure the window is redrawn when necessary.
===============
*/
void TrainingSite_Window::think()
{
	ProductionSite_Window::think();

	Widelands::BaseImmovable const * const base_immovable =
		m_parent->egbase().map()[m_ms_location].get_immovable();
	if
		(not dynamic_cast<const Building *>(base_immovable)
		 or
		 dynamic_cast<const ConstructionSite *>(base_immovable))
	{
		// The Site has been removed. Die quickly.
		die();
		return;
	}
	update();
}

/*
==============
Update the listselect, maybe there are new soldiers
FIXME What if a soldier have been removed and another added? This needs review.
=============
*/
void TrainingSite_Window::update() {
	std::vector<Soldier *> soldiers = get_trainingsite()->presentSoldiers();

	if (soldiers.size() != m_table->size())
		m_table->clear();

	for (uint32_t i = 0; i < soldiers.size(); ++i) {
		Soldier & s = *soldiers[i];
		UI::Table<Soldier &>::Entry_Record * er = m_table->find(s);
		if (not er)                           er = &m_table->add(s);
		uint32_t const  hl = s.get_hp_level         ();
		uint32_t const mhl = s.get_max_hp_level     ();
		uint32_t const  al = s.get_attack_level     ();
		uint32_t const mal = s.get_max_attack_level ();
		uint32_t const  dl = s.get_defense_level    ();
		uint32_t const mdl = s.get_max_defense_level();
		uint32_t const  el = s.get_evade_level      ();
		uint32_t const mel = s.get_max_evade_level  ();
		er->set_string(0, s.descname().c_str());
		char buffer[sizeof("4294967295 / 4294967295")];
		sprintf(buffer,  "%u / %u", hl,                mhl);
		er->set_string(1, buffer);
		sprintf(buffer,  "%u / %u",      al,                 mal);
		er->set_string(2, buffer);
		sprintf(buffer,  "%u / %u",           dl,                  mdl);
		er->set_string(3, buffer);
		sprintf(buffer,  "%u / %u",                el,                   mel);
		er->set_string(4, buffer);
		sprintf(buffer, "%2u / %u", hl + al + dl + el, mhl + mal + mdl + mel);
		er->set_string(5, buffer);
	}
	m_table->sort();

	char buffer[sizeof("4294967295")];
	sprintf
		(buffer,
		 "%2u",
		 dynamic_cast<const TrainingSite &>(*get_building()).soldierCapacity());
	m_capacity->set_text (buffer);
}


/*
==============
Handle the click at drop soldier. Enqueue a command at command queue to
get out selected soldier from this training site.
=============
*/
void TrainingSite_Window::drop_button_clicked()
{
	assert(*m_reg == this);
	if (m_table->selection_index() != m_table->no_selection_index())
		act_drop_soldier(m_table->get_selected().serial());
}

/*
===============
Create the training site information window.
===============
*/
void TrainingSite::create_options_window
	(Interactive_Player & plr, UI::Window * & registry)
{
	new TrainingSite_Window(&plr, this, &registry);
}
