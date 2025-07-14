/**
 * Filesystem helpers

 * Copyright (C) 2022 CRTS, Yeoldetoast

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

#include <libintl.h>
#define _(String) gettext(String)

#include "AppFreeDink.h"
#include "bgm.h"
#include "dinkc_console.h"
#include "scripting.h"
#include "dinkc_console_renderer.h"
#include "debug.h"
#include "debug_renderer.h"
#include "game_engine.h"
#include "live_sprites_manager.h"
#include "editor_screen.h"
#include "gfx.h"
#include "IOGfxDisplay.h"
#include "input.h"
#include "log.h"
#include "sfx.h"
#include "game_choice.h"
#include "update_frame.h"

#include "SDL.h"

#include "debug_imgui.h"
#include "imgui_impl_sdl2.h"

#if defined _WIN32 || defined __WIN32__ || defined __CYGWIN__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* Text choice selection (e.g. "Load game" in the title screen) */
void freedink_update_mouse_text_choice(int dx, int dy) {
	play.mouse += dy;
}
void freedink_update_mouse(SDL_Event* ev) {
	if (ev->type != SDL_MOUSEMOTION)
		return;

	/* Players controls mouse */
	if ((mode == 1 || keep_mouse) && (spr[1].active && spr[1].brain == 13) && !debug_mode) {
		//SDL_SetRelativeMouseMode(SDL_TRUE);
		spr[1].x = ev->motion.x;
		spr[1].y = ev->motion.y;

		/* Clip the cursor to our client area */
		if (spr[1].x > (640 - 1))
			spr[1].x = 640 - 1;
		if (spr[1].y > (480 - 1))
			spr[1].y = 480 - 1;
		if (spr[1].x < 0)
			spr[1].x = 0;
		if (spr[1].y < 0)
			spr[1].y = 0;
	}

	if (game_choice.active && !debug_mode)
		freedink_update_mouse_text_choice(ev->motion.xrel, ev->motion.yrel);
}

/* Global game shortcuts */
static void freedink_input_global_shortcuts(SDL_Event* ev) {
	if (ev->type != SDL_KEYDOWN)
		return;
	if (ev->key.repeat)
		return;

	if (ev->key.keysym.scancode == SDL_SCANCODE_RETURN) {
		g_display->toggleFullScreen();
	} else if (ev->key.keysym.sym == SDLK_q) {
		//shutdown game
		SDL_Event ev;
		ev.type = SDL_QUIT;
		SDL_PushEvent(&ev);
	} else if (ev->key.keysym.sym == SDLK_d) {
		/* Debug mode */
		debug_mode == false ? log_debug_on(dbg.loglevel) : log_debug_off();
	} else if (ev->key.keysym.sym == SDLK_c) {
		console_active == false ? dinkc_console_show() : dinkc_console_hide();
	} else if (ev->key.keysym.sym == SDLK_m) {
		//shutdown music
		StopMidi();
		//Yeolde: added this to see collision, thanks drone1400
	} else if (ev->key.keysym.sym == SDLK_h && !debug_hardenedmode) {
		drawhard == true ? drawhard = false : drawhard = true;
		//unfreeze player
	} else if (ev->key.keysym.sym == SDLK_u && !debug_hardenedmode) {
		spr[1].freeze = 0;
	} else if (ev->key.keysym.sym == SDLK_s) {
		//Take screenshot
		save_screenshot_dmoddir();
	} else if (ev->key.keysym.sym == SDLK_p) {
		//pause
		debug_paused == true ? debug_paused = false : debug_paused = true;
		pause_everything();
	}
}

void freedink_input_window(SDL_Event* ev) {
	if (ev->type == SDL_WINDOWEVENT) {
		if (ev->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
			int w = ev->window.data1;
			int h = ev->window.data2;
			g_display->onSizeChange(w, h);
		}
		//pause/resume on loss of focus
		if (dbg.focuspause && (ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST || ev->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)) {
			if (dbg.focuspause) {
				pause_everything();
			}
		}
	}
}

void log_touch(SDL_Event* ev) {
	char* desc;
	//ye: log multigestures too
	if (ev->type == SDL_MULTIGESTURE) {
		desc = "SDL_MULTIGESTURE";
		SDL_MultiGestureEvent* mev = (SDL_MultiGestureEvent*)ev;
		log_debug("%s, fingers: %d, id: %" SDL_PRIs64 ", x: %f, y: %f, dist: %f", desc, mev->numFingers, mev->touchId, mev->x, mev->y, mev->dDist);
	}

	if (ev->type == SDL_FINGERUP) {
		desc = "SDL_FINGERUP";
	} else if (ev->type == SDL_FINGERDOWN) {
		desc = "SDL_FINGERDOWN";
	} else if (ev->type == SDL_FINGERMOTION) {
		desc = "SDL_FINGERMOTION";
	} else {
		return;
	}
	SDL_TouchFingerEvent* tev = (SDL_TouchFingerEvent*)ev;
	log_debug("%s @%d id%" SDL_PRIs64 ",%" SDL_PRIs64 " (%f,%f) %f,%f p%f",
			desc, tev->timestamp, tev->touchId, tev->fingerId, tev->x, tev->y,
			tev->dx, tev->dy, tev->pressure);
}

