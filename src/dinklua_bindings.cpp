/**
 * Lua DinkC integration

 * Copyright (C) 2013  Alexander Krivács Schrøder

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

#define _GNU_SOURCE
//#define SOL_ALL_SAFETIES_ON 1

#include <sol/sol.hpp>
#include <LuaBridge/LuaBridge.h>
#include <lua.hpp>
#include <random.hpp>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <libintl.h>
#define _(String) gettext (String)

#include "freedink.h"
#include "log.h"
#include "brain.h"
#include "game_engine.h"
#include "game_choice_renderer.h"
#include "gfx_sprites.h"
#include "sfx.h"
#include "script_bindings.h"
#include "scripting.h"
#include "dinklua.h"
#include "live_screen.h"
#include "dinkc_bindings.h"
#include "input.h"
#include "bgm.h"
#include "str_util.h"
#include "gfx.h"
#include "paths.h"
#include "gfx_fonts.h"
#include "gfx_palette.h"
#include "live_sprites_manager.h"
#include "editor_screen.h"
#include "IOGfxPrimitives.h"
#include "DMod.h"
#include "text.h"
#include "status.h"
#include "dinkini.h"
#include "savegame.h"
#include "ImageLoader.h"
#include "gfx_fonts.h"
#include "hardness_tiles.h"
#include "game_choice.h"
#include "debug_imgui.h"
#include "debug.h"
#include "gnulib.h"
#include "dinkc_console.h"

extern lua_State* luaVM;
extern int current_lua_script;

using Random = effolkronium::random_static;

#define LUAREG(name) {#name, dinklua_ ## name}
#define LUAYREG(name) {#name, dinklua_yield_ ## name}
#define LUACYREG(name) {#name, dinklua_conditional_yield_ ## name}
#define LUACMREG(name) {#name, dinklua_choice_menu_ ## name}

#define LUA_REPORT_PROPERTY_NUMBER_ERROR(name)                                     \
if (!lua_isnumber(l, -1))                                                           \
{                                                                                    \
  return luaL_error(l, "The " #name " property must be set to a number");             \
}
/*if (!lua_tointeger(l, -1) == -1)                                                        \
{                                                                                        \
  return luaL_error(l, "The " #name " property must be set to a number other than -1");   \
}*/

#define LUA_REPORT_PROPERTY_BOOLEAN_ERROR(name)                             \
if (!lua_isboolean(l, -1))                                                  \
{                                                                           \
  return luaL_error(l, "The " #name " property must be set to a boolean");  \
}

#define LUA_REPORT_PROPERTY_STRING_ERROR(name)                               \
if (!lua_isstring(l, -1))                                                    \
{                                                                            \
  return luaL_error(l, "The " #name " property must be set to a string");    \
}

#define LUA_REPORT_READ_ONLY(name)                                           \
return luaL_error(l, "The " #name " property is a read-only property");

#define LUA_REPORT_WRITE_ONLY(name)                                          \
return luaL_error(l, "The " #name " property is a write-only property");

/*#define LUA_CHECK_PARAMETER_NUMBER_ERROR(name, index)                        \
if (!lua_isnumber(l, index))                                                 \
{                                                                            \
  return luaL_error(l, "The parameter " #name " must be set to a number");   \
}                                                                            \
int name = lua_tonumber(l, index);

#define LUA_CHECK_PARAMETER_SPRITE_ERROR(name, index)                            \
if (!lua_isnumber(l, index))                                                      \
{                                                                                  \
  return luaL_error(l, "The parameter " #name " must be set to a sprite number");   \
}                                                                                    \
int name = lua_tonumber(l, index);                                                    \
if (name <= 0 || name >= MAX_SPRITES_AT_ONCE)                                        \
{                                                                                   \
  int line = dinklua_get_current_line(l);                                          \
  log_error("[Lua] %s.lua:%d: invalid sprite %d",                                 \
            sinfo[current_lua_script]->name, line, name);                        \
}

#define LUA_CHECK_PARAMETER_BOOLEAN_ERROR(name, index)                       \
if (!lua_isboolean(l, index))                                                \
{                                                                            \
  return luaL_error(l, "The parameter " #name " must be set to a boolean");  \
}                                                                            \
int name = lua_toboolean(l, index);

#define LUA_CHECK_PARAMETER_STRING_ERROR(name, index)                       \
if (!lua_isstring(l, index))                                                 \
{                                                                            \
  return luaL_error(l, "The parameter " #name " must be set to a string");   \
}                                                                            \
const char* name = lua_tostring(l, index);*/

static int dinklua_set_sprite_value(lua_State *l)
{
  int sprite_number = lua_tointeger(l, -3);
  const char* sprite_command = lua_tostring(l, -2);

  log_debug("🌝[Lua] set sprite property %s on sprite %d",
            sprite_command, sprite_number);

  if (strcmp(sprite_command, "active") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(active);

    int active = lua_toboolean(l, -1);
    int val = (int)spr[sprite_number].active;
    change_sprite_noreturn(sprite_number, active, &val);

    if (!val)
      lsm_remove_sprite(sprite_number);
  }
  else if (strcmp(sprite_command, "attack_hit_sound") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(attack_hit_sound);

    int attack_hit_sound = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, attack_hit_sound, &spr[sprite_number].attack_hit_sound);
  }
  else if (strcmp(sprite_command, "attack_hit_sound_speed") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(attack_hit_sound_speed);

    int attack_hit_sound_speed = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, attack_hit_sound_speed, &spr[sprite_number].attack_hit_sound_speed);
  }
  else if (strcmp(sprite_command, "attack_wait") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(attack_wait);

    int attack_wait = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, attack_wait+thisTickCount, &spr[sprite_number].attack_wait);
  }
  else if (strcmp(sprite_command, "base_attack") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(base_attack);

    int base_attack = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, base_attack, &spr[sprite_number].base_attack);
  }
  else if (strcmp(sprite_command, "base_die") == 0 || strcmp(sprite_command, "base_death") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(base_die);

    int base_die = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, base_die, &spr[sprite_number].base_die);
  }
  else if (strcmp(sprite_command, "base_hit") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(base_hit);

    int base_hit = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, base_hit, &spr[sprite_number].base_hit);
  }
  else if (strcmp(sprite_command, "base_idle") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(base_idle);

    int base_idle = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, base_idle, &spr[sprite_number].base_idle);
  }
  else if (strcmp(sprite_command, "base_walk") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(base_walk);

    int base_walk = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, base_walk, &spr[sprite_number].base_walk);
  }
  else if (strcmp(sprite_command, "brain") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(brain);

    int brain = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, brain, &spr[sprite_number].brain);
  }
  else if (strcmp(sprite_command, "brain_parm") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(brain_parm);

    int brain_parm = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, brain_parm, &spr[sprite_number].brain_parm);
  }
  else if (strcmp(sprite_command, "brain_parm2") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(brain_parm2);

    int brain_parm2 = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, brain_parm2, &spr[sprite_number].brain_parm2);
  }
  else if (strcmp(sprite_command, "defense") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(defense);

    int defense = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, defense, &spr[sprite_number].defense);
  }
  else if (strcmp(sprite_command, "dir") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(dir);

    int dir = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, dir, &spr[sprite_number].dir);
    changedir(spr[sprite_number].dir, sprite_number, spr[sprite_number].base_walk);
  }
  else if (strcmp(sprite_command, "disabled") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(disabled);

    int disabled = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, disabled, &spr[sprite_number].disabled);
  }
  else if (strcmp(sprite_command, "distance") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(distance);

    int distance = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, distance, &spr[sprite_number].distance);
  }
  else if (strcmp(sprite_command, "exp") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(exp);

    int exp = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, exp, &spr[sprite_number].exp);
  }
  else if (strcmp(sprite_command, "flying") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(flying);

    int flying = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, flying, &spr[sprite_number].flying);
  }
  else if (strcmp(sprite_command, "follow") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(follow);

    int follow = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, follow, &spr[sprite_number].follow);
  }
  else if (strcmp(sprite_command, "frame") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(frame);

    int frame = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, frame, &spr[sprite_number].frame);
  }
  else if (strcmp(sprite_command, "frame_delay") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(frame_delay);

    int frame_delay = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, frame_delay, &spr[sprite_number].frame_delay);
  }
  else if (strcmp(sprite_command, "gold") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(gold);

    int gold = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, gold, &spr[sprite_number].gold);
  }
  else if (strcmp(sprite_command, "hard") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(hard);
    //ye: gotta invert this because of historical reasons
    int hard = !lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, hard, &spr[sprite_number].hard);
    if (spr[sprite_number].sp_index != 0)
      cur_ed_screen.sprite[spr[sprite_number].sp_index].hard = hard;
  }
  else if (strcmp(sprite_command, "hitpoints") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(hitpoints);

    int hitpoints = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, hitpoints, &spr[sprite_number].hitpoints);
  }
  else if (strcmp(sprite_command, "move_nohard") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(move_nohard);

    int move_nohard = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, move_nohard, &spr[sprite_number].move_nohard);
  }
  else if (strcmp(sprite_command, "mx") == 0 || strcmp(sprite_command, "move_x") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(mx);

    int mx = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, mx, &spr[sprite_number].mx);
  }
  else if (strcmp(sprite_command, "my") == 0 || strcmp(sprite_command, "move_y") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(my);

    int my = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, my, &spr[sprite_number].my);
  }
  //ye: aliases
  else if (strcmp(sprite_command, "clip") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(clip);

    int noclip = !lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, noclip, &spr[sprite_number].noclip);
  }
  else if (strcmp(sprite_command, "noclip") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(noclip);

    int noclip = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, noclip, &spr[sprite_number].noclip);
  }
  else if (strcmp(sprite_command, "control") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(control);

    int nocontrol = !lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, nocontrol, &spr[sprite_number].nocontrol);
  }
  else if (strcmp(sprite_command, "nocontrol") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(nocontrol);

    int nocontrol = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, nocontrol, &spr[sprite_number].nocontrol);
  }
  else if (strcmp(sprite_command, "draw") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(draw);

    int nodraw = !lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, nodraw, &spr[sprite_number].nodraw);
  }
  else if (strcmp(sprite_command, "nodraw") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(nodraw);

    int nodraw = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, nodraw, &spr[sprite_number].nodraw);
  }
  else if (strcmp(sprite_command, "hit") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(hit);

    int nohit = !lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, nohit, &spr[sprite_number].nodraw);
  }
  else if (strcmp(sprite_command, "nohit") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(nohit);

    int nohit = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, nohit, &spr[sprite_number].nohit);
  }
  else if (strcmp(sprite_command, "touch") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(touch);

    int notouch = !lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, notouch, &spr[sprite_number].notouch);
  }
  else if (strcmp(sprite_command, "notouch") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(notouch);

    int notouch = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, notouch, &spr[sprite_number].notouch);
  }
  else if (strcmp(sprite_command, "pframe") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(pframe);

    int pframe = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, pframe, &spr[sprite_number].pframe);
  }
  else if (strcmp(sprite_command, "picfreeze") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(picfreeze);

    int picfreeze = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, picfreeze, &spr[sprite_number].picfreeze);
  }
  else if (strcmp(sprite_command, "pseq") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(pseq);

    int pseq = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, pseq, &spr[sprite_number].pseq);
  }
  else if ((strcmp(sprite_command, "que") == 0) || (strcmp(sprite_command, "cue") == 0))
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(que);

    int que = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, que, &spr[sprite_number].que);
  }
  else if (strcmp(sprite_command, "range") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(range);

    int range = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, range, &spr[sprite_number].range);
  }
  else if (strcmp(sprite_command, "reverse") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(reverse);

    int reverse = lua_toboolean(l, -1);
    change_sprite_noreturn(sprite_number, reverse, &spr[sprite_number].reverse);
  }
  else if (strcmp(sprite_command, "seq") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(seq);

    int seq = lua_tointeger(l, -1);

    if ((seq < 0 || seq >= MAX_SEQUENCES))
    {
      // TODO: Test that this outputs the right information
      int line = dinklua_get_current_line(l);
      log_error("🌝[Lua] %s.lua:%d: invalid sequence %d, ignoring",
                sinfo[current_lua_script]->name, line, seq);
    }
    else
    {
      change_sprite_noreturn(sprite_number, seq, &spr[sprite_number].seq);
    }
  }
  else if (strcmp(sprite_command, "size") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(size);

    int size = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, size, &spr[sprite_number].size);
  }
  else if (strcmp(sprite_command, "sound") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(sound);

    int sound = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, sound, &spr[sprite_number].sound);

    if (sound > 0)
    {
      sfx_stop_sprite_sound(sprite_number);
      sfx_play_sprite_sound(spr[sprite_number].sound, sprite_number, 1);
    }
    else
		  sfx_stop_sprite_sound(sprite_number);
  }
  else if (strcmp(sprite_command, "speed") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(speed);

    int speed = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, speed, &spr[sprite_number].speed);
    changedir(spr[sprite_number].dir, sprite_number, spr[sprite_number].base_walk);
  }
  else if (strcmp(sprite_command, "strength") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(strength);

    int strength = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, strength, &spr[sprite_number].strength);
  }
  else if (strcmp(sprite_command, "target") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(target);

    int target = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, target, &spr[sprite_number].target);
  }
  else if (strcmp(sprite_command, "timing") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(timing);

    int timing = lua_tointeger(l, -1);
    change_sprite(sprite_number, timing, &spr[sprite_number].timing);
  }
  else if (strcmp(sprite_command, "touch_damage") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(touch_damage);

    int touch_damage = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, touch_damage, &spr[sprite_number].touch_damage);
  }
  else if (strcmp(sprite_command, "x") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(x);

    int x = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, x, &spr[sprite_number].x);
  }
  else if (strcmp(sprite_command, "y") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(y);

    int y = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, y, &spr[sprite_number].y);
  }
  else if (strcmp(sprite_command, "kill") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(kill);

    int kill = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, kill, &spr[sprite_number].kill_ttl);
  }
  else if (strcmp(sprite_command, "editor_num") == 0)
  {
    LUA_REPORT_READ_ONLY(editor_num);
  }
  else if (strcmp(sprite_command, "script") == 0)
  {
    LUA_REPORT_PROPERTY_STRING_ERROR(script);

    const char *script = lua_tostring(l, -1);
    scripting_sp_script(sprite_number, script);
  }
  else if (strcmp(sprite_command, "blood_num") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(blood_num);

    int blood_num = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, blood_num, &spr[sprite_number].bloodnum);
  }
  else if (strcmp(sprite_command, "blood_seq") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(blood_seq);

    int blood_seq = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, blood_seq, &spr[sprite_number].bloodseq);
  }
  else if (strcmp(sprite_command, "clip_bottom") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(clip_bottom);

    int clip_bottom = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, clip_bottom, &spr[sprite_number].alt.bottom);
  }
  else if (strcmp(sprite_command, "clip_left") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(clip_left);

    int clip_left = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, clip_left, &spr[sprite_number].alt.left);
  }
  else if (strcmp(sprite_command, "clip_right") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(clip_right);

    int clip_right = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, clip_right, &spr[sprite_number].alt.right);
  }
  else if (strcmp(sprite_command, "clip_top") == 0)
  {
    LUA_REPORT_PROPERTY_NUMBER_ERROR(clip_top);

    int clip_top = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, clip_top, &spr[sprite_number].alt.top);
  }
  else if (strcmp(sprite_command, "freeze") == 0)
  {
    LUA_REPORT_PROPERTY_BOOLEAN_ERROR(freeze);

    int freeze = lua_toboolean(l, -1);

    // Set the value
    if (freeze == 0)
      spr[sprite_number].freeze = 0;
    else if (freeze == 1)
      spr[sprite_number].freeze = current_lua_script;
  }
  else if (strcmp(sprite_command, "action") == 0)
  {
    //ye: used by sprite brains
    LUA_REPORT_PROPERTY_NUMBER_ERROR(action);

    int action = lua_tointeger(l, -1);
    change_sprite_noreturn(sprite_number, action, &spr[sprite_number].action);
  }
  else
  {
    return luaL_error(l, "Sprites do not have a '%s' property", sprite_command);
  }

  return 0;
}

