/**
 * Load and save game progress

 * Copyright (C) 1997, 1998, 1999, 2002, 2003  Seth A. Robinson
 * Copyright (C) 2003, 2004, 2005, 2007, 2008, 2009, 2010, 2012, 2014, 2015  Sylvain Beucler

 * This file is part of GNU FreeDink

 * GNU FreeDink is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.

 * GNU FreeDink is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <string.h>
#include <fstream>
#include <ctime>
#include <nlohmann/json.hpp>

#include "game_engine.h"
#include "live_sprites_manager.h"
#include "DMod.h"
#include "dinkc.h"
#include "scripting.h"
#include "sfx.h"
#include "bgm.h"
#include "str_util.h"
#include "log.h"
#include "paths.h"
#include "input.h"
#include "gfx.h"
#include "debug.h"
#include "gfx_palette.h"
#include "ImageLoader.h"

bool sginfo = false;
using json = nlohmann::json;

/*bool*/ int add_time_to_saved_game(int num) {
	FILE* f = NULL;

	f = paths_savegame_fopen(num, "rb");
	if (!f) {
		log_error("💾 Couldn't load save game %d", num);
		return /*false*/ 0;
	}

	int minutes = 0;
	int minutes_offset = 200;
	fseek(f, minutes_offset, SEEK_SET);
	minutes = read_lsb_int(f);
	fclose(f);

	//great, now let's resave it with added time
	log_info("⏰ Ok, adding time.");
	time_t ct;

	time(&ct);
	minutes += (int)(difftime(ct, time_start) / 60);

	f = paths_savegame_fopen(num, "rb+");
	if (f) {
		fseek(f, minutes_offset, SEEK_SET);
		write_lsb_int(minutes, f);
		fclose(f);
	}
                log_info("✍️ Wrote it.(%d of time)", minutes);

	return /*true*/ 1;
}

bool load_game_json(int num) {
	std::ifstream i(paths_dmodfile("savegame.json"), std::ios::binary);
	json loadgame;
	i >> loadgame;
	//TODO: field-length validation
	play.minutes = loadgame["play_minutes"];
	//player sprite
	spr[1].x = loadgame["player"]["x"];
	spr[1].y = loadgame["player"]["y"];
	spr[1].size = loadgame["player"]["size"];
	spr[1].defense = loadgame["player"]["def"];
	spr[1].dir = loadgame["player"]["dir"];
	spr[1].pframe = loadgame["player"]["pframe"];
	spr[1].pseq = loadgame["player"]["pseq"];
	spr[1].seq = loadgame["player"]["seq"];
	spr[1].frame = loadgame["player"]["frame"];
	spr[1].strength = loadgame["player"]["strength"];
	spr[1].base_walk = loadgame["player"]["base_walk"];
	spr[1].base_idle = loadgame["player"]["base_idle"];
	spr[1].base_hit = loadgame["player"]["base_hit"];
	spr[1].que = loadgame["player"]["cue"];
	//magic
	for (int i = 1; i < NB_MITEMS + 1; i++) {
		play.mitem[i].active = (short)loadgame["magic"]["active"][i];
		/* The item script could overflow, overwriting 'seq'; if */
		std::string name = loadgame["magic"]["name"][i];
		sprintf(play.mitem[i].name, "%s", name.c_str());
		play.mitem[i].seq = loadgame["magic"]["seq"][i];
		play.mitem[i].frame = loadgame["magic"]["frame"][i];
	}
	//items
	for (int i = 1; i < NB_ITEMS + 1; i++) {
		play.item[i].active = (short)loadgame["item"]["active"][i];
		std::string name = loadgame["item"]["name"][i];
		sprintf(play.mitem[i].name, "%s", name.c_str());
		play.item[i].name[11 - 1] = '\0'; // safety
		play.item[i].seq = loadgame["item"]["seq"][i];
		play.item[i].frame = loadgame["item"]["seq"][i];
	}
}

