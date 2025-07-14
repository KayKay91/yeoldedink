/**
 * Draw background from tiles

 * Copyright (C) 1997, 1998, 1999, 2002, 2003  Seth A. Robinson
 * Copyright (C) 2007, 2008, 2009  Sylvain Beucler

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

#include "gfx_tiles.h"

#include "editor_screen.h"
#include "gfx.h"
#include "log.h"
#include "random.hpp"
//for random tile timing
using Random = effolkronium::random_static;

/* Animated tiles current status */
static unsigned int water_timer = 0;
int timer_water = 2000;
static int fire_flip = 0;
int tiles_fire_flip_range = 3;
int tiles_fire_start_sheet = 19;
int tiles_water_flip_range = 2;
int tiles_water_start_sheet = 8;

/**
 * Draw tile number 'dsttile_square_id0x' (in [0, 96-1]) in the
 * current screen
 */
void gfx_tiles_draw(IOGfxSurface** gfx_tiles, int srctileset_idx0,
					int srctile_square_idx0, int dsttile_square_idx0) {
	SDL_Rect src;
	int srctile_square_x = srctile_square_idx0 % GFX_TILES_SCREEN_W;
	int srctile_square_y = srctile_square_idx0 / GFX_TILES_SCREEN_W;
	src.x = srctile_square_x * GFX_TILES_SQUARE_SIZE;
	src.y = srctile_square_y * GFX_TILES_SQUARE_SIZE;
	src.w = GFX_TILES_SQUARE_SIZE;
	src.h = GFX_TILES_SQUARE_SIZE;

	int dsttile_x = dsttile_square_idx0 % GFX_TILES_SCREEN_W;
	int dsttile_y = dsttile_square_idx0 / GFX_TILES_SCREEN_W;
	SDL_Rect dst;
	dst.x = GFX_PLAY_LEFT + dsttile_x * GFX_TILES_SQUARE_SIZE;
	dst.y = GFX_PLAY_TOP + dsttile_y * GFX_TILES_SQUARE_SIZE;

	IOGFX_background->blit(gfx_tiles[srctileset_idx0 + 1], &src, &dst);
}

/**
 * Draw all background tiles in the current screen
 */
void gfx_tiles_draw_screen(IOGfxSurface** gfx_tiles,
						struct editor_screen_tilerefs* tilerefs) {
	int x = 0;
	for (; x < GFX_TILES_PER_SCREEN; x++) {
		int srctileset_idx0 = tilerefs[x].square_full_idx0 / 128;
		int srctile_square_idx0 = tilerefs[x].square_full_idx0 % 128;
		gfx_tiles_draw(gfx_tiles, srctileset_idx0, srctile_square_idx0, x);
	}
}

/* Game-specific: animate background (water, fire, ...) */
void process_animated_tiles(IOGfxSurface** gfx_tiles,
							struct editor_screen_tilerefs* tilerefs,
							Uint64 thisTickCount) {
	// Water:
	if (water_timer < thisTickCount) {

		water_timer = thisTickCount + Random::get(0, timer_water);
		int flip = Random::get(0, tiles_water_flip_range);

		for (int x = 0; x < 96; x++) {
			int screen_square_full_idx0 = tilerefs[x].square_full_idx0;
			int start_full_idx0 = (tiles_water_start_sheet - 1) * 128; // 8th tileset -> 896
			if (screen_square_full_idx0 >= start_full_idx0 && screen_square_full_idx0 < (start_full_idx0 + 128))
				gfx_tiles_draw(gfx_tiles, (tiles_water_start_sheet - 1) + flip, screen_square_full_idx0 % 128, x);
		}
	}

	// Fire:
	{
		fire_flip--;
		if (fire_flip < 0)
			fire_flip = tiles_fire_flip_range;

		int x = 0;
		for (; x < 96; x++) {
			int screen_square_full_idx0 = tilerefs[x].square_full_idx0;
			int start_full_idx0 = (tiles_fire_start_sheet - 1) * 128; // 19th tileset -> 2304
			if (screen_square_full_idx0 >= start_full_idx0 && screen_square_full_idx0 < (start_full_idx0 + 128))
				gfx_tiles_draw(gfx_tiles, (tiles_fire_start_sheet - 1) + fire_flip, screen_square_full_idx0 % 128, x);
		}
	}
}
