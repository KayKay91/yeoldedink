#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "IOGfxDisplaySW.h"
#include "IOGfxSurfaceSW.h"
#include "gfx_palette.h"
#include "gfx.h"
#include "ImageLoader.h" /* GFX_ref_pal */ // TODO: break dep

#include "log.h"
#include "paths.h"
#include "debug.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#ifndef DINKEDIT
#include "debug_imgui.h"
#include "dinkc_console.h"
#else
#include "debug_imgui_editor.h"
#endif
#include "implot.h"
#include "SDL2_rotozoom.h"
#include "imgui_freetype.h"

IOGfxDisplaySW::IOGfxDisplaySW(int w, int h, bool truecolor, Uint32 flags)
	: IOGfxDisplay(w, h, truecolor, flags), renderer(NULL),
	render_texture_linear(NULL), render_texture_nearest(NULL),
	rgb_screen(NULL) {
}

IOGfxDisplaySW::~IOGfxDisplaySW() {
	close();
}

bool IOGfxDisplaySW::open() {
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

void IOGfxDisplaySW::close() {
	if (render_texture_linear)
		SDL_DestroyTexture(render_texture_linear);
	render_texture_linear = NULL;
	if (rgb_screen)
		SDL_FreeSurface(rgb_screen);
	rgb_screen = NULL;

	if (renderer)
		SDL_DestroyRenderer(renderer);
	renderer = NULL;

	ImGui_ImplSDLRenderer2_Shutdown();

	IOGfxDisplay::close();
}

bool IOGfxDisplaySW::createRenderer() {
	/* TODO SDL2: make it configurable for speed: nearest/linear/DX-specific-best */
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, 0);

	#ifdef __APPLE__
	//metal messes with imgui for some reason
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	#endif

	/* TODO SDL2: make render driver configurable to ease software-mode testing */
	renderer = SDL_CreateRenderer(window, -1 /*autoselect*/, 0);
	/* TODO SDL2: optionally pass SDL_RENDERER_PRESENTVSYNC to 3rd param */
	//yeolde: vsync screws up fast mode
	if (renderer == NULL) {
		log_error("Unable to create renderer: %s\n", SDL_GetError());
		return false;
	}
	// Specify aspect ratio
	SDL_RenderSetLogicalSize(renderer, w, h);
	//Imgui Init for SDLRenderer
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
	#ifdef IMGUI_ENABLE_FREETYPE_PLUTOSVG
	cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
	io.Fonts->AddFontFromFileTTF(paths_pkgdatafile("TwitterColorEmoji-SVGinOT-MacOS.ttf"), debug_fontsize, &cfg, ranges);
	#else
	io.Fonts->AddFontFromFileTTF(paths_pkgdatafile("NotoEmoji-VariableFont_wght.ttf"), debug_fontsize, &cfg, ranges);
	#endif
	load_console_font();
	//Other config stuff for input
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.WantCaptureMouse = true;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.MouseDrawCursor = false;
	io.WantCaptureKeyboard = true;
	//Turn off ini file default location
	io.IniFilename = NULL;
	switch (dbg.uistyle)
    {
	case 0: dinktheme(); break;
    case 1: ImGui::StyleColorsDark(); break;
    case 2: ImGui::StyleColorsLight(); break;
    case 3: ImGui::StyleColorsClassic(); break;
    }
	//Init renderer
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);

	return true;
}

/**
 * Screen-like destination texture
 */
bool IOGfxDisplaySW::createRenderTexture(int w, int h) {
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
	render_texture_linear =
			SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
							SDL_TEXTUREACCESS_STREAMING, GFX_RES_W, GFX_RES_H);
	if (render_texture_linear == NULL) {
		log_error("Unable to create render texture: %s", SDL_GetError());
		return false;
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	render_texture_nearest =
			SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
							SDL_TEXTUREACCESS_STREAMING, GFX_RES_W, GFX_RES_H);
	if (render_texture_nearest == NULL) {
		log_error("Unable to create debug render texture: %s", SDL_GetError());
		return false;
	}

	Uint32 render_texture_format;
	Uint32 Rmask, Gmask, Bmask, Amask;
	int bpp;
	SDL_QueryTexture(render_texture_linear, &render_texture_format, NULL, NULL,
					NULL);
	SDL_PixelFormatEnumToMasks(render_texture_format, &bpp, &Rmask, &Gmask,
							&Bmask, &Amask);
	if (!truecolor)
		rgb_screen =
				SDL_CreateRGBSurface(0, w, h, bpp, Rmask, Gmask, Bmask, Amask);

	return true;
}

static void logRendererInfo(SDL_RendererInfo* info) {
	log_info("👨‍🎨  Renderer driver: %s", info->name);
	log_info("👨‍🎨  Renderer flags:");
	if (info->flags & SDL_RENDERER_SOFTWARE)
		log_info("👨‍🎨    SDL_RENDERER_SOFTWARE");
	if (info->flags & SDL_RENDERER_ACCELERATED)
		log_info("👨‍🎨    SDL_RENDERER_ACCELERATED");
	if (info->flags & SDL_RENDERER_PRESENTVSYNC)
		log_info("👨‍🎨    SDL_RENDERER_PRESENTVSYNC");
	if (info->flags & SDL_RENDERER_TARGETTEXTURE)
		log_info("👨‍🎨    SDL_RENDERER_TARGETTEXTURE");
	log_info("👨‍🎨  Renderer texture formats:");
	for (unsigned int i = 0; i < info->num_texture_formats; i++) {
		log_info("👨‍🎨   %s", SDL_GetPixelFormatName(info->texture_formats[i]));
	}
	log_info("👨‍🎨 Renderer max texture width: %d", info->max_texture_width);
	log_info("👨‍🎨 Renderer max texture height: %d", info->max_texture_height);
}