/*bool*/ int load_game(int num) {
	char skipbuf[10000]; // more than any fseek we do
	char dinkdat[50] = "";
	char mapdat[50] = "";

	FILE* f = NULL;

	//lets get rid of our magic and weapon scripts
	if (weapon_script != 0) {
		scripting_run_proc(weapon_script, "DISARM");
	}

	if (magic_script != 0)
		scripting_run_proc(magic_script, "DISARM");

	bow.active = /*false*/ 0;
	weapon_script = 0;
	magic_script = 0;
	midi_active = /*true*/ 1;

	if (last_saved_game > 0) {
		log_info("💾 Modifying saved game.");
		if (!add_time_to_saved_game(last_saved_game))
			log_error("💾 Error modifying saved game.");
	}
	StopMidi();

	f = paths_savegame_fopen(num, "rb");
	if (!f) {
		log_error("💾 Couldn't load save game %d", num);
		return /*false*/ 0;
	}

	/* Portably load struct player_info play from disk */
	int i = 0;
	// TODO: check 'version' field and warn/upgrade/downgrade if
	// savegame version != dversion
	//ye: there are implications for editor sprite overrides when importing saves from rtdink for ticks
	fread(skipbuf, 4, 1, f);
	fread(skipbuf, 77 + 1, 1, f); // skip save_game_info, cf. save_game_small(...)
	fread(skipbuf, 118, 1, f); // unused
	// offset 200
	play.minutes = read_lsb_int(f);
	spr[1].x = read_lsb_int(f);
	spr[1].y = read_lsb_int(f);
	fread(skipbuf, 4, 1, f); // unused 'die' field
	spr[1].size = read_lsb_int(f);
	spr[1].defense = read_lsb_int(f);
	spr[1].dir = read_lsb_int(f);
	spr[1].pframe = read_lsb_int(f);
	spr[1].pseq = read_lsb_int(f);
	spr[1].seq = read_lsb_int(f);
	spr[1].frame = read_lsb_int(f);
	spr[1].strength = read_lsb_int(f);
	spr[1].base_walk = read_lsb_int(f);
	spr[1].base_idle = read_lsb_int(f);
	spr[1].base_hit = read_lsb_int(f);
	spr[1].que = read_lsb_int(f);
	// offset 264

	// skip first originally unused mitem entry
	fread(skipbuf, 20, 1, f);
	for (i = 1; i < NB_MITEMS + 1; i++) {
		play.mitem[i].active = fgetc(f);
		fread(play.mitem[i].name, 11, 1, f);
		/* The item script could overflow, overwriting 'seq'; if */
		play.mitem[i].name[11 - 1] = '\0'; // safety
		play.mitem[i].seq = read_lsb_int(f);
		play.mitem[i].frame = read_lsb_int(f);
	}
	// skip first originally unused item entry
	fread(skipbuf, 20, 1, f);
	for (i = 1; i < NB_ITEMS + 1; i++) {
		play.item[i].active = fgetc(f);
		fread(play.item[i].name, 11, 1, f);
		play.item[i].name[11 - 1] = '\0'; // safety
		play.item[i].seq = read_lsb_int(f);
		play.item[i].frame = read_lsb_int(f);
	}
	// offset 784

	play.curitem = read_lsb_int(f);
	fread(skipbuf, 4, 1, f); // reproduce unused 'unused' field
	fread(skipbuf, 4, 1, f); // reproduce unused 'counter' field
	fread(skipbuf, 1, 1, f); // reproduce unused 'idle' field
	fread(skipbuf, 3, 1, f); // reproduce memory alignment
	// offset 796

	for (i = 0; i < 769; i++) {
		/* Thoses are char arrays, not null-terminated strings */
		int j = 0;
		fread(play.spmap[i].type, 100, 1, f);
		for (j = 0; j < 100; j++)
			play.spmap[i].seq[j] = read_lsb_short(f);
		fread(play.spmap[i].frame, 100, 1, f);
		play.spmap[i].last_time = read_lsb_uint(f);
	}

	/* Here's we'll perform a few tricks to respect a misconception in
the original savegame format */
	// skip first originally unused play.button entry
	fread(skipbuf, 4, 1, f);
	// first play.var entry (cf. below) was overwritten by
	// play.button[10], writing 10 play.button entries:
	for (i = 0; i < 10; i++) // use fixed 10 rather than NB_BUTTONS
		input_set_button_action(i, read_lsb_int(f));
	// skip the rest of first unused play.var entry
	fread(skipbuf, 32 - 4, 1, f);

	// reading the rest of play.var
	for (i = 1; i < 250; i++) {
		play.var[i].var = read_lsb_int(f);
		fread(play.var[i].name, 20, 1, f);
		play.var[i].name[20 - 1] = '\0'; // safety
		play.var[i].scope = read_lsb_int(f);
		play.var[i].active = fgetc(f);
		fread(skipbuf, 3, 1, f); // reproduce memory alignment
	}

	play.push_active = fgetc(f);
	fread(skipbuf, 3, 1, f); // reproduce memory alignment
	play.push_dir = read_lsb_int(f);

	play.push_timer = read_lsb_int(f);

	play.last_talk = read_lsb_int(f);
	play.mouse = read_lsb_int(f);
	play.item_type = (enum item_type)fgetc(f);
	fread(skipbuf, 3, 1, f); // reproduce memory alignment
	play.last_map = read_lsb_int(f);
	fread(skipbuf, 4, 1, f); // reproduce unused 'crap' field
	fread(skipbuf, 95 * 4, 1, f); // reproduce unused 'buff' field
	fread(skipbuf, 20 * 4, 1, f); // reproduce unused 'dbuff' field
	fread(skipbuf, 10 * 4, 1, f); // reproduce unused 'lbuff' field

	/* v1.08: use wasted space for storing file location of map.dat,
dink.dat, palette, and tiles */
	/* char cbuff[6000]; */
	/* Thoses are char arrays, not null-terminated strings */
	fread(mapdat, 50, 1, f);
	fread(dinkdat, 50, 1, f);
	fread(play.palette, 50, 1, f);

	for (i = 0; i <= 41; i++) {
		fread(play.tile[i].file, 50, 1, f);
		play.tile[i].file[50 - 1] = '\0'; // safety
	}
	for (i = 0; i < 100; i++) {
		fread(play.func[i].file, 10, 1, f);
		play.func[i].file[10 - 1] = '\0'; // safety
		fread(play.func[i].func, 20, 1, f);
		play.func[i].func[20 - 1] = '\0'; // safety
	}
	/* Remains 750 unused chars at the end of the file. */
	/* fread(play.cbuff, 750, 1, f); */

	fclose(f);

	if (dversion >= 108) {
		// new map, if exist
		if (strlen(mapdat) > 0 && strlen(dinkdat) > 0) {
			g_dmod.map = EditorMap(dinkdat, mapdat);
			g_dmod.map.load();
		}

		// load palette
		if (strlen(play.palette) > 0) {
			if (gfx_palette_set_from_bmp(play.palette) < 0)
				log_error("🎨 Couldn't load palette from '%s': %s", play.palette,
						SDL_GetError());
			gfx_palette_get_phys(GFX_ref_pal);
		}

		/* Reload tiles */
		g_dmod.bgTilesets.loadDefault();

		/* Replace with custom tiles if needed */
		for (i = 1; i <= 41; i++)
			if (strlen(play.tile[i].file) > 0)
				g_dmod.bgTilesets.loadSlot(i, play.tile[i].file);
	}

	spr[1].damage = 0;
	walk_off_screen = 0;
	spr[1].nodraw = 0;
	push_active = 1;

	time(&time_start);

  	int script = scripting_load_script("main", 0, /*true*/1);
  	scripting_run_proc(script, "main");
	//lets attach our vars to the scripts

	attach();
	log_debug("🦟 Attached vars.");
	dinkspeed = 3;

	if (*pcur_weapon >= 1 && *pcur_weapon <= NB_ITEMS) {
		if (play.item[*pcur_weapon].active == 0) {
			*pcur_weapon = 1;
			weapon_script = 0;
			log_error("🏹 Loadgame error: Player doesn't have armed weapon - "
					"changed to 1.");
		} else {
			weapon_script = scripting_load_script(play.item[*pcur_weapon].name, 1000, /*false*/0);
	  		scripting_run_proc(weapon_script, "DISARM");

			weapon_script = scripting_load_script(play.item[*pcur_weapon].name, 1000, /*false*/0);
	  		scripting_run_proc(weapon_script, "ARM");
		}
	}
	if (*pcur_magic >= 1 && *pcur_magic <= NB_MITEMS) {
		if (play.item[*pcur_magic].active == /*false*/ 0) {
			*pcur_magic = 0;
			magic_script = 0;
			log_error("🏹 Loadgame error: Player doesn't have armed magic - "
					"changed to 0.");
		} else {

			magic_script = scripting_load_script(play.mitem[*pcur_magic].name, 1000, /*false*/0);
	  		scripting_run_proc(magic_script, "DISARM");
	 		 magic_script = scripting_load_script(play.mitem[*pcur_magic].name, 1000, /*false*/0);
	  		scripting_run_proc(magic_script, "ARM");
		}
	}
	kill_repeat_sounds_all();
	game_load_screen(g_dmod.map.loc[*pplayer_map]);
	log_info("🌎 Loaded map.");
	draw_screen_game();
	log_info("🌎 Map drawn.");

	last_saved_game = num;

	return /*true*/ 1;
}