static int dinklua_get_sprite_value(lua_State *l)
{
  int sprite_number = lua_tointeger(l, -2);
  const char* sprite_command = lua_tostring(l, -1);

  log_debug("🌝[Lua] get sprite value %s from sprite %d",
            sprite_command, sprite_number);

  if (strcmp(sprite_command, "active") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].active);
  }
  else if (strcmp(sprite_command, "attack_hit_sound") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].attack_hit_sound);
  }
  else if (strcmp(sprite_command, "attack_hit_sound_speed") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].attack_hit_sound_speed);
  }
  else if (strcmp(sprite_command, "attack_wait") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].attack_wait);
  }
  else if (strcmp(sprite_command, "base_attack") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].base_attack);
  }
  else if (strcmp(sprite_command, "base_die") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].base_die);
  }
  else if (strcmp(sprite_command, "base_hit") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].base_hit);
  }
  else if (strcmp(sprite_command, "base_idle") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].base_idle);
  }
  else if (strcmp(sprite_command, "base_walk") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].base_walk);
  }
  else if (strcmp(sprite_command, "brain") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].brain);
  }
  else if (strcmp(sprite_command, "brain_parm") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].brain_parm);
  }
  else if (strcmp(sprite_command, "brain_parm2") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].brain_parm2);
  }
  else if (strcmp(sprite_command, "defense") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].defense);
  }
  else if (strcmp(sprite_command, "dir") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].dir);
  }
  else if (strcmp(sprite_command, "disabled") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].disabled);
  }
  else if (strcmp(sprite_command, "distance") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].distance);
  }
  else if (strcmp(sprite_command, "exp") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].exp);
  }
  else if (strcmp(sprite_command, "flying") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].flying);
  }
  else if (strcmp(sprite_command, "follow") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].follow);
  }
  else if (strcmp(sprite_command, "frame") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].frame);
  }
  else if (strcmp(sprite_command, "frame_delay") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].frame_delay);
  }
  else if (strcmp(sprite_command, "gold") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].gold);
  }
  else if (strcmp(sprite_command, "hard") == 0)
  {
    lua_pushboolean(l, !(bool)spr[sprite_number].hard);
  }
  else if (strcmp(sprite_command, "hitpoints") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].hitpoints);
  }
  else if (strcmp(sprite_command, "move_nohard") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].move_nohard);
  }
  else if (strcmp(sprite_command, "mx") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].mx);
  }
  else if (strcmp(sprite_command, "my") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].my);
  }
  else if (strcmp(sprite_command, "noclip") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].noclip);
  }
  else if (strcmp(sprite_command, "nocontrol") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].nocontrol);
  }
  else if (strcmp(sprite_command, "nodraw") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].nodraw);
  }
  else if (strcmp(sprite_command, "nohit") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].nohit);
  }
  else if (strcmp(sprite_command, "notouch") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].notouch);
  }
  else if (strcmp(sprite_command, "pframe") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].pframe);
  }
  else if (strcmp(sprite_command, "picfreeze") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].picfreeze);
  }
  else if (strcmp(sprite_command, "pseq") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].pseq);
  }
  else if ((strcmp(sprite_command, "que") == 0) || (strcmp(sprite_command, "cue") == 0))
  {
    lua_pushinteger(l, spr[sprite_number].que);
  }
  else if (strcmp(sprite_command, "range") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].range);
  }
  else if (strcmp(sprite_command, "reverse") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].reverse);
  }
  else if (strcmp(sprite_command, "seq") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].seq);
  }
  else if (strcmp(sprite_command, "size") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].size);
  }
  else if (strcmp(sprite_command, "sound") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].sound);
  }
  else if (strcmp(sprite_command, "speed") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].speed);
  }
  else if (strcmp(sprite_command, "strength") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].strength);
  }
  else if (strcmp(sprite_command, "target") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].target);
  }
  else if (strcmp(sprite_command, "timing") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].timing);
  }
  else if (strcmp(sprite_command, "touch_damage") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].touch_damage);
  }
  else if (strcmp(sprite_command, "x") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].x);
  }
  else if (strcmp(sprite_command, "y") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].y);
  }
  else if (strcmp(sprite_command, "kill") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].kill_ttl);
  }
  else if (strcmp(sprite_command, "editor_num") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].sp_index);
  }
  else if (strcmp(sprite_command, "script") == 0)
  {
    int script_number = spr[sprite_number].script;
    if (sinfo[script_number] != NULL)
      if (sinfo[script_number]->name != NULL)
        lua_pushstring(l, sinfo[script_number]->name);
      else
        lua_pushnil(l);
    else
      lua_pushnil(l);
  }
  else if (strcmp(sprite_command, "blood_num") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].bloodnum);
  }
  else if (strcmp(sprite_command, "blood_seq") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].bloodseq);
  }
  else if (strcmp(sprite_command, "clip_bottom") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].alt.bottom);
  }
  else if (strcmp(sprite_command, "clip_left") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].alt.left);
  }
  else if (strcmp(sprite_command, "clip_right") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].alt.right);
  }
  else if (strcmp(sprite_command, "clip_top") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].alt.top);
  }
  else if (strcmp(sprite_command, "freeze") == 0)
  {
    lua_pushboolean(l, spr[sprite_number].freeze);
  }
  else if (strcmp(sprite_command, "action") == 0)
  {
    lua_pushinteger(l, spr[sprite_number].action);
  }
  else
  {
    return luaL_error(l, "Sprites do not have a '%s' property", sprite_command);
  }

  return 1;
}

static int validate_sprite(lua_State *l, int sprite)
{
  int valid = sprite > 0 || sprite < MAX_SPRITES_AT_ONCE;
  if (!valid)
  {
    int line = dinklua_get_current_line(l);
    log_error("🌝[Lua] %s.lua:%d: invalid sprite %d",
              sinfo[current_lua_script]->name, line, sprite);
  }
  return valid;
}

static int dinklua_sprite_object_kill_wait(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -1);

  if (validate_sprite(l, sprite))
  {
    spr[sprite].wait = 0;
  }

  return 0;
}

static int dinklua_sprite_object_unfreeze(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -1);

  if (validate_sprite(l, sprite))
  {
    spr[sprite].freeze = 0;
  }

  return 0;
}

static int dinklua_sprite_object_freeze(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -1);

  if (validate_sprite(l, sprite))
  {
    if (spr[sprite].active)
      spr[sprite].freeze = current_lua_script;
    else
    {
      int line = dinklua_get_current_line(l);
      log_error("🌝[Lua] %s.lua:%d: Couldn't freeze sprite %d, it doesn't exist.",
                sinfo[current_lua_script]->name, line, sprite);
    }
  }

  return 0;
}

