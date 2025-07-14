/**
 * OpenGL backend powered by SDL-GPU

 * Copyright (c) 2023 Yeoldetoast

 * This file is part of Yeoldedink

 * Yeoldedink is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.

 * Yeoldedink is distributed in the hope that it will be useful, but
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

#include "IOGfxDisplayGL2.h"
#include "IOGfxSurfaceSW.h"
#ifdef HAVE_SDL_GPU
#include "SDL_gpu.h"
#include "SDL_opengl.h"
#include <string.h>

#include "log.h"
#include "gfx_palette.h"
#include "gfx.h"
#include "ImageLoader.h"
#include "debug.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "debug_imgui.h"
#include "paths.h"
#include "misc/freetype/imgui_freetype.h"


IOGfxDisplayGL2::IOGfxDisplayGL2(int w, int h, bool truecolor, Uint32 flags)
	: IOGfxDisplay(w, h, truecolor, flags), renderer(NULL),
	render_texture_linear(NULL), render_texture_nearest(NULL),
	rgb_screen(NULL) {
}

IOGfxDisplayGL2::~IOGfxDisplayGL2() {
	close();
}

bool IOGfxDisplayGL2::open() {
	int req_w = w;
	int req_h = h;

	if (!IOGfxDisplay::open())
		return false;
	if (!createRenderer())
		return false;
	if (!createRenderTexture(req_w, req_h))
		return false;

	return true;
}

void IOGfxDisplayGL2::close() {
	ImGui_ImplOpenGL3_Shutdown();
	GPU_FreeShaderProgram(shader);
	if (render_texture_linear)
		GPU_FreeImage(render_texture_linear);
	render_texture_linear = NULL;
	//if (rgb_screen)
	//	SDL_FreeSurface(rgb_screen);
		rgb_screen = NULL;
	if (renderer)
		GPU_FreeTarget(renderer);
	renderer = NULL;

	IOGfxDisplay::close();
}

//For colour shader
void update_color_shader(float r, float g, float b, float a, int color_loc)
{
    float fcolor[4] = {r, g, b, a};
    GPU_SetUniformfv(color_loc, 4, 1, fcolor);
}

GPU_ShaderBlock load_shader_program(Uint32* p, const char* vertex_shader_file, const char* fragment_shader_file)
{
    Uint32 v, f;
    v = GPU_LoadShader(GPU_VERTEX_SHADER, vertex_shader_file);

    if(!v)
        GPU_LogError("Failed to load vertex shader (%s): %s\n", vertex_shader_file, GPU_GetShaderMessage());

    f = GPU_LoadShader(GPU_FRAGMENT_SHADER, fragment_shader_file);

    if(!f)
        GPU_LogError("Failed to load fragment shader (%s): %s\n", fragment_shader_file, GPU_GetShaderMessage());

    *p = GPU_LinkShaders(v, f);

    if(!*p)
    {
		GPU_ShaderBlock b = {-1, -1, -1, -1};
        GPU_LogError("Failed to link shader program (%s + %s): %s\n", vertex_shader_file, fragment_shader_file, GPU_GetShaderMessage());
        return b;
    }

	{
		GPU_ShaderBlock block = GPU_LoadShaderBlock(*p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
		GPU_ActivateShaderProgram(*p, &block);

		return block;
	}
}

bool IOGfxDisplayGL2::createRenderer() {
	//TODO: find working settings for windows
	GPU_SetPreInitFlags(GPU_RENDERER_OPENGL_2 | GPU_INIT_DISABLE_VSYNC);
	GPU_SetInitWindow(SDL_GetWindowID(window));
	renderer = GPU_Init(640, 480, GPU_INIT_DISABLE_VSYNC);
	if (renderer == NULL) {
		log_error("Unable to create renderer: %s\n", SDL_GetError());
		return false;
	}
	//Imgui Init for OpenGL
	IMGUI_CHECKVERSION();
    ImGui::CreateContext();
	ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
	//Load our normal display font
	io.Fonts->AddFontFromFileTTF(paths_pkgdatafile(debugfon), debug_fontsize);
	//Load our emojikanker
	static ImFontConfig cfg;
	static ImWchar ranges[] = { 0x1, 0x1FFFF, 0 };
	cfg.OversampleH = cfg.OversampleV = 1;
	cfg.MergeMode = true;
	#ifdef IMGUI_ENABLE_FREETYPE_LUNASVG
	cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
	io.Fonts->AddFontFromFileTTF(paths_pkgdatafile("TwitterColorEmoji-SVGinOT-MacOS.ttf"), debug_fontsize, &cfg, ranges);
	#else
	io.Fonts->AddFontFromFileTTF(paths_pkgdatafile("NotoEmoji-VariableFont_wght.ttf"), debug_fontsize, &cfg, ranges);
	#endif

	//Other config stuff for input
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.WantCaptureMouse = true;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.MouseDrawCursor = false;
	io.WantCaptureKeyboard = true;
	io.IniFilename = NULL;
	ImGui::StyleColorsDark();
	//ImGuiStyle style = ImGui::GetStyle();
	//style.ScaleAllSizes(1.0);
	//style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.30f, 0.00f, 0.00f, 1.00f);
	//Init renderer
	ImGui_ImplSDL2_InitForOpenGL(window, renderer->context->context);
	const char* glsl_version = "#version 150";
	ImGui_ImplOpenGL3_Init(glsl_version);

	//Load test shader
	color_block = load_shader_program(&shader, paths_pkgdatafile("common.vert"), paths_pkgdatafile("color.frag"));
	color_loc = GPU_GetUniformLocation(shader, "myColor");

	return true;
}

/**
 * Screen-like destination texture
 */