void save_game_json(int num) {
	json savegame;
	//lets set some vars first
	time_t ct;
	time(&ct);
	play.minutes += (int)(difftime(ct, time_start) / 60);
	//yeolde: save date instead of the level, but not if they've specified a custom string
	if (!sginfo) {
	}
	//reset timer
	time(&time_start);

	last_saved_game = num;
	//metadata
	savegame["dversion"] = dversion;
	savegame["info_string"] = save_game_info;
	savegame["play_minutes"] = play.minutes;
	//108
	savegame["mapdat"] = g_dmod.map.map_dat;
	savegame["dinkdat"] = g_dmod.map.dink_dat;
	savegame["palette"] = play.palette;
	for (int i=0; i < GFX_TILES_NB_SETS; i++)
		if (!compare(play.tile[i].file, ""))
			savegame["tileset"][i] = play.tile[i].file;
	for (int i = 0; i < 100; i++) {
		if (!compare(play.func[i].file, "")) {
			savegame["func"]["file"][i] = play.func[i].file;
			savegame["func"]["function"][i] = play.func[i].func;
		}
	}
	//various crap
	savegame["last_talk"] = play.last_talk;
	savegame["last_map"] = play.last_map;
	savegame["push_dir"] = play.push_dir;
	savegame["push_timer"] = play.push_timer;
	savegame["mouse"] = play.mouse;
	savegame["item_type"] = play.item_type;
	//player sprite info
	savegame["push_active"] = play.push_active;
	savegame["player"]["x"] = spr[1].x;
	savegame["player"]["y"] = spr[1].y;
	savegame["player"]["size"] = spr[1].size;
	savegame["player"]["def"] = spr[1].defense;
	savegame["player"]["dir"] = spr[1].dir;
	savegame["player"]["pframe"] = spr[1].pframe;
	savegame["player"]["pseq"] = spr[1].pseq;
	savegame["player"]["seq"] = spr[1].seq;
	savegame["player"]["frame"] = spr[1].frame;
	savegame["player"]["strength"] = spr[1].strength;
	savegame["player"]["base_walk"] = spr[1].base_walk;
	savegame["player"]["base_idle"] = spr[1].base_idle;
	savegame["player"]["base_hit"] = spr[1].base_hit;
	savegame["player"]["cue"] = spr[1].que;
	//magic
	for (int i = 1; i < NB_MITEMS + 1; i++) {
		savegame["magic"]["active"][i] = play.mitem[i].active;
		savegame["magic"]["name"][i] = play.mitem[i].name;
		savegame["magic"]["seq"][i] = play.mitem[i].seq;
		savegame["magic"]["frame"][i] = play.mitem[i].frame;
	}
	//items
	savegame["item"]["current"] = play.curitem;
	for (int i = 1; i < NB_ITEMS + 1; i++) {
		savegame["item"]["active"][i] = play.item[i].active;
		savegame["item"]["name"][i] = play.item[i].name;
		savegame["item"]["seq"][i] = play.item[i].seq;
		savegame["item"]["frame"][i] = play.item[i].frame;
	}
	//editor sprite overrides
	for (int i = 0; i < 769; i++) {
		savegame["override"]["type"][i] = play.spmap[i].type;
		savegame["override"]["seq"][i] = play.spmap[i].seq;
		savegame["override"]["frame"][i] = play.spmap[i].frame;
		savegame["override"]["last_time"][i] = play.spmap[i].last_time;
	}
	//dinkc variables
	for (int i = 1; i < MAX_VARS; i++) {
		if (play.var[i].active) {
			savegame["variable"]["value"][i] = play.var[i].var;
			savegame["variable"]["name"][i] = play.var[i].name;
			savegame["variable"]["scope"][i] = play.var[i].scope;
			savegame["variable"]["active"][i] = play.var[i].active;
		}
	}
	//TODO: write a path thingy for these
	char mysave[255];
	sprintf(mysave, "save%d.json", num);
	std::ofstream os(paths_luasave(mysave, "wb"), std::ios::binary);
	os << std::setw(4) << savegame << std::endl;
	os << savegame << std::endl;
}