static int dinklua_set_callback_random(lua_State *l)
{
  const char* procedure = luaL_checkstring(l, -3);
  int min = luaL_checkinteger(l, -2);
  int max = luaL_checkinteger(l, -1);

  int retval = scripting_add_callback(procedure, min, max, current_lua_script);
  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_dink_object_set_speed(lua_State *l)
{
  int speed = luaL_checkinteger(l, -1);

  if (speed == 0)
    ; // do nothing
  else
    dinkspeed = speed;

  return 0;
}

static int dinklua_dink_object_get_speed(lua_State *l)
{
  lua_pushinteger(l, dinkspeed);
  return 1;
}

static int dinklua_reset_timer(lua_State *l)
{
  time(&time_start);
  play.minutes = 0;

  return 0;
}

static int dinklua_set_keep_mouse(lua_State *l)
{
  int keep = lua_toboolean(l, -1);

  keep_mouse = keep;

  return 0;
}

static int dinklua_add_item(lua_State *l)
{
  const char* script = luaL_checkstring(l, -3);
  int sequence = luaL_checkinteger(l, -2);
  int frame = luaL_checkinteger(l, -1);

  add_item((char*)script, sequence, frame, ITEM_REGULAR);

  return 0;
}

static int dinklua_add_magic(lua_State *l)
{
  const char* script = luaL_checkstring(l, -3);
  int sequence = luaL_checkinteger(l, -2);
  int frame = luaL_checkinteger(l, -1);

  add_item((char*)script, sequence, frame, ITEM_MAGIC);

  return 0;
}

static int dinklua_add_exp(lua_State *l)
{
  int experience = luaL_checkinteger(l, -2);
  int sprite = luaL_checkinteger(l, -1);

  if (validate_sprite(l, sprite))
  {
    add_exp_force(experience, sprite);
  }

  return 0;
}

static int dinklua_kill_this_item(lua_State *l)
{
  const char* script_name = luaL_checkstring(l, -1);

  kill_item_script((char*)script_name);

  return 0;
}

static int dinklua_kill_this_magic(lua_State *l)
{
  const char* script_name = luaL_checkstring(l, -1);

  kill_mitem_script((char*)script_name);

  return 0;
}

static int dinklua_yield_show_bmp(lua_State *l)
{
  const char* bmp_name = luaL_checkstring(l, -2);
  int show_position = lua_toboolean(l, -1);

  log_info("🏞️ showing BMP");
  wait4b.active = /*false*/0;
  show_bmp((char*)bmp_name, current_lua_script);
  showb.active = /*true*/ 1;
	showb.showdot = show_position;
	showb.script = current_lua_script;

  return 0;
}

static int dinklua_copy_bmp_to_screen(lua_State *l)
{
  const char* bmp_name = luaL_checkstring(l, -1);

  log_info("🏞️ copying BMP");
  copy_bmp((char*)bmp_name);

  return 0;
}

static int dinklua_yield_wait_for_button(lua_State *l)
{
  log_info("🖲️ waiting for button with script %d", current_lua_script);
  wait4b.script = current_lua_script;
  wait4b.active = /*true*/1;
  wait4b.button = 0;

  return 0;
}

static int dinklua_stop_wait_for_button(lua_State *l)
{
  wait4b.active = /*false*/0;

  return 0;
}

static int dinklua_load_screen(lua_State *l)
{
  update_screen_time();
  game_load_screen(g_dmod.map.loc[*pplayer_map]);

  // update indicator on mini-map
  if (g_dmod.map.indoor[*pplayer_map] == 0)
		play.last_map = *pplayer_map;

  return 0;
}

static int dinklua_sprite_object_say(lua_State *l)
{
  const char* text = luaL_checkstring(l, -2);
  int sprite_owner = luaL_checkinteger(l, -1);

  if (sprite_owner == 0)
  {
    int line = dinklua_get_current_line(l);
    log_error("🤫 [Lua] %s.lua:%d: Sprite 0 can talk? Yeah, didn't think so.",
              sinfo[current_lua_script]->name, line);
    return 0;
  }

  if (sprite_owner != 1000)
    kill_text_owned_by(sprite_owner);

  int tsprite = say_text((char*)text, sprite_owner, current_lua_script);
  lua_pushinteger(l, tsprite);
  return 1;
}

static int dinklua_yield_sprite_object_say_stop(lua_State *l)
{
  //int top = lua_gettop (l);
  //log_debug("TOP: %d", top);

  const char* text = luaL_checkstring(l, -2);
  int sprite_owner = luaL_checkinteger(l, -1);

  if (sprite_owner == 0)
  {
    int line = dinklua_get_current_line(l);
    log_error("🤫 [Lua] %s.lua:%d: Sprite 0 can talk? Yeah, didn't think so.",
              sinfo[current_lua_script]->name, line);
    return 0;
  }

  kill_text_owned_by(sprite_owner);
  kill_text_owned_by(1);
  scripting_kill_callbacks(current_lua_script);

  int tsprite = say_text((char*)text, sprite_owner, current_lua_script);
  lua_pushinteger(l, tsprite);

  spr[tsprite].say_stop_callback = current_lua_script;
  play.last_talk = current_lua_script;

  return 1;
}

static int dinklua_yield_sprite_object_say_stop_npc(lua_State *l)
{
  const char* text = luaL_checkstring(l, -2);
  int sprite_owner = luaL_checkinteger(l, -1);

  /* no-op if already talking */
  if (text_owned_by(sprite_owner))
  {
    lua_pushinteger(l, 0);
    return 1;
  }

  scripting_kill_callbacks(current_lua_script);

  int tsprite = say_text((char*)text, sprite_owner, current_lua_script);
  lua_pushinteger(l, tsprite);

  spr[tsprite].say_stop_callback = current_lua_script;

  return 0;
}

static int dinklua_yield_say_stop_xy(lua_State *l)
{
  const char* text = luaL_checkstring(l, -3);
  int x = luaL_checkinteger(l, -2);
  int y = luaL_checkinteger(l, -1);

  scripting_kill_callbacks(current_lua_script);

  int tsprite = say_text_xy((char*)text, x, y, current_lua_script);
  spr[tsprite].say_stop_callback = current_lua_script;
  spr[tsprite].live = /*true*/1;
  play.last_talk = current_lua_script;

  return 0;
}

static int dinklua_say_xy(lua_State *l)
{
  const char* text = luaL_checkstring(l, -3);
  int x = luaL_checkinteger(l, -2);
  int y = luaL_checkinteger(l, -1);

  scripting_kill_callbacks(current_lua_script);

  int tsprite = say_text_xy((char*)text, x, y, current_lua_script);
  lua_pushinteger(l, tsprite);
  return 1;
}

static int dinklua_conditional_yield_draw_screen(lua_State *l)
{
  /* only refresh screen if not in a cut-scene */
  /* do it before draw_screen_game() because that one calls
     kill_all_scripts(), which NULLifies sinfo(script) */
  if (sinfo[current_lua_script]->sprite != 1000)
    lua_pushboolean(l, 1);
  else
    lua_pushboolean(l, 0);

  draw_screen_game();
  return 1;
}

static int dinklua_free_items(lua_State *l)
{
  int retval = scripting_free_items();

  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_free_magic(lua_State *l)
{
  int retval = scripting_free_magic();

  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_yield_kill_cur_item(lua_State *l)
{
  kill_cur_item();
  return 0;
}

static int dinklua_yield_kill_cur_magic(lua_State *l)
{
  kill_cur_magic();
  return 0;
}

static int dinklua_draw_status(lua_State *l)
{
  draw_status_all();
  return 0;
}

static int dinklua_arm_weapon(lua_State *l)
{
  if (weapon_script != 0)
    scripting_run_proc(weapon_script, "DISARM");

  weapon_script = scripting_load_script(play.item[*pcur_weapon].name, 1000, /*false*/0);
  scripting_run_proc(weapon_script, "ARM");

  return 0;
}

static int dinklua_arm_magic(lua_State *l)
{
  if (magic_script != 0)
    scripting_run_proc(magic_script, "DISARM");

  magic_script = scripting_load_script(play.mitem[*pcur_magic].name, 1000, /*false*/0);
  scripting_run_proc(magic_script, "ARM");

  return 0;
}

static int dinklua_yield_restart_game(lua_State *l)
{
  game_restart();

  return 0;
}

static int dinklua_yield_wait(lua_State *l)
{
  int delayms = luaL_checkinteger(l, -1);

  scripting_kill_callbacks(current_lua_script);
  scripting_add_callback("", delayms, 0, current_lua_script);
  return 0;
}

static int dinklua_preload_seq(lua_State *l)
{
  int sequence = luaL_checkinteger(l, -1);

  check_seq_status(sequence);
  return 0;
}

static int dinklua_script_attach(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -1);

  sinfo[current_lua_script]->sprite = sprite;
  return 0;
}

static int dinklua_sprite_object_draw_hard_sprite(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -1);

  if (validate_sprite(l, sprite))
  {
    update_play_changes();
    int l = sprite;
    rect mhard;
    rect_copy(&mhard, &k[seq[spr[l].pseq].frame[spr[l].pframe]].hardbox);
    rect_offset(&mhard, (spr[l].x- 20), spr[l].y);

    fill_hardxy(mhard);
    fill_back_sprites();
    fill_hard_sprites();
  }

  return 0;
}

static int dinklua_yield_activate_bow(lua_State *l)
{
  spr[1].seq = 0;
  spr[1].pseq = 100+spr[1].dir;
  spr[1].pframe = 1;
  bow.active = /*true*/1;
  bow.script = current_lua_script;
  bow.hitme = /*false*/0;
  bow.time = 0;

  return 0;
}

static int dinklua_disable_all_sprites(lua_State *l)
{
  for (int jj = 1; jj <= last_sprite_created; jj++)
    if (spr[jj].active) spr[jj].disabled = /*true*/1;

  return 0;
}

static int dinklua_draw_background(lua_State *l)
{
  draw_screen_game_background();

  return 0;
}

static int dinklua_draw_hard_map(lua_State *l)
{
  // (sprite, direction, until, nohard);
  log_info("🌐 Drawing hard map..");
  update_play_changes();
  fill_whole_hard();
  fill_hard_sprites();
  fill_back_sprites();

  return 0;
}

static int dinklua_enable_all_sprites(lua_State *l)
{
  for (int jj = 1; jj <= last_sprite_created; jj++)
    if (spr[jj].active) spr[jj].disabled = /*false*/0;

  return 0;
}

static int dinklua_yield_fade_down(lua_State *l)
{
  // (sprite, direction, until, nohard);
  if (process_upcycle)
  {
    int line = dinklua_get_current_line(l);
    log_error("🎞️ [Lua] %s.lua:%d: fade_down() called during fade_up(), ignoring fade_down()",
              sinfo[current_lua_script]->name, line);
  }
  else
  {
    process_downcycle = /*true*/1;
    cycle_clock = thisTickCount+timer_fade;
    cycle_script = current_lua_script;
  }

  return 0;
}

static int dinklua_yield_fade_up(lua_State *l)
{
  // (sprite, direction, until, nohard);
  if (process_downcycle)
  {
    int line = dinklua_get_current_line(l);
    log_error("🎞️ [Lua] %s.lua:%d: fade_up() called during fade_down(), forcing fade_up()",
              sinfo[current_lua_script]->name, line);
  }

  process_downcycle = 0; // priority over concurrent fade_down()
  process_upcycle = /*true*/1;
  cycle_script = current_lua_script;

  return 0;
}

static int dinklua_get_burn(lua_State *l)
{
  lua_pushinteger(l, 1);
  return 1;
}

static int dinklua_get_last_bow_power(lua_State *l)
{
  lua_pushinteger(l, bow.last_power);
  return 1;
}

static int dinklua_get_version(lua_State *l)
{
  lua_pushinteger(l, dversion);
  return 1;
}

//yeolde: put my stuff here
static int dinklua_get_client_fork(lua_State *l)
{
  lua_pushinteger(l, dfork);
  return 1;
}

static int dinklua_get_client_version(lua_State *l)
{
	int ver = atof(VERSION) * 100;
  lua_pushinteger(l, ver);
  return 1;
}

static int dinklua_get_fps(lua_State *l)
{
  lua_pushinteger(l, fps);
  return 1;
}

static int dinklua_get_platform(lua_State *l)
{
  int plat = -1;
	#ifdef _WIN32
	plat = 0;
	#elif defined __APPLE__
	plat = 2;
	#elif defined __linux__
	plat = 3;
	#endif
  lua_pushinteger(l, plat);
  return 1;
}

static int dinklua_get_last_sprite(lua_State *l)
{
  lua_pushinteger(l, last_sprite_created);
  return 1;
}

static int dinklua_playavi(lua_State *l)
{
  const char* video_file = luaL_checkstring(l, -1);
  log_debug("🎥 Attempting to play a video file: %s", video_file);
	StopMidi();
	kill_repeat_sounds_all();
	char cmd[255];
	sprintf(cmd, "ffplay -fs -autoexit -noborder -exitonkeydown -exitonmousedown \"%s\"", paths_dmodfile(video_file));
	system(cmd);
  return 0;
}

static int dinklua_get_seq_box(lua_State *l)
{
  int myseq = luaL_checkinteger(l, -2);
  int myframe = luaL_checkinteger(l, -1);

  lua_pushinteger(l, k[seq[myseq].frame[myframe]].box.left);
  lua_pushinteger(l, k[seq[myseq].frame[myframe]].box.right);
  lua_pushinteger(l, k[seq[myseq].frame[myframe]].box.top);
  lua_pushinteger(l, k[seq[myseq].frame[myframe]].box.bottom);

  return 4;
}

static int dinklua_get_seq_offset(lua_State *l)
{
  int myseq = luaL_checkinteger(l, -2);
  int myframe = luaL_checkinteger(l, -1);

  lua_pushinteger(l, k[seq[myseq].frame[myframe]].xoffset);
  lua_pushinteger(l, k[seq[myseq].frame[myframe]].yoffset);

  return 2;
}

static int dinklua_get_seq_hbox(lua_State *l)
{
  int myseq = luaL_checkinteger(l, -2);
  int myframe = luaL_checkinteger(l, -1);

  lua_pushinteger(l, k[seq[myseq].frame[myframe]].hardbox.left);
  lua_pushinteger(l, k[seq[myseq].frame[myframe]].hardbox.right);
  lua_pushinteger(l, k[seq[myseq].frame[myframe]].hardbox.top);
  lua_pushinteger(l, k[seq[myseq].frame[myframe]].hardbox.bottom);

  return 4;
}

static int dinklua_kill_all_sounds(lua_State *l)
{
  kill_repeat_sounds_all();
  return 0;
}

static int dinklua_yield_kill_game(lua_State *l)
{
  log_info("😵 Was told to kill game, so doing it like a good boy.");
  /* Send QUIT event to the main game loop,
     which will cleanly exit */
  SDL_Event ev;
  ev.type = SDL_QUIT;
  SDL_PushEvent(&ev);
  return 0;
}

static int dinklua_yield_kill_this_task(lua_State *l)
{
  scripting_kill_script(current_lua_script);
  return 0;
}

static int dinklua_scripts_used(lua_State *l)
{
  int m = 0;
  int i;
  for (i = 1; i < MAX_SCRIPTS; i++)
    if (sinfo[i] != NULL) m++;

  lua_pushinteger(l, m);
  return 1;
}

static int dinklua_stopcd(lua_State *l)
{
  sol::state_view lua(l);
  lua.script("dink.get_sprite(1):say('I do not have a CD player to stop :(')");
  return 0;
}

static int dinklua_stopmidi(lua_State *l)
{
  StopMidi();
  return 0;
}

static int dinklua_turn_midi_off(lua_State *l)
{
  midi_active = /*false*/0;
  return 0;
}

static int dinklua_turn_midi_on(lua_State *l)
{
  midi_active = /*true*/1;
  return 0;
}

static int dinklua_count_item(lua_State *l)
{
  const char* script = luaL_checkstring(l, -1);

  lua_pushinteger(l, scripting_count_item((char*)script));
  return 1;
}

static int dinklua_count_magic(lua_State *l)
{
  const char* script = luaL_checkstring(l, -1);

  lua_pushinteger(l, scripting_count_magic((char*)script));
  return 1;
}

static int dinklua_compare_sprite_object_script(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -2);
  const char* script = luaL_checkstring(l, -1);
  if (sinfo[spr[sprite].script]->name != NULL)
    log_debug("[Lua] Comparing sprite %d with script %s, and script %s", sprite, sinfo[spr[sprite].script]->name, script);

  if (validate_sprite(l, sprite))
  {
    if (spr[sprite].active)
    {
      if (spr[sprite].script == 0)
      {
        int line = dinklua_get_current_line(l);
        log_error("⚖️ [Lua] %s.lua:%d: compare_sprite_script: Sprite %d has no script.",
                  sinfo[current_lua_script]->name, line, sprite);

        lua_pushboolean(l, 0);
        return 1;
      }
      if (sinfo[spr[sprite].script] == NULL)
      {
        int line = dinklua_get_current_line(l);
        log_error("⚖️ [Lua] %s.lua:%d: compare_sprite_script: script %d for sprite %d was already killed!",
                  sinfo[current_lua_script]->name, line, spr[sprite].script, sprite);

        lua_pushboolean(l, 0);
        return 1;
      }
      if (compare((char*)script, sinfo[spr[sprite].script]->name))
      {
        //log_debug("[Lua] Successfully compared!");
        lua_pushboolean(l, 1);
        return 1;
      }
    }
    else
    {
      int line = dinklua_get_current_line(l);
      log_error("⚖️ [Lua] %s.lua:%d: compare_sprite_script: Can't compare sprite script, sprite not active.",
                sinfo[current_lua_script]->name, line);

      lua_pushboolean(l, 0);
      return 1;
    }
  }

  lua_pushboolean(l, 0);
  return 1;
}