bool IOGfxDisplayGL2::createRenderTexture(int w, int h) {
	//Currently meaningless distinction between the two
	render_texture_linear = GPU_CreateImage(GFX_RES_W, GFX_RES_H, GPU_FORMAT_RGB);
	if (render_texture_linear == NULL) {
		log_error("Unable to create render texture: %s", SDL_GetError());
		return false;
	}

	render_texture_nearest = GPU_CreateImage(GFX_RES_W, GFX_RES_H, GPU_FORMAT_RGBA);
	if (render_texture_nearest == NULL) {
		log_error("Unable to create debug render texture: %s", SDL_GetError());
		return false;
	}

	Uint32 render_texture_format;
	Uint32 Rmask, Gmask, Bmask, Amask;
	int bpp;
	//SDL_QueryTexture(render_texture_linear, &render_texture_format, NULL, NULL,
	//				NULL);
	//SDL_PixelFormatEnumToMasks(render_texture_format, &bpp, &Rmask, &Gmask,
	//						&Bmask, &Amask);
	if (!truecolor)
		rgb_screen =
				SDL_CreateRGBSurface(0, w, h, 8, Rmask, Gmask, Bmask, Amask);

	return true;
}

static void logRendererInfo(GPU_RendererID info) {
	log_info("👨‍🎨  Renderer driver: %s", info.name);
	log_info("👨‍🎨  OpenGL Version: %d.%d", info.major_version, info.minor_version);
}

void IOGfxDisplayGL2::logRenderersInfo() {
	log_info("👨‍🎨 Current renderer:\n");
	GPU_Renderer* renderer = GPU_GetCurrentRenderer();
	GPU_RendererID id = renderer->id;
	logRendererInfo(id);
	log_info("👨‍🎨 Shader versions supported: %d to %d", renderer->min_shader_version, renderer->max_shader_version);
}

