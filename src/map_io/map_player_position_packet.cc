/*
 * Copyright (C) 2002-2004, 2006-2008, 2010 by the Widelands Development Team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "map_io/map_player_position_packet.h"

#include "logic/editor_game_base.h"
#include "logic/game_data_error.h"
#include "logic/map.h"
#include "map_io/coords_profile.h"
#include "profile/profile.h"

namespace Widelands {

#define CURRENT_PACKET_VERSION 2


void MapPlayerPositionPacket::Read
	(FileSystem & fs, EditorGameBase & egbase, bool, MapObjectLoader &)
{
	Profile prof;
	prof.read("player_position", nullptr, fs);
	Section & s = prof.get_safe_section("global");

	try {
		uint32_t const packet_version = s.get_safe_positive("packet_version");
		if (2 <= packet_version && packet_version <= CURRENT_PACKET_VERSION) {
			//  Read all the positions
			//  This could bring trouble if one player position/ is not set (this
			//  is possible in the editor), is also -1, -1.
			Map               & map        = egbase.map();
			Extent        const extent     = map.extent       ();
			PlayerNumber const nr_players = map.get_nrplayers();
			iterate_player_numbers(p, nr_players) {
				try {
					char buffer[10];
					snprintf(buffer, sizeof(buffer), "player_%u", p);
					map.set_starting_pos(p, get_safe_coords(buffer, extent, &s));
				} catch (const WException & e) {
					throw GameDataError("player %u: %s", p, e.what());
				}
			}
		} else
			throw GameDataError
				("unknown/unhandled version %u", packet_version);
	} catch (const WException & e) {
		throw GameDataError("player positions: %s", e.what());
	}
}


void MapPlayerPositionPacket::Write
	(FileSystem & fs, EditorGameBase & egbase, MapObjectSaver &)
{
	Profile prof;
	Section & s = prof.create_section("global");

	s.set_int("packet_version", CURRENT_PACKET_VERSION);

	// Now, all positions in order
	const Map & map = egbase.map();
	const PlayerNumber nr_players = map.get_nrplayers();
	iterate_player_numbers(p, nr_players) {
		char buffer[10];
		snprintf(buffer, sizeof(buffer), "player_%u", p);
		set_coords(buffer, map.get_starting_pos(p), &s);
	}

	prof.write("player_position", false, fs);
}

}