static int dinklua_compare_weapon(lua_State *l)
{
  const char* script = luaL_checkstring(l, -1);

  int retval = 0;
  if (*pcur_weapon >= 1 && *pcur_weapon <= NB_ITEMS)
  {
    if (compare(play.item[*pcur_weapon].name, (char*)script))
      retval = 1;
  }

  lua_pushboolean(l, retval);
  return 1;
}

static int dinklua_compare_magic(lua_State *l)
{
  const char* script = luaL_checkstring(l, -1);

  int retval = 0;
  if (*pcur_magic >= 1 && *pcur_magic <= NB_MITEMS)
  {
    if (compare(play.mitem[*pcur_magic].name, (char*)script))
      retval = 1;
  }

  lua_pushboolean(l, retval);
  return 1;
}

static int dinklua_init(lua_State *l)
{
  const char* dink_ini_line = luaL_checkstring(l, -1);
  figure_out((char*)dink_ini_line);
  return 0;
}

static int dinklua_dink_object_can_walk_off_screen(lua_State *l)
{
  int can_walk_off_screen_p = lua_toboolean(l, -1);
  walk_off_screen = can_walk_off_screen_p;
  return 0;
}

static int dinklua_push_active(lua_State *l)
{
  int dink_can_push_p = lua_toboolean(l, -1);
  push_active = dink_can_push_p;
  return 0;
}

static int dinklua_stop_entire_game(lua_State *l)
{
  int stop_p = lua_toboolean(l, -1);
	stop_entire_game = stop_p;
	IOGFX_background->blit(IOGFX_backbuffer, NULL, NULL);
  return 0;
}

static int dinklua_set_editor_value(lua_State *l)
{
  int editor_sprite = lua_tointeger(l, -3);
  const char* editor_command = lua_tostring(l, -2);

  log_debug("✍🏻 [Lua] set editor value %s on editor sprite %d",
            editor_command, editor_sprite);

  if (editor_sprite < 0 || editor_sprite >= 100)
    return 0;

  if (strcmp(editor_command, "type") == 0)
  {
    int type = luaL_checkinteger(l, -1);
    change_edit_char(editor_sprite, type,
                     &play.spmap[*pplayer_map].type[editor_sprite]);
  }
  else if (strcmp(editor_command, "seq") == 0)
  {
    int seq = luaL_checkinteger(l, -1);
    change_edit(editor_sprite, seq,
                &play.spmap[*pplayer_map].seq[editor_sprite]);
  }
  else if (strcmp(editor_command, "frame") == 0)
  {
    int frame = luaL_checkinteger(l, -1);
    change_edit_char(editor_sprite, frame,
                     &play.spmap[*pplayer_map].frame[editor_sprite]);
  }
  else
  {
    return luaL_error(l, "Editor sprites do not have a '%s' property", editor_command);
  }

  return 0;
}

static int dinklua_get_editor_value(lua_State *l)
{
  int editor_sprite = lua_tointeger(l, -2);
  const char* editor_command = lua_tostring(l, -1);

  log_debug("✍🏼 [Lua] get editor value %s on editor sprite %d",
            editor_command, editor_sprite);

  if (editor_sprite < 0 || editor_sprite >= 100)
    return 0;

  if (strcmp(editor_command, "type") == 0)
  {
    lua_pushinteger(l, change_edit_char(editor_sprite, -1,
                                        &play.spmap[*pplayer_map].type[editor_sprite]));
  }
  else if (strcmp(editor_command, "seq") == 0)
  {
    lua_pushinteger(l, change_edit(editor_sprite, -1,
                                   &play.spmap[*pplayer_map].seq[editor_sprite]));
  }
  else if (strcmp(editor_command, "frame") == 0)
  {
    lua_pushinteger(l, change_edit_char(editor_sprite, -1,
                                        &play.spmap[*pplayer_map].frame[editor_sprite]));
  }
  else if (strcmp(editor_command, "sprite") == 0)
  {
    int i = find_sprite(editor_sprite);
    int retval;
    if (i != 0)
    {
      log_debug("Sp returned %d.", i);
      retval = i;
    }
    else
    {
      if (last_sprite_created == 1)
        log_warn("😡 you can't call SP() from a screen-ref,"
           " no sprites have been created yet.");
      retval = 0; /* not found */
    }
    lua_pushinteger(l, retval);
  }
  else
  {
    return luaL_error(l, "Editor sprites do not have a '%s' property", editor_command);
  }

  return 1;
}

static int dinklua_sprite_object_move(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -4);
  int direction = luaL_checkinteger(l, -3);
  int destination_limit = luaL_checkinteger(l, -2);
  int ignore_hardness_p = lua_toboolean(l, -1);

  if (validate_sprite(l, sprite))
  {
    spr[sprite].move_active = /*true*/1;
    spr[sprite].move_dir = direction;
    spr[sprite].move_num = destination_limit;
    spr[sprite].move_nohard = ignore_hardness_p;
    spr[sprite].move_script = 0;
    log_debug("Moving: Sprite %d, dir %d, num %d", sprite, direction, destination_limit);
  }

  return 0;
}

