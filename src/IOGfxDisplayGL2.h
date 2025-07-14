/**
 * Graphics primitives with OpenGL (ES) 2

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

#ifndef IOGRAPHICS_H
#define IOGRAPHICS_H

#include "SDL.h"
#ifdef HAVE_SDL_GPU
#include "SDL_gpu.h"

#include "SDL_opengl.h"

#include "IOGfxDisplay.h"

#include <map>
#include <string>

class IOGfxDisplayGL2 : public IOGfxDisplay {
private:
	GPU_Target* renderer;
	//For shaders
	Uint32 shader;
	GPU_ShaderBlock color_block;
	int color_loc;
	/* Streaming texture to push software buffer -> hardware */
	GPU_Image* render_texture_linear;
	/* Non-interpolated texture for printing raw textures during tests screenshots */
	GPU_Image* render_texture_nearest;
	/* Intermediary texture to convert 8bit->24bit in non-truecolor */
	SDL_Surface* rgb_screen;

public:
	IOGfxDisplayGL2(int w, int h, bool truecolor, Uint32 flags);
	virtual ~IOGfxDisplayGL2();

	virtual bool open();
	virtual void close();
	virtual void logDisplayInfo();

	virtual void clear();
	virtual void onSizeChange(int w, int h);
	virtual IOGfxSurface* upload(SDL_Surface* s);
	virtual IOGfxSurface* allocBuffer(int surfW, int surfH);
	virtual void flip(IOGfxSurface* backbuffer, SDL_Rect* dstrect,
					bool interpolation, bool hwflip);

	virtual SDL_Surface* screenshot(SDL_Rect* rect);

	bool createRenderer();
	void logRenderersInfo();

	bool createRenderTexture(int w, int h);
	void logRenderTextureInfo();
};

#endif

#endif