void save_game(int num) {
	char skipbuf[10000]; // more than any fseek we do
	memset(skipbuf, 0, 10000);

	FILE* f;

	//lets set some vars first
	time_t ct;
	time(&ct);
	play.minutes += (int)(difftime(ct, time_start) / 60);
	//yeolde: save date instead of the level, but not if they've specified a custom string
	if (!sginfo && dbg.savetime) {
		struct tm datetime = *localtime(&ct);
		if (dbg.savetime == 1)
			strftime(save_game_info, 200, "%D %R", &datetime);
		if (dbg.savetime == 2)
			strftime(save_game_info, 200, "%F %T", &datetime);
		if (dbg.savetime == 3)
			strftime(save_game_info, 200, "%c", &datetime);
	}
	//reset timer
	time(&time_start);

	last_saved_game = num;
	f = paths_savegame_fopen(num, "wb");
	if (f == NULL) {
		perror("Cannot save game");
		return;
	}

	/* Portably dump struct player_info play to disk */
	write_lsb_int(dversion, f);
	// set_save_game_info() support:
	{
		//ye: changed this a bit so it wouldn't overflow
		//old way for for windows because it lacks strndup
		#ifndef _WIN32
		char* info_temp = strndup(save_game_info, 77 + 1);
		decipher_string(&info_temp, 0);
		fwrite(info_temp, strlen(info_temp), 1, f);
		fwrite(skipbuf, (77 + 1) - strlen(info_temp), 1, f);
		#else
		char* info_temp = strdup(save_game_info);
		decipher_string(&info_temp, 0);
		fwrite(info_temp, 77 + 1, 1, f);
		#endif
		free(info_temp);
	}
	fwrite(skipbuf, 118, 1, f); // unused
	// offset 200
	write_lsb_int(play.minutes, f);
	write_lsb_int(spr[1].x, f);
	write_lsb_int(spr[1].y, f);
	fwrite(skipbuf, 4, 1, f); // unused 'die' field
	write_lsb_int(spr[1].size, f);
	write_lsb_int(spr[1].defense, f);
	write_lsb_int(spr[1].dir, f);
	write_lsb_int(spr[1].pframe, f);
	write_lsb_int(spr[1].pseq, f);
	write_lsb_int(spr[1].seq, f);
	write_lsb_int(spr[1].frame, f);
	write_lsb_int(spr[1].strength, f);
	write_lsb_int(spr[1].base_walk, f);
	write_lsb_int(spr[1].base_idle, f);
	write_lsb_int(spr[1].base_hit, f);
	write_lsb_int(spr[1].que, f);
	// offset 264

	// skip first originally unused mitem entry
	fwrite(skipbuf, 20, 1, f);
	for (int i = 1; i < NB_MITEMS + 1; i++) {
		fputc(play.mitem[i].active, f);
		fwrite(play.mitem[i].name, 11, 1, f);
		write_lsb_int(play.mitem[i].seq, f);
		write_lsb_int(play.mitem[i].frame, f);
	}
	// skip first originally unused item entry
	fwrite(skipbuf, 20, 1, f);
	for (int i = 1; i < NB_ITEMS + 1; i++) {
		fputc(play.item[i].active, f);
		fwrite(play.item[i].name, 11, 1, f);
		write_lsb_int(play.item[i].seq, f);
		write_lsb_int(play.item[i].frame, f);
	}
	// offset 784

	write_lsb_int(play.curitem, f);
	fwrite(skipbuf, 4, 1, f); // reproduce unused 'unused' field
	fwrite(skipbuf, 4, 1, f); // reproduce unused 'counter' field
	fwrite(skipbuf, 1, 1, f); // reproduce unused 'idle' field
	fwrite(skipbuf, 3, 1, f); // reproduce memory alignment
	// offset 796

	for (int i = 0; i < 769; i++) {
		fwrite(play.spmap[i].type, 100, 1, f);
		for (int j = 0; j < 100; j++)
			write_lsb_short(play.spmap[i].seq[j], f);
		fwrite(play.spmap[i].frame, 100, 1, f);
		write_lsb_uint(play.spmap[i].last_time, f);
	}

	/* Here's we'll perform a few tricks to respect a misconception in
the original savegame format */
	// skip first originally unused play.button entry
	fwrite(skipbuf, 4, 1, f);
	// first play.var entry (cf. below) was overwritten by
	// play.button[10], writing 10 play.button entries:
	for (int i = 0; i < 10; i++) // use fixed 10 rather than NB_BUTTONS
		write_lsb_int(input_get_button_action(i), f);
	// skip the rest of first unused play.var entry
	fwrite(skipbuf, 32 - 4, 1, f);

	// writing the rest of play.var
	//ye: specify 250
	for (int i = 1; i < 250; i++) {
		write_lsb_int(play.var[i].var, f);
		fwrite(play.var[i].name, 20, 1, f);
		write_lsb_int(play.var[i].scope, f);
		fputc(play.var[i].active, f);
		fwrite(skipbuf, 3, 1, f); // reproduce memory alignment
	}

	fputc(play.push_active, f);
	fwrite(skipbuf, 3, 1, f); // reproduce memory alignment
	write_lsb_int(play.push_dir, f);

	write_lsb_int(play.push_timer, f);

	write_lsb_int(play.last_talk, f);
	write_lsb_int(play.mouse, f);
	fputc(play.item_type, f);
	fwrite(skipbuf, 3, 1, f); // reproduce memory alignment
	write_lsb_int(play.last_map, f);
	fwrite(skipbuf, 4, 1, f); // reproduce unused 'crap' field
	fwrite(skipbuf, 95 * 4, 1, f); // reproduce unused 'buff' field
	fwrite(skipbuf, 20 * 4, 1, f); // reproduce unused 'dbuff' field
	fwrite(skipbuf, 10 * 4, 1, f); // reproduce unused 'lbuff' field

	/* v1.08: use wasted space for storing file location of map.dat,
dink.dat, palette, and tiles */
	/* char cbuff[6000];*/
	fwrite(g_dmod.map.map_dat.c_str(), 50, 1, f);
	fwrite(g_dmod.map.dink_dat.c_str(), 50, 1, f);
	fwrite(play.palette, 50, 1, f);
	//ye: only make the default set saveable. Does this even do anything?
	for (int i = 0; i < 41 + 1; i++)
		fwrite(play.tile[i].file, 50, 1, f);
	for (int i = 0; i < 100; i++) {
		fwrite(play.func[i].file, 10, 1, f);
		fwrite(play.func[i].func, 20, 1, f);
	}
	fwrite(skipbuf, 750, 1, f);

	fclose(f);

	if (dbg.savejson)
		save_game_json(num);

#ifdef __EMSCRIPTEN__
	// Flush changes to IDBFS
	EM_ASM(FS.syncfs(false, function(err) {
		if (err) {
			console.trace();
			console.log(err);
		}
	}));
#endif
}