static int dinklua_spawn(lua_State *l)
{
  const char* script = luaL_checkstring(l, -1);

  int mysc = scripting_load_script(script, 1000, /*true*/1);
  if (mysc == 0)
  {
    lua_pushinteger(l, 0);
    return 1;
  }

  int tempreturn = mysc;
  scripting_run_proc(mysc, "MAIN");

  lua_pushinteger(l, tempreturn);
  return 1;
}

static int dinklua_run_script_by_number(lua_State *l)
{
  int script_index = luaL_checkinteger(l, -2);
  const char* funcname = luaL_checkstring(l, -1);

  scripting_run_proc(script_index, (char*)funcname);
  return 0;
}

static int dinklua_playmidi(lua_State *l)
{
  const char* midi_file = luaL_checkstring(l, -2);
  int fadein = luaL_checkinteger(l, -1);

  PlayMidi((char*)midi_file, fadein);
  return 0;
}

static int dinklua_playsound(lua_State *l)
{
  int sound_number = luaL_checkinteger(l, -5);
  int min_speed = luaL_checkinteger(l, -4);
  int rand_speed_to_add = luaL_checkinteger(l, -3);
  int sprite = luaL_checkinteger(l, -2);
  int repeat_p = lua_toboolean(l, -1);

  if (sprite < 0 || sprite >= MAX_SPRITES_AT_ONCE)
    sprite = 0; // no "3d" volume effect... and no segfault :p

  int retval;
  if (sound_on)
    retval = playsound(sound_number, min_speed, rand_speed_to_add, sprite, repeat_p);
  else {
    retval = 0;
  }


  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_set_soundbank_value(lua_State *l)
{
  const char* soundbank_command = lua_tostring(l, -2);
  int soundbank = lua_tointeger(l, -3);

  log_debug("🔊 [Lua] set sound bank value %s on sound bank %d",
            soundbank_command, soundbank);

  if (!sound_on || soundbank < 1)
    return 0;

  if (strcmp(soundbank_command, "survive") == 0)
  {
    int survive = lua_toboolean(l, -1);
    sound_set_survive(soundbank, survive);
  }
  else if (strcmp(soundbank_command, "vol") == 0)
  {
    int vol = luaL_checkinteger(l, -1);
    lua_set_vol(soundbank, vol);
  }
  else if (strcmp(soundbank_command, "pan") == 0)
  {
    int pan = luaL_checkinteger(l, -1);
    sound_set_pan(soundbank, pan);
  }
  else if (strcmp(soundbank_command, "hz") == 0)
  {
    int hz = luaL_checkinteger(l, -1);
    sound_set_hz(soundbank, hz);
  }
  else if (strcmp(soundbank_command, "speed") == 0)
  {
    int speed = luaL_checkinteger(l, -1);
    sound_set_speed(soundbank, speed);
  }
  else if (strcmp(soundbank_command, "pause") == 0)
  {
    bool pause = lua_toboolean(l, -1);
    sound_set_pause(soundbank, pause);
  }
  else if (strcmp(soundbank_command, "looppoint") == 0)
  {
    int looppoint = luaL_checkinteger(l, -1);
    sound_set_loop_point(soundbank, looppoint);
  }
  else if (strcmp(soundbank_command, "loopcount") == 0)
  {
    LUA_REPORT_READ_ONLY(soundbank_command);
  }
  else if (strcmp(soundbank_command, "pos") == 0)
  {
    LUA_REPORT_READ_ONLY(soundbank_command);
  }
  else if (strcmp(soundbank_command, "repeating") == 0)
  {
    bool repeat = lua_toboolean(l, -1);
    sound_set_looping(soundbank, repeat);
  }
  else
  {
    return luaL_error(l, "Sound banks do not have a '%s' property", soundbank_command);
  }

  return 0;
}

static int dinklua_get_soundbank_value(lua_State *l)
{
  int soundbank = lua_tointeger(l, -2);
  const char* soundbank_command = lua_tostring(l, -1);

  log_debug("🔊 [Lua] get sound bank value %s on sound bank %d",
            soundbank_command, soundbank);

  if (!sound_on || soundbank < 1)
    return 0;

  if (strcmp(soundbank_command, "survive") == 0)
  {
    lua_pushboolean(l, sound_get_survive(soundbank));
  }
  else if (strcmp(soundbank_command, "vol") == 0)
  {
    lua_pushinteger(l, sound_get_vol(soundbank));
  }
  else if (strcmp(soundbank_command, "overall_vol") == 0)
  {
    lua_pushinteger(l, sound_get_overall_vol(soundbank));
  }
  else if (strcmp(soundbank_command, "pan") == 0)
  {
    lua_pushinteger(l, sound_get_pan(soundbank));
  }
  else if (strcmp(soundbank_command, "hz") == 0)
  {
    lua_pushinteger(l, sound_get_hz(soundbank));
  }
  else if (strcmp(soundbank_command, "speed") == 0)
  {
    lua_pushinteger(l, sound_get_speed(soundbank));
  }
  else if (strcmp(soundbank_command, "pause") == 0)
  {
    lua_pushboolean(l, sound_get_pause(soundbank));
  }
  else if (strcmp(soundbank_command, "looppoint") == 0)
  {
    lua_pushinteger(l, sound_get_loop_point(soundbank));
  }
  else if (strcmp(soundbank_command, "loopcount") == 0)
  {
    lua_pushinteger(l, sound_get_loop_count(soundbank));
  }
  else if (strcmp(soundbank_command, "pos") == 0)
  {
    lua_pushinteger(l, sound_get_pos(soundbank));
  }
  else if (strcmp(soundbank_command, "repeating") == 0)
  {
    lua_pushboolean(l, sound_get_looping(soundbank));
  }
  else
  {
    return luaL_error(l, "Sound banks do not have a '%s' property", soundbank_command);
  }

  return 1;
}

static int dinklua_soundbank_object_set_kill(lua_State *l)
{
  int soundbank = lua_tointeger(l, -1);

  if (sound_on && soundbank > 0)
    sound_set_kill(soundbank);

  return 0;
}

static int dinklua_soundbank_object_fade_vol(lua_State *l)
{
  int soundbank = lua_tointeger(l, -3);
  int vol = lua_tointeger(l, -2);
  int time = lua_tointeger(l, -1);

  sound_fade_vol(soundbank, vol, time);

  return 0;
}

static int dinklua_soundbank_object_fade_pan(lua_State *l)
{
  int soundbank = lua_tointeger(l, -3);
  int pan = lua_tointeger(l, -2);
  int time = lua_tointeger(l, -1);

  sound_fade_pan(soundbank, pan, time);

  return 0;
}

static int dinklua_soundbank_object_fade_speed(lua_State *l)
{
  int soundbank = lua_tointeger(l, -3);
  int speed = lua_tointeger(l, -2);
  int time = lua_tointeger(l, -1);

  sound_fade_speed(soundbank, speed, time);

  return 0;
}

static int dinklua_soundbank_object_schedule_pause(lua_State *l)
{
  int soundbank = lua_tointeger(l, -2);
  int time = lua_tointeger(l, -1);

  sound_schedule_pause(soundbank, time);

  return 0;
}

static int dinklua_soundbank_object_schedule_stop(lua_State *l)
{
  int soundbank = lua_tointeger(l, -2);
  int time = lua_tointeger(l, -1);

  sound_schedule_stop(soundbank, time);

  return 0;
}

static int dinklua_soundbank_object_oscillate_vol(lua_State *l)
{
  int soundbank = lua_tointeger(l, -4);
  int from = lua_tointeger(l, -3);
  int to = lua_tointeger(l, -2);
  int time = lua_tointeger(l, -1);

  sound_oscillate_vol(soundbank, from, to, time);

  return 0;
}

static int dinklua_soundbank_object_oscillate_pan(lua_State *l)
{
  int soundbank = lua_tointeger(l, -4);
  int from = lua_tointeger(l, -3);
  int to = lua_tointeger(l, -2);
  int time = lua_tointeger(l, -1);

  sound_oscillate_pan(soundbank, from, to, time);

  return 0;
}

static int dinklua_soundbank_object_oscillate_speed(lua_State *l)
{
  int soundbank = lua_tointeger(l, -4);
  int from = lua_tointeger(l, -3);
  int to = lua_tointeger(l, -2);
  int time = lua_tointeger(l, -1);

  sound_oscillate_speed(soundbank, from, to, time);

  return 0;
}

static int dinklua_playsfx(lua_State *l)
{
  int speed = lua_tointeger(l, -1);
  int pan = lua_tointeger(l, -2);
  const char* sfx = luaL_checkstring(l, -3);

  int retval = playsfx((char*)sfx, speed, pan);
  lua_pushinteger(l, retval);

  return 1;
}

static int dinklua_save_game(lua_State *l)
{
  int game_slot = lua_tointeger(l, -1);

  save_game(game_slot);

  return 0;
}

static int dinklua_force_vision(lua_State *l)
{
  int vision = luaL_checkinteger(l, -1);

  *pvision = vision;
  sinfo[current_lua_script]->sprite = 1000;
  fill_whole_hard();
  draw_screen_game();

  return 0;
}

static int dinklua_fill_screen(lua_State *l)
{
  int palette_index = luaL_checkinteger(l, -1);

  if (palette_index < 0 || palette_index > 255)
		palette_index = 0;
	IOGFX_background->fill_screen(palette_index, GFX_ref_pal);
  return 0;
}

static int dinklua_yield_load_game(lua_State *l)
{
  int game_slot = luaL_checkinteger(l, -1);

  scripting_kill_all_scripts_for_real();
  int retval = load_game(game_slot);
  dinklua_clear_globals();
  log_info("⌛ load completed.");
  if (sinfo[current_lua_script] == NULL)
    log_error("😨 [Lua] Script %d is suddenly null!", current_lua_script);
  *pupdate_status = 1;
  draw_status_all();

  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_game_exist(lua_State *l)
{
  int game_slot = luaL_checkinteger(l, -1);

  FILE *fp;
  if ((fp = paths_savegame_fopen(game_slot, "rb")) != NULL)
  {
    fclose(fp);
    lua_pushboolean(l, 1);
  }
  else
  {
    lua_pushboolean(l, 0);
  }

  return 1;
}

static int dinklua_yield_sprite_object_move_stop(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -4);
  int direction = luaL_checkinteger(l, -3);
  int destination_limit = luaL_checkinteger(l, -2);
  int ignore_hardness_p = lua_toboolean(l, -1);

  if (validate_sprite(l, sprite))
  {
    spr[sprite].move_active = /*true*/1;
    spr[sprite].move_dir = direction;
    spr[sprite].move_num = destination_limit;
    spr[sprite].move_nohard = ignore_hardness_p;
    spr[sprite].move_script = current_lua_script;
    log_debug("🏃‍♂️ Moving: Sprite %d, dir %d, num %d", sprite, direction, destination_limit);
  }

  return 0;
}

static int dinklua_load_sound(lua_State *l)
{
  const char* wav_file = luaL_checkstring(l, -2);
  int sound_index = luaL_checkinteger(l, -1);

  if (sound_on) {
		log_info("🔊 getting %s", wav_file);
		CreateBufferFromWaveFile((char*)wav_file, sound_index);
	}

  return 0;
}

static int dinklua_debug(lua_State *l)
{
  const char* debug_string = luaL_checkstring(l, -1);

  log_debug("%s", debug_string);
  //pause if set by user
	if (debug_debugpause)
  {
		debug_paused = true;
		pause_everything();
	}

  return 0;
}

static int dinklua_sprite_object_busy(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -1);

  if (validate_sprite(l, sprite))
  {
    int retval = does_sprite_have_text(sprite);
    log_debug("🐝 Busy: Return int is %d and %d.",
              retval, does_sprite_have_text(sprite));

    lua_pushboolean(l, retval);
  }
  else
    lua_pushboolean(l, 0);

  return 1;
}

