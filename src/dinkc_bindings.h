/**
 * Link game engine and DinkC script engine

 * Copyright (C) 2008  Sylvain Beucler
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

#ifndef _DINKC_BINDINGS_H
#define _DINKC_BINDINGS_H

#include "dinkc.h"

extern void dinkc_bindings_init();
extern void dinkc_bindings_quit();
extern void attach(void);
extern /*bool*/ int talk_get(int script);
//Accessed by Lua
extern int change_sprite_noreturn(int h, int val, int* change);
extern int change_sprite(int h, int val, int* change);
extern int change_edit_char(int h, int val, unsigned char* change);
extern int change_edit(int h, int val, unsigned short* change);
#endif
