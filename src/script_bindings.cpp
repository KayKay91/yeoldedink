/**
 * Dink Script Bindings

 * Copyright (C) 2013  Alexander Krivács Schrøder
 * Copyright (C) 2023-2025 Yeoldetoast

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

#include "game_engine.h"
#include "log.h"
#include "sfx.h"
#include "bgm.h"
#include "inventory.h"

#include "str_util.h"
#include "debug.h"
#include "input.h"
#include "dinkc_console.h"
#include "debug_imgui.h"
#include "live_sprites_manager.h"

int scripting_sp_script(int sprite, const char *script)
{
  // (sprite, direction, until, nohard);
  if (sprite <= 0 || (sprite >= MAX_SPRITES_AT_ONCE && sprite != 1000))
  {
    sprintf(scripting_error, "cannot process sprite %d", sprite);
    return -1;
  }
  if (!debug_scriptkill)
    scripting_kill_scripts_owned_by(sprite);
  if (scripting_load_script(script, sprite, /*true*/1) == 0)
  {
    return 0;
  }

  int tempreturn = 0;
  if (sprite != 1000)
  {
    if (screen_main_is_running == /*true*/1)
      log_info("Not running %s until later..", sinfo[spr[sprite].script]->name);
    else if (screen_main_is_running == /*false*/0)
    {
      scripting_run_proc(spr[sprite].script, "MAIN");
      tempreturn = spr[sprite].script;
    }
  }

  return tempreturn;
}

void scripting_set_disable_savestates(bool state)
{
    //turn off autosaves and disable quicksave
    //TODO: make it switch back on
    if (state == true)
      dbg.autosave = !state;
    debug_savestates = !state;
}

void scripting_enable_hardened_mode()
{
  //switch off the console, autosaves, and all debug goodies
  debug_hardenedmode = true;
  debug_savestates = false;
  dbg.autosave = false;
  dbg.devmode = false;
}

bool scripting_is_joystick_present()
{
  //return true if joystick detected at launch
  if (ginfo)
    return true;
  else
    return false;
}

void scripting_rumble_controller(int freq1, int freq2, int duration)
{
  if (dbg.jrumble)
    SDL_GameControllerRumble(ginfo, freq1, freq2, duration);
}

int scripting_count_item(char* script)
{
  int count = 0;
	for (int i = 1; i < NB_ITEMS + 1; i++) {
		if (play.item[i].active && compare(play.item[i].name, script))
			count++;
	}

  return count;
}

int scripting_count_magic(char* script)
{
  int count = 0;
	for (int i = 1; i < NB_MITEMS + 1; i++) {
		if (play.mitem[i].active && compare(play.mitem[i].name, script))
			count++;
	}
  return count;
}

int scripting_free_items()
{
  int count = 0;
	for (int i = 1; i < NB_ITEMS + 1; i++) {
		if (play.item[i].active == 0)
			count++;
	}
  return count;
}

int scripting_free_magic()
{
  int count = 0;
  for (int i = 1; i < NB_MITEMS + 1; i++) {
		if (play.mitem[i].active == 0)
			count++;
	}
  return count;
}

int scripting_get_item(char *script)
{
    int retval = 0;
    for (int i = 1; i < NB_ITEMS + 1; i++)
    {
        if (play.item[i].active && compare(play.item[i].name, script))
        {
            retval = i;
            break;
        }
    }
    return retval;
}

int scripting_get_magic(char *script)
{
    int retval = 0;
    for (int i = 1; i < NB_MITEMS + 1; i++)
    {
        if (play.mitem[i].active && compare(play.mitem[i].name, script))
        {
            retval = i;
            break;
        }
    }
    return retval;
}

void scripting_show_console()
{
  log_debug_on(dbg.loglevel);
	dinkc_console_show();
  if (dbg.pause_on_console_show) {
		debug_paused = true;
		pause_everything();
	}
}