static int dinklua_make_global_int(lua_State *l)
{
  const char* varname = luaL_checkstring(l, -2);
  int default_val = luaL_checkinteger(l, -1);

  char variable_name[strlen(varname) + 2];
  sprintf(variable_name, "&%s", varname);

  if (!scripting_make_int(variable_name, default_val, VAR_GLOBAL_SCOPE))
  {
    int line = dinklua_get_current_line(l);
    log_error("🚧 [Lua] %s.lua:%d: %s",
              sinfo[current_lua_script]->name, line, scripting_error);
  }

  return 0;
}

static int dinklua_inside_box(lua_State *l)
{
  int x = luaL_checkinteger(l, -6);
  int y = luaL_checkinteger(l, -5);
  int left = luaL_checkinteger(l, -4);
  int right = luaL_checkinteger(l, -3);
  int top = luaL_checkinteger(l, -2);
  int bottom = luaL_checkinteger(l, -1);

  rect myrect;
  rect_set(&myrect, left, right, top, bottom);
  int retval = inside_box(x, y, myrect);
  log_debug("📦 Inbox is int is %d and %d.  Nlist got %d.", retval, x, y);

  lua_pushboolean(l, retval);
  return 1;
}

static int dinklua_random(lua_State *l)
{
  int range = luaL_checkinteger(l, -2);
  int base = luaL_checkinteger(l, -1);

  lua_pushinteger(l, Random::get<int>(base, base + range -1));
  return 1;
}

static int dinklua_initfont(lua_State *l)
{
  const char* fontname = luaL_checkstring(l, -1);

  initfont((char*)fontname);
  log_info("🔡 Initted font %s", fontname);

  return 0;
}

static int dinklua_set_mode(lua_State *l)
{
  int newmode = luaL_checkinteger(l, -1);

  set_mode(newmode);

  lua_pushinteger(l, mode);
  return 1;
}

static int dinklua_sprite_object_kill_shadow(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -1);

  for (int jj = 1; jj <= last_sprite_created; jj++)
  {
    if (spr[jj].brain == 15 && spr[jj].brain_parm == sprite)
    {
      spr[jj].active = 0;
    }
  }

  return 0;
}

static int dinklua_create_sprite(lua_State *l)
{
  int x = luaL_checkinteger(l, -5);
  int y = luaL_checkinteger(l, -4);
  int brain = luaL_checkinteger(l, -3);
  int sequence = luaL_checkinteger(l, -2);
  int frame = luaL_checkinteger(l, -1);
  const int size = 100;

  int retval = add_sprite_dumb(x, y, brain, sequence, frame, size);

  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_is_script_attached(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -1);
  if (validate_sprite(l, sprite))
  {
    lua_pushinteger(l, spr[sprite].script);
  }
  else
  {
    lua_pushinteger(l, 0);
  }

  return 1;
}

static int dinklua_get_sprite_with_this_brain(lua_State *l)
{
  int brain = luaL_checkinteger(l, -2);
  int sprite_ignore = luaL_checkinteger(l, -1);

  for (int i = 1; i <= last_sprite_created; i++)
  {
    if (spr[i].brain == brain && i != sprite_ignore && spr[i].active == 1)
    {
      log_debug("🧠 Ok, sprite with brain %d is %d", brain, i);

      lua_pushinteger(l, i);
      return 1;
    }
  }

  lua_pushinteger(l, 0); /* not found */
  return 1;
}

static int dinklua_get_rand_sprite_with_this_brain(lua_State *l)
{
  int brain = luaL_checkinteger(l, -2);
  int sprite_ignore = luaL_checkinteger(l, -1);

  int nb_matches = 0;
  for (int i = 1; i <= last_sprite_created; i++)
  {
    if (spr[i].brain == brain && i != sprite_ignore && spr[i].active == 1)
      nb_matches++;
  }
  if (nb_matches == 0)
  {
    log_debug("🧠 Get rand brain can't find any brains with %d.", brain);
    lua_pushinteger(l, 0);
    return 1;
  }

  int mypick = (rand() % nb_matches) + 1;
  int cur_match = 0;
  for (int ii = 1; ii <= last_sprite_created; ii++)
  {
    if (spr[ii].brain == brain && ii != sprite_ignore && spr[ii].active == 1)
    {
      cur_match++;
      if (cur_match == mypick)
      {
        lua_pushinteger(l, ii);
        return 1;
      }
    }
  }

  lua_pushinteger(l, 0);
  return 1;
}

/* BIG FAT WARNING: in DinkC, buttons are in [1, 10] (not [0, 9]) */
static int dinklua_set_button(lua_State *l)
{
  int button = luaL_checkinteger(l, -2);
  int function = luaL_checkinteger(l, -1);

  input_set_button_action(button - 1, function);

  return 0;
}

static int dinklua_sprite_object_hurt(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -2);
  int damage = luaL_checkinteger(l, -1);

  if (validate_sprite(l, sprite))
  {
    if (damage < 0)
	  return 0;

    if (hurt_thing(sprite, damage, 0) > 0)
      random_blood(spr[sprite].x, spr[sprite].y-40, sprite);

    if (spr[sprite].nohit != 1
        && spr[sprite].script != 0
        && scripting_proc_exists(spr[sprite].script, "HIT"))
    {
      if (sinfo[current_lua_script]->sprite != 1000)
      {
        *penemy_sprite = sinfo[current_lua_script]->sprite;
        *pmissle_source = sinfo[current_lua_script]->sprite;
      }
      scripting_kill_callbacks(spr[current_lua_script].script);
      scripting_run_proc(spr[current_lua_script].script, "HIT");
    }
  }

  return 0;
}

static int dinklua_screenlock(lua_State *l)
{
  int param = luaL_checkinteger(l, -1);

  // returns the screenlock value to Lua
  if (param == 0 || param == 1)
    screenlock = param;

  lua_pushboolean(l, screenlock);
  return 1;
}

static int dinklua_sp_custom(lua_State *l)
{
  int sprite = luaL_checkinteger(l, -3);
  const char *key = luaL_checkstring(l, -2);
  int val = luaL_checkinteger(l, -1);

  if (validate_sprite(l, sprite))
  {
    if (spr[sprite].active == 0)
    {
      return 0;
    }
    else
    {
    if (spr[sprite].custom->find(key) == spr[sprite].custom->end())
			spr[sprite].custom->insert(std::make_pair(key, 0));

		// Set the value
		if (val != -1) {
			spr[sprite].custom->erase(key);
			spr[sprite].custom->insert(std::make_pair(key, val));
		}

      lua_pushinteger(l, spr[sprite].custom->find(key)->second);
      return 1;
    }
  }

  return 0;
}

static int dinklua_clear_editor_info(lua_State *l)
{
  for (int i = 0; i < 769; i++)
  {
    for (int j = 0; j < 100; j++)
    {
      play.spmap[i].seq[j] = 0;
      play.spmap[i].frame[j] = 0;
      play.spmap[i].type[j] = 0;
      play.spmap[i].last_time = 0;
    }
  }
  return 0;
}

static int dinklua_get_date_day(lua_State *l)
{
  char mytime[5];
  time_t ct;
  struct tm* time_now;
  time(&ct);
  time_now = localtime(&ct);
  strftime(mytime, 5, "%d", time_now);

  lua_pushinteger(l, atoi(mytime));
  return 1;
}

static int dinklua_get_date_month(lua_State *l)
{
  char mytime[5];
  time_t ct;
  struct tm* time_now;
  time(&ct);
  time_now = localtime(&ct);
  strftime(mytime, 5, "%m", time_now);

  lua_pushinteger(l, atoi(mytime));
  return 1;
}

static int dinklua_get_date_year(lua_State *l)
{
  char mytime[5];
  time_t ct;
  struct tm* time_now;
  time(&ct);
  time_now = localtime(&ct);
  strftime(mytime, 5, "%Y", time_now);

  lua_pushinteger(l, atoi(mytime));
  return 1;
}

static int dinklua_get_time_game(lua_State *l)
{
  time_t ct;
  time(&ct);

  lua_pushinteger(l, play.minutes + (difftime (ct, time_start) / 60));
  return 1;
}