void IOGfxDisplaySW::logRenderersInfo() {
	log_info("👨‍🎨 Current renderer:\n");
	SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);
	logRendererInfo(&info);

	log_info("👨‍🎨 Available renderers:");
	for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
		SDL_RendererInfo info;
		SDL_GetRenderDriverInfo(i, &info);
		log_info("%d:\n", i);
		logRendererInfo(&info);
	}
}

void IOGfxDisplaySW::logRenderTextureInfo() {
	Uint32 format;
	int access, w, h;
	SDL_QueryTexture(render_texture_linear, &format, &access, &w, &h);
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

void IOGfxDisplaySW::logDisplayInfo() {
	log_info("👨‍🎨 YeOldeDink graphics mode: IOGfxDisplaySW");
	IOGfxDisplay::logDisplayInfo();
	logRenderersInfo();
	logRenderTextureInfo();
}

void IOGfxDisplaySW::clear() {
	SDL_RenderClear(renderer);
}

void gfx_fade_apply(SDL_Surface* screen, int brightness) {
	/* Check SDL_blit.h in the SDL source code for guidance */
	SDL_LockSurface(screen);
	/* Progress per pixel rather than per byte */
	int remainder =
			(screen->pitch - (screen->w * screen->format->BytesPerPixel)) /
			screen->format->BytesPerPixel;
	/* Using aligned Uint32 is faster than working with Uint8 values */
	Uint32* p = (Uint32*)screen->pixels;
	int height = screen->h;
	while (height--) {
		int x;
		for (x = 0; x < screen->w; x++) {
			/* Assume that pixel order is RGBA */
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
			if (*p != 0x00FFFFFF || debug_fullfade) { // skip white
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

void IOGfxDisplaySW::flip(IOGfxSurface* backbuffer, SDL_Rect* dstrect,
						bool interpolation, bool hwflip) {
	/* For now we do all operations on the CPU side and perform a big
	texture update at each frame; this is necessary to support
	palette and fade_down/fade_up. */
	ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

	//interpolation = false;
	ImGuiIO& io = ImGui::GetIO();
	//Imgui start
	if (debug_mode) {
		//Run our debug stuff
		dbg_imgui();
		//Stuck this here so I could more easily get the texture
		if (debug_gamewindow) {
		ImGui::Begin("Game Display", &debug_gamewindow, ImGuiWindowFlags_AlwaysAutoResize);
		//Shows the currently-used texture according to filter settings
		if (dbg.debug_interp)
		ImGui::Image((ImTextureID)render_texture_linear,ImVec2((int)640,480));
		else {
			ImGui::Image((ImTextureID)render_texture_nearest,ImVec2((int)640,480));
		}
		ImGui::End();
		}
	}
	#ifndef DINKEDIT
	else if (console_active) {
		dbg.dswitch = true;
		infowindow();
	} else if (debug_alttext) {
		alttext();
		io.MouseDrawCursor = false;
	}
	#endif
	else {
		//hide the cursor outside of debugh mode
		io.MouseDrawCursor = false;
	}
	/* Convert to destination buffer format */
	SDL_Surface* source = dynamic_cast<IOGfxSurfaceSW*>(backbuffer)->image;

	if (truecolor && brightness < 256)
		gfx_fade_apply(source, brightness);

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
	interpolation = dbg.debug_interp;
	if (dbg.debug_assratio)
	dstrect = NULL;

	if (interpolation) {
		SDL_UpdateTexture(render_texture_linear, NULL, source->pixels, source->pitch);

		if (debug_drawgame)
			SDL_RenderCopy(renderer, render_texture_linear, NULL, dstrect);

	} else {
		SDL_UpdateTexture(render_texture_nearest, NULL, source->pixels, source->pitch);

		if (debug_drawgame)
			SDL_RenderCopy(renderer, render_texture_nearest, NULL, dstrect);
	}

	if (hwflip)
		ImGui::Render();
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);

}

void IOGfxDisplaySW::onSizeChange(int w, int h) {
	this->w = w;
	this->h = h;
	SDL_RenderSetLogicalSize(renderer, w, h);
}

SDL_Surface* IOGfxDisplaySW::screenshot(SDL_Rect* rect) {
	Uint32 Rmask = 0, Gmask = 0, Bmask = 0, Amask = 0;
	int bpp = 0;
	SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_RGBA8888, &bpp, &Rmask, &Gmask,
							&Bmask, &Amask);
	SDL_Surface* surface = SDL_CreateRGBSurface(0, rect->w, rect->h, 32, Rmask,
												Gmask, Bmask, Amask);

	if (SDL_RenderReadPixels(renderer, rect, SDL_PIXELFORMAT_RGBA8888,
							surface->pixels, surface->pitch) < 0) {
		log_error("IOGfxDisplaySW::screenshot: %s", SDL_GetError());
		SDL_FreeSurface(surface);
		return NULL;
	}

	return surface;
}

IOGfxSurface* IOGfxDisplaySW::upload(SDL_Surface* image) {
	/* Set RLE encoding to save memory space and improve perfs if colorkey */
	SDL_SetSurfaceRLE(image, SDL_TRUE);
	/* Force RLE-encode */
	SDL_LockSurface(image);
	SDL_UnlockSurface(image);

	return new IOGfxSurfaceSW(image);
}

IOGfxSurface* IOGfxDisplaySW::allocBuffer(int surfW, int surfH) {
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
