/**
 * Texture handler for OpenGL (ES) 2

 * Copyright (C) 2015  Sylvain Beucler

 * This file is part of GNU FreeDink

 * GNU FreeDink is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.

 * GNU FreeDink is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public
 * License along with GNU FreeDink.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IOGfxSurfaceGL2.h"

#include "SDL.h"
#include "SDL2_rotozoom.h"
#ifdef HAVE_SDL_GPU
#include "SDL_gpu.h"
#endif
#include "log.h"

IOGfxSurfaceGL2::IOGfxSurfaceGL2(SDL_Surface* image)
	: IOGfxSurface(image->w, image->h) {
	this->image = image;
}

IOGfxSurfaceGL2::~IOGfxSurfaceGL2() {
	SDL_FreeSurface(image);
}

/* Function specifically made for Dink'C fill_screen() */
void IOGfxSurfaceGL2::fill_screen(int num, SDL_Color* palette) {
	/* Warning: palette indexes 0 and 255 are hard-coded
	to black and white (cf. gfx_palette.c). */
	if (image->format->format == SDL_PIXELFORMAT_INDEX8)
		SDL_FillRect(image, NULL, num);
	else
		SDL_FillRect(image, NULL,
					SDL_MapRGB(image->format, palette[num].r, palette[num].g,
								palette[num].b));
}

int IOGfxSurfaceGL2::fillRect(const SDL_Rect* rect, Uint8 r, Uint8 g, Uint8 b) {
	return SDL_FillRect(image, rect, SDL_MapRGB(image->format, r, g, b));
}

int IOGfxSurfaceGL2::blit(IOGfxSurface* src, const SDL_Rect* srcrect,
						SDL_Rect* dstrect) {
	if (src == NULL)
		return SDL_SetError("IOGfxSurfaceSW::blit: passed a NULL surface");
	SDL_Surface* src_sdl = dynamic_cast<IOGfxSurfaceGL2*>(src)->image;
	return SDL_BlitSurface(src_sdl, srcrect, image, dstrect);
}

/**
 * Blit and resize so that 'src' fits in 'dst_rect'
 */
int gl_blit_stretch(SDL_Surface* src_surf, const SDL_Rect* src_rect_opt,
					SDL_Surface* dst_surf, SDL_Rect* dst_rect) {
	int retval = -1;

	SDL_Rect src_rect;
	if (src_rect_opt == NULL) {
		src_rect.x = 0;
		src_rect.y = 0;
		src_rect.w = src_surf->w;
		src_rect.h = src_surf->h;
	} else {
		src_rect = *src_rect_opt;
	}

	if (src_rect.w <= 0 || src_rect.h <= 0)
		return 0; // OK, ignore drawing, and don't mess sx/sy below

	double sx = 1.0 * dst_rect->w / src_rect.w;
	double sy = 1.0 * dst_rect->h / src_rect.h;
	/* In principle, double's are precise up to 15 decimal digits */
	if (fabs(sx - 1) > 1e-10 && fabs(sy - 1) > 1e-10) {
		//log_info("sx is %f, and sy is %f", sx, sy);
		SDL_Surface* scaled = zoomSurface(src_surf, sx, sy, SMOOTHING_ON);
		
		/* Keep the same transparency / alpha parameters (SDL_gfx bug,
	report submitted to the author: SDL_gfx adds transparency to
	non-transparent surfaces) */
		Uint8 r, g, b, a;
		Uint32 colorkey;
		int colorkey_enabled = (SDL_GetColorKey(src_surf, &colorkey) != -1);
		SDL_GetRGBA(colorkey, src_surf->format, &r, &g, &b, &a);

		SDL_SetColorKey(scaled, colorkey_enabled,
						SDL_MapRGBA(scaled->format, r, g, b, a));
		/* Don't mess with alpha transparency, though: */
		/* int alpha_flag = src->flags & SDL_SRCALPHA; */
		/* int alpha = src->format->alpha; */
		/* SDL_SetAlpha(scaled, alpha_flag, alpha); */

		src_rect.x = (int)round(src_rect.x * sx);
		src_rect.y = (int)round(src_rect.y * sy);
		src_rect.w = (int)round(src_rect.w * sx);
		src_rect.h = (int)round(src_rect.h * sy);
		//retval = SDL_BlitSurface(scaled, &src_rect, dst_surf, dst_rect);
		retval = SDL_BlitScaled(scaled, &src_rect, dst_surf, dst_rect);
		SDL_FreeSurface(scaled);
	} else {
		/* No scaling */

		retval = SDL_BlitSurface(src_surf, &src_rect, dst_surf, dst_rect);
	}
	return retval;
}

int IOGfxSurfaceGL2::blitStretch(IOGfxSurface* src, const SDL_Rect* srcrect,
								SDL_Rect* dstrect) {
	if (src == NULL)
		return SDL_SetError(
				"IOGfxSurfaceSW::blitStretch: passed a NULL surface");
	SDL_Surface* src_sdl = dynamic_cast<IOGfxSurfaceGL2*>(src)->image;
	return gl_blit_stretch(src_sdl, srcrect, image, dstrect);
}

/**
 * Temporary disable src's transparency and blit it to dst
 */
int gl_blit_nocolorkey(SDL_Surface* src, const SDL_Rect* src_rect,
						SDL_Surface* dst, SDL_Rect* dst_rect) {
	int retval = -1;
	if (src == NULL) {
		log_error("attempting to blit a NULL surface");
		return retval;
	}

	Uint32 colorkey;
	SDL_BlendMode blendmode;
	Uint32 rle_flags = src->flags & SDL_RLEACCEL;
	int has_colorkey = (SDL_GetColorKey(src, &colorkey) != -1);
	SDL_GetSurfaceBlendMode(src, &blendmode);

	if (has_colorkey)
		SDL_SetColorKey(src, SDL_FALSE, 0);
	SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
	retval = SDL_BlitSurface(src, src_rect, dst, dst_rect);

	SDL_SetSurfaceBlendMode(src, blendmode);
	if (has_colorkey)
		SDL_SetColorKey(src, SDL_TRUE, colorkey);
	if (rle_flags)
		SDL_SetSurfaceRLE(src, 1);

	return retval;
}

int IOGfxSurfaceGL2::blitNoColorKey(IOGfxSurface* src, const SDL_Rect* srcrect,
								SDL_Rect* dstrect) {
	if (src == NULL)
		return SDL_SetError(
				"IOGfxSurfaceSW::blitNoColorKey: passed a NULL surface");
	SDL_Surface* src_sdl = dynamic_cast<IOGfxSurfaceGL2*>(src)->image;
	return gl_blit_nocolorkey(src_sdl, srcrect, image, dstrect);
}

SDL_Surface* IOGfxSurfaceGL2::screenshot() {
	return SDL_ConvertSurface(image, image->format, 0);
}

unsigned int IOGfxSurfaceGL2::getMemUsage() {
	// TODO: take RLE and metadata into account
	return image->h * image->pitch;
}