static int dinklua_get_time_real(lua_State *l)
{
  char mytime[5];
  time_t ct;
  struct tm* time_now;
  time(&ct);
  time_now = localtime(&ct);
  strftime(mytime, 5, "%M", time_now);
  int retval = atoi (mytime);
  strftime(mytime, 5, "%H", time_now);
  retval += 60 * atoi (mytime);

  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_get_truecolor(lua_State *l)
{
  lua_pushboolean(l, truecolor);
  return 1;
}

static int dinklua_show_inventory(lua_State *l)
{
  show_inventory = 1;
  return 0;
}

static int dinklua_var_used(lua_State *l)
{
  int m = 0;
  for (int i = 1; i < MAX_VARS; i++)
    if (play.var[i].active == 1)
      m++;

  lua_pushinteger(l, m);
  return 1;
}


static int dinklua_loopmidi(lua_State *l)
{
  int loop_midi = lua_toboolean(l, -1);

  loopmidi(loop_midi);

  return 0;
}

static int dinklua_set_save_game_info(lua_State *l)
{
  const char* info = luaL_checkstring(l, -1);
  sginfo = true;
  strncpy(save_game_info, info, LEN_SAVE_GAME_INFO);
  save_game_info[LEN_SAVE_GAME_INFO - 1] = '\0';

  return 0;
}

static int dinklua_load_map(lua_State *l)
{
  const char* mapdat_file = luaL_checkstring(l, -2);
  const char* dinkdat_file = luaL_checkstring(l, -1);

  // load a new map/dink.dat
  g_dmod.map = EditorMap((char*)dinkdat_file, (char*)mapdat_file);
	g_dmod.map.load();

  return 0;
}

static int dinklua_load_tile(lua_State *l)
{
  const char* tileset_file = luaL_checkstring(l, -2);
  int tileset_index = luaL_checkinteger(l, -1);

  // load new tiles
  if (tileset_index >= 1 && tileset_index <= GFX_TILES_NB_SETS)
  {
    //Load in the new tiles...
    g_dmod.bgTilesets.loadSlot(tileset_index, (char*)tileset_file);

    //Store in save game
    strncpy(play.tile[tileset_index].file, (char*)tileset_file, 50);
  }
  else
  {
    int line = dinklua_get_current_line(l);
    log_error("🧱 [Lua] %s.lua:%d: load_tile: invalid tileset index '%d'",
              sinfo[current_lua_script]->name, line, tileset_index);
  }

  return 0;
}

static int dinklua_map_tile(lua_State *l)
{
  int tile_position = luaL_checkinteger(l, -2);
  int tile_index = luaL_checkinteger(l, -1);

  // developers can change or see what tile is at any given position
  // Yeah... they can only modify valid tiles
  if (tile_position >= 1 && tile_position <= 96)
  {
    int max = GFX_TILES_NB_SQUARES - 1;

    if (tile_index >= 0 && tile_index <= max)
      cur_ed_screen.t[tile_position - 1].square_full_idx0 = tile_index;
    else
    {
      int line = dinklua_get_current_line(l);
      log_error("🧱 [Lua] %s.lua:%d: map_tile: invalid tile index '%d'",
                sinfo[current_lua_script]->name, line, tile_index);
    }

    lua_pushinteger(l, cur_ed_screen.t[tile_position - 1].square_full_idx0);
    return 1;
  }

  return 0;
}

static int dinklua_map_hard_tile(lua_State *l)
{
  int tile_position = luaL_checkinteger(l, -2);
  int hard_tile_index = luaL_checkinteger(l, -1);

  // developers can retrieve/modify a hard tile
  // Yeah... they can only modify valid tiles
  if (tile_position >= 1 && tile_position <= 96)
  {
    //Only change the value if it is greater than 0...
    if (hard_tile_index > 0)
      cur_ed_screen.t[tile_position - 1].althard = hard_tile_index;

    lua_pushinteger(l, cur_ed_screen.t[tile_position - 1].althard);
    return 1;
  }

  return 0;
}

static int dinklua_load_palette(lua_State *l)
{
  const char* bmp_file = luaL_checkstring(l, -1);

  // load a pallete from any bmp
  char *name = strdup(bmp_file);

  if (gfx_palette_set_from_bmp(name) < 0)
    log_error("🎨 [Lua] Couldn't load palette from '%s': %s", name, SDL_GetError());
  gfx_palette_get_phys(GFX_ref_pal);

  free(name);

  //Store in save game -- TODO: Check is bmp_file is proper for this (was slist[0])
  strncpy(play.palette, (char*)bmp_file, 50);

  return 0;
}

static int dinklua_get_item(lua_State *l)
{
  const char* script = luaL_checkstring(l, -1);

  // get index of specified item
  int retval = scripting_get_item((char*)script);

  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_get_magic(lua_State *l)
{
  const char* script = luaL_checkstring(l, -1);

  // get index of specified magic spell
  int retval = scripting_get_magic((char*)script);

  lua_pushinteger(l, retval);
  return 1;
}

static int dinklua_set_font_color(lua_State *l)
{
  int index = luaL_checkinteger(l, -4);
  int r = luaL_checkinteger(l, -3);
  int g = luaL_checkinteger(l, -2);
  int b = luaL_checkinteger(l, -1);

  // sets font color
  set_font_color(index, r, g, b);

  return 0;
}

static int dinklua_get_next_sprite_with_this_brain(lua_State *l)
{
  int brain = luaL_checkinteger(l, -3);
  int sprite_ignore = luaL_checkinteger(l, -2);
  int sprite_start_with = luaL_checkinteger(l, -1);

  // make Paul Pliska's life more fulfilling
  {
    for (int i = sprite_start_with; i <= last_sprite_created; i++)
    {
      if ((spr[i].brain == brain) && (i != sprite_ignore))
        if (spr[i].active == 1)
        {
          log_debug("🧠 Ok, sprite with brain %d is %d", brain, i);

          lua_pushinteger(l, i);
          return 1;
        }
    }
  }
  log_debug("🧠 Ok, sprite with brain %d is 0", brain);

  lua_pushinteger(l, 0); /* not found */
  return 1;
}

static int dinklua_set_smooth_follow(lua_State *l)
{
  int smooth_p = luaL_checkinteger(l, -1);
  smooth_follow = smooth_p;
  //if (smooth_p == 0)
  //  smooth_follow = 0;
  //else if (smooth_p == 1)
  //  smooth_follow = 1;

  return 0;
}

static int dinklua_set_dink_base_push(lua_State *l)
{
  int base_sequence = luaL_checkinteger(l, -1);

  dink_base_push = base_sequence;

  return 0;
}

static int dinklua_callback_kill(lua_State *l)
{
  int callback_index = luaL_checkinteger(l, -1);

  scripting_kill_callback(callback_index);

  return 0;
}

static int dinklua_set_global_variable_value(lua_State *l)
{
  const char* global_name = luaL_checkstring(l, -2);
  int global_value = luaL_checkinteger(l, -1);

  char variable_name[strlen(global_name) + 2];
  sprintf(variable_name, "&%s", global_name);

  int variable = search_var_with_this_scope(variable_name, VAR_GLOBAL_SCOPE);
  if (variable < 1)
    return luaL_error(l, "No global variable %s exists.", variable_name);

  play.var[variable].var = global_value;
  log_debug("🌏 [Lua] set global var %s to value %d", variable_name, global_value);
  return 0;
}

static int dinklua_get_global_variable_value(lua_State *l)
{
  const char* global_name = lua_tostring(l, -1);
  char variable_name[strlen(global_name) + 2];
  sprintf(variable_name, "&%s", global_name);

  int variable = search_var_with_this_scope(variable_name, VAR_GLOBAL_SCOPE);
  if (variable < 1)
    return luaL_error(l, "No global variable %s exists.", variable_name);

  lua_pushinteger (l, play.var[variable].var);
  return 1;
}

static int dinklua_choice_menu_prepare(lua_State *l)
{
  const char *title = lua_tostring(l, -3);
  int y = lua_tointeger(l, -2);
  int title_color = lua_tointeger(l, -1);

  game_choice_clear();
  game_choice.last++; // Because Seth doesn't (didn't?) like 0-indexing...

  if (strlen(title) > 0)
  {
    strncpy(game_choice.buffer, title, TALK_TITLE_BUFSIZ - 1);
    game_choice.buffer[TALK_TITLE_BUFSIZ - 1] = '\0';
  }

  game_choice.newy = y;
  game_choice.color = title_color;

  return 0;
}

static int dinklua_choice_menu_add_choice(lua_State *l)
{
  int index = lua_tointeger(l, -2);
  const char *choice = luaL_checkstring(l, -1);

  if (strcmp(choice, "&savegameinfo") == 0)
  {
    char gameinfo[196] = "";
    int mytime = 0;
    char *savechoice;

    if (load_game_small(index, gameinfo, &mytime) == 1)
      asprintf(&savechoice, _("Slot %d - %d:%02d - %s"), index,
               mytime/60, mytime%60, gameinfo);
    else
      asprintf(&savechoice, _("Slot %d - Empty"), index);

    strncpy(game_choice.line[game_choice.last], savechoice, TALK_LINE_BUFSIZ - 1);
    free(savechoice);
  }
  else if (strstr(choice, "&buttoninfo") != NULL)
  {
    char *buttonchoice = (char*)xmalloc(200);
    strncpy(buttonchoice, choice, 200);
    buttonchoice[199] = 0;

    int button_action = index;
    if      (button_action == ACTION_ATTACK)    replace("&buttoninfo", _("Attack"), &buttonchoice);
    else if (button_action == ACTION_TALK)      replace("&buttoninfo", _("Talk/Examine"), &buttonchoice);
    else if (button_action == ACTION_MAGIC)     replace("&buttoninfo", _("Magic"), &buttonchoice);
    else if (button_action == ACTION_INVENTORY) replace("&buttoninfo", _("Item Screen"), &buttonchoice);
    else if (button_action == ACTION_MENU)      replace("&buttoninfo", _("Main Menu"), &buttonchoice);
    else if (button_action == ACTION_MAP)       replace("&buttoninfo", _("Map"), &buttonchoice);
    else if (button_action == ACTION_BUTTON7)   replace("&buttoninfo", _("Do not use"), &buttonchoice);
    else if (button_action == ACTION_BUTTON8)   replace("&buttoninfo", _("Unused"), &buttonchoice);
    else if (button_action == ACTION_BUTTON9)   replace("&buttoninfo", _("Unused"), &buttonchoice);
    else if (button_action == ACTION_BUTTON10)  replace("&buttoninfo", _("Unused"), &buttonchoice);
    else if (button_action == ACTION_DOWN)      replace("&buttoninfo", _("Down"), &buttonchoice);
    else if (button_action == ACTION_LEFT)      replace("&buttoninfo", _("Left"), &buttonchoice);
    else if (button_action == ACTION_RIGHT)     replace("&buttoninfo", _("Right"), &buttonchoice);
    else if (button_action == ACTION_UP)        replace("&buttoninfo", _("Up"), &buttonchoice);
    else replace("&buttoninfo", _("Error: not mapped"), &buttonchoice);

    strncpy(game_choice.line[game_choice.last], buttonchoice, TALK_LINE_BUFSIZ - 1);
    free(buttonchoice);
  }
  else
  {
    strncpy(game_choice.line[game_choice.last], choice, TALK_LINE_BUFSIZ - 1);
  }

  game_choice.line_return[game_choice.last] = index;
  game_choice.last++;

  return 0;
}

// game_choice <- struct of type "struct game_choice_struct", defined in game_choice.h. Contains all
// information that the choice menu uses.
//
// game_choice.line[#] is the text for the given choice menu line.
// game_choice.line_return[#] is the value given to &result for that line.
// game_choice.last is the number of choices in the menu
// game_choice.cur is set to 1. (I presume it's the currently selected choice. Might allow us to offer to have a different one as the default one
// game_choice.active is set to 1. Obviously
// game_choice.page is set to 1. Not sure how that one works.
// game_choice.cur_view is set to 1. Not sure how that one works.
// game_choice.script is set to the script number that triggered the choice menu.
static int dinklua_choice_menu_show(lua_State *l)
{
  game_choice.cur = 1;
  game_choice.last--;
  game_choice.active = /*true*/1;
  game_choice.page = 1;
  game_choice.cur_view = 1;
  game_choice.script = current_lua_script;

  kill_text_owned_by(1);

  return 0;
}

static int dinklua_engine_find_script(lua_State *l)
{
  const char *filename = luaL_checkstring(l, -1);

  char *script_full_path;
  script_full_path = scripting_find_script_for_engine(lua_engine, filename);

  if (script_full_path == NULL)
  {
    lua_pushnil(l);
  }
  else
  {
    lua_pushstring(l, script_full_path);
    free(script_full_path);
  }
  return 1;
}

void dinklua_bind_init()
{
  luaL_Reg dink_funcs[] =
  {
    LUAREG(set_callback_random),
    LUAREG(reset_timer),
    LUAREG(set_keep_mouse),
    LUAREG(add_item),
    LUAREG(add_magic),
    LUAREG(add_exp),
    LUAREG(kill_this_item),
    LUAREG(kill_this_magic),
    LUAREG(copy_bmp_to_screen),
    LUAREG(stop_wait_for_button),
    LUAREG(load_screen),
    LUAREG(free_items),
    LUAREG(free_magic),
    LUAREG(draw_status),
    LUAREG(arm_weapon),
    LUAREG(arm_magic),
    LUAREG(preload_seq),
    LUAREG(script_attach),
    LUAREG(disable_all_sprites),
    LUAREG(draw_background),
    LUAREG(draw_hard_map),
    LUAREG(enable_all_sprites),
    LUAREG(get_burn),
    LUAREG(get_last_bow_power),
    LUAREG(get_version),
    LUAREG(get_client_fork),
    LUAREG(get_client_version),
    LUAREG(get_platform),
    LUAREG(get_last_sprite),
    LUAREG(get_fps),
    LUAREG(kill_all_sounds),
    LUAREG(scripts_used),
    LUAREG(stopcd),
    LUAREG(stopmidi),
    LUAREG(turn_midi_off),
    LUAREG(turn_midi_on),
    LUAREG(count_item),
    LUAREG(count_magic),
    LUAREG(compare_weapon),
    LUAREG(compare_magic),
    LUAREG(init),
    LUAREG(push_active),
    LUAREG(stop_entire_game),
    LUAREG(spawn),
    LUAREG(run_script_by_number),
    LUAREG(playsound),
    LUAREG(playsfx),
    LUAREG(playavi),
    LUAREG(get_seq_box),
    LUAREG(get_seq_offset),
    LUAREG(get_seq_hbox),
    LUAREG(save_game),
    LUAREG(force_vision),
    LUAREG(fill_screen),
    LUAREG(game_exist),
    LUAREG(load_sound),
    LUAREG(debug),
    LUAREG(inside_box),
    LUAREG(random),
    LUAREG(initfont),
    LUAREG(set_mode),
    LUAREG(debug),
    LUAREG(is_script_attached),
    LUAREG(set_button),
    LUAREG(screenlock),
    LUAREG(clear_editor_info),
    LUAREG(get_date_day),
    LUAREG(get_date_month),
    LUAREG(get_date_year),
    LUAREG(get_time_game),
    LUAREG(get_time_real),
    LUAREG(get_truecolor),
    LUAREG(show_inventory),
    LUAREG(var_used),
    LUAREG(playmidi),
    LUAREG(loopmidi),
    LUAREG(set_save_game_info),
    LUAREG(load_map),
    LUAREG(load_tile),
    LUAREG(map_tile),
    LUAREG(map_hard_tile),
    LUAREG(load_palette),
    LUAREG(get_item),
    LUAREG(get_magic),
    LUAREG(set_font_color),
    LUAREG(set_smooth_follow),
    LUAREG(callback_kill),
    {NULL, NULL}
  };
  luaL_newlib(luaVM, dink_funcs);
  lua_setglobal(luaVM, "dink");

  //Binding functions with sol2
  //TODO: get rid of sol2 and use luabridge exclusively
  sol::state_view lua(luaVM);
  lua["dink"]["copy_to_clipboard"] = copy_to_clipboard;
  lua["dink"]["set_music_vol"] = set_music_vol;
  lua["dink"]["show_console"] = scripting_show_console;
  lua["dink"]["rumble_controller"] = scripting_rumble_controller;
  //TODO: this should be under engine and changed in init file
  lua["savepath"] = paths_luasave;

  //sfx overrides with luabridge3
  luabridge::getGlobalNamespace(luaVM)
  .beginNamespace("sfx")
    .addProperty("sprite_hz", &sfx_spritehz)
    .beginNamespace("duck")
      .addProperty("sound", &sfx_ducksound)
      .addProperty("hz", &sfx_duckhz)
    .endNamespace()
    .beginNamespace("pig")
      .addProperty("sound", &sfx_pigsound)
      .addProperty("hz", &sfx_pighz)
    .endNamespace()
    .beginNamespace("attack")
      .addProperty("sound", &sfx_thwacksound)
      .addProperty("hz", &sfx_thwackhz)
    .endNamespace()
    .beginNamespace("warp")
      .addProperty("sound", &sfx_warpsound)
      .addProperty("hz", &sfx_warphz)
    .endNamespace()
    .beginNamespace("hurt")
      .addProperty("sound", &sfx_hurtsound)
      .addProperty("hz", &sfx_hurthz)
    .endNamespace()
    .beginNamespace("mouse")
      .addProperty("sound", &sfx_mousesound)
      .addProperty("hz", &sfx_mousehz)
    .endNamespace()
    .beginNamespace("choice_menu")
      .beginNamespace("scroll")
        .addProperty("sound", &sfx_choicescrollsound)
        .addProperty("hz", &sfx_choicescrollhz)
      .endNamespace()
      .beginNamespace("select")
        .addProperty("sound", &sfx_choiceselectsound)
        .addProperty("hz", &sfx_choiceselecthz)
      .endNamespace()
    .endNamespace()
    .beginNamespace("status")
      .beginNamespace("stats")
        .addProperty("sound", &sfx_statusstatsound)
        .addProperty("hz", &sfx_statusstathz)
      .endNamespace()
      .beginNamespace("gold")
        .addProperty("sound", &sfx_statusgoldsound)
        .addProperty("hz", &sfx_statusgoldhz)
      .endNamespace()
      .beginNamespace("exp_tick")
        .addProperty("sound", &sfx_statusexpticksound)
        .addProperty("hz", &sfx_statusexptickhz)
      .endNamespace()
      .beginNamespace("exp_count")
        .addProperty("sound", &sfx_statusexpcountsound)
        .addProperty("hz", &sfx_statusexpcounthz)
      .endNamespace()
    .endNamespace()
    .beginNamespace("inventory")
      .beginNamespace("enter")
        .addProperty("sound", &sfx_inventoryopensound)
        .addProperty("hz", &sfx_inventoryopenhz)
      .endNamespace()
      .beginNamespace("move")
        .addProperty("sound", &sfx_inventorymovesound)
        .addProperty("hz", &sfx_inventorymovehz)
      .endNamespace()
      .beginNamespace("select")
        .addProperty("sound", &sfx_inventoryselectsound)
        .addProperty("hz", &sfx_inventorymovehz)
      .endNamespace()
      .beginNamespace("exit")
        .addProperty("sound", &sfx_inventoryexitsound)
        .addProperty("hz", &sfx_inventoryexithz)
      .endNamespace()
    .endNamespace()
    //TODO: change this into something else
    .addFunction("play", SoundPlayEffect)
    .addFunction("halt_all", halt_all_sounds)
  .endNamespace();

  luabridge::getGlobalNamespace(luaVM)
  .beginNamespace("music")
    .addFunction("play", dinklua_object_playmidi)
    .addFunction("play_mod_order", play_modorder)
    .addFunction("pause", PauseMidi)
    .addFunction("resume", ResumeMidi)
    .addFunction("stop", StopMidi)
    .addFunction("fade_out", fade_music)
    .addFunction("load_soundfont", load_music_soundfont)
    .addProperty("position", get_music_position, set_music_position)
    .addProperty("title", &tag_title, false)
    .addProperty("artist", &tag_artist, false)
    .addProperty("album", &tag_album, false)
    .addProperty("duration", &mus_duration, false)
    .addProperty("is_playing", something_playing)
    .addProperty("volume", get_music_vol, set_music_vol)
    .addProperty("tempo", get_music_tempo, set_music_tempo)
    .addProperty("track", get_music_tracks, set_music_track)
    .addProperty("loop", &loop_midi)
  .endNamespace();

//engine behaviour overrides
  luabridge::getGlobalNamespace(luaVM)
  .beginNamespace("game")
    .addProperty("experience_limit", &customexp)
    .addProperty("map_width", &map_width)
    .addProperty("animate_tiles", &debug_tileanims)
    .addProperty("dinkc_divequals_fix", &debug_divequals)
    .addProperty("alttext_display", &debug_alttext)
    .addProperty("sprite_multiscript", &debug_scriptkill)
    .addFunction("getticks", game_GetTicks)
    .addFunction("enable_hardened_mode", scripting_enable_hardened_mode)
    .addFunction("get_cheat", get_cheat)
    .addFunction("get_screen_hitmap", get_screen_hitmap)
    .addFunction("is_controller_present", scripting_is_joystick_present)

  .endNamespace();

  //timers
  luabridge::getGlobalNamespace(luaVM)
  .beginNamespace("timer")
    .addProperty("choice_arrows", &timer_arrow)
    .addProperty("inventory", &timer_inventory)
    .addProperty("notouch", &timer_notouch)
    //.addProperty("showbmp", &timer_showbmp)
    .addProperty("water_tiles", &timer_water)
    .addProperty("bow", &timer_bow)
    .addProperty("bow_pull", &timer_bow_pull)
    .addProperty("fade_resume", &timer_fade)
    .addProperty("fade_time", &timer_fadetime)
    .addProperty("push", &timer_push)
  .endNamespace();

  //palette
  luabridge::getGlobalNamespace(luaVM)
  .beginNamespace("palette")
    .addFunction("setindex", gfx_palette_set_index)
    .addFunction("getindex", gfx_palette_get_index)
    .addFunction("set_choice_options", scripting_set_choice_colour)
    .addFunction("set_choice_hover", scripting_set_choice_hover_colour)
  .endNamespace();

  //map editing
  luabridge::getGlobalNamespace(luaVM)
  .beginNamespace("mapedit")
    .addFunction("save_hard", save_hard)
  .endNamespace();

  //Set our package path for require to ancillary
  char package_path[255];
  sprintf(package_path, "%s", paths_getluarequire());
  lua["package"]["path"] = package_path;

  luaL_Reg yield_funcs[] =
  {
    LUAYREG(show_bmp),
    LUAYREG(wait_for_button),
    LUAYREG(say_stop_xy),
    LUACYREG(draw_screen),
    LUAYREG(kill_cur_item),
    LUAYREG(kill_cur_magic),
    LUAYREG(restart_game),
    LUAYREG(wait),
    LUAYREG(activate_bow),
    LUAYREG(fade_up),
    LUAYREG(fade_down),
    LUAYREG(kill_game),
    LUAYREG(kill_this_task),
    LUAYREG(load_game),
    {NULL, NULL}
  };
  luaL_newlib(luaVM, yield_funcs);
  lua_setglobal(luaVM, "yield");

  luaL_Reg object_funcs[] =
  {
    LUAREG(set_sprite_value),
    LUAREG(get_sprite_value),
    LUAREG(sprite_object_kill_wait),
    LUAREG(sprite_object_freeze),
    LUAREG(sprite_object_unfreeze),
    LUAREG(dink_object_set_speed),
    LUAREG(dink_object_get_speed),
    LUAREG(sprite_object_say),
    LUAREG(yield_sprite_object_say_stop),
    LUAREG(yield_sprite_object_say_stop_npc),
    LUAREG(dink_object_can_walk_off_screen),
    LUAREG(set_editor_value),
    LUAREG(get_editor_value),
    LUAREG(sprite_object_move),
    LUAREG(set_soundbank_value),
    LUAREG(get_soundbank_value),
    LUAREG(soundbank_object_set_kill),
    LUAREG(soundbank_object_fade_vol),
    LUAREG(soundbank_object_fade_pan),
    LUAREG(soundbank_object_fade_speed),
    LUAREG(soundbank_object_schedule_pause),
    LUAREG(soundbank_object_schedule_stop),
    LUAREG(soundbank_object_oscillate_pan),
    LUAREG(soundbank_object_oscillate_speed),
    LUAREG(soundbank_object_oscillate_vol),
    LUAREG(yield_sprite_object_move_stop),
    LUAREG(sprite_object_busy),
    LUAREG(make_global_int),
    LUAREG(sprite_object_kill_shadow),
    LUAREG(create_sprite),
    LUAREG(get_sprite_with_this_brain),
    LUAREG(get_rand_sprite_with_this_brain),
    LUAREG(get_next_sprite_with_this_brain),
    LUAREG(sprite_object_hurt),
    LUAREG(compare_sprite_object_script),
    LUAREG(sp_custom),
    LUAREG(say_xy),
    LUAREG(set_dink_base_push),
    LUAREG(set_global_variable_value),
    LUAREG(get_global_variable_value),
    LUAREG(sprite_object_draw_hard_sprite),
    {NULL, NULL}
  };
  luaL_newlib(luaVM, object_funcs);
  lua_setglobal(luaVM, "object");

  luaL_Reg choice_menu_funcs[] =
  {
    LUACMREG(prepare),
    LUACMREG(add_choice),
    LUACMREG(show),
    //LUACMREG(get_result),
    {NULL, NULL}
  };
  luaL_newlib(luaVM, choice_menu_funcs);
  lua_setglobal(luaVM, "choice_menu");

  luaL_Reg engine_funcs[] =
  {
    {"find_script", dinklua_engine_find_script},
    {NULL, NULL}
  };
  luaL_newlib(luaVM, engine_funcs);
  lua_setglobal(luaVM, "engine");
}
