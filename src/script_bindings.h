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

//TODO: rewrite a ton of scripting functions to be common to this file

#ifndef _SCRIPT_BINDINGS_H
#define _SCRIPT_BINDINGS_H

extern int scripting_sp_script(int sprite, const char *script);
extern void scripting_set_disable_savestates(bool state);
extern void scripting_enable_hardened_mode();
extern bool scripting_is_joystick_present();
extern void scripting_rumble_controller(int freq1, int freq2, int duration);
extern void scripting_show_console();
//inventory
extern int scripting_count_item(char* script);
extern int scripting_count_magic(char* script);
extern int scripting_free_items();
extern int scripting_free_magic();
extern int scripting_get_item(char* script);
extern int scripting_get_magic(char* script);

#endif