void freedink_controls_renderer_render() {
	// SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

	// SDL_RenderDrawLine(renderer, 0, 0, 100, 100);

	// SDL_Rect rectangle;
	// rectangle.x = 0;
	// rectangle.y = 0;
	// rectangle.w = 50;
	// rectangle.h = 50;
	// SDL_RenderFillRect(renderer, &rectangle);

	// SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

AppFreeDink::AppFreeDink() {
	description = _("Starts the Dink Smallwood game or one of its D-Mods.");
	splash_path = "Tiles/Splash.bmp";
	dinkini_playmidi = true;
}

/**
 * do work required for every instance of the application: create the
 * window, initialize data
 */
void AppFreeDink::init() {
	/* Notify other apps that FreeDink is playing */
	//ye: antiquated I think.
	//writeDinksmallwoodIni(true);

	/* Game-specific initialization */
	/* Start with this initialization as it resets structures that are
	filled in other subsystems initialization */
	game_init(opt_version);

	if (sound_on)
		bgm_init();


	//Activate dink, but don't really turn him on
	//spr[1].active = TRUE;
	spr[1].timing = 33;

	// ** SETUP **
	last_sprite_created = 1;
	set_mode(0);

	//lets run our init script
	int script = scripting_load_script("main", 0, 1);
	scripting_run_proc(script, "main");

	/* lets attach our vars to the scripts, they must be declared in the
	main.c DinkC script */
	attach();
}

AppFreeDink::~AppFreeDink() {
	bgm_quit();
	game_quit();
	writeDinksmallwoodIni(/*playing=*/0);
}

void AppFreeDink::input(SDL_Event* ev) {
	// Show IME (virtual keyboard) on Android
	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureKeyboard) {
	if (ev->type == SDL_KEYDOWN &&
		ev->key.keysym.scancode == SDL_SCANCODE_MENU) {
		g_display->toggleScreenKeyboard();
	}
	if (ev->type == SDL_KEYDOWN &&
		ev->key.keysym.scancode == SDL_SCANCODE_AC_BACK) {
		// quit for now
		SDL_Event ev;
		ev.type = SDL_QUIT;
		SDL_PushEvent(&ev);
	}

	freedink_input_window(ev);
	if (dbg.logtouch)
		log_touch(ev);

	if (dbg.touchcont)
		touchConv.convertInput(ev);

	if (ev->type == SDL_KEYUP &&
		input_getscancodestate(ev->key.keysym.scancode) == SDL_PRESSED) {
		// always tell the game when the player releases a key
		input_update(ev);
	} else if ((ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP) &&
			(ev->key.keysym.mod & KMOD_ALT)) {
		freedink_input_global_shortcuts(ev);
	} else if (console_active) {
		dinkc_console_process_key(ev);
	} else {
		// forward all events to the game
		if ((ev->type == SDL_CONTROLLERDEVICEREMOVED) && mode == 3) {
		//yeolde: make it show the inventory if the cord's ripped out or something
		show_inventory = 1;
		log_info("🎮 Controller removed!");
		}
		//Attempt to auto-reconnect when plugged back in
		if ((ev->type == SDL_CONTROLLERDEVICEADDED) && joystick) {
			input_init();
			log_info("🎮 Controller added, attempting to initialise input");
		}
		input_update(ev);
	}
	}
	else {
		//Send input to ImGui when text enabled
		SDL_Event event;
		while (SDL_PollEvent(&event))
        {
			SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);
			dinkc_console_process_key(&event);
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

	}
	// Process mouse even in console mode
	freedink_update_mouse(ev);
}

void AppFreeDink::logic() {
#ifndef __EMSCRIPTEN__
	// TODO: fine-tune framerate from emscripten - should mostly be 60FPS as we want *for 1.08*
	if (!dbg.framelimit && mode > 0)
		SDL_framerateDelay(&framerate_manager);
#endif
	//Yeolde: added this to pause the game
	if (!game_paused()) {
	if (high_speed == 2) {
		for (int i = 0; i < 3; i++) {
		debug_logic();
		updateFrame();
		debug_engine_cycles++;
		}
	}
	else {
		debug_logic();
		updateFrame();
		debug_engine_cycles++;
	}
	}
	/* Renderers */
	if (!abort_this_flip) {
		g_display->clear();
		//dinkc_console_renderer_render();
		g_display->flipStretch(IOGFX_backbuffer); // game area
		freedink_controls_renderer_render(); // TODO: always display me on flip_it
	}
}

/**
 * Save where Dink is installed in a .ini file, read by third-party
 * applications like the DinkFrontEnd. Also notify whether Dink is
 * running or not.
 */
void AppFreeDink::writeDinksmallwoodIni(bool playing) {
	if (g_b_no_write_ini)
		return; //fix problem with NT security if -noini is set
		/* TODO: saves it in the user home instead. */

#if defined _WIN32 || defined __WIN32__ || defined __CYGWIN__
	char windir[MAX_PATH];
	char inifile[MAX_PATH];
	GetWindowsDirectory(windir, MAX_PATH);
	sprintf(inifile, "%s\\dinksmallwood.ini", windir);

	unlink(inifile);

	FILE* f = fopen(inifile, "w");
	if (f == NULL) {
		log_error("🗡️ Couldn't write dinksmallwood.ini: %s", strerror(errno));
		return;
	}
	fprintf(f, "[Dink Smallwood Directory Information for the CD to read]\n");
	fprintf(f, "%s", SDL_GetBasePath());
	fprintf(f, "\n");
	if (playing)
		fprintf(f, "TRUE\n");
	else
		fprintf(f, "FALSE\n");
	fclose(f);
#endif
}
