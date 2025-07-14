#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "SDL.h"
#include "bgm.h"
#include "debug.h"
#include "gfx.h"
#include "gfx_sprites.h"
#include "live_sprite.h"
#include "live_sprites_manager.h"
#include "log.h"
#include "paths.h"
#include "sfx.h"
// For saving settings
#include "debug_imgui.h"
#include "game_engine.h"
#include "imgui.h"
#include "paths.h"
#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/unordered_map.hpp>
#include <fstream>

//Settings struct for serialising
struct debugsettings dbg;

int fps = 0;

static uint_fast16_t frames = 0;
static Uint64 fps_lasttick = 0;

void debug_refreshFPS() {
    /* Refresh frame counter twice per second */
    if ((SDL_GetTicks64() - fps_lasttick) > 500) {
        fps = frames * (1000 / 500);
        frames = 0;
        fps_lasttick = SDL_GetTicks64();
    }
    frames++;
}

void debug_logic() {
    debug_refreshFPS();
}

// save debug mode settings to disk, usually at exit.
void save_user_settings() {
#ifndef __EMSCRIPTEN__
    if (mode > 1) {
        try {
            std::ofstream os(paths_pkgdatafile("settings.dink"),
                             std::ios::binary);
            cereal::BinaryOutputArchive archive(os);
            archive(dbg);
            log_info("⛏️ Saved debug mode settings to disk");
        } catch (const std::exception &e) {
            log_error("😱 Could not save settings");
        }
    }
#endif
}

void load_user_settings() {
    // Load our debug mode settings
    try {
        std::ifstream is(paths_pkgdatafile("settings.dink"), std::ios::binary);
        cereal::BinaryInputArchive archive(is);
        archive(dbg);
    } catch (const std::exception &e) {
        log_error("😱 Could not load settings! Will create upon exit");
    }
    if (debug_authorkey > 0)
        dbg.fakekey = debug_authorkey;
}

// Pause and unpause
void pause_everything() {
    if (!game_paused()) {
        ResumeMidi();
        pause_sfx();
    } else {
        PauseMidi();
        pause_sfx();
        pauseTickCount = thisTickCount + 100;
    }
}

bool game_paused() {
    return debug_paused || (dbg.focuspause && !(SDL_GetWindowFlags(g_display->window) & SDL_WINDOW_INPUT_FOCUS));
}

void load_console_font() {
    if (confont == NULL) {
        ImGuiIO &io = ImGui::GetIO();
        confont = io.Fonts->AddFontFromFileTTF(
            paths_pkgdatafile("CascadiaCode.ttf"), debug_fontsize);
    }
}

void copy_to_clipboard(const char *text) {
    SDL_SetClipboardText(text);
}

void set_window_res(int w, int h) {
    SDL_SetWindowFullscreen(g_display->window, 0);
    SDL_SetWindowSize(g_display->window, w, h);
    // get which monitor it's on
    int mon = SDL_GetWindowDisplayIndex(g_display->window);
    SDL_SetWindowPosition(g_display->window,
                          SDL_WINDOWPOS_CENTERED_DISPLAY(mon),
                          SDL_WINDOWPOS_CENTERED_DISPLAY(mon));
}

bool check_window_res(int w, int h) {
    if (g_display->w == w && g_display->h == h &&
        (SDL_GetWindowFlags(g_display->window) !=
         SDL_WINDOW_FULLSCREEN_DESKTOP))
        return true;
    else
        return false;
}

void xyoffsetprev(int sprite) {
    rect ddot;
    ddot.left = spr[sprite].x - 2;
    ddot.right = ddot.left + 3;
    ddot.top = spr[sprite].y - 2;
    ddot.bottom = ddot.top + 3;
    SDL_Rect i = {ddot.left, ddot.top, 2, 2};
    IOGFX_backbuffer->fillRect(&i, 255, 10, 0);
}

void hboxprevoverlay(int sprite) {
    rect hbox;
    rect_copy(&hbox, &k[getpic(sprite)].hardbox);
    SDL_Rect h = {hbox.left + spr[sprite].x, hbox.top + spr[sprite].y,
                  hbox.right - hbox.left, hbox.bottom - hbox.top};
    IOGFX_backbuffer->fillRect(&h, 0, 100, 255);
}

// launch script in editor
void edit_script(char *script) {
    char cmd[255];
    // Search for the file using the thing
    sprintf(cmd, "%s %s%s%s%s", debug_editor, paths_dmodfile("STORY"), "/",
            script, ".lua");
    system(cmd);
}

void edit_dinkini() {
    char cmd[255];
    sprintf(cmd, "%s %s", debug_editor, paths_dmodfile("dink.ini"));
    system(cmd);
}