void IOGfxDisplayGL2::logRenderTextureInfo() {
	Uint32 format;
	int access, w, h;
	//SDL_QueryTexture(render_texture_linear, &format, &access, &w, &h);
	log_info("👨‍🎨 Render texture: format: %s", SDL_GetPixelFormatName(format));
	char* str_access;
	switch (access) {
	case SDL_TEXTUREACCESS_STATIC:
		str_access = "SDL_TEXTUREACCESS_STATIC";
		break;
	case SDL_TEXTUREACCESS_STREAMING:
		str_access = "SDL_TEXTUREACCESS_STREAMING";
		break;
	case SDL_TEXTUREACCESS_TARGET:
		str_access = "SDL_TEXTUREACCESS_TARGET";
		break;
	default:
		str_access = "Unknown!";
		break;
	}
	log_info("👨‍🎨 Render texture: access: %s", str_access);
	log_info("👨‍🎨 Render texture: width : %d", w);
	log_info("👨‍🎨 Render texture: height: %d", h);
	Uint32 Rmask, Gmask, Bmask, Amask;
	int bpp;
	SDL_PixelFormatEnumToMasks(format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
	log_info("👨‍🎨 Render texture: bpp   : %d", bpp);
	log_info("👨‍🎨 Render texture: masks : %08x %08x %08x %08x", Rmask, Gmask, Bmask,
			Amask);
}

void IOGfxDisplayGL2::logDisplayInfo() {
	log_info("👨‍🎨 YeOldeDink graphics mode: IOGfxDisplayGL2");
	IOGfxDisplay::logDisplayInfo();
	logRenderersInfo();
	logRenderTextureInfo();
}

void IOGfxDisplayGL2::clear() {
	GPU_Clear(renderer);
}

void gl_fade_apply(SDL_Surface* screen, int brightness) {
	//Check SDL_blit.h in the SDL source code for guidance
	SDL_LockSurface(screen);
	// Progress per pixel rather than per byte
	int remainder =
			(screen->pitch - (screen->w * screen->format->BytesPerPixel)) /
			screen->format->BytesPerPixel;
	// Using aligned Uint32 is faster than working with Uint8 values
	Uint32* p = (Uint32*)screen->pixels;
	int height = screen->h;
	while (height--) {
		int x;
		for (x = 0; x < screen->w; x++) {
			// Assume that pixel order is RGBA
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
			if (*p != 0x00FFFFFF) { // skip white
#else
			if (*p != 0xFFFFFF00) { // TODO: I need a PPC tester for this
#endif
				*((Uint8*)p) = *((Uint8*)p) * brightness >> 8;
				*((Uint8*)p + 1) = *((Uint8*)p + 1) * brightness >> 8;
				*((Uint8*)p + 2) = *((Uint8*)p + 2) * brightness >> 8;
			}
			p++;
		}
		p += remainder;
	}
	SDL_UnlockSurface(screen);
}

void IOGfxDisplayGL2::flip(IOGfxSurface* backbuffer, SDL_Rect* dstrect,
						bool interpolation, bool hwflip) {
	/* For now we do all operations on the CPU side and perform a big
	texture update at each frame; this is necessary to support
	palette and fade_down/fade_up. */
	ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

	//interpolation = false;

	//Imgui start
	if (debug_mode) {
		//Run our debug stuff
		dbg_imgui();
		//Stuck this here so I could more easily get the texture
		if (debug_gamewindow) {
		ImGui::Begin("Game Display", &debug_gamewindow, ImGuiWindowFlags_AlwaysAutoResize);
		//Shows the currently-used texture according to filter settings
		if (dbg.debug_interp)
		ImGui::Image((ImTextureID)GPU_GetTextureHandle(render_texture_linear),ImVec2((int)640,480));
		else {
			ImGui::Image((ImTextureID)GPU_GetTextureHandle(render_texture_nearest),ImVec2((int)640,480));
		}
		ImGui::End();
		}
	}
	else {
		//hide the cursor outside of debugh mode
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
	}

	/* Convert to destination buffer format */
	SDL_Surface* source = dynamic_cast<IOGfxSurfaceSW*>(backbuffer)->image;

	if (truecolor && brightness < 256)
		gl_fade_apply(source, brightness);

	if (truecolor) {
		if (source->format->format != getFormat())
			log_error("Wrong backbuffer format");
	} else {
		/* Convert 8-bit buffer for truecolor texture upload */

		/* Use "physical" screen palette - use SDL_SetPaletteColors to invalidate SDL cache */
		SDL_Color pal_bak[256];
		SDL_Color pal_phys[256];
		memcpy(pal_bak, source->format->palette->colors, sizeof(pal_bak));
		gfx_palette_get_phys(pal_phys);
		SDL_SetPaletteColors(source->format->palette, pal_phys, 0, 256);

		if (SDL_BlitSurface(source, NULL, rgb_screen, NULL) < 0) {
			log_error("ERROR: 8-bit->truecolor conversion failed: %s",
					SDL_GetError());
		}
		SDL_SetPaletteColors(source->format->palette, pal_bak, 0, 256);

		source = rgb_screen;
	}
	//Set our filtering
	interpolation = dbg.debug_interp;
	if (interpolation) {
	GPU_SetImageFilter(render_texture_linear, GPU_FILTER_LINEAR);
	GPU_SetImageFilter(render_texture_nearest, GPU_FILTER_LINEAR);
	}
	else
	{
	GPU_SetImageFilter(render_texture_linear, GPU_FILTER_NEAREST);
	GPU_SetImageFilter(render_texture_nearest, GPU_FILTER_NEAREST);
	}
	//TODO: Set filling of whole screen
	GPU_Rect drect;
	if (!dbg.debug_assratio)
		drect = GPU_MakeRect(dstrect->x, dstrect->y, dstrect->w, dstrect->h);

	//Render both textures so we get imgui or something
	GPU_UpdateImage(render_texture_linear, NULL, source, NULL);
	if (debug_drawgame)
		GPU_BlitRect(render_texture_linear, NULL, renderer, &drect);
	//} else {
		GPU_UpdateImage(render_texture_nearest, NULL, source, NULL);
	if (debug_drawgame)
		GPU_BlitRect(render_texture_nearest, NULL, renderer, &drect);

	//Put shader here
	GPU_ActivateShaderProgram(shader, &color_block);
	//if (dbg.shader_colcycle) {
	//float t = SDL_GetTicks()/1000.0f;
	//update_color_shader((1+sin(t))/2, (1+sin(t+1))/2, (1+sin(t+2))/2, 1.0f, color_loc);
	//}
	//else {
		update_color_shader(dbg.shader_r, dbg.shader_g, dbg.shader_b, 1.0f, color_loc);
	//}


	if (hwflip)
		//Render imgui
		ImGui::Render();
		SDL_GL_MakeCurrent(window, renderer->context->context);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		//Render our draw data
		GPU_Flip(renderer);

}

void IOGfxDisplayGL2::onSizeChange(int w, int h) {
	this->w = w;
	this->h = h;
	GPU_SetVirtualResolution(renderer, w, h);
}

SDL_Surface* IOGfxDisplayGL2::screenshot(SDL_Rect* rect) {
	Uint32 Rmask = 0, Gmask = 0, Bmask = 0, Amask = 0;
	int bpp = 0;
	SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_RGBA8888, &bpp, &Rmask, &Gmask,
							&Bmask, &Amask);
	SDL_Surface* surface = SDL_CreateRGBSurface(0, rect->w, rect->h, 32, Rmask,
												Gmask, Bmask, Amask);

	if (GPU_CopySurfaceFromTarget(renderer)) {
		log_error("IOGfxDisplayGL2::screenshot: %s", SDL_GetError());
		SDL_FreeSurface(surface);
		return NULL;
	}

	return surface;
}

IOGfxSurface* IOGfxDisplayGL2::upload(SDL_Surface* image) {
	/* Set RLE encoding to save memory space and improve perfs if colorkey */
	SDL_SetSurfaceRLE(image, SDL_TRUE);
	/* Force RLE-encode */
	SDL_LockSurface(image);
	SDL_UnlockSurface(image);

	return new IOGfxSurfaceSW(image);
}

IOGfxSurface* IOGfxDisplayGL2::allocBuffer(int surfW, int surfH) {
	Uint32 Rmask = 0, Gmask = 0, Bmask = 0, Amask = 0;
	int bpp = 0;
	SDL_PixelFormatEnumToMasks(getFormat(), &bpp, &Rmask, &Gmask, &Bmask,
							&Amask);
	SDL_Surface* image = SDL_CreateRGBSurface(0, surfW, surfH, bpp, Rmask,
											Gmask, Bmask, Amask);

	/* When a new image is loaded in DX, it's color-converted using the
	main palette (possibly altering the colors to match the palette);
	currently we emulate that by wrapping SDL_LoadBMP, converting
	image to the internal palette at load time - and we never change
	the buffer's palette again, so we're sure there isn't any
	conversion even if we change the screen palette: */
	if (!truecolor)
		SDL_SetPaletteColors(image->format->palette, GFX_ref_pal, 0, 256);

	return new IOGfxSurfaceSW(image);
}

#endif
