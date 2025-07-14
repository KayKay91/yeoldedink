/*
 * Replacement Debug Interface

 * Copyright (C) 2022-2024 Yeoldetoast

 * This file is part of YeoldeDink

 * Yeoldedink is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.

 * Yeoldedink is distributed in the hope that it will be useful, but
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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <time.h>
#include <stdint.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <utility>
#include <locale>

#include "SDL.h"
#ifdef HAVE_SDL_GPU
#include "SDL_gpu.h"
#endif
#ifdef SDL_MIXER_X
#include <SDL2/SDL_mixer_ext.h>
#else
#include <SDL_mixer.h>
#endif

#include <cereal/cereal.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_freeverbfilter.h"
#include <entt/entt.hpp>

//Engine includes
#include "debug_imgui.h"
#include "game_state.h"
#include "game_engine.h"
#include "live_sprites_manager.h"
#include "input.h"
#include "log.h"
#include "dinkc.h"
#include "dinkc_console.h"
#include "scripting.h"
#include "freedink.h"
#include "gfx.h"
#include "brain.h"
#include "gfx_sprites.h"
#include "gfx_fonts.h"
#include "game_choice.h"
#include "update_frame.h"
#include "live_screen.h"
#include "inventory.h"
#include "ImageLoader.h"
#include "status.h"
#include "sfx.h"
#include "bgm.h"
#include "DMod.h"
#include "savegame.h"
#include "paths.h"
#include "app.h"
#include "editor_screen.h"
#include "gfx_palette.h"
#include "hardness_tiles.h"
#include "dinklua.h"
#include "dinklua_bindings.h"
#include "scripting.h"
#include "script_bindings.h"
#include "debug.h"
#include "str_util.h"

//header-only libraries
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_internal.h"
#include "implot.h"
#include "SimpleIni.h"
#include "color/color.hpp"
#include "random.hpp"
#include "spdlog/spdlog.h"
#include <glob.hpp>
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

using Random = effolkronium::random_static;
using json = nlohmann::json;

//font
ImFont* confont;
bool smurmeladeribuloso, debug_hardenedmode = false;
const std::map<std::string, int> textcolmap { {"#", 13}, {"@", 12}, {"!", 14}, {"%", 15}, {"$", 14}, {"0", 10}, {"1", 1}, {"2", 2}, {"3", 3}, {"4", 4}, {"5", 5}, {"6", 6}, {"7", 7}, {"8", 8}, {"9", 9} };
//For the log window
Uint32 debug_engine_cycles = 0;
ImGuiTextBuffer Buf, errBuf, dbgBuf, infBuf, traceBuf;
ImGuiTextFilter Filter;
ImVector <int> LineOffsets;
bool debug_autoscroll[5];
//ImVec2 log_child_size = ImVec2(0.0f, 300.0f);
//For transparent overlay and console
char debugbuf[128];
static int DinkcHistoryPos = -1;
ImVector<char*> DinkcHistory;
char luabuf[128];
static int LuaHistoryPos = -1;
ImVector<char*> LuaHistory;
static int ts = 1;
char sfbuf[128];
//For various other window settings
bool render_tiles = true;
bool drawhard;

//For music and other things
static int modorder;
double muspos;
static int zero = 0;
static int tilemax = GFX_TILES_NB_SQUARES;
static int htilemax = HARDNESS_NB_TILES;
Mix_MusicType tracker = MUS_MOD;
char midichlorian[40];
int midiplayer;
static int juker;
ImVector<char[400]> jlist;
static float mybassboost = 0.0f;

//Speech synthesis
int spwave;
int spfreq;
int spdec;
int spspeed;

//for the var/sprite editors
int spriedit = 2;
int sprigrid = 0;
int mspried = 1;
bool spoutline;
bool sprilivechk;
bool spriscripted;
bool sprinodraw;
bool hboxprev, ddotprev;
//copy paste buffers
editor_sprite spritebuf[100 + 1];
editor_screen screenbuf;
static int xmax = 620;
static int ymax = 400;
static int ypos;
static ImPlotRect hbox, trimbox;
static bool sprplotedit;
static bool mspriunused;
static int spriview;
static char customkey[200];
static int customval;

//Tab item flags
ImGuiTabItemFlags_ flag = ImGuiTabItemFlags_None;
ImGuiTabItemFlags_ mflag = ImGuiTabItemFlags_None;
ImGuiTabItemFlags_ errflag = ImGuiTabItemFlags_None;

//Script ed
static int myscript;
static char myscriptpath[200];
struct tm *timeinfo;

//For dink.ini viewer
bool freeslots, zeroslots;

//For gamepad
static int key_idx = 0;

//For sprite overrider
static bool sprovermod;
static int sproversel = 1;

//For colour palette thingo
static int palcol;
static int mycol;
static bool autopal;
ImVec4 mypalcol;
SDL_Color mypal[256] = {};
char bmppal[100];

//Text
static ImVec4 mytextcol;
char myinitfont[200];

//For the box previewer
rect boxpreview;
ImPlotRect bprev;
static bool bprevview;

//for callbacks
static bool callbactive;

//need for speed
int debug_base_timing = 7;

//saves
bool debug_saveenabled[5];

//For the metrics, crap temp vars too
//static float t = 0;
//static float shistory = 60.0f;
const char* dispfont;
const char* debugfont;
char debug_editor[20];
char debugfon[200];
int debug_fontsize;
int displayx;
int displayy;
bool dispfilt;
bool dispstretch;
int audiosamp;
int audiobuf;
int audiovol;
int sfx_vol;
bool dbgtxtwrite;
bool dbgconpause;
bool dbginfowindow;
float pan, muspeed, muhandle;
int debug_authorkey = 0;

//For hard.dat and tile viewer
static int htile;
static int tindexscr = 1;
editor_screen_tilerefs mytiles;
static int mytile = 1;

//Toggleable settings
bool debug_drawsprites = true;
bool debug_drawgame = true;
bool debug_gamewindow;
bool debug_paused;
bool debug_conpause;
bool debug_infhp;
bool debug_magcharge;
bool debug_invspri;
bool debug_noclip;
bool debug_vision;
bool debug_screenlock;
bool debug_assratio;
bool debug_walkoff;
bool debug_audiopos;
bool debug_speechsynth;
bool debug_returnint;
bool debug_ranksprites;
bool debug_pausetag;
bool debug_pausehit;
bool debug_pausehits;
bool debug_pausetalk;
bool debug_boxanddot;
bool debug_missilesquares;
bool debug_hitboxsquares;
bool debug_talksquares;
bool debug_hitboxfix = true;
bool debug_talkboxexp;
bool debug_framepause;
bool debug_pausemissile;
bool debug_pausemissilehit;
bool debug_funcfix = true;
bool debug_pushsquares;
bool debug_pausepush;
bool debug_savestates = true;
bool debug_idledirs;
bool debug_errorpause;
bool debug_debugpause;
bool debug_alttext;
bool debug_alttextlock;
bool debug_tileanims = true;
bool debug_divequals;
bool debug_extratextcols = true;
bool debug_fullfade;
bool debug_maxdam;
bool debug_scriptkill;
bool debug_overignore;

extern lua_State* luaVM;

//Toggles for smaller windows
bool ultcheat;
bool graphicsopt;
bool dswitch;
bool keeb;
bool scripfo;
bool sprinfo;
bool variableinfo;
bool metrics;
bool mapsquares;
bool maptiles;
bool sysinfo;
bool mapsprites;
bool dinkini;
bool helpbox;
bool aboutbox;
bool sproverride;
bool paledit;
bool iniconf;
static bool boxprev;
static bool htiles;
static bool shaderwindow;
static bool jukebox;
static bool magicitems;
static bool luaviewer;
static bool spreditors;
static bool callbinfo;
static bool dinkcinfo;
static bool luainfo;
static bool sfxinfo;
static bool moresfxinfo;
static bool imdemowindow;

//For the floating info window
static int vars_in_use() {
	int m = 0;
	for (int_fast32_t i = 1; i < MAX_VARS; i++)
		if (play.var[i].active == 1)
			m++;

	return m;
}

static int callbacks_in_use() {
	int_fast32_t m = 0;
	for (int i=0; i < MAX_CALLBACKS; i++)
		if (callback[i].active)
			m++;

	return m;
}

static int scripts_in_use() {
	int_fast32_t m = 0;
	for (int i = 0; i < MAX_SCRIPTS; i++)
		if (sinfo[i] != NULL)
			m++;

	return m;
}

static void used_scripts_bar() {
	int m = 0;
	for (int i = 1; i < MAX_SCRIPTS; i++)
		if (sinfo[i] != NULL)
			m++;
	float progress = (float)m / (float)MAX_SCRIPTS;
	char buf[32];
	sprintf(buf, "%d/%d scripts", m, MAX_SCRIPTS);
	ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f), buf);
}

static void live_sprites_bar() {
	//Sprite counter
	int m = 0;
	for (int i = 1; i < MAX_SPRITES_AT_ONCE; i++)
	if (spr[i].active == 1)
		m++;
	float progress = (float)m / ((float)MAX_SPRITES_AT_ONCE - 1);
	char buf[32];
	sprintf(buf, "%d/%d live sprites", m, MAX_SPRITES_AT_ONCE);
	ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f), buf);
}

static void var_used_bar() {
	int m = 0;
	int i;
	for (i = 1; i < MAX_VARS; i++)
		if (play.var[i].active == 1)
			m++;
	float progress = (float)m / (float)MAX_VARS;
	char buf[32];
	sprintf(buf, "%d/%d vars in use", m, MAX_VARS);
	ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f), buf);
}

static void sfx_used_bar() {
	int m = gSoloud.getActiveVoiceCount();
	float voices = (float)m / (float)NUM_CHANNELS;
	char buff[64];
	sprintf(buff, "%d/%d Concurrent voices", m, NUM_CHANNELS);
	ImGui::ProgressBar(voices, ImVec2(-1.0f, 0.0f), buff);
}

static void tooltippy(char* tip) {
	if (dbg.tooltips)
		ImGui::SetItemTooltip("%s", tip);
}

//The most important feature
void play_sneedle() {
	playsfx(paths_pkgdatafile("sneedle.mp3"), 0, 0);
}

bool get_cheat() {
	return smurmeladeribuloso;
}

static int LuaConsoleCallback(ImGuiInputTextCallbackData *data)
{
	const int prev_history_pos = LuaHistoryPos;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
	if (data->EventKey == ImGuiKey_UpArrow)
	{
		if (LuaHistoryPos == -1)
			LuaHistoryPos = LuaHistory.Size - 1;
		else if (LuaHistoryPos > 0)
			LuaHistoryPos--;
	}
	else if (data->EventKey == ImGuiKey_DownArrow)
	{
		if (LuaHistoryPos != -1)
			if (++LuaHistoryPos >= LuaHistory.Size)
				LuaHistoryPos = -1;
	}

	// A better implementation would preserve the data on the current input line along with cursor position.
	if (prev_history_pos != LuaHistoryPos)
	{
		const char *history_str = (LuaHistoryPos >= 0) ? LuaHistory[LuaHistoryPos] : "";
		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, history_str);
	}
	}
	return 0;
}

static int DinkcConsoleCallback(ImGuiInputTextCallbackData *data)
{
	const int prev_history_pos = DinkcHistoryPos;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
	if (data->EventKey == ImGuiKey_UpArrow)
	{
		if (DinkcHistoryPos == -1)
			DinkcHistoryPos = DinkcHistory.Size - 1;
		else if (DinkcHistoryPos > 0)
			DinkcHistoryPos--;
	}
	else if (data->EventKey == ImGuiKey_DownArrow)
	{
		if (DinkcHistoryPos != -1)
			if (++DinkcHistoryPos >= DinkcHistory.Size)
				DinkcHistoryPos = -1;
	}

	// A better implementation would preserve the data on the current input line along with cursor position.
	if (prev_history_pos != DinkcHistoryPos)
	{
		const char *history_str = (DinkcHistoryPos >= 0) ? DinkcHistory[DinkcHistoryPos] : "";
		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, history_str);
	}
	}
	return 0;
}

void gameload(int slot) {
	FILE* fp;
	if ((fp = paths_savegame_fopen(slot, "rb")) != NULL) {
		fclose(fp);
		scripting_kill_all_scripts_for_real();
		kill_repeat_sounds_all();
		load_game(slot);
		*pupdate_status = 1;
		draw_status_all();
	}
	else {
		log_error("😱 No save in that slot!");
	}
}

void dinktheme() {
ImVec4* colors = ImGui::GetStyle().Colors;
colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
colors[ImGuiCol_FrameBg]                = ImVec4(0.27f, 0.09f, 0.09f, 0.54f);
colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.82f, 0.22f, 0.22f, 0.40f);
colors[ImGuiCol_FrameBgActive]          = ImVec4(0.98f, 0.26f, 0.26f, 0.67f);
colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
colors[ImGuiCol_TitleBgActive]          = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
colors[ImGuiCol_CheckMark]              = ImVec4(0.98f, 0.26f, 0.31f, 1.00f);
colors[ImGuiCol_SliderGrab]             = ImVec4(0.68f, 0.19f, 0.19f, 1.00f);
colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.98f, 0.29f, 1.00f);
colors[ImGuiCol_Button]                 = ImVec4(0.78f, 0.18f, 0.18f, 0.40f);
colors[ImGuiCol_ButtonHovered]          = ImVec4(0.43f, 0.13f, 0.13f, 1.00f);
colors[ImGuiCol_ButtonActive]           = ImVec4(0.98f, 0.06f, 0.11f, 1.00f);
colors[ImGuiCol_Header]                 = ImVec4(0.89f, 0.25f, 0.25f, 0.31f);
colors[ImGuiCol_HeaderHovered]          = ImVec4(0.98f, 0.26f, 0.26f, 0.80f);
colors[ImGuiCol_HeaderActive]           = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.75f, 0.10f, 0.24f, 0.78f);
colors[ImGuiCol_SeparatorActive]        = ImVec4(0.75f, 0.10f, 0.30f, 1.00f);
colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.98f, 0.35f, 0.20f);
colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.98f, 0.26f, 0.26f, 0.67f);
colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.98f, 0.26f, 0.41f, 0.95f);
colors[ImGuiCol_TabHovered]             = ImVec4(0.98f, 0.26f, 0.26f, 0.80f);
colors[ImGuiCol_Tab]                    = ImVec4(0.58f, 0.18f, 0.19f, 0.86f);
colors[ImGuiCol_TabSelected]            = ImVec4(0.68f, 0.20f, 0.20f, 1.00f);
colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.98f, 0.26f, 0.28f, 1.00f);
colors[ImGuiCol_TabDimmed]              = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.42f, 0.14f, 0.14f, 1.00f);
colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
colors[ImGuiCol_TextLink]               = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.98f, 0.26f, 0.60f, 0.35f);
colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
colors[ImGuiCol_NavCursor]              = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

//For jukebox
void jukelist() {
	for (auto& p : glob::rglob({paths_dmodfile("SOUND/*.mid"), paths_dmodfile("SOUND/*.MID"), paths_dmodfile("SOUND/*.spc"), paths_dmodfile("SOUND/*.mod"), paths_dmodfile("SOUND/*.xm"), paths_dmodfile("SOUND/*.it"), paths_dmodfile("SOUND/*.s3m"), paths_dmodfile("SOUND/*.ogg"), paths_dmodfile("SOUND/*.opus"), paths_dmodfile("SOUND/*.mp3")})) {
		char buf[400];
		#ifndef _WIN32
		sprintf(buf, p.filename().c_str());
		#endif
		jlist.push_back(buf);
		//shove font loading in here too perhaps
		//or quicksave checking
	}
	for (int i = 1; i < 5; i++) {
		debug_saveenabled[i] = paths_savegame_fopen(i + 1336, "rb");
		}
}

//Alter HSV for entire custom palette
void changehsv(int param, int amount, int space)
{
  // 0 is h, 1 s, 2 v/i/l
  for (int c = 0; c < 256; c++)
  {
	  // Convert colour to HSV
	  color::rgb<std::uint8_t> c1({mypal[c].r, mypal[c].g, mypal[c].b});
	  color::hsv<std::uint8_t> c2;
	  // Or HSI/HSL
	  if (space == 1)
	  color::hsl<std::uint8_t> c2;
	  if (space == 2)
	  color::hsi<std::uint8_t> c2;

	  c2 = c1;
	  // Change its parameter
	  c2[param] += amount;
	  // Convert back to RGB
	  c1 = c2;
	  mypal[c].r = ::color::get::red(c1);
	  mypal[c].g = ::color::get::green(c1);
	  mypal[c].b = ::color::get::blue(c1);
  }
}

void livesprtomap(int srcspr) {
	//Saves a live sprite to an editor sprite. Not all values are transferred of course
	cur_ed_screen.sprite[spr[srcspr].sp_index].brain = spr[srcspr].brain;
	cur_ed_screen.sprite[spr[srcspr].sp_index].x = spr[srcspr].x;
	cur_ed_screen.sprite[spr[srcspr].sp_index].y = spr[srcspr].y;
	cur_ed_screen.sprite[spr[srcspr].sp_index].size = spr[srcspr].size;
	cur_ed_screen.sprite[spr[srcspr].sp_index].seq = spr[srcspr].pseq;
	cur_ed_screen.sprite[spr[srcspr].sp_index].frame = spr[srcspr].pframe;
	cur_ed_screen.sprite[spr[srcspr].sp_index].touch_damage = spr[srcspr].touch_damage;
	cur_ed_screen.sprite[spr[srcspr].sp_index].gold = spr[srcspr].gold;
	cur_ed_screen.sprite[spr[srcspr].sp_index].exp = spr[srcspr].exp;
	cur_ed_screen.sprite[spr[srcspr].sp_index].que = spr[srcspr].que;
	cur_ed_screen.sprite[spr[srcspr].sp_index].hitpoints = spr[srcspr].hitpoints;
	cur_ed_screen.sprite[spr[srcspr].sp_index].sound = spr[srcspr].sound;
	//trimming
	cur_ed_screen.sprite[spr[srcspr].sp_index].alt = spr[srcspr].alt;

}

void show_consoles() {
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_EscapeClearsAll;
	ImGui::PushFont(confont);
	if (dinkc_enabled && !debug_hardenedmode) {
		if (ImGui::InputTextWithHint("##DinkCConsole", "DinkC Console", debugbuf, IM_ARRAYSIZE(debugbuf), input_text_flags, &DinkcConsoleCallback))
			{
			dinkc_execute_one_liner(debugbuf);
			DinkcHistoryPos = -1;
			DinkcHistory.push_back(strdup(debugbuf));
			strcpy(debugbuf, "");
			}
			}

			if (dinklua_enabled && !debug_hardenedmode) {
			if (ImGui::InputTextWithHint("##LuaConsole", "Lua Console", luabuf, IM_ARRAYSIZE(luabuf), input_text_flags, &LuaConsoleCallback)) {
				dinklua_execute_one_liner(luabuf);
				LuaHistoryPos = -1;
				LuaHistory.push_back(strdup(luabuf));
				strcpy(luabuf, "");
			}
			//Console hide button
			}
			ImGui::PopFont();
			if (debug_hardenedmode)
				ImGui::Text("Hardened mode enabled by author");
			ImGui::SameLine();
			if (ImGui::SmallButton("Hide"))
				dinkc_console_hide();
}

//Stolen from the editor. Writes to dink.ini
static void add_text() {
	char death[150];
	char filename[10];
	//I should have just used getpic()
	sprintf(death, "SET_SPRITE_INFO %d %d %d %d %d %d %d %d\n",
		spr[spriedit].pseq, spr[spriedit].pframe,
		k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].xoffset,
		k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].yoffset,
		k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.left,
		k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.top,
		k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.right,
		k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.bottom);

	strcpy(filename, "dink.ini");

	FILE* fp = paths_dmodfile_fopen(filename, "ab");
	if (fp != NULL) {
		fwrite(death, strlen(death), 1, fp);
		fclose(fp);
	} else {
		perror("add_text");
	}
}

//also taken from the editor
int add_new_map() {
	int loc_max = 0;
	for (int i = 0; i <= number_of_screens; i++)
		if (g_dmod.map.loc[i] > loc_max)
			loc_max = g_dmod.map.loc[i];

	int loc_new = loc_max + 1;
	if (loc_new > number_of_screens)
		return -1;

	save_screen(g_dmod.map.map_dat.c_str(), loc_new);
	return loc_new;
}

void infowindowmenu() {
	ImGui::MenuItem("FPS", nullptr, &dbg.dshowfps);
	tooltippy("Frames per second and time of last frame");
	ImGui::MenuItem("Clock", nullptr, &dbg.dshowclock);
	tooltippy("Date and time");
	ImGui::MenuItem("Engine info", nullptr, &dbg.dshowinfo);
	tooltippy("Scripts, sprites, and callbacks in use, plus current screen");
	ImGui::MenuItem("Traditional info", nullptr, &dbg.dtradinfo);
	tooltippy("Info line from 1.08 and earlier");
	ImGui::MenuItem("Last log message", nullptr, &dbg.dshowlog);
}

void infowindow() {
ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
if (debug_mode)
	window_flags += ImGuiWindowFlags_NoNav;
if (dbg.corner != -1)
    {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (dbg.corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (dbg.corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (dbg.corner & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (dbg.corner & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        window_flags |= ImGuiWindowFlags_NoMove;
    }
	ImGui::SetNextWindowBgAlpha(dbg.dopacity); // Transparent background
    if (ImGui::Begin("Debug info overlay", NULL, window_flags))
    {
		if (debug_mode)
        ImGui::Text("Debug interface on. Alt+D or R3 to leave, Alt+C for console");
		else
		ImGui::Text("Console active. Keyboard controls ignored. Alt+C to return to normal.");
        ImGui::Separator();
		//Change the colour according to framerate
		if (dbg.dshowfps) {
		ImVec4 col = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
		float fram = ImGui::GetIO().Framerate;
		if (fram >= 50)
			col = ImVec4(0.1f, 1.0f, 0.0f, 1.0f);
		if (fram < 30)
			col = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		ImGui::TextColored(col, "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}

		if (dbg.dshowinfo) {
			ImGui::Text("Sprites: %d, Vars: %d, Scripts: %d, Callbacks: %d, Screen: %d (%d)",
					last_sprite_created, vars_in_use(),
					scripts_in_use(), callbacks_in_use(), *pplayer_map, g_dmod.map.loc[*pplayer_map]);
		}

		if (dbg.dtradinfo) {
			if (mode == 1)
				ImGui::Text("X is %d y is %d", spr[1].x, spr[1].y);

			if (mode == 3)
				ImGui::Text("Sprites: %d  FPS: %d Moveman X%d X%d: %d Y%d Y%d Map %d",
					last_sprite_created,fps, spr[1].lpx[0],spr[1].lpy[0],spr[1].moveman,spr[1].lpx[1],
					spr[1].lpy[1], *pplayer_map);
		}

		if (dbg.dshowlog)
        	ImGui::Text("%s", log_getLastLog());

		if (dbg.dshowclock) {
			time_t rawtime;
			struct tm *timeinfo;
			char timebuf[100];
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(timebuf, 100, "🕰️ %c", timeinfo);
			ImGui::Text("%s", timebuf);
		}

		if (console_active) {
			show_consoles();
		}
		else if (!debug_hardenedmode) {
			if (ImGui::Button("Show Console(s)")) {
				dinkc_console_show();
			}
			ImGui::SameLine();
			if (mode == 3) {
			if (ImGui::Button("Unfreeze Player")) {
				spr[1].freeze = 0;
			}
			}
			else {
			if (ImGui::Button("Reset Cursor")) {
				spr[1].x = 640 / 2;
				spr[1].y = 480 / 2;
			}
			}
			ImGui::SameLine();
			if (ImGui::Button("Halt Audio")) {
				halt_all_sounds();
				StopMidi();
			}
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Pause", &debug_paused))
			pause_everything();
        if (ImGui::BeginPopupContextWindow())
        {
			infowindowmenu();
			ImGui::PushItemWidth(50);
			ImGui::SliderFloat("Opacity", &dbg.dopacity, 0.0f, 1.0f);
			ImGui::PopItemWidth();
			ImGui::Separator();
            if (ImGui::MenuItem("Custom",       NULL, dbg.corner == -1)) dbg.corner = -1;
            if (ImGui::MenuItem("Top-left",     NULL, dbg.corner == 0)) dbg.corner = 0;
            if (ImGui::MenuItem("Top-right",    NULL, dbg.corner == 1)) dbg.corner = 1;
            if (ImGui::MenuItem("Bottom-left",  NULL, dbg.corner == 2)) dbg.corner = 2;
            if (ImGui::MenuItem("Bottom-right", NULL, dbg.corner == 3)) dbg.corner = 3;
            if (ImGui::MenuItem("Close")) dbg.dswitch = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void logwindowtab(int level, ImGuiTextBuffer myBuf) {
	char myname[32];
	sprintf(myname, "scrolling%d", level);
	if (ImGui::Button("Clear")) {
		Buf.clear();
		errBuf.clear();
		infBuf.clear();
		dbgBuf.clear();
		traceBuf.clear();
	}
	ImGui::SameLine();
	if (ImGui::Button("Copy all to clipboard")) {
		ImGui::LogToClipboard();
		ImGui::LogText(myBuf.c_str());
		ImGui::LogFinish();
	}
	ImGui::BeginChild(myname);
	const char *buf = myBuf.begin();
	const char *buf_end = myBuf.end();
	ImGui::TextUnformatted(buf, buf_end);
	if (dbg.logautoscroll && debug_autoscroll[level]) {
		ImGui::SetScrollHereY(1.0f);
		debug_autoscroll[level] = false;
	}

	ImGui::EndChild();
}

//Runs every frame in the render loop to show the gui stuff
void dbg_imgui(void) {

//Only show stuff if debug is on and the game has loaded to title screen
if (debug_mode && mode && !screen_main_is_running) {
//Show the mouse
ImGuiIO& io = ImGui::GetIO();
io.MouseDrawCursor = true;
//Main Menu bar
ImGui::BeginMainMenuBar();
if (ImGui::BeginMenu("File"))
{
    if (ImGui::MenuItem("Hide menubar and debug interface", "Alt+D"))
    	log_debug_off();
	ImGui::Separator();
	#ifndef _WIN32
	ImGui::MenuItem("Jukebox", NULL, &jukebox);
	#endif

	if (ImGui::MenuItem("Pause", "Alt+P", &debug_paused))
		pause_everything();

	if (mode < 2)
		ImGui::BeginDisabled();
	if (ImGui::MenuItem("Save user settings"))
		save_user_settings();
	if (mode < 2)
		ImGui::EndDisabled();

	#ifndef __EMSCRIPTEN__
	if (ImGui::MenuItem("Save screenshot", "Alt+S"))
		save_screenshot_dmoddir();

	if (ImGui::MenuItem("Restart"))
		game_restart();

	if (ImGui::MenuItem("Quit", "Alt+Q")) {
	    SDL_Event ev;
	    ev.type = SDL_QUIT;
	    SDL_PushEvent(&ev);
	}
	#else
	ImGui::Separator();
	if (ImGui::MenuItem("Download saves as zip")) {
		emscripten_run_script("OnSavegamesExport()");
	}
	#endif

	ImGui::EndMenu();
}

if (ImGui::BeginMenu("Settings"))
{
	ImGui::MenuItem("Pause upon loss of focus", NULL, &dbg.focuspause);
	tooltippy("And resume upon gain of focus");
	if (debug_alttextlock)
		ImGui::BeginDisabled();
	ImGui::MenuItem("Alternate text boxes", NULL, &debug_alttext);
	if (debug_alttextlock)
		ImGui::EndDisabled();
	tooltippy("For use with multiple colours in text. To be enabled in the event of extraneous backticks.");
	ImGui::MenuItem("German text strings", NULL, &dbg.germantext);
	tooltippy("When talking to nobody, or without magic. Taken from the 1.07 source.");
	ImGui::MenuItem("Close inventory upon selection", NULL, &dbg.invexit);
	ImGui::MenuItem("Lerp gold and stats", NULL, &dbg.statlerp);
	tooltippy("Values will increase rapidly, rather than ticking up slowly");
	if (ImGui::BeginMenu("Colour scheme")) {
		if (ImGui::MenuItem("Dink", nullptr, dbg.uistyle == 0)) {
			dinktheme();
			dbg.uistyle = 0;
		}
		if (ImGui::MenuItem("Dark", NULL, dbg.uistyle == 1)) {
			ImGui::StyleColorsDark();
			dbg.uistyle = 1;
		}
        if (ImGui::MenuItem("Light", NULL, dbg.uistyle == 2)) {
			ImGui::StyleColorsLight();
			dbg.uistyle = 2;
		}
        if (ImGui::MenuItem("Classic", NULL, dbg.uistyle == 3)) {
			ImGui::StyleColorsClassic();
			dbg.uistyle = 3;
		}

		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Save Info Style")) {
		if (ImGui::MenuItem("Traditional", nullptr, dbg.savetime == 0))
			dbg.savetime = 0;
		tooltippy("Level only");
		if (ImGui::MenuItem("MM/DD/YY and 24-hour time", nullptr, dbg.savetime == 1))
			dbg.savetime = 1;
		if (ImGui::MenuItem("YYYY-MM-DD and 24-hour time", nullptr, dbg.savetime == 2))
			dbg.savetime = 2;
		if (ImGui::MenuItem("Full date and time", NULL, dbg.savetime == 3))
			dbg.savetime = 3;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Speedhacks")) {
		ImGui::MenuItem("Faster script loading", NULL, &dbg.varreplace);
		tooltippy("Use RTDink's faster string interpolation algo");
		ImGui::MenuItem("Skip loading existing graphics", NULL, &dbg.gfxspeed);
		ImGui::MenuItem("Skip loading existing SFX", NULL, &dbg.sfxspeed);
		tooltippy("Most egregious when restarting from the escape menu");

		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Graphics")) {
		ImGui::MenuItem("Apply texture filtering", NULL, &dbg.debug_interp);
		tooltippy("Smooth the screen at higher resolutions");
		ImGui::MenuItem("Ignore aspect ratio", NULL, &dbg.debug_assratio);
		tooltippy("Use the full width of the screen with no pillarboxing");
		ImGui::MenuItem("Screen transition", NULL, &dbg.screentrans);
		tooltippy("The slide effect upon visiting a neighbouring screen");
		ImGui::SetNextItemWidth(50);
		ImGui::SliderInt("Transition speed", &dbg.transition_speed, 1, 3);
		if (!truecolor)
			ImGui::BeginDisabled();
		ImGui::MenuItem("Smooth fades", NULL, &dbg.altfade);
		if (!truecolor) {
			ImGui::EndDisabled();
			tooltippy("Only available in 24-bit display mode");
		}

		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Windowed mode")) {
		tooltippy("The default resolution at launch is set in yedink.ini");
	if (ImGui::MenuItem("640x480", NULL, check_window_res(640, 480)))
		set_window_res(640, 480);

	if (ImGui::MenuItem("800x600", NULL, check_window_res(800, 600)))
		set_window_res(800, 600);

	if (ImGui::MenuItem("1024x768", NULL, check_window_res(1024, 768)))
		set_window_res(1024, 768);

	if (ImGui::MenuItem("1280x960", NULL, check_window_res(1280, 960)))
		set_window_res(1280, 960);

	if (ImGui::MenuItem("1440x1080", NULL, check_window_res(1440, 1080)))
		set_window_res(1440, 1080);

	if (ImGui::MenuItem("1600x1200", NULL, check_window_res(1600, 1200)))
		set_window_res(1600, 1200);

	if (ImGui::MenuItem("1920x1440"))
		set_window_res(1920, 1440);

	ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Borderless full-screen", "Alt+Enter", (SDL_GetWindowFlags(g_display->window) & SDL_WINDOW_FULLSCREEN_DESKTOP))) {
		g_display->toggleFullScreen();
	}
	#ifndef __EMSCRIPTEN__
	if ((SDL_GetWindowFlags(g_display->window) & SDL_WINDOW_FULLSCREEN_DESKTOP))
		ImGui::BeginDisabled();
	if (ImGui::MenuItem("Borderless window", NULL, (SDL_GetWindowFlags(g_display->window) & SDL_WINDOW_BORDERLESS))) {
		g_display->toggleBorderless();
	}
	tooltippy("Removes the operating system window header/outline");
	if ((SDL_GetWindowFlags(g_display->window) & SDL_WINDOW_FULLSCREEN_DESKTOP))
		ImGui::EndDisabled();
	#endif
	ImGui::Separator();
    ImGui::MenuItem("Input", NULL, &keeb);
	tooltippy("Controllers and touch screen settings");
    ImGui::MenuItem("Audio", NULL, &sfxinfo);
	tooltippy("Volume controls and MIDI output");
	#ifndef __EMSCRIPTEN__
	if (ImGui::MenuItem("Open yedink.ini in text editor")) {
			char cmd[255];
			sprintf(cmd, "%s %s", debug_editor, paths_pkgdatafile("yedink.ini"));
			system(cmd);
	}
	tooltippy("For some extra settings");
	#endif
	ImGui::Separator();
	if (ImGui::BeginMenu("Developer mode")) {
		if (debug_hardenedmode)
			ImGui::BeginDisabled();
		ImGui::MenuItem("Enable developer mode", NULL, &dbg.devmode);
		if (debug_hardenedmode) {
			ImGui::EndDisabled();
			tooltippy("Switched off by author!");
		}
		ImGui::EndMenu();
	}
	ImGui::EndMenu();
}

if (dbg.devmode) {

if (ImGui::BeginMenu("Debug"))
{
	if (ImGui::BeginMenu("Info Window")) {
	ImGui::MenuItem("Display", NULL, &dbg.dswitch);
	ImGui::Separator();
	infowindowmenu();

	ImGui::EndMenu();
	}
	tooltippy("Floating window with consoles and log messages. Right-click for options.");
	ImGui::MenuItem("Debug Log", NULL, &dbg.logwindow);
	ImGui::MenuItem("Game Display Window", NULL, &debug_gamewindow);
	ImGui::MenuItem("Ultimate Cheat", NULL, &ultcheat);
		ImGui::Separator();
		//if (!dinklua_enabled)
		//	ImGui::BeginDisabled();
			//This hoses the scripting environment
		//if (ImGui::MenuItem("Reload init.lua"))
		//	dinklua_reload_init();
		//if (!dinklua_enabled)
		//	ImGui::EndDisabled();

		if (ImGui::BeginMenu("Log Level"))
		{
			ImGui::MenuItem("Progress bars", NULL, &dbg.progressbars);
			tooltippy("Will appear on the command line when running in debug mode for diagnosing load errors");
			ImGui::Separator();
			if (ImGui::MenuItem("Errors only", NULL, dbg.loglevel == 0)) {
				dbg.loglevel = 0;
				log_debug_on(0);
			}
			if (ImGui::MenuItem("Standard", NULL, dbg.loglevel == 1)) {
				dbg.loglevel = 1;
				log_debug_on(1);
			}
			if (ImGui::MenuItem("Verbose", NULL, dbg.loglevel == 2)) {
				log_debug_on(2);
				dbg.loglevel = 2;
			}
			tooltippy("Only for diagnosing engine bugs");

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Auto-pause")) {
			ImGui::MenuItem("At end of every frame", NULL, &debug_framepause);
			ImGui::MenuItem("When activating console through scripting", NULL, &dbg.pause_on_console_show);
			ImGui::MenuItem("For scripted debug messages", NULL, &debug_debugpause);
			tooltippy("Allows for a sort of breakpoint");
			//ImGui::MenuItem("Upon Lua error", NULL, &dbg.luaerrorpause);
			//ImGui::MenuItem("Upon any error", NULL, &debug_errorpause);
			//tooltippy("Whenever an error is written to the debug log");
			ImGui::Separator();
			ImGui::MenuItem("Upon attack", NULL, &debug_pausetag);
			tooltippy("When an attacking sprite's special frame appears");
			ImGui::MenuItem("Upon attack registering", NULL, &debug_pausehits);
			tooltippy("Of any sprite");
			ImGui::MenuItem("When player attacked", NULL, &debug_pausehit);
			tooltippy("Successful attack on sprite 1");
			ImGui::MenuItem("While missile(s) present", NULL, &debug_pausemissile);
			tooltippy("To see hitboxes with");
			ImGui::MenuItem("Upon missile colliding", NULL, &debug_pausemissilehit);
			ImGui::MenuItem("Upon talking", nullptr, &debug_pausetalk);
			tooltippy("To see talkboxes with");
			ImGui::MenuItem("Upon pushing", nullptr, &debug_pausepush);

			ImGui::EndMenu();
			}

		if (ImGui::BeginMenu("Visual options"))
		{
			ImGui::MenuItem("Render game in main window", NULL, &debug_drawgame);
			tooltippy("For use with the popout window");
			ImGui::MenuItem("Touch boxes", NULL, &dbg.debug_pinksquares);
			tooltippy("Displays the boxes which run touch() when intersected with");
			ImGui::MenuItem("Push boxes", nullptr, &debug_pushsquares);
			tooltippy("Show valid push targets in yellow");
			ImGui::MenuItem("Talk boxes", nullptr, &debug_talksquares);
			tooltippy("The sprite area for talk() to trigger");
			ImGui::MenuItem("Missile targets", NULL, &debug_missilesquares);
			tooltippy("Orange inflated rectangles when a missile is thrown");
			ImGui::MenuItem("Hitboxes", NULL, &debug_hitboxsquares);
			tooltippy("Box expansion upon attack");
			ImGui::MenuItem("Dink.ini data", nullptr, &debug_boxanddot);
			tooltippy("Show assigned \"depth dot\" and box for all sprites");
			ImGui::MenuItem("Hard.dat overlay", "Alt+H", &drawhard);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Sprite/Tile Rendering"))
		{
			// ImGui::MenuItem("Render tiles and background", NULL, &render_tiles);
			if (ImGui::MenuItem("Draw invisible sprites", NULL, &debug_invspri))
			{
				game_place_sprites_background();
				draw_screen_game_background();
			}
			ImGui::MenuItem("Ignore sprite visions ", NULL, &debug_vision);
			tooltippy("Show all sprites across all visions");
			ImGui::MenuItem("Alternate sprite ranking", NULL, &debug_ranksprites);
			tooltippy("Faster sorting at the expense of breaking some warps");
			ImGui::MenuItem("Blood and carcasses", nullptr, &debug_drawblood);
			tooltippy("Draw blood when sprite hurt");
			ImGui::MenuItem("Damage text sprites", nullptr, &debug_drawdam);
			tooltippy("White damage indicator text");
			ImGui::MenuItem("Flashing 'Please Wait' indicator", NULL, &dbg.showplswait);
			tooltippy("Switching this off may prevent certain crashes");
			ImGui::MenuItem("Animate tiles", nullptr, &debug_tileanims);
			tooltippy("For tilesheets traditionally reserved for fire and water");
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("Enhancements")) {
			ImGui::MenuItem("Make global functions", NULL, &debug_funcfix);
			tooltippy("More than one global function may be made");
			ImGui::MenuItem("Proper hitbox dimensions", NULL, &debug_hitboxfix);
			tooltippy("Fixes an historical bug");
			ImGui::MenuItem("Nohit fix", nullptr, &debug_nohitfix);
			tooltippy("Makes it so you can't hit certain text and blood sprites");
			ImGui::MenuItem("Easier discussion", NULL, &debug_talkboxexp);
			tooltippy("Expands the talkbox above and below sprites");
			ImGui::MenuItem("All idle directions for player", NULL, &debug_idledirs);
			tooltippy("Rather than changing to cardinals");
			ImGui::MenuItem("Seseler's improved brain 9 movement", NULL, &dbg.monsteredgecol);
			tooltippy("Prevents monsters from getting stuck in corners or walking partly off-screen");
			ImGui::MenuItem("Brain 6 warp fix", nullptr, &dbg.warpfix);
			tooltippy("Allows for loop-brained sprites with warps to finish warping, such as in Quest for Dorinthia");
			ImGui::MenuItem("Extra text colours", nullptr, &debug_extratextcols);
			tooltippy("Allows for RTDink's extra colours in normal text");
			ImGui::MenuItem("DinkC /= functionality", NULL, &debug_divequals);
			tooltippy("Allows for /= to work properly in scripts");
			ImGui::MenuItem("Fully fade everything", nullptr, &debug_fullfade);
			tooltippy("Rather than leaving white behind");
			ImGui::MenuItem("JSON Saves", nullptr, &dbg.savejson);
			tooltippy("Human-readable for inspecting values");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Hacks")) {
			ImGui::MenuItem("Never process editor sprite overrides", nullptr, &debug_overignore);
			tooltippy("Sprites will render as if the screen was first visited");
			ImGui::MenuItem("Allow sprites to have multiple scripts", NULL, &debug_scriptkill);
			tooltippy("Refuses to remove previous script when assigning another");
			ImGui::MenuItem("Deterministic missile damage", NULL, &debug_maxdam);
			tooltippy("Maximum damage always");

			ImGui::EndMenu();
		}
		// ImGui::MenuItem("Write to debug.txt", NULL, &dbg.debug_conwrite);
		ImGui::MenuItem("Dear ImGui Demo", NULL, &imdemowindow);

	ImGui::EndMenu();
}
if (ImGui::BeginMenu("Engine"))
{
	if (ImGui::BeginMenu("Sprites")) {
	ImGui::MenuItem("List", NULL, &sprinfo);
	ImGui::MenuItem("Editor", NULL, &spreditors);
	ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Scripting")) {
	ImGui::MenuItem("Variables", NULL, &variableinfo);
	ImGui::MenuItem("Scripts", NULL, &scripfo);
	ImGui::MenuItem("Callbacks", NULL, &callbinfo);
	tooltippy("Scripts left waiting");
	//if (!dinklua_enabled)
	//ImGui::BeginDisabled();
	//ImGui::MenuItem("Lua state", NULL, &luaviewer);
	//if (!dinklua_enabled)
	//ImGui::EndDisabled();
	if (!dinkc_enabled)
		ImGui::BeginDisabled();
	ImGui::MenuItem("DinkC Bindings", NULL, &dinkcinfo);
	if (!dinkc_enabled)
		ImGui::EndDisabled();

	if (!dinklua_enabled)
		ImGui::BeginDisabled();
	ImGui::MenuItem("DinkLua Bindings", NULL, &luainfo);
	if (!dinklua_enabled)
		ImGui::EndDisabled();

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Graphics")) {
		ImGui::MenuItem("Sequences and Frames", NULL, &dinkini);
		ImGui::MenuItem("Colour Palette", NULL, &paledit);
		ImGui::MenuItem("Text and Fonts", NULL, &graphicsopt);
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (mode < 3)
	ImGui::BeginDisabled();
	ImGui::MenuItem("Save Data", NULL, &sproverride);
	tooltippy("Editor overrides, and more!");
	ImGui::MenuItem("Tiles", NULL, &maptiles);
	ImGui::MenuItem("Inventory", NULL, &magicitems);
    ImGui::Separator();
	if (mode < 3)
	ImGui::EndDisabled();
	ImGui::MenuItem("Map Index", NULL, &mapsquares);
	tooltippy("The contents of Dink.dat");
	ImGui::MenuItem("Hard.dat", NULL, &htiles);
	ImGui::MenuItem("Sound Effects", NULL, &moresfxinfo);
	//ImGui::Separator();
	//ImGui::MenuItem("Metrics", NULL, &metrics);

	ImGui::EndMenu();
}
}

/*
if (ImGui::BeginMenu("Tools")) {
	ImGui::MenuItem("Box previewer", NULL, &boxprev);

	ImGui::EndMenu();
}
*/

#ifndef __EMSCRIPTEN__
if (dbg.devmode) {
if (ImGui::BeginMenu("Speed"))
{
	if (ImGui::MenuItem("Normal speed", NULL, high_speed == 0))
		game_set_normal_speed();
	if (ImGui::MenuItem("High speed", NULL, high_speed == 1))
		game_set_high_speed();
	tooltippy("3x speed");
	//if (ImGui::MenuItem("Turbo Speed", NULL, high_speed == 2))
	//	game_set_turbo_speed();
	//tooltippy("6x normal speed");
	if (ImGui::MenuItem("Slow speed", NULL, high_speed == -1))
		game_set_slow_speed();
	tooltippy("Half normal speed");
	if (ImGui::MenuItem("Incredibly slow speed", NULL, high_speed == -2))
		game_set_extremely_slow_speed();
	tooltippy("One-tenth of normal speed");
	ImGui::Separator();
	ImGui::MenuItem("Remove framerate limiter", NULL, &dbg.framelimit);
	tooltippy("Will do nothing if you're Vsync-locked");
	ImGui::MenuItem("Lock sprite speed variability", NULL, &dbg.lockspeed);
	tooltippy("Will interfere with fast mode!");
	if (!dbg.lockspeed)
		ImGui::BeginDisabled();
	ImGui::PushItemWidth(90);
	ImGui::SliderInt("Base timing", &debug_base_timing, 1, 20);
	ImGui::PopItemWidth();
	if (!dbg.lockspeed)
		ImGui::EndDisabled();
	ImGui::EndMenu();
}
}
#endif

// Only show if game has started. Does weird stuff otherwise and I can't be bothered to fix it
if (!debug_savestates || debug_hardenedmode)
	ImGui::BeginDisabled();
if (ImGui::BeginMenu("Quicksave"))
{
	if (ImGui::MenuItem("Autosave on screen change", NULL, &dbg.autosave)) {
		//autosave only works with screen transitions on
		dbg.screentrans = true;
	}
	tooltippy("Requires screen transitions to be enabled");
	ImGui::SetNextItemWidth(100);
	if (!dbg.autosave)
		ImGui::BeginDisabled();
	ImGui::SliderInt("Autosave slot", &dbg.autosaveslot, 1, 10);
	if (!dbg.autosave)
		ImGui::EndDisabled();
	ImGui::Separator();
	//quicksaves
	if (mode < 2)
		ImGui::BeginDisabled();
	for (int s = 1; s < 5; s++) {
		int slot = s + 1336;
		ImGui::PushID(s * 4520);
		if (ImGui::MenuItem("Save")) {
			save_game(slot);
			debug_saveenabled[s] = true;
		}
		ImGui::SetItemTooltip("Slot %d", slot);
		ImGui::PopID();
	}
	//quickload
	ImGui::Separator();
	for (int l = 1; l < 5; l++) {
		int slot = l + 1336;
		if (!debug_saveenabled[l])
			ImGui::BeginDisabled();
		ImGui::PushID(l * 4521);
		if (ImGui::MenuItem("Load")) {
			//this might fix seseler's crash
			please_wait_toggle_frame = 0;
			log_debug_off();
			gameload(slot);
		}
		ImGui::PopID();
		if (!debug_saveenabled[l])
			ImGui::EndDisabled();
	}

	if (mode < 2)
		ImGui::EndDisabled();

	ImGui::EndMenu();
}
if (!debug_savestates || debug_hardenedmode) {
	ImGui::EndDisabled();
	tooltippy("Switched off by author");
}

if (ImGui::BeginMenu("Help"))
{
//ImGui::MenuItem("Contents", NULL, &helpbox);
ImGui::MenuItem("Tooltips", NULL, &dbg.tooltips);
tooltippy("Switch off these");
ImGui::Separator();
ImGui::MenuItem("System Info", NULL, &sysinfo);
ImGui::MenuItem("About", NULL, &aboutbox);
tooltippy("Credits and whatnot");
ImGui::EndMenu();
}

if (console_active == 1) {
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
	if (ImGui::BeginMenu("Warning! Console Active!")) {
		ImGui::PopStyleColor();
		if (ImGui::MenuItem("Deactivate Console", "Alt+C"))
			dinkc_console_hide();
		ImGui::EndMenu();
	} else {
	ImGui::PopStyleColor();
	}
}


ImGui::EndMainMenuBar();

if (imdemowindow)
	ImGui::ShowDemoWindow();
//ImPlot::ShowDemoWindow();
//End of menus, start of windows and such
//Transparent overlay info window, stolen from the demo
if (dbg.dswitch) {
	infowindow();
}
//Keyboard state and gamepad
if (keeb)
{
    ImGui::Begin("Gamepad and Touch", &keeb, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("Reinitialise input (and detect gamepads)"))
    {
        joystick = 1;
        input_init();
    }

    if (ginfo)
    {
        ImGui::Text("Name: %s", SDL_GameControllerName(ginfo));
		if (SDL_GameControllerMapping(ginfo) == NULL)
        	ImGui::Text("Button map failed to load!");
		else
			ImGui::Text("Button map successfully loaded from database");
		ImGui::Spacing();
	}
	else
    {
        ImGui::Text("Joystick/Gamepad could not be found");
    }
		#ifndef __EMSCRIPTEN__
		ImGui::SeparatorText("Options");
		ImGui::Checkbox("Start + Select instant exit", &dbg.jquit);
		#endif
		ImGui::Checkbox("Tab key locates cursor in mouse mode", &dbg.mouseloc);
		ImGui::Checkbox("Click/scroll choice menus while in debug mode", &dbg.clickmouse);
		ImGuiIO& io = ImGui::GetIO();
		ImGui::CheckboxFlags("Debug interface controller interaction", &io.ConfigFlags, ImGuiConfigFlags_NavEnableGamepad);
		ImGui::SliderInt("Diagonal threshold", &dbg.joythresh, 10, 90);
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset"))
			dbg.joythresh = 30;
        ImGui::Checkbox("Rumble effect", &dbg.jrumble);
		ImGui::SameLine();
		if (ImGui::Button("Test")) {
            SDL_GameControllerRumble(ginfo, 0xFFFF, 0xFFFF, 500);
            SDL_GameControllerRumbleTriggers(ginfo, 0xFFFF, 0xFFFF, 900);
        }
        //ImGui::SameLine();
       // if (ImGui::Button("Reload button map from database")) {
       //     int mapping = SDL_GameControllerAddMappingsFromFile(paths_pkgdatafile("gamecontrollerdb.txt"));
        //    if (mapping > 0)
        //        log_info("Successfully loaded %d mappings", SDL_GameControllerNumMappings());
       // }
		//drop-down box
		ImGui::Spacing();
		static const char* combo_preview_value = "Keyboard Key for L1";
		static int flags = 0;
        if (ImGui::BeginCombo("##combo 1", combo_preview_value, flags))
        {
            for (int n = 32; n < 96; n++)
            {
                const bool is_selected = (dbg.fakekey == n);
				static char num[25];
				sprintf(num, "%d (%c)", n, n);
                if (ImGui::Selectable(num, is_selected))
                    dbg.fakekey = n;

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
		ImGui::Text("Left shoulder (L1) will trigger key-%d script", dbg.fakekey);
	ImGui::Separator();
	if (ImGui::TreeNode("Touch Controls")) {
	ImGui::Checkbox("Touch input with virtual joystick", &dbg.touchcont);
    ImGui::SameLine();
    //switch it on
    if (dbg.fingswipe)
        dbg.touchcont = true;
    ImGui::Checkbox("Finger swipe input", &dbg.fingswipe);
    ImGui::Text("Lightly swipe over the screen with the selected amount of fingers to trigger");
    ImGui::SliderInt("Attack/select/Use item", &dbg.touchfing[0], 1, 10);
    ImGui::SliderInt("Talk/interact", &dbg.touchfing[1], 1, 10);
    ImGui::SliderInt("Magic", &dbg.touchfing[2], 1, 10);
    ImGui::SliderInt("Inventory", &dbg.touchfing[3], 1, 10);
    ImGui::SliderInt("Escape menu", &dbg.touchfing[4], 1, 10);

	ImGui::TreePop();
	}
	ImGui::End();
}

    //Log Window
    if (dbg.logwindow)
    {
        ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debug Message Log", &dbg.logwindow);
        ImGui::Text("Log Level:");
        ImGui::SameLine();

        if (ImGui::RadioButton("Errors only", &dbg.loglevel, 0))
            log_debug_on(0);
		tooltippy("Only errors will show up");
        ImGui::SameLine();
        if (ImGui::RadioButton("Standard", &dbg.loglevel, 1))
            log_debug_on(1);
		tooltippy("Debug, info, and error level messages");
        ImGui::SameLine();
        if (ImGui::RadioButton("Verbose", &dbg.loglevel, 2))
            log_debug_on(2);
		tooltippy("For diagnosing engine bugs only");
        ImGui::SameLine();
        ImGui::Checkbox("Coloured stdout", &dbg.debug_coltext);
		tooltippy("More pleasant console output");
        ImGui::Separator();
        ImGui::Text("Log:");
        ImGui::SameLine();
        ImGui::Checkbox("Meminfo", &dbg.debug_meminfo);
		tooltippy("Show memory usage of graphics and SFX upon changing screens");
        ImGui::SameLine();
        ImGui::Checkbox("Boot", &dbg.debug_scripboot);
		tooltippy("'Giving script the boot'");
        ImGui::SameLine();
        ImGui::Checkbox("'Bad pic' warnings", &dbg.debug_badpic);
        ImGui::SameLine();
        ImGui::Checkbox("Returnint values", &dbg.debug_returnint);
        ImGui::SameLine();
        ImGui::Checkbox("Touch events", &dbg.logtouch);
		tooltippy("Tablets and similar devices");
		ImGui::SameLine();
		ImGui::Checkbox("Script load errors", &dbg.errorscript);
        if (console_active)
			show_consoles();
ImGui::Separator();
ImGui::Checkbox("Auto-scroll upon new entry", &dbg.logautoscroll);
ImGui::SameLine();
if (ImGui::Button("Copy last message to clipboard"))
{
		ImGui::LogToClipboard();
		ImGui::LogText("%s", log_getLastLog());
		ImGui::LogFinish();
}
ImGui::Separator();
// Child window with text in it
if (ImGui::BeginTabBar("logbar")) {
	if (ImGui::BeginTabItem("All")) {
		logwindowtab(0, Buf);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Errors and Warnings")) {
		logwindowtab(1, errBuf);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Info")) {
		logwindowtab(2, infBuf);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Debug")) {
		logwindowtab(3, dbgBuf);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Trace")) {
		logwindowtab(4, traceBuf);
		ImGui::EndTabItem();
	}


ImGui::EndTabBar();
}
ImGui::End();
}

//Ultimate Cheat
if (ultcheat) {
	smurmeladeribuloso = true;
ImGui::Begin("Ultimater Cheat", &ultcheat, ImGuiWindowFlags_AlwaysAutoResize);

if (ImGui::BeginTabBar("cheaterlol")) {
if (ImGui::BeginTabItem("Screen and Stats")) {
if (plife == plifemax)
	ImGui::BeginDisabled();
if (ImGui::Button("Refill health")) {
    *plife = *plifemax;
}
if (plife == plifemax)
	ImGui::EndDisabled();
ImGui::SameLine();
if (ImGui::Button("Re-draw status bar"))
    draw_status_all();
ImGui::SameLine();
if (ImGui::Button("Draw background"))
	draw_screen_game_background();
if (ImGui::Button("Toggle screenlock"))
	screenlock == 0 ? screenlock = 1 : screenlock = 0;
ImGui::SameLine();
if (ImGui::Button("Load and re-draw screen")) {
	game_load_screen(g_dmod.map.loc[*pplayer_map]);
	draw_screen_game();
}
if (ImGui::Button("Recalc hardmap")) {
	update_play_changes();
	fill_whole_hard();
	fill_hard_sprites();
	fill_back_sprites();
}
ImGui::SameLine();
if (ImGui::Button("Kill enemies (no experience gained)")) {
	for (int i = 2; i <= last_sprite_created; i++) {
		if (spr[i].hitpoints > 0)
			spr[i].damage = spr[i].hitpoints;
	}
}
if (ImGui::Button("Fade down")) {
	if (!process_upcycle) {
		process_downcycle = /*true*/ 1;

	cycle_clock = thisTickCount + timer_fade;
	}
}
ImGui::SameLine();
if (ImGui::Button("Fade up")) {
	process_downcycle = 0;
	process_upcycle = /*true*/ 1;
}
ImGui::Separator();
ImGui::Checkbox("Infinite health", &debug_infhp);
ImGui::SameLine();
ImGui::Checkbox("Infinite magic", &debug_magcharge);
//ImGui::SameLine();
ImGui::Checkbox("No clipping  ", &debug_noclip);
ImGui::SameLine();
ImGui::Checkbox("Walk off screen", &debug_walkoff);
ImGui::Spacing();
ImGui::SeparatorText("Player variables");
ImGui::InputInt("Gold", pgold);
ImGui::InputInt("Attack", pstrength);
ImGui::InputInt("Defense", pdefense);
ImGui::InputInt("Magic", pmagic);
ImGui::SliderInt("Life", plife, 0, *plifemax);
ImGui::InputInt("Lifemax", plifemax, 1, 500);
ImGui::InputInt("Experience", pexp);
ImGui::SliderInt("Level", plevel, 1, 32);
ImGui::SliderInt("Map Screen", pplayer_map, 1, 768);
ImGui::SliderInt("Walk Speed", &dinkspeed, -1, 5);
tooltippy("Lower is faster");
ImGui::SliderInt("Screen X", &spr[1].x, 18, 620);
ImGui::SliderInt("Screen Y", &spr[1].y, -1, 400);
//Position plot
if (ImPlot::BeginPlot("Player Positioning", ImVec2(xmax / 2, ymax / 2), ImPlotFlags_Crosshairs | ImPlotFlags_NoLegend)) {
			ImPlot::SetupAxesLimits(0, xmax, 0, ymax);
			ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
			ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
			ImPlot::PlotScatter("Sprite 1", &spr[1].x, &spr[1].y, 1, ImPlotScatterFlags_NoClip);

			if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(0)) {
            ImPlotPoint pt = ImPlot::GetPlotMousePos();
			if (ImGui::GetIO().KeyCtrl) {
            spr[1].x = pt.x;
			} else if (ImGui::GetIO().KeyShift)
			{
			spr[1].y = pt.y;
			}
			else {
				spr[1].x = pt.x;
				spr[1].y = pt.y;
			}
        }
			ImPlot::EndPlot();
		}
		if (ImGui::Button("Copy X and Y values to clipboard")) {
			ImGui::LogToClipboard();
			ImGui::LogText("%d, %d", spr[1].x, spr[1].y);
			ImGui::LogFinish();
		}
ImGui::EndTabItem();
}

if (mode < 3)
	ImGui::BeginDisabled();

if (ImGui::BeginTabItem("Map Warp")) {
	if (ImGui::BeginTable("table1336", map_width, ImGuiTableFlags_Borders))
{
    for (int item = 1; item < 769; item++)
    {
        ImGui::TableNextColumn();
		if (g_dmod.map.loc[item] > 0)
		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImVec4(1.0f, 0.1f, 0.2f, 0.65f)));
		else {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImVec4(0.0f, 0.1f, 1.0f, 0.65f)));
		}
		if (item == *pplayer_map)
		ImGui::TextColored(ImVec4(0,1,0,1), "%i", item);
		else if (g_dmod.map.loc[item] == 0) {
			ImGui::Text("%d", item);
		}
		else {
			ImGui::PushID(item * 9000);
			char label[8];
			sprintf(label, "%d", item);
			if (ImGui::SmallButton(label)) {
				*pplayer_map = item;
				game_load_screen(g_dmod.map.loc[*pplayer_map]);
				draw_screen_game();
			}
			ImGui::PopID();
		}
		if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Screen: %i (%i), Indoor: %i, Music: %i", item, g_dmod.map.loc[item],  g_dmod.map.indoor[item], g_dmod.map.music[item]);
    }
    ImGui::EndTable();
}
ImGui::EndTabItem();
}

if (ImGui::BeginTabItem("Inventory")) {
	ImGui::Text("These assume the scripts and frames exist in base data");
	ImGui::Spacing();
	if (ImGui::Button("Add fireball, hellfire, and acid rain magic")) {
		add_item("item-fb", 437, 1, ITEM_MAGIC);
		add_item("item-sfb", 437, 2, ITEM_MAGIC);
		add_item("item-ice", 437, 5, ITEM_MAGIC);
	}
	if (ImGui::Button("Add the throwing axe, light sword, and fire bow")) {
		add_item("item-sw2", 438, 21, ITEM_REGULAR);
		add_item("item-b3", 438, 13, ITEM_REGULAR);
		add_item("item-axe", 438, 6, ITEM_REGULAR);
	}
	if (ImGui::Button("Add a bomb"))
		add_item("item-bom", 438, 3, ITEM_REGULAR);
	if (ImGui::Button("Add an AlkTree nut"))
		add_item("item-nut", 438, 19, ITEM_REGULAR);
	ImGui::Separator();
	if (ImGui::Button("Clear all inventory slots")) {
		memset(play.mitem, 0, sizeof(play.mitem));
		memset(play.item, 0, sizeof(play.item));
	}
	ImGui::EndTabItem();
}


if (mode < 3)
	ImGui::EndDisabled();
ImGui::EndTabBar();
}

ImGui::End();
}

if (graphicsopt) {
//Text colours
ImGui::Begin("Text", &graphicsopt, ImGuiWindowFlags_AlwaysAutoResize);
ImGui::Text("Current display TTF is: %s", dispfon);
if (ImGui::InputText("Font", myinitfont, sizeof(myinitfont), ImGuiInputTextFlags_EnterReturnsTrue)) {
	if (initfont(myinitfont) != -1)
		myinitfont[200] = {0};
}
ImGui::SameLine();
if (ImGui::Button("Init")) {
	initfont(myinitfont);
	myinitfont[200] = {0};
}
ImGui::SeparatorText("Text colours");
ImGui::Text("Click on a square to edit the index.");
if (ImGui::Button("Reset all text colours to default"))
	gfx_fonts_init_colors();
if (ImGui::BeginTable("textcoltable", 2, ImGuiTableFlags_Borders)) {
	ImGui::TableSetupColumn("Index");
	ImGui::TableSetupColumn("Colour");
	ImGui::TableHeadersRow();

	for (int i = 1; i <= 15; i++) {
		ImGui::TableNextColumn();
		ImGui::Text("%d", i);
		ImGui::PushID(i * 9);
		ImGui::TableNextColumn();
		mytextcol = ImVec4(font_colors[i].r / 255.0f, font_colors[i].g / 255.0f, font_colors[i].b / 255.0f, 1.0f);
		if (ImGui::ColorEdit3("##bloop", (float*)&mytextcol, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
			font_colors[i].r = mytextcol.x * 255;
			font_colors[i].g = mytextcol.y * 255;
			font_colors[i].b = mytextcol.z * 255;
		}
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::PushID(i * 8);
		if (ImGui::Button("Copy values to clipboard")) {
			ImGui::LogToClipboard();
			ImGui::LogText("%d, %d, %d", font_colors[i].r, font_colors[i].g, font_colors[i].b);
			ImGui::LogFinish();
		}
		ImGui::PopID();
	}

	ImGui::EndTable();
}
ImGui::End();
}

if (scripfo) {
ImGui::Begin("Scrips in use", &scripfo, ImGuiWindowFlags_AlwaysAutoResize);
//For making scrip to show just one time
if (ImGui::BeginTable("table2", 4, ImGuiTableFlags_Borders)) {
	ImGui::TableSetupColumn("No.");
	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("Sprite");
	ImGui::TableSetupColumn("Engine");
	ImGui::TableHeadersRow();
for (int i = 0; i < MAX_SCRIPTS; i++)
	{
	if (sinfo[i] != NULL)
    {
		ImGui::TableNextColumn();
		ImGui::Text("%d", i);
		ImGui::TableNextColumn();
		ImGui::Text(sinfo[i]->name);
		//if (ImGui::Button(sinfo[i]->name)) {
		//	myscript = i;
		//}
		//ImGui::SameLine();
		//ImGui::PushID(i);
		//if (ImGui::Button("Kill")) {
		//	kill_script(i);
		//}
		//ImGui::PopID();
        //ImGui::Text("%s", rinfo[i]->name);
		ImGui::TableNextColumn();
		ImGui::Text("%d", sinfo[i]->sprite);
		ImGui::TableNextColumn();
		ImGui::Text("%s", sinfo[i]->engine->name);
    }
	}
	ImGui::EndTable();
}
ImGui::Separator();
ImGui::Text("Scrips to show:");
ImGui::SameLine();

used_scripts_bar();
ImGui::Separator();
if (ImGui::InputText("Scrip file", myscriptpath, sizeof(myscriptpath), ImGuiInputTextFlags_EnterReturnsTrue)) {
	int x = scripting_load_script(myscriptpath, 1000, 0);
	scripting_run_proc(x, "main");
	myscriptpath[200] = {0};
}
ImGui::SameLine();
if (ImGui::Button("Execute")) {
	int x = scripting_load_script(myscriptpath, 1000, 0);
	scripting_run_proc(x, "main");
	myscriptpath[200] = {0};
}
tooltippy("Reminder to omit the file extension");
ImGui::End();
}

//Sprite Information
if (sprinfo) {
    ImGui::Begin("Sprite Lists", &sprinfo, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::BeginTabBar("spritab")) {
		if (ImGui::BeginTabItem("Live sprites")) {
			live_sprites_bar();
			ImGui::SameLine();
			ImGui::Checkbox("Filter editor sprites", &sprilivechk);
			ImGui::SameLine();
			ImGui::Checkbox("Scripted", &spriscripted);
			ImGui::SameLine();
			ImGui::Checkbox("Not drawn", &sprinodraw);
    	if (ImGui::BeginTable("table1", 21, ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable)) {
			ImGui::TableSetupColumn("Number");
			ImGui::TableSetupColumn("x");
			ImGui::TableSetupColumn("y");
			ImGui::TableSetupColumn("Seq");
			ImGui::TableSetupColumn("Frame");
			ImGui::TableSetupColumn("pSeq");
			ImGui::TableSetupColumn("pFrame");
			ImGui::TableSetupColumn("Timing");
			ImGui::TableSetupColumn("Script");
			ImGui::TableSetupColumn("Brain/parms");
			ImGui::TableSetupColumn("HP");
			ImGui::TableSetupColumn("Dir");
			ImGui::TableSetupColumn("Cue");
			ImGui::TableSetupColumn("Freeze");
			ImGui::TableSetupColumn("Size");
			ImGui::TableSetupColumn("Noclip");
			ImGui::TableSetupColumn("Editor No.");
			ImGui::TableSetupColumn("Hard");
			ImGui::TableSetupColumn("Nodraw");
			ImGui::TableSetupColumn("Speed");
			ImGui::TableSetupColumn("Sound");
			ImGui::TableHeadersRow();
			for (int x = 1; x < MAX_SPRITES_AT_ONCE; ++x) {
				if ((spr[x].active && spr[x].brain != 8)) {
					if ((!sprilivechk && !spriscripted && !sprinodraw) || (sprilivechk && spr[x].sp_index > 0) || (spriscripted && spr[x].script > 0) || (sprinodraw && spr[x].nodraw == 1)) {
					ImGui::TableNextColumn();
					ImGui::Text("%d", x);
					ImGui::SameLine();
					ImGui::PushID(x * 10000);
					if (ImGui::Button("Edit")) {
					spriedit = x;
					spreditors = true;
					flag = ImGuiTabItemFlags_SetSelected;
					}
					ImGui::PopID();
					ImGui::SameLine();
					ImGui::PushID(x * 1000000);
					if (ImGui::Button("Kill"))
					spr[x].kill_ttl = 1;
					ImGui::PopID();
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].x);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].y);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].seq);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].frame);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].pseq);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].pframe);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].timing);
					ImGui::TableNextColumn();
					if (spr[x].script > 0)
					//ImGui::Text("%s", rinfo[spr[x].script]->name);
					ImGui::Text("%i", spr[x].script);
					else
					ImGui::Text("None");
					ImGui::TableNextColumn();
					ImGui::Text("%d/%d/%d", spr[x].brain, spr[x].brain_parm, spr[x].brain_parm2);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].hitpoints);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].dir);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].que);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].freeze);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].size);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].noclip);
					ImGui::TableNextColumn();
					//edit button
					ImGui::PushID(x * (200000));
					if (spr[x].sp_index > 0) {
					if (ImGui::Button("Edit")) {
					mspried = spr[x].sp_index;
					spreditors = true;
					mflag = ImGuiTabItemFlags_SetSelected;
					}
					ImGui::SameLine();
					}
					ImGui::PopID();
					ImGui::Text("%d", spr[x].sp_index);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].hard);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].nodraw);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].speed);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].sound);
				}
    	}
			}
		ImGui::EndTable();
		}
		ImGui::EndTabItem();
		}

		//Comprehensive live sprite info
		if (ImGui::BeginTabItem("Comprehensive live sprite details")) {
			ImGui::SetNextItemWidth(50.0f);
			ImGui::DragInt("Selected sprite", &spriedit, 1, 1, last_sprite_created);
			ImGui::SameLine();
			ImGui::Checkbox("Punch to select", &dbg.punchedit);
			tooltippy("The most recently attacked sprite will be selected");
			static int selected = 0;
			{
            ImGui::BeginChild("left pane selector", ImVec2(90, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
            for (int i = 0; i <= last_sprite_created; i++)
            {
                if (spr[i].active) {
                char label[12];
                sprintf(label, "Sprite %d", i);
                if (ImGui::Selectable(label, spriedit == i))
                    spriedit = i;
				}
            }
            ImGui::EndChild();
        }
        ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::BeginChild("live sprite properties", ImVec2(460, 830));
		if (spr[spriedit].disabled)
			ImGui::Text("Sprite is set disabled");

		if (spr[spriedit].sp_index) {
			ImGui::BulletText("Map sprite is %d", spr[spriedit].sp_index);
			tooltippy("Index in map.dat");
		}
		ImGui::Spacing();

		if (ImGui::TreeNode("Position and movement")) {
			if (ImPlot::BeginPlot("##Screen Positioning", ImVec2(xmax / 2, ymax / 2), ImPlotFlags_Crosshairs | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
			ImPlot::SetupAxesLimits(0, xmax, 0, ymax);
			ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
			ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
			ImPlot::PlotScatter("Selected sprite", &spr[spriedit].x, &spr[spriedit].y, 1, ImPlotScatterFlags_NoClip);

			ImPlot::EndPlot();
		}
			ImGui::BulletText("Co-ordinates are X: %d, and Y: %d", spr[spriedit].x, spr[spriedit].y);
			ImGui::BulletText("Last legal position was X0: %d, Y0: %d", spr[spriedit].lpx[0], spr[spriedit].lpy[0]);
			ImGui::BulletText("Sprite speed is %d", spr[spriedit].speed);
			ImGui::BulletText("Direction is %d", spr[spriedit].dir);
			ImGui::BulletText("Freeze is %d", spr[spriedit].freeze);
			ImGui::BulletText("Noclip is %d", spr[spriedit].noclip);
			if (spr[spriedit].flying)
				ImGui::BulletText("Sprite can fly!");
			ImGui::BulletText("Movement speed is mx: %d, and my: %d", spr[spriedit].mx, spr[spriedit].my);
			ImGui::BulletText("Movement manager is at %d", spr[spriedit].moveman);
			if (spr[spriedit].move_active == 1) {
				ImGui::Separator();
				ImGui::BulletText("Active movement in progress!");
				ImGui::BulletText("Move direction is %d", spr[spriedit].move_dir);
				ImGui::BulletText("Destination is %d", spr[spriedit].move_num);
				if (spr[spriedit].move_nohard)
					ImGui::BulletText("Movement will ignore hard");
			}
			ImGui::BulletText("Movement script is %d", spr[spriedit].move_script);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Graphics and sequences")) {
			ImGui::BulletText("Nodraw is %d", spr[spriedit].nodraw);
			ImGui::BulletText("Sequence of sprite is %d with frame %d", spr[spriedit].seq, spr[spriedit].frame);
			ImGui::BulletText("Parameter sequence is %d with pframe %d", spr[spriedit].pseq, spr[spriedit].pframe);
			ImGui::BulletText("Frame delay is %d", spr[spriedit].frame_delay);
			ImGui::BulletText("Size is %d", spr[spriedit].size);
			ImGui::BulletText("Depth cue is %d", spr[spriedit].que);
			ImGui::BulletText("Sequence reversal: %d", spr[spriedit].reverse);
			ImGui::BulletText("Picfreeze: %d", spr[spriedit].picfreeze);
			ImGui::BulletText("Original sequence: %d", spr[spriedit].seq_orig);
			ImGui::BulletText("Base walk sequence is %d", spr[spriedit].base_walk);
			ImGui::BulletText("Base idle sequence is %d", spr[spriedit].base_idle);
			ImGui::BulletText("Base attack sequence is %d", spr[spriedit].base_attack);
			ImGui::BulletText("Base hit sequence is %d", spr[spriedit].base_hit);
			ImGui::BulletText("Base death sequence is %d", spr[spriedit].base_die);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Trimming/clipping")) {
			ImGui::BulletText("Left is %d", spr[spriedit].alt.left);
			ImGui::BulletText("Right is %d", spr[spriedit].alt.right);
			ImGui::BulletText("Top is %d", spr[spriedit].alt.top);
			ImGui::BulletText("Bottom is %d", spr[spriedit].alt.bottom);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Brain and parameters")) {
			ImGui::BulletText("Sprite has brain %d", spr[spriedit].brain);
			ImGui::BulletText("First brain parameter: %d", spr[spriedit].brain_parm);
			ImGui::BulletText("Second brain parameter: %d", spr[spriedit].brain_parm2);
			ImGui::BulletText("Timing interval is %d", spr[spriedit].timing);
			tooltippy("Amount of time before running the brain again");
			ImGui::BulletText("Following sprite %d", spr[spriedit].follow);
			tooltippy("Will attempt to move no closer than 40 pixels to the sprite");
			if (spr[spriedit].follow > 0) {
				int dir;
				ImGui::BulletText("Distance to sprite is %d", get_distance_and_dir(spr[spriedit].follow, spriedit, &dir));
			}
			ImGui::BulletText("Action is %d", spr[spriedit].action);
			tooltippy("Used by the NPC brain to determine if it'll walk around or not");
			ImGui::BulletText("Distance set to %d", spr[spriedit].distance);
			tooltippy("How close the sprite should attempt to get to its target");
			ImGui::BulletText("Nocontrol is %d", spr[spriedit].nocontrol);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Sound")) {
			ImGui::BulletText("Sound slot contains index %d", spr[spriedit].sound);
			if (!gSoloud.isVoiceGroupEmpty(spr[spriedit].sounds))
				ImGui::BulletText("Sprite has positional audio playing");
			ImGui::BulletText("Attack hit sound is %d", spr[spriedit].attack_hit_sound);
			ImGui::BulletText("Attack hit sound playback rate is %dHz", spr[spriedit].attack_hit_sound_speed);
			ImGui::BulletText("Last sound was %d", spr[spriedit].last_sound);
			tooltippy("Only used by pigs");

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Enemy and NPC parameters")) {
			ImGui::BulletText("Nohit %d", spr[spriedit].nohit);
			ImGui::BulletText("Hitpoints: %d", spr[spriedit].hitpoints);
			ImGui::BulletText("Experience points: %d", spr[spriedit].exp);
			ImGui::BulletText("Strength is %d", spr[spriedit].strength);
			ImGui::BulletText("Defense is %d", spr[spriedit].defense);
			ImGui::BulletText("Damage is %d", spr[spriedit].damage);
			ImGui::BulletText("Targeting sprite %d", spr[spriedit].target);
			if (spr[spriedit].target > 0) {
				int dir;
				ImGui::BulletText("Distance to target is %d", get_distance_and_dir(spr[spriedit].target, spriedit, &dir));
			}
			ImGui::BulletText("Last hit by %d", spr[spriedit].last_hit);
			ImGui::BulletText("Blood sequence is %d", spr[spriedit].bloodseq);
			ImGui::BulletText("Blood number is %d", spr[spriedit].bloodnum);
			ImGui::BulletText("Attack wait is %d", spr[spriedit].attack_wait);
			ImGui::BulletText("Range is %d", spr[spriedit].range);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Touch")) {
			ImGui::BulletText("Touch damage: %d", spr[spriedit].touch_damage);
			ImGui::BulletText("Notouch: %d", spr[spriedit].notouch);
			ImGui::BulletText("Notouch timer is %zu", spr[spriedit].notouch_timer);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Timers")) {
			ImGui::Text("Time to live is %d", spr[spriedit].kill_ttl);
			ImGui::Text("Start of kill time was %zu", spr[spriedit].kill_start);
			ImGui::Text("Delay is %ld", spr[spriedit].delay);
			ImGui::Text("Wait value is %ld", spr[spriedit].wait);
			ImGui::Text("Move wait is %zu", spr[spriedit].move_wait);
			ImGui::Text("Skip time is %d", spr[spriedit].skiptimer);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Custom data")) {
			if (spr[spriedit].custom != NULL) {
				if (spr[spriedit].custom->empty())
					ImGui::Text("Sprite has no custom data");
				for (const auto &i: *spr[spriedit].custom) {
					ImGui::BulletText("%s is %d", i.first.c_str(), i.second);
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Text")) {
			if (spr[spriedit].brain != 8)
				ImGui::Text("This is not a text sprite!");
			else {
			ImGui::BulletText("Text string: %s", spr[spriedit].text);
			ImGui::BulletText("Owner sprite: %d", spr[spriedit].text_owner);
			ImGui::BulletText("Live: %d", spr[spriedit].live);
			tooltippy("Indicates if created by say_stop_xy so it won't be removed by sprite manager");
			ImGui::BulletText("Rect is X: %d, Y: %d, W: %d, H: %d", spr[spriedit].text_cache_reldst.x, spr[spriedit].text_cache_reldst.y, spr[spriedit].text_cache_reldst.w, spr[spriedit].text_cache_reldst.h);
			ImVec4 textcol = ImVec4(spr[spriedit].text_cache_color.r / 255.0f, spr[spriedit].text_cache_color.g / 255.0f, spr[spriedit].text_cache_color.b / 255.0f, 1.0f);
			ImGui::TextColored(textcol, "Text cache colour is R: %d, G: %d, B: %d", spr[spriedit].text_cache_color.r, spr[spriedit].text_cache_color.g, spr[spriedit].text_cache_color.b);
			}
			ImGui::BulletText("Say_stop callback: %d", spr[spriedit].say_stop_callback);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Other")) {
			ImGui::BulletText("Skip is %d", spr[spriedit].skip);
			ImGui::BulletText("Attrib is %d", spr[spriedit].attrib);
			ImGui::BulletText("Idle is %d", spr[spriedit].idle);
			ImGui::BulletText("Gold is %d", spr[spriedit].gold);

			ImGui::TreePop();
		}

		ImGui::EndChild();
		ImGui::EndGroup();
		ImGui::EndTabItem();
		}
	//Text sprites
	if (ImGui::BeginTabItem("Text sprites")) {
		if (ImGui::BeginTable("table2", 7, ImGuiTableFlags_Borders)) {
			ImGui::TableSetupColumn("Number");
			ImGui::TableSetupColumn("x");
			ImGui::TableSetupColumn("y");
			ImGui::TableSetupColumn("Strength");
			ImGui::TableSetupColumn("Defense");
			ImGui::TableSetupColumn("Parent sprite");
			ImGui::TableSetupColumn("Text");
			ImGui::TableHeadersRow();

			for (int x = 1; x <= last_sprite_created; ++x) {
				if (spr[x].active && spr[x].brain == 8) {
					ImGui::TableNextColumn();
					ImGui::Text("%d", x);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].x);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].y);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].strength);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].defense);
					ImGui::TableNextColumn();
					ImGui::Text("%d", spr[x].text_owner);
					ImGui::TableNextColumn();
					ImGui::PushItemWidth(500);
					ImGui::Text("%s", spr[x].text);
					ImGui::PopItemWidth();
				}
			}
			ImGui::EndTable();
		}
		ImGui::EndTabItem();
	}

	//Map Sprites table
	if (mode < 2)
		ImGui::BeginDisabled();
	if (ImGui::BeginTabItem("Map Sprites")) {
		ImGui::Checkbox("Show vacant slots", &mspriunused);
		ImGui::Separator();
		if (strlen(cur_ed_screen.script) > 1)
			ImGui::Text("Screen script: %s", cur_ed_screen.script);
		if (ImGui::BeginTable("table3", 23, ImGuiTableFlags_Borders)) {
			ImGui::TableSetupColumn("Number");
			ImGui::TableSetupColumn("x");
			ImGui::TableSetupColumn("y");
			ImGui::TableSetupColumn("Seq");
			ImGui::TableSetupColumn("Frame");
			ImGui::TableSetupColumn("Type");
			ImGui::TableSetupColumn("Size");
			ImGui::TableSetupColumn("In Use");
			ImGui::TableSetupColumn("Brain");
			ImGui::TableSetupColumn("Script");
			ImGui::TableSetupColumn("Speed");
			ImGui::TableSetupColumn("Timing");
			ImGui::TableSetupColumn("Cue");
			ImGui::TableSetupColumn("Hard");
			ImGui::TableSetupColumn("Warp on");
			ImGui::TableSetupColumn("Screen");
			ImGui::TableSetupColumn("x dest");
			ImGui::TableSetupColumn("y dest");
			ImGui::TableSetupColumn("Warp seq");
			ImGui::TableSetupColumn("Sound");
			ImGui::TableSetupColumn("Vision");
			ImGui::TableSetupColumn("Touch damage");
			ImGui::TableSetupColumn("Gold");
			ImGui::TableHeadersRow();

			for (int x = 1; x <= MAX_SPRITES_EDITOR; x++) {
				if (cur_ed_screen.sprite[x].active || mspriunused) {
					ImGui::TableNextColumn();
					ImGui::Text("%d", x);
					ImGui::SameLine();
					ImGui::PushID(x * 10);
					if (ImGui::Button("Edit")) {
						mspried = x;
						spreditors = true;
						mflag = ImGuiTabItemFlags_SetSelected;
					}
					ImGui::PopID();
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].x);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].y);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].seq);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].frame);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].type);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].size);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].active);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].brain);
					ImGui::TableNextColumn();
					ImGui::Text("%s", cur_ed_screen.sprite[x].script);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].speed);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].timing);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].que);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].hard);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].is_warp);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].warp_map);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].warp_x);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].warp_y);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].parm_seq);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].sound);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].vision);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].touch_damage);
					ImGui::TableNextColumn();
					ImGui::Text("%d", cur_ed_screen.sprite[x].gold);
				}
			}

			ImGui::EndTable();
		}
		ImGui::EndTabItem();
	}
	if (mode < 2)
		ImGui::EndDisabled();
	ImGui::EndTabBar();
	}
ImGui::End();
}

if (spreditors) {
	ImGui::Begin("Sprite Editor", &spreditors, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::BeginTabBar("edittab")) {

	//Sprite editor tab
	if (ImGui::BeginTabItem("Live Sprite Editor", NULL, flag)) {
		//Tab item flags for edit button clicking
		flag = ImGuiTabItemFlags_None;
		//Left pane with list
		{
		ImGui::BeginChild("left list", ImVec2(90, 0), ImGuiChildFlags_Borders);
		for (int i = 1; i <= last_sprite_created; i++)
            {
				if (spr[i].active) {
                char label[12];
                sprintf(label, "Sprite %d", i);
                if (ImGui::Selectable(label, spriedit == i))
                    spriedit = i;
				}
            }
			ImGui::EndChild();
		}
			ImGui::SameLine();
		//Right, sprite properties
		ImGui::BeginGroup();
		ImGui::BeginChild("live sprite view", ImVec2(380, 850));
		//ImGui::Text("Activate the console to type directly into the boxes");
		ImGui::Text("Sprite %d", spriedit);
		ImGui::SameLine();
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
		if (ImGui::ArrowButton("##left", ImGuiDir_Left) && spriedit != 1) { spriedit--; }
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		if (ImGui::ArrowButton("##right", ImGuiDir_Right)) { spriedit++; }
		ImGui::PopItemFlag();
		ImGui::SameLine();
		ImGui::Checkbox("Highlight", &spoutline);
		ImGui::SameLine();
		ImGui::Checkbox("Punch select", &dbg.punchedit);
		tooltippy("The most recently attacked sprite will be selected");
		ImGui::Separator();
		//Sprite properties
		if (spr[spriedit].active) {
		if (ImGui::Button("Deactivate sprite")) {
			spr[spriedit].active = 0;
			}
		}
		else {
			if (ImGui::Button("Activate sprite")) {
				spr[spriedit].active = 1;
			}
		}
		if (spr[spriedit].sp_index > 0) {
			ImGui::SameLine();
			if (ImGui::Button("Edit corresponding map sprite")) {
			mspried = spr[spriedit].sp_index;
			mflag = ImGuiTabItemFlags_SetSelected;
			}
			if (ImGui::Button("Copy and save live sprite data to map sprite")) {
				mspried = spr[spriedit].sp_index;
				livesprtomap(spriedit);
				save_screen(g_dmod.map.map_dat.c_str(), g_dmod.map.loc[*pplayer_map]);
			}
		}
		ImGui::SliderInt("X", &spr[spriedit].x, 0, xmax);
		//arrows
		ImGui::SameLine();
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
        if (ImGui::ArrowButton("##spriteleft", ImGuiDir_Left)) { spr[spriedit].x--; }
        ImGui::SameLine();
        if (ImGui::ArrowButton("##spriteright", ImGuiDir_Right)) { spr[spriedit].x++; }
        ImGui::PopItemFlag();
		ImGui::SliderInt("Y", &spr[spriedit].y, 0, ymax);
		//arrows
		ImGui::SameLine();
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
        if (ImGui::ArrowButton("##spriteup", ImGuiDir_Up)) { spr[spriedit].y--; }
        ImGui::SameLine();
        if (ImGui::ArrowButton("##spritedown", ImGuiDir_Down)) { spr[spriedit].y++; }
        ImGui::PopItemFlag();
		//ImGui::SliderInt("mx", &spr[spriedit].mx, 0, xmax);
		//ImGui::SliderInt("my", &spr[spriedit].my, 0, ymax);
		//ImGui::Text("Moveman: %d", spr[spriedit].moveman);
		if (ImPlot::BeginPlot("Screen Positioning", ImVec2(xmax / 2, ymax / 2), ImPlotFlags_Crosshairs | ImPlotFlags_NoLegend)) {
			ImPlot::SetupAxesLimits(0, xmax, 0, ymax);
			ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
			ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
			ImPlot::PlotScatter("Selected sprite", &spr[spriedit].x, &spr[spriedit].y, 1, ImPlotScatterFlags_NoClip);

			if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(0)) {
            ImPlotPoint pt = ImPlot::GetPlotMousePos();
			if (ImGui::GetIO().KeyCtrl) {
            spr[spriedit].x = pt.x;
			} else if (ImGui::GetIO().KeyShift)
			{
			spr[spriedit].y = pt.y;
			}
			else {
				spr[spriedit].x = pt.x;
				spr[spriedit].y = pt.y;
			}
        }
			ImPlot::EndPlot();
		}
		if (ImGui::Button("Copy X and Y values to clipboard")) {
			ImGui::LogToClipboard();
			ImGui::LogText("%d, %d", spr[spriedit].x, spr[spriedit].y);
			ImGui::LogFinish();
		}
		ImGui::Separator();
		ImGui::RadioButton("Frozen", &spr[spriedit].freeze, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Thawed", &spr[spriedit].freeze, 0);
		ImGui::RadioButton("Not nohit", &spr[spriedit].nohit, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Nohit active", &spr[spriedit].nohit, 1);
		ImGui::RadioButton("Drawn", &spr[spriedit].nodraw, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Nodraw", &spr[spriedit].nodraw, 1);
		ImGui::RadioButton("Hard on", &spr[spriedit].hard, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Hard off", &spr[spriedit].hard, 1);
		ImGui::RadioButton("Clip", &spr[spriedit].noclip, 0);
		ImGui::SameLine();
		ImGui::RadioButton("No clip", &spr[spriedit].noclip, 1);
		ImGui::SliderInt("pSequence", &spr[spriedit].pseq, 1, 1000);
		ImGui::SliderInt("pFrame", &spr[spriedit].pframe, 1, 69);
		ImGui::SliderInt("Direction", &spr[spriedit].dir, 1, 8);
		ImGui::SliderInt("Size", &spr[spriedit].size, 1, 200);
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset"))
			spr[spriedit].size = 100;
		ImGui::InputInt("Frame Delay", &spr[spriedit].frame_delay);
		//Change to drop down box eventually
		//ImGui::InputInt("Brain", &spr[spriedit].brain, 1, 2);
		ImGui::SliderInt("Depth Cue", &spr[spriedit].que, -1000, 1000);
		ImGui::InputInt("Sound", &spr[spriedit].sound);
		ImGui::InputText("Text", spr[spriedit].text, sizeof(spr[spriedit].text));
		//ImGui::SliderInt("Hitpoints", &spr[spriedit].hitpoints, 0, 100);
		ImGui::Separator();
		if (ImGui::TreeNode("More sequences")) {
			ImGui::InputInt("Sequence", &spr[spriedit].seq);
			ImGui::InputInt("Frame", &spr[spriedit].frame);
			ImGui::InputInt("Blood seq", &spr[spriedit].bloodseq);
			ImGui::InputInt("Blood number", &spr[spriedit].bloodnum);
			ImGui::InputInt("Base Walk", &spr[spriedit].base_walk);
			ImGui::InputInt("Base Idle", &spr[spriedit].base_idle);
			ImGui::InputInt("Base Hit", &spr[spriedit].base_hit);
			ImGui::InputInt("Base Attack", &spr[spriedit].base_attack);
			ImGui::InputInt("Base Die", &spr[spriedit].base_die);
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("Enemy and NPC parameters")) {
			ImGui::InputInt("Hitpoints", &spr[spriedit].hitpoints);
			ImGui::InputInt("Touch damage", &spr[spriedit].touch_damage);
			ImGui::InputInt("Defence", &spr[spriedit].defense);
			ImGui::InputInt("Strength", &spr[spriedit].strength);
			ImGui::InputInt("Experience", &spr[spriedit].exp);
			ImGui::InputInt("Attack hit sound", &spr[spriedit].attack_hit_sound);
			ImGui::InputInt("Hit sound Hz", &spr[spriedit].attack_hit_sound_speed);
			ImGui::InputInt("Follow sprite", &spr[spriedit].follow);
			ImGui::InputInt("Target sprite", &spr[spriedit].target);
			ImGui::InputInt("Distance", &spr[spriedit].distance);
			ImGui::InputInt("Range", &spr[spriedit].range);
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("Brain and parms")) {
			ImGui::InputInt("Brain", &spr[spriedit].brain);
			ImGui::InputInt("Parm 1", &spr[spriedit].brain_parm);
			ImGui::InputInt("Parm 2", &spr[spriedit].brain_parm2);
			ImGui::InputInt("Timing", &spr[spriedit].timing);
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("Trimming")) {
			if (ImGui::Button("Set to bounding box")) {
				spr[spriedit].alt.right = k[getpic(spriedit)].box.right;
				spr[spriedit].alt.bottom = k[getpic(spriedit)].box.bottom;
			}
			ImGui::SliderInt("Trim top", &spr[spriedit].alt.top, 0, k[getpic(spriedit)].box.bottom);
			ImGui::SliderInt("Trim left", &spr[spriedit].alt.left, 0, k[getpic(spriedit)].box.right);
			//ImGui::InputInt("Trim left", &spr[spriedit].alt.left);
			//ImGui::InputInt("Trim/show right", &spr[spriedit].alt.right);
			ImGui::SliderInt("Trim/show right", &spr[spriedit].alt.right, 0, k[getpic(spriedit)].box.right);
			//ImGui::InputInt("Trim/show bottom", &spr[spriedit].alt.bottom);
			ImGui::SliderInt("Trim/show bottom", &spr[spriedit].alt.bottom, 0, k[getpic(spriedit)].box.bottom);
			//TODO: add easy trimming feature with implot
			//Get the bounding box of the sprite
			if (ImPlot::BeginPlot("Drag to trim", ImVec2(320, 240), ImPlotFlags_NoLegend)) {
				ImPlot::SetupAxesLimits(0, k[getpic(spriedit)].box.right, 0, k[getpic(spriedit)].box.bottom);
				//invert axis because dink is dumb
				ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert);
				trimbox.X.Min = spr[spriedit].alt.left;
				trimbox.X.Max = spr[spriedit].alt.right;
				trimbox.Y.Min = spr[spriedit].alt.top;
				trimbox.Y.Max = spr[spriedit].alt.bottom;
				ImPlot::DragRect(0, &trimbox.X.Min, &trimbox.Y.Min, &trimbox.X.Max, &trimbox.Y.Max,ImVec4(1,0,1,1), ImPlotDragToolFlags_Delayed);
				//Set trimming with dragging
				spr[spriedit].alt.left = trimbox.X.Min;
				spr[spriedit].alt.right = trimbox.X.Max;
				spr[spriedit].alt.top = trimbox.Y.Min;
				spr[spriedit].alt.bottom = trimbox.Y.Max;
				ImPlot::EndPlot();
			}
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("Custom data")) {
			if (spr[spriedit].custom != NULL) {
			ImGui::InputText("Key", customkey, 200);
			ImGui::InputInt("Value", &customval);
			ImGui::SameLine();
			if (ImGui::Button("Add")) {
				auto success = spr[spriedit].custom->insert(std::make_pair(customkey, customval));
				if (!success.second)
					success.first->second = customval;
				customkey[200] = {0};
				customval = 0;
			}
			ImGui::Spacing();
			if (!spr[spriedit].custom->empty()) {
			ImGui::BeginTable("tablecustom", 2, ImGuiTableFlags_Borders);
			ImGui::TableSetupColumn("Key");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
			for (const auto &x: *spr[spriedit].custom) {
				ImGui::TableNextColumn();
				//ImGui::Text("%s", x.first.c_str());
				if (ImGui::Button(x.first.c_str())) {
					customval = x.second;
					sprintf(customkey, "%s", x.first.c_str());
				}
				ImGui::TableNextColumn();
				ImGui::Text("%d", x.second);
			}

			ImGui::EndTable();
			if (ImGui::Button("Clear all entries"))
				spr[spriedit].custom->clear();
			}
			}
			ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::TreeNode("Dink.ini properties")) {
			ImGui::Text("These values are not persistent after editing");
			ImGui::Checkbox("Hardbox preview", &hboxprev);
			ImGui::SameLine();
			ImGui::Checkbox("Dot preview", &ddotprev);
			if (ImGui::Button("Set all to zero")) {
				memset(&k[getpic(spriedit)].hardbox, 0, sizeof(pic_info));
				k[getpic(spriedit)].xoffset = 0;
				k[getpic(spriedit)].yoffset = 0;
			}
			ImGui::SeparatorText("Hardbox");
			ImGui::TextWrapped("Be aware that these may have been edited by an init() line at some point.");
			ImGui::InputInt("Left", &k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.left);
			ImGui::InputInt("Top", &k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.top);
			ImGui::InputInt("Right", &k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.right);
			ImGui::InputInt("Bottom", &k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.bottom);
			ImGui::SeparatorText("Depth dot offset");
			ImGui::InputInt("X", &k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].xoffset);
			ImGui::InputInt("Y", &k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].yoffset);
			ImGui::Separator();
			//ImGui::Text("Box is %d right %d bottom", k[getpic(spriedit)].box.right, k[getpic(spriedit)].box.bottom);

			if (spr[spriedit].brain != 8) {
				ImGui::Checkbox("Interactive hardbox editing", &sprplotedit);
				ImGui::Text("Ctrl+Click to adjust dot");
			if (ImPlot::BeginPlot(("Box and dot"), ImVec2(320, 240), ImPlotFlags_NoLegend)) {
				ImPlot::SetupAxesLimits(0, k[getpic(spriedit)].box.right, 0, k[getpic(spriedit)].box.bottom);
				ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert);
				//ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin);
				ImPlot::PlotScatter("Dot", &k[getpic(spriedit)].xoffset, &k[getpic(spriedit)].yoffset, 1, ImPlotScatterFlags_NoClip);
				hbox.X.Min = k[getpic(spriedit)].xoffset + k[getpic(spriedit)].hardbox.left;
				hbox.X.Max = k[getpic(spriedit)].xoffset + k[getpic(spriedit)].hardbox.right;
				hbox.Y.Min = k[getpic(spriedit)].yoffset + k[getpic(spriedit)].hardbox.top;
				hbox.Y.Max = k[getpic(spriedit)].yoffset + k[getpic(spriedit)].hardbox.bottom;
				ImPlot::DragRect(0, &hbox.X.Min, &hbox.Y.Min, &hbox.X.Max, &hbox.Y.Max,ImVec4(1,0,1,1), ImPlotDragToolFlags_Delayed);
				if (sprplotedit) {
				k[getpic(spriedit)].hardbox.left = hbox.X.Min - k[getpic(spriedit)].xoffset;
				k[getpic(spriedit)].hardbox.right = hbox.X.Max - k[getpic(spriedit)].xoffset;
				k[getpic(spriedit)].hardbox.top = hbox.Y.Min - k[getpic(spriedit)].yoffset;
				k[getpic(spriedit)].hardbox.bottom = hbox.Y.Max - k[getpic(spriedit)].yoffset;
				}
				if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(0) && ImGui::GetIO().KeyCtrl) {
            		ImPlotPoint pt = ImPlot::GetPlotMousePos();
            		k[getpic(spriedit)].xoffset = pt.x;
					k[getpic(spriedit)].yoffset = pt.y;
        		}
				ImPlot::EndPlot();
			}
			}
			ImGui::Separator();
			if (ImGui::Button("Copy depth dot and hardbox values to clipboard")) {
				ImGui::LogToClipboard();
				ImGui::LogText("%d %d %d %d %d %d", k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].xoffset, k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].yoffset,
				k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.left, k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.top, k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.right, k[seq[spr[spriedit].pseq].frame[spr[spriedit].pframe]].hardbox.bottom);
				ImGui::LogFinish();
			}
			if (ImGui::Button("Append SET_SPRITE_INFO line to Dink.ini")) {
				add_text();
		}
		ImGui::TreePop();
		}
		ImGui::Separator();
		if (ImGui::Button("Copy create_sprite() command to clipboard")) {
			ImGui::LogToClipboard();
			ImGui::LogText("create_sprite(%d, %d, %d, %d, %d)", spr[spriedit].x, spr[spriedit].y, spr[spriedit].brain, spr[spriedit].pseq, spr[spriedit].pframe);
			ImGui::LogFinish();
		}
		ImGui::EndChild();
		ImGui::EndGroup();
		ImGui::EndTabItem();
	} // Start of screen sprite editor
	if (mode < 3)
		ImGui::BeginDisabled();
	if (ImGui::BeginTabItem("Map Sprite Editor", nullptr, mflag)) {
		mflag = ImGuiTabItemFlags_None;
		ImGui::InputText("Screen Script", cur_ed_screen.script, sizeof(cur_ed_screen.script));
		if (dinklua_enabled) {
		ImGui::SameLine();
			if (ImGui::Button("Edit"))
				edit_script(cur_ed_screen.script);
			tooltippy("Launches text editor");
		}
		if (ImGui::Button("Remove all sprites on this screen")) {
			memset(&cur_ed_screen.sprite, 0, sizeof(cur_ed_screen.sprite));
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove all sprite scripts")) {
			for (int i = 1; i <= MAX_SPRITES_EDITOR; i++)
				memset(cur_ed_screen.sprite[i].script, 0, sizeof(cur_ed_screen.sprite[i].script));
		}
		ImGui::Checkbox("Show free slots", &mspriunused);
		if (mspriunused) {
		ImGui::SameLine();
		if (ImGui::Button("Jump to free slot")) {
			for (int i = 1; i <= MAX_SPRITES_EDITOR; i++) {
				if (!cur_ed_screen.sprite[i].active) {
					mspried = i;
					break;
				}
			}
		}
		}
		ImGui::Separator();
		//Left pane
		{
		ImGui::BeginChild("left list", ImVec2(90, 0), ImGuiChildFlags_Borders);
		for (int i = 1; i <= MAX_SPRITES_EDITOR; i++)
            {
				if (cur_ed_screen.sprite[i].active || mspriunused) {
                char label[12];
                sprintf(label, "Sprite %d", i);
                if (ImGui::Selectable(label, mspried == i))
                    mspried = i;
				}
            }
			ImGui::EndChild();
		}
		//Right pane
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::BeginChild("map sprite view", ImVec2(380, 850));
		int maxframe = 0;
		//Index selector
		ImGui::Text("Selected editor sprite: %d", mspried);
		ImGui::SameLine();
		ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
		if (ImGui::ArrowButton("##back", ImGuiDir_Left)) {if (mspried > 1) mspried--;}
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		if (ImGui::ArrowButton("##forward", ImGuiDir_Right)) { mspried++; }
		ImGui::PopItemFlag();
		//Various buttons
		ImGui::Separator();

		if (cur_ed_screen.sprite[mspried].active == 0) {
		if (ImGui::Button("Activate sprite")) {
			cur_ed_screen.sprite[mspried].active = 1;
		}
		}
		else {
		if (ImGui::Button("Remove sprite"))
			cur_ed_screen.sprite[mspried].active = 0;

		ImGui::SameLine();
		if (ImGui::Button("Make duplicate of")) {
			//Find an empty slot
			for (int i = 1; i <= MAX_SPRITES_EDITOR; i++) {
				if (cur_ed_screen.sprite[i].active == 0) {
					memcpy(&cur_ed_screen.sprite[i], &cur_ed_screen.sprite[mspried], sizeof(editor_sprite));
					mspried = i;
					break;
				}
			}
		}
		}
		ImGui::Separator();
		//ImGui::TextWrapped("Some of these values may have been overwritten by editor_frame() etc");
		if (play.spmap[*pplayer_map].type[mspried] != 0 || play.spmap[*pplayer_map].frame[mspried] != 0)
		ImGui::TextColored(ImVec4(1.0f,0.0f,0.0f,1.0f), "Warning: values overwritten by editor_frame() etc");
		//Start of parameters
		ImGui::InputText("Script", cur_ed_screen.sprite[mspried].script, sizeof(cur_ed_screen.sprite[mspried].script));
		ImGui::SameLine();
		if (dinklua_enabled) {
			if (ImGui::Button("Edit"))
				edit_script(cur_ed_screen.sprite[mspried].script);
			tooltippy("Launch text editor for sprite's script");
		}
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::SliderInt("X position", &cur_ed_screen.sprite[mspried].x, 0, xmax);
		ImGui::SliderInt("Y position", &cur_ed_screen.sprite[mspried].y, 0, ymax);
		if (ImPlot::BeginPlot("Positioning", ImVec2(xmax / 2, ymax / 2), ImPlotFlags_Crosshairs| ImPlotFlags_NoLegend)) {
			ImPlot::SetupAxesLimits(0, xmax, 0, ymax);
			ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin);
			ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin);
			ImPlot::PlotScatter("Selected sprite", &cur_ed_screen.sprite[mspried].x, &cur_ed_screen.sprite[mspried].y, 1, ImPlotScatterFlags_NoClip);

			if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(0)) {
            ImPlotPoint pt = ImPlot::GetPlotMousePos();
			if (ImGui::GetIO().KeyCtrl) {
            cur_ed_screen.sprite[mspried].x = pt.x;
			} else if (ImGui::GetIO().KeyShift)
			{
			cur_ed_screen.sprite[mspried].y = pt.y;
			}
			else {
				cur_ed_screen.sprite[mspried].x = pt.x;
				cur_ed_screen.sprite[mspried].y = pt.y;
			}
        }
			ImPlot::EndPlot();
		}
		ImGui::RadioButton("Type 0: background", &cur_ed_screen.sprite[mspried].type, 0);
		ImGui::SameLine();
		ImGui::RadioButton("1: normal", &cur_ed_screen.sprite[mspried].type, 1);
		ImGui::SameLine();
		ImGui::RadioButton("2: invisible", &cur_ed_screen.sprite[mspried].type, 2);
		ImGui::RadioButton("Hard off", &cur_ed_screen.sprite[mspried].hard, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Hard on", &cur_ed_screen.sprite[mspried].hard, 0);
		ImGui::RadioButton("Nohit off", &cur_ed_screen.sprite[mspried].nohit, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Nohit on", &cur_ed_screen.sprite[mspried].nohit, 1);
		ImGui::SliderInt("Vision", &cur_ed_screen.sprite[mspried].vision, 0, 9);
		ImGui::InputInt("Sequence", &cur_ed_screen.sprite[mspried].seq,1);
		ImGui::SliderInt("Frame", &cur_ed_screen.sprite[mspried].frame, 1, 50);
		ImGui::SliderInt("Size", &cur_ed_screen.sprite[mspried].size, 1, 200);
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset"))
			cur_ed_screen.sprite[mspried].size = 100;
		ImGui::InputInt("Brain", &cur_ed_screen.sprite[mspried].brain);
		ImGui::SliderInt("Depth Cue", &cur_ed_screen.sprite[mspried].que, -100, 100);
		ImGui::Separator();
		ImGui::InputInt("Speed", &cur_ed_screen.sprite[mspried].speed);
		ImGui::InputInt("Timing", &cur_ed_screen.sprite[mspried].timing);
		ImGui::Separator();
		if (ImGui::TreeNode("Warp properties")) {
			ImGui::RadioButton("Warp Off", &cur_ed_screen.sprite[mspried].is_warp, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Warp On", &cur_ed_screen.sprite[mspried].is_warp, 1);
			ImGui::InputInt("Map screen", &cur_ed_screen.sprite[mspried].warp_map);
			ImGui::SliderInt("X position", &cur_ed_screen.sprite[mspried].warp_x, 0, xmax);
			ImGui::SliderInt("Y position", &cur_ed_screen.sprite[mspried].warp_y, 0, ymax);
			ImGui::InputInt("Warp sequence", &cur_ed_screen.sprite[mspried].parm_seq);
			ImGui::InputInt("Sound", &cur_ed_screen.sprite[mspried].sound);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Enemy properties")) {
			ImGui::InputInt("Hitpoints", &cur_ed_screen.sprite[mspried].hitpoints);
			ImGui::InputInt("Strength", &cur_ed_screen.sprite[mspried].strength);
			ImGui::InputInt("Defence", &cur_ed_screen.sprite[mspried].defense);
			ImGui::InputInt("Experience", &cur_ed_screen.sprite[mspried].exp);
			ImGui::InputInt("Touch damage", &cur_ed_screen.sprite[mspried].touch_damage);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Base sequences")) {
			ImGui::InputInt("Base walk", &cur_ed_screen.sprite[mspried].base_walk);
			ImGui::InputInt("Base attack", &cur_ed_screen.sprite[mspried].base_attack);
			ImGui::InputInt("Base idle", &cur_ed_screen.sprite[mspried].base_idle);
			ImGui::InputInt("Base die", &cur_ed_screen.sprite[mspried].base_die);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Trimming")) {
			ImGui::InputInt("Trim top", &cur_ed_screen.sprite[mspried].alt.top);
			ImGui::InputInt("Trim left", &cur_ed_screen.sprite[mspried].alt.left);
			ImGui::InputInt("Trim/show right", &cur_ed_screen.sprite[mspried].alt.right);
			ImGui::InputInt("Trim/show bottom", &cur_ed_screen.sprite[mspried].alt.bottom);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Obscure/unused")) {
			ImGui::InputInt("Base hit", &cur_ed_screen.sprite[mspried].base_hit);
			ImGui::InputInt("Gold", &cur_ed_screen.sprite[mspried].gold);
			ImGui::InputInt("Rotation", &cur_ed_screen.sprite[mspried].rotation);
			ImGui::TreePop();
		}

		ImGui::Separator();
		if (ImGui::Button("Save changes to MAP.DAT")) {
			save_screen(g_dmod.map.map_dat.c_str(), g_dmod.map.loc[*pplayer_map]);
		}
		ImGui::EndChild();
		ImGui::EndGroup();

		ImGui::EndTabItem();
	}
	if (mode < 3)
		ImGui::EndDisabled();
	//start of plot
	if (ImGui::BeginTabItem("Plot")) {
		if (ImPlot::BeginPlot("Sprites", ImVec2(640, 480), ImPlotFlags_NoLegend)) {
			ImPlot::SetupAxesLimits(0, xmax, 0, ymax);
			ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
			ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin);
			ImPlot::PlotScatter("DinkPlot", &spr[1].x, spr[1].y, 1, ImPlotScatterFlags_NoClip);
			for (int i = 2; i <= last_sprite_created; i++) {
				if (spr[i].active && spr[i].brain != 8) {
					ImPlot::PlotScatter("##sprite", &spr[i].x, &spr[i].y, 1, ImPlotScatterFlags_NoClip);
				}
			}
			ImPlot::EndPlot();
		}
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
	}
	ImGui::End();
}


if (variableinfo) {
    ImGui::Begin("Variables", &variableinfo, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::BeginTabBar("variables")) {
		if (ImGui::BeginTabItem("Global variables")) {
			var_used_bar();
	//Table begins
	if (ImGui::BeginTable("table5", 5, ImGuiTableFlags_Borders)) {
	ImGui::TableSetupColumn("No.");
	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("Value");
	ImGui::TableSetupColumn("Active");
	ImGui::TableSetupColumn("Scope");
	ImGui::TableHeadersRow();
    for (int i = 1; i < MAX_VARS; i++) {
		if (!strncmp(play.var[i].name, "&", 1)) {
			if (play.var[i].active && play.var[i].scope == 0)
			{
		ImGui::TableNextColumn();
		ImGui::Text("%d", i);
		ImGui::TableNextColumn();
		ImGui::Text("%s", play.var[i].name);
        //ImGui::Text(play.var[i].name);
		ImGui::TableNextColumn();
		//ImGui::Text("%d", play.var[i].var);
		ImGui::PushID(i * 100);
		ImGui::SetNextItemWidth(100);
		ImGui::InputInt("##value", &play.var[i].var);
		ImGui::PopID();
		ImGui::TableNextColumn();
		ImGui::Text("%d", play.var[i].active);
		ImGui::TableNextColumn();
		ImGui::Text("%d", play.var[i].scope);
		ImGui::TableNextRow();
			}
		}

    }
	ImGui::EndTable();
	}
	ImGui::EndTabItem();
	}
	if (!dinkc_enabled)
		ImGui::BeginDisabled();

	if (ImGui::BeginTabItem("Local variables")) {
		if (ImGui::BeginTable("table999", 5, ImGuiTableFlags_Borders)) {
			ImGui::TableSetupColumn("No.");
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Value");
			ImGui::TableSetupColumn("Active");
			ImGui::TableSetupColumn("Script");
			ImGui::TableHeadersRow();

		for (int i = 1; i < MAX_VARS; i++) {
		if (!strncmp(play.var[i].name, "&", 1)) {
			if (play.var[i].active && play.var[i].scope != 0)
			{
			ImGui::TableNextColumn();
			ImGui::Text("%d", i);
			ImGui::TableNextColumn();
			ImGui::Text("%s", play.var[i].name);
			ImGui::TableNextColumn();
			ImGui::PushID(i * 100);
			ImGui::SetNextItemWidth(100);
			ImGui::InputInt("##value", &play.var[i].var);
			ImGui::PopID();
			ImGui::TableNextColumn();
			ImGui::Text("%d", play.var[i].active);
			ImGui::TableNextColumn();
			ImGui::Text("%d", play.var[i].scope);
			ImGui::TableNextRow();
			}
		}

    }

		ImGui::EndTable();
		}

		ImGui::EndTabItem();
	}

	if (!dinkc_enabled)
		ImGui::EndDisabled();

	if (ImGui::BeginTabItem("Engine variables")) {
		ImGui::BulletText("dversion is %d", dversion);
		ImGui::BulletText("last_saved_game: %d", last_saved_game);
		ImGui::BulletText("stop_entire_game is %d", stop_entire_game);
		ImGui::BulletText("smooth_follow is %d", smooth_follow);
		ImGui::BulletText("show_inventory is %d", show_inventory);
		ImGui::BulletText("but_timer is %" SDL_PRIs64, but_timer);
		ImGui::BulletText("returnint is %d", returnint);
		ImGui::BulletText("missle_source is %d", *pmissle_source);
		ImGui::BulletText("missile_target is %d", *pmissile_target);
		ImGui::BulletText("enemy_sprite is %d", *penemy_sprite);
		ImGui::BulletText("last_text is %d", *plast_text);
		ImGui::Spacing();

		if (ImGui::TreeNode("Wait for Button")) {
			ImGui::BulletText("script is %d", wait4b.script);
			ImGui::BulletText("button is %d", wait4b.button);
			ImGui::BulletText("active is %d", wait4b.active);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Screen")) {
			ImGui::BulletText("walk_off_screen is %d", walk_off_screen);
			ImGui::BulletText("screenlock is %d", screenlock);
			ImGui::BulletText("vision is %d", *pvision);
			ImGui::BulletText("screen_main_is_running: %d", screen_main_is_running);
			ImGui::BulletText("keep_mouse is %d", keep_mouse);
			ImGui::BulletText("player_map is %d", *pplayer_map);

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Fades and Warp")) {
			ImGui::BulletText("cycle_clock is %zu", cycle_clock);
			ImGui::BulletText("cycle_script is %d", cycle_script);
			ImGui::BulletText("process_downcycle is %d", process_downcycle);
			ImGui::BulletText("process_upcycle is %d", process_upcycle);
			ImGui::BulletText("process_warp is %d", process_warp);
			ImGui::BulletText("warp_editor_sprite is %d", warp_editor_sprite);

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Show BMP")) {
			ImGui::BulletText("active is %d", showb.active);
			ImGui::BulletText("showdot is %d", showb.showdot);
			ImGui::BulletText("reserved is %d", showb.reserved);
			ImGui::BulletText("script is %d", showb.script);
			ImGui::BulletText("stime is %d", showb.stime);
			ImGui::BulletText("picframe is %d", showb.picframe);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Choice menu")) {
			ImGui::BulletText("Active: %d", game_choice.active);
			ImGui::BulletText("Script: %d", game_choice.script);
			ImGui::BulletText("Title: %s", game_choice.buffer);
			ImGui::BulletText("Colour: %d", game_choice.color);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Timers")) {
			Uint64 rawTicks = SDL_GetTicks64();
			ImGui::BulletText("Raw SDL ticks: %zu", rawTicks);
			ImGui::BulletText("thisTickCount: %zu", thisTickCount);
			ImGui::BulletText("lastTickCount: %zu", lastTickCount);
			ImGui::BulletText("pauseTickCount: %zu", pauseTickCount);
			ImGui::BulletText("FPS Final: %d", fps_final);
			ImGui::BulletText("base_timing: %.2f", base_timing);
			ImGui::BulletText("time_start: %zu", time_start);
			timeinfo = localtime(&time_start);
			tooltippy(asctime(timeinfo));

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Bow")) {
			ImGui::BulletText("active: %d", bow.active);
			ImGui::BulletText("time is %d", bow.time);
			ImGui::BulletText("script is %d", bow.script);
			ImGui::BulletText("hitme is %d", bow.hitme);
			ImGui::BulletText("last_power: %d", bow.last_power);
			ImGui::BulletText("wait: %zu", bow.wait);
			ImGui::BulletText("pull_wait: %zu", bow.pull_wait);

			ImGui::TreePop();
		}

		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
	}
    ImGui::End();
}


if (jukebox) {
	ImGui::Begin("Jukebox", &jukebox, ImGuiWindowFlags_AlwaysAutoResize);
	//info
	if (something_playing()) {
		ImGui::Text("Title: %s", tag_title);
		ImGui::Text("Artist: %s", tag_artist);
		ImGui::Text("Album: %s", tag_album);
		ImGui::Text("Duration: %1.2f seconds", mus_duration);
		//muspos = get_music_position();
		//ImGui::BeginDisabled();
		//ImGui::SliderScalar("##Position", ImGuiDataType_Double, &muspos, &zero, &mus_duration, "%.4f seconds");
		//ImGui::EndDisabled();
	}
		else {
			ImGui::Text("Nothing is playing at the moment");
		}
		//if (mus_type == tracker)
		//if (ImGui::SliderInt("Set tracker pattern", &modorder, 0, 128))
		//Mix_ModMusicJumpToOrder(modorder);
	//Main box
	if (ImGui::BeginListBox("##juke"))
	{
	for (int n = 0; n < jlist.Size; n++)
	{
		const bool is_selected = (juker == n);
		if (jlist[n] != NULL && ImGui::Selectable(jlist[n], is_selected))
		{
			juker = n;
			PlayMidi(jlist[n], 0);
		}

		if (is_selected)
            ImGui::SetItemDefaultFocus();
	}
	ImGui::EndListBox();
	}

	//if (ImGui::Button("Refresh list"))
	//	jukelist();

	ImGui::End();
}

if (moresfxinfo) {
	ImGui::Begin("Sound Effects", &moresfxinfo, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::BeginTabBar("Hello")) {
		if (ImGui::BeginTabItem("Sound slots")) {
			float *buf = gSoloud.getWave();
			ImGui::PlotLines("##Wave", buf, 256, 0, "Wave", -1, 1, ImVec2(-1.0, 80));
	if (ImGui::BeginTable("table9002", 4, ImGuiTableFlags_Borders)) {
		ImGui::TableSetupColumn("No.");
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Duration");
		ImGui::TableSetupColumn("Preview");
		ImGui::TableHeadersRow();

		for (int i = 0; i < MAX_SOUNDS; i++) {
			if (sound_slot_occupied(i)) {
				ImGui::TableNextColumn();
				ImGui::Text("%d", i);
				ImGui::TableNextColumn();
				ImGui::Text("%s", soundnames[i]);
				ImGui::TableNextColumn();
				ImGui::Text("%.2f seconds", sound_slot_duration(i));
				ImGui::TableNextColumn();
				ImGui::PushID(i * 100);
				if (ImGui::ArrowButton("##right", ImGuiDir_Right)) {
					sound_slot_preview(i);
					}
				ImGui::PopID();
				//ImGui::SameLine();
				//if (ImGui::Button("Stop")
			}
		}


		ImGui::EndTable();
	}
	ImGui::EndTabItem();
		}

	if (ImGui::BeginTabItem("Empty slots")) {
		ImGui::Text("The following sound slots are empty:");
		for (int i = 1; i < MAX_SOUNDS; i++) {
			if (!sound_slot_occupied(i)) {
				ImGui::Text("%d", i);
			}
		}
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
	}

	ImGui::End();
}

if (sfxinfo) {
    ImGui::Begin("Audio Settings", &sfxinfo, ImGuiWindowFlags_AlwaysAutoResize);
	dbg.sfxvol = gSoloud.getGlobalVolume();
	#ifdef SDL_MIXER_X
	dbg.musvol = Mix_GetVolumeMusicGeneral();
	#else
	dbg.musvol = Mix_VolumeMusic(-1);
	#endif
	//ImGui::Separator();
	//ImGui::SliderInt("Mod/XM order", &modorder, 0, 99);
	//ImGui::SameLine();
	//if (ImGui::Button("Set")) {
	//Mix_ModMusicJumpToOrder(modorder);
	//}
	ImGui::SeparatorText("Volume");
	if (ImGui::SliderFloat("SFX", &dbg.sfxvol, 0, 2.0f))
	gSoloud.setGlobalVolume(dbg.sfxvol);
	if (ImGui::SliderInt("Music", &dbg.musvol, 0, MIX_MAX_VOLUME)) {
	#ifdef SDL_MIXER_X
	Mix_VolumeMusicGeneral(dbg.musvol);
	#else
	Mix_VolumeMusic(dbg.musvol);
	#endif
	}

	//if (ImGui::SliderFloat("Bass boost", &mybassboost, 0.0f, 11.0f)) {
	//	sfx_set_bassboost(mybassboost);
	//}

	ImGui::SliderFloat("Speech", &dbg.speevol, 0.5f, 6.0f);
	ImGui::Checkbox("Speech synthesis narration of text", &dbg.debug_speechsynth);
	ImGui::Separator();
	if (ImGui::TreeNode("Background Music")) {
	//ImGui::SeparatorText("Background Music");
		//if (ImGui::InputText("Media file", midichlorian, IM_ARRAYSIZE(midichlorian), ImGuiInputTextFlags_EnterReturnsTrue)) {
		//PlayMidi(midichlorian);
		//memset(midichlorian, 0, 30);
		//modorder = 0;
		//}
		//ImGui::SameLine();
		//if (ImGui::SmallButton("Play")) {
		//	PlayMidi(midichlorian);
		//	memset(midichlorian, 0, 30);
		//}
		if (something_playing()) {
		ImGui::Text("Title: %s", tag_title);
		ImGui::Text("Artist: %s", tag_artist);
		ImGui::Text("Album: %s", tag_album);
		//ImGui::Text("Position: %1.2f / %1.2f", Mix_GetMusicPosition(NULL), Mix_MusicDuration(NULL));
		ImGui::Text("Duration: %1.2f seconds", mus_duration);
	}
		else {
			ImGui::Text("Nothing is playing at the moment");
		}
		if (mus_type == tracker)
		if (ImGui::SliderInt("Set pattern", &modorder, 0, 128))
			play_modorder(modorder);
	if (something_playing()) {
		if (ImGui::Button("Halt BGM"))
			StopMidi();

		//ImGui::SameLine();
	//if (ImGui::Button("Pause BGM"))
	//	PauseMidi();
	}
	//ImGui::SameLine();
	//if (ImGui::Button("Resume BGM"))
	//	ResumeMidi();

	ImGui::TreePop();
	}
	if (ImGui::TreeNode("Sound Effects")) {
	float *fft = gSoloud.calcFFT();
	if (dbg.devmode) {
	if (ImGui::TreeNode("Speech parameters")) {
		ImGui::SliderInt("Frequency", &spfreq, 100, 8000);
		ImGui::SliderInt("Declination", &spdec, 1, 10);
		ImGui::SliderInt("Speed", &spspeed, 1, 20);
		ImGui::SliderInt("Waveform", &spwave, 0, 6);
		if (ImGui::Button("Set parameters")) {
			speech.setParams(spfreq, spspeed, spdec, spwave);
		}
		ImGui::TreePop();
	}
	}
	//ImGui::Checkbox("Direction-based positional audio", &dbg.debug_audiopos);
	ImGui::Checkbox("Positional audio distance delay", &dbg.audiodelay);
	tooltippy("Attempts to simulate sounds at a distance. Must restart to take effect.");
	if (dbg.devmode) {
	if (ImGui::TreeNode("Distance attenuation parameters")) {
		ImGui::SliderFloat("Attenuation", &atten, 0.0001f, 2.0f);
		ImGui::SliderFloat("Distance Minimum", &dist_min, 0.01f, 50.0f);
		ImGui::SliderFloat("Distance Max", &dist_max, 1.0f, 700.0f);
		ImGui::SliderInt("Model", &dist_model, 0, 3);
		if (dist_model == 0) {
			ImGui::Text("No attenuation");
		} else if (dist_model == 1) {
			ImGui::Text("Inverse distance model");
		} else if (dist_model == 2) {
			ImGui::Text("Linear");
		}
		ImGui::TreePop();
	}
	}
	ImGui::PlotHistogram("##FFT", fft, 256 / 2, 0, "FFT", 0, 10, ImVec2(-1.0, 80), 8);
	//Progress bar
	sfx_used_bar();
	if (ImGui::Button("Halt all SFX")) {
		halt_all_sounds();
	}
	ImGui::TreePop();
	}
	//Only show in developer mode
	if (dbg.devmode) {
	if (ImGui::TreeNode("Audio decoders in use")) {
	if (Mix_GetNumMusicDecoders() == 0) {
		ImGui::Text("No music decoders enabled. Sound is probably switched off.");
	}
	for (int i=0; i < Mix_GetNumMusicDecoders(); i++) {
		ImGui::Text("%s ", Mix_GetMusicDecoder(i));
		ImGui::SameLine();
	}
	ImGui::TreePop();
	ImGui::Separator();
	}
	}
	//MixerX-specific stuff. Should probably make a define thing for standard mixer
	#ifdef SDL_MIXER_X
	if (ImGui::TreeNode("MixerX MIDI Settings")) {
	if (Mix_GetSoundFonts() != NULL) {
		ImGui::Text("SoundFont in use: %s", Mix_GetSoundFonts());
	}
	else {
	ImGui::InputText("SoundFont Path", sfbuf, IM_ARRAYSIZE(sfbuf));
	ImGui::SameLine();
	if (ImGui::SmallButton("Set"))
	{
	Mix_SetSoundFonts(sfbuf);
	}
	}
	ImGui::Separator();
	ImGui::Text("MIDI Output");
	//Get our MIDI player for the radio buttons
	int midiplayer = Mix_GetMidiPlayer();
	if (ImGui::RadioButton("ADLMIDI", midiplayer == MIDI_ADLMIDI))
		Mix_SetMidiPlayer(0);
	ImGui::SameLine();
	if (ImGui::RadioButton("OPNMIDI", midiplayer == MIDI_OPNMIDI))
		Mix_SetMidiPlayer(3);
	ImGui::SameLine();
	if (ImGui::RadioButton("FluidSynth", midiplayer == MIDI_Fluidsynth))
		Mix_SetMidiPlayer(4);
	ImGui::SameLine();
	if (ImGui::RadioButton("TiMidity", midiplayer == 2))
		Mix_SetMidiPlayer(2);
	//no system midi on lunix... or anywhere now
	//#ifndef __linux__
	//ImGui::SameLine();
	//if (ImGui::RadioButton("System", midiplayer == 1))
	//	Mix_SetMidiPlayer(1);
	//#endif
	ImGui::SameLine();
	if (ImGui::RadioButton("Any", midiplayer == MIDI_ANY))
		Mix_SetMidiPlayer(MIDI_ANY);
	ImGui::Separator();
	//and adl emu
	int adlemu = Mix_ADLMIDI_getEmulator();
	ImGui::Text("ADLMIDI Emulator");
	ImGui::PushID(423452);
	if (ImGui::RadioButton("Default", adlemu == ADLMIDI_OPL3_EMU_DEFAULT))
		Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_DEFAULT);
	ImGui::PopID();
	ImGui::SameLine();
	if (ImGui::RadioButton("Nuked 1.8", adlemu == ADLMIDI_OPL3_EMU_NUKED))
		Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_NUKED);
	ImGui::SameLine();
	if (ImGui::RadioButton("Nuked 1.7.4", adlemu == ADLMIDI_OPL3_EMU_NUKED_1_7_4))
		Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_NUKED_1_7_4);
	ImGui::SameLine();
	if (ImGui::RadioButton("DOSBox", adlemu == ADLMIDI_OPL3_EMU_DOSBOX))
		Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_DOSBOX);
	ImGui::SameLine();
	if (ImGui::RadioButton("Opal", adlemu == ADLMIDI_OPL3_EMU_OPAL))
		Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_OPAL);
	ImGui::SameLine();
	if (ImGui::RadioButton("Java", adlemu == ADLMIDI_OPL3_EMU_JAVA))
		Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_JAVA);
	ImGui::Separator();
	ImGui::Text("OPNMIDI Emulator");
	int opnemu = Mix_OPNMIDI_getEmulator();
	if (ImGui::RadioButton("Default", opnemu == OPNMIDI_OPN2_EMU_DEFAULT))
		Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_DEFAULT);
	ImGui::SameLine();
	if (ImGui::RadioButton("MAME OPN2", opnemu == OPNMIDI_OPN2_EMU_MAME_OPN2))
		Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_MAME_OPN2);
	ImGui::SameLine();
	if (ImGui::RadioButton("Nuked YM3438", opnemu == OPNMIDI_OPN2_EMU_NUKED_YM3438))
		Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_NUKED_YM3438);
	ImGui::SameLine();
	if (ImGui::RadioButton("GENS", opnemu == OPNMIDI_OPN2_EMU_GENS))
		Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_GENS);
	if (ImGui::RadioButton("YMFM OPN2", opnemu == OPNMIDI_OPN2_EMU_YMFM_OPN2))
		Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_YMFM_OPN2);
	ImGui::SameLine();
	if (ImGui::RadioButton("NP2", opnemu == OPNMIDI_OPN2_EMU_NP2))
		Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_NP2);
	ImGui::SameLine();
	if (ImGui::RadioButton("MAME OPNA", opnemu == OPNMIDI_OPN2_EMU_MAME_OPNA))
		Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_MAME_OPNA);
	ImGui::SameLine();
	if (ImGui::RadioButton("Nuked YM2612", opnemu == OPNMIDI_OPN2_EMU_NUKED_YM2612))
		Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_NUKED_YM2612);

	ImGui::TreePop();
	}
	#endif
	ImGui::End();
}

if (sysinfo) {
	ImGui::Begin("System Information", &sysinfo, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::BulletText("SDL Revision: %s", SDL_GetRevision());
	ImGui::BulletText("Dear ImGui Release: %s", IMGUI_VERSION);
	ImGui::BulletText("Platform: %s", SDL_GetPlatform());
	ImGui::BulletText("Video driver: %s", SDL_GetCurrentVideoDriver());
	ImGui::BulletText("Audio driver: %s", SDL_GetCurrentAudioDriver());
	//ImGui::Text("Mixer ver: %d", Mix_Linked_Version());
	ImGui::BulletText("System RAM: %iMiB", SDL_GetSystemRAM());
	if (truecolor)
	ImGui::BulletText("Truecolour 24-bit mode enabled");
	else
	ImGui::BulletText("256 colour-paletted 8-bit graphics mode in use");

	ImGui::SeparatorText("SoLoud SFX Output");
	ImGui::BulletText("Output backend is %s", gSoloud.getBackendString());
	ImGui::BulletText("Number of channels is %d", gSoloud.getBackendChannels());
	ImGui::BulletText("Samplerate of %dHz with buffer of %d", gSoloud.getBackendSamplerate(), gSoloud.getBackendBufferSize());
	ImGui::SeparatorText("SDL Mixer(X) Output");
	SDL_version compile_version;
	const SDL_version *link_version = Mix_Linked_Version();
	SDL_MIXER_VERSION(&compile_version);
	ImGui::BulletText("Running with revision: %d.%d.%d", link_version->major, link_version->minor, link_version->patch);
	ImGui::BulletText("Compiled with %d.%d.%d", compile_version.major, compile_version.minor, compile_version.patch);

	ImGui::Separator();

	if (ImGui::TreeNode("Optional Libraries")) {
	#ifdef IMGUI_ENABLE_FREETYPE_LUNASVG
	ImGui::BulletText("LunaSVG");
	#endif

	#ifdef IMGUI_ENABLE_FREETYPE_PLUTOSVG
	ImGui::BulletText("PlutoSVG");
	#endif

	#ifdef SDL_MIXER_X
	ImGui::BulletText("SDL Mixer X");
	#endif

	#ifdef HAVE_FONTCONFIG
	ImGui::BulletText("Fontconfig");
	#endif
	ImGui::TreePop();
	}
	//int w, h;
	//SDL_RenderGetLogicalSize(SDL_GetRenderer(g_display->window), &w, &h);
	//ImGui::BulletText("Renderer logical size %d by %d", w, h);
	//float sx, sy;
	//SDL_RenderGetScale(SDL_GetRenderer(g_display->window), &sx, &sy);
	//ImGui::BulletText("Renderer scale is %f by %f", sx, sy);
	if (ImGui::TreeNode("Paths")) {
	ImGui::BulletText("Name is: %s", paths_getdmodname());
	ImGui::BulletText("Location is: %s", paths_getdmoddir());
	ImGui::BulletText("Fallback is: %s", paths_getfallbackdir());
	ImGui::BulletText("Ancillary data is: %s", paths_getpkgdatadir());
	ImGui::TreePop();
	}

	if (ImGui::TreeNode("Maxima")) {
	ImGui::BulletText("Tiles: %d", GFX_TILES_NB_SQUARES);
	ImGui::BulletText("Sprites: %d", MAX_SPRITES);
	tooltippy("This is in dink.ini");
	ImGui::BulletText("Sequences: %d", MAX_SEQUENCES);
	ImGui::BulletText("Living sprites: %d", MAX_SPRITES_AT_ONCE);
	tooltippy("Per screen");
	ImGui::BulletText("Sound slots: %d", MAX_SOUNDS);
	ImGui::BulletText("Concurrent channels: %d", NUM_CHANNELS);
	tooltippy("SFX able to be played simultaneously");
	ImGui::BulletText("Scripts: %d", MAX_SCRIPTS);
	ImGui::BulletText("Callbacks: %d", MAX_CALLBACKS);
	tooltippy("Scripts able to be suspended with a wait");
	ImGui::TreePop();
	}
	ImGui::Spacing();
	if (ImGui::SmallButton("Okay"))
	sysinfo = false;

	ImGui::End();
}

if (metrics) {
	ImGui::Begin("Metrics", &metrics, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::BeginTabBar("mettabs")) {
		if (ImGui::BeginTabItem("Limits"))
		{
			var_used_bar();
			tooltippy("Variables");
			used_scripts_bar();
			tooltippy("Scripts");
			live_sprites_bar();
			tooltippy("Living sprites");
			sfx_used_bar();
			tooltippy("SFX");

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	ImGui::End();
}

if (mapsquares) {
	ImGui::Begin("Map Screens", &mapsquares, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::SliderInt("Map width", &map_width, 1, 768);
	ImGui::SeparatorText("Current Screen Data");
	ImGui::InputInt("MIDI Number", &g_dmod.map.music[*pplayer_map]);
	ImGui::InputInt("Screen reference (don't change this)", &g_dmod.map.loc[*pplayer_map]);
	ImGui::RadioButton("Outdoors", &g_dmod.map.indoor[*pplayer_map], 0);
	ImGui::SameLine();
	ImGui::RadioButton("Indoors", &g_dmod.map.indoor[*pplayer_map], 1);
	ImGui::SameLine();
	if (ImGui::Button("Save to Dink.dat")) {
		g_dmod.map.save();
	}
	ImGui::Separator();
	if (ImGui::BeginTable("table3", map_width, ImGuiTableFlags_Borders) && map_width < 512)
{
    for (int item = 1; item < 769; item++)
    {
        ImGui::TableNextColumn();
		if (g_dmod.map.loc[item] > 0)
		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImVec4(1.0f, 0.1f, 0.2f, 0.65f)));
		else {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImVec4(0.0f, 0.1f, 1.0f, 0.65f)));
		}
		if (item == *pplayer_map)
		ImGui::TextColored(ImVec4(0,1,0,1), "%i", item);
		else if (g_dmod.map.loc[item] == 0) {

			ImGui::PushID(item * 100000);
			if (ImGui::SmallButton("New")) {
				g_dmod.map.loc[item] = add_new_map();
				g_dmod.map.save();
			}
			ImGui::PopID();
		}
		else {
			ImGui::Text("%i", item);
		}
		if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Screen: %i (%i), Indoor: %i, Music: %i", item, g_dmod.map.loc[item],  g_dmod.map.indoor[item], g_dmod.map.music[item]);
    }
    ImGui::EndTable();
}
ImGui::End();
}

if (maptiles) {
	ImGui::Begin("Screen tiles", &maptiles, ImGuiWindowFlags_AlwaysAutoResize);
	//if (ImGui::SliderInt("Screen", &ts, 1, 41)) {
		//refresh_map_tiles();
	//}
	if (ImGui::Button("Reset all tiles")) {
		for (int i=0; i<96; i++)
            cur_ed_screen.t[i].square_full_idx0 = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset all alt-hard")) {
        for (int i=0; i<96; i++)
            cur_ed_screen.t[i].althard = 0;
    }
	ImGui::SameLine();
	ImGui::SliderInt("Tilescreen", &mytile, 1, GFX_TILES_NB_SETS);
	ImGui::SameLine();
	if (ImGui::Button("Paint")) {
		for (int i = 0; i < 96; i++)
			cur_ed_screen.t[i].square_full_idx0 = ((mytile - 1) * 128) + i;
	}
	ImGui::Separator();
	ImGui::Text("Visible tile assignation and alt-hard");
    if (ImGui::BeginTable("table15", 12, ImGuiTableFlags_Borders)) {
        for (int i=0; i < 96; i++) {
            ImGui::TableNextColumn();
			ImGui::PushItemWidth(50.0f);
			ImGui::PushID(i * 4);
			ImGui::DragScalar("##sneed", ImGuiDataType_S16, &cur_ed_screen.t[i].square_full_idx0, 0.2f, &zero, &tilemax);
            //ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "Index: %d", cur_ed_screen.t[i].square_full_idx0);
			ImGui::PopID();
			ImGui::PushID(i * 400);
			ImGui::DragScalar("##chuck", ImGuiDataType_S16, &cur_ed_screen.t[i].althard, 0.2f, &zero, &htilemax);
            //ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Alt: %d", cur_ed_screen.t[i].althard);
			ImGui::PopID();
			ImGui::PopItemWidth();
        }
		ImGui::EndTable();
	}
	if (ImGui::Button("Save changes to MAP.DAT")) {
			save_screen(g_dmod.map.map_dat.c_str(), g_dmod.map.loc[*pplayer_map]);
		}
	ImGui::End();
}

if (dinkini) {
	ImGui::Begin("Dink.ini Graphics", &dinkini);
	if (ImGui::BeginTabBar("dinkinitab")) {
		if (ImGui::BeginTabItem("Sequences")) {
		if (ImGui::Button("Edit Dink.ini in text editor")) {
		edit_dinkini();
	}
	ImGui::SameLine();
	ImGui::Checkbox("List unused sequence slots", &freeslots);
	ImGui::SameLine();
	ImGui::Checkbox("Show only loaded sequences", &zeroslots);
	ImGui::Separator();
	if (ImGui::BeginTable("table9", 3, ImGuiTableFlags_Borders)) {
		ImGui::TableSetupColumn("Number", ImGuiTableColumnFlags_WidthFixed, 35.1f);
		ImGui::TableSetupColumn("ini line or init command");
		ImGui::TableSetupColumn("Frames", ImGuiTableColumnFlags_WidthFixed, 35.1f);
		ImGui::TableHeadersRow();
		for (int i = 1; i < MAX_SEQUENCES; i++) {
			if ((!seq[i].is_active && freeslots) || (seq[i].len != 0 && zeroslots) || (seq[i].is_active && !zeroslots && !freeslots)) {
				ImGui::TableNextColumn();
				ImGui::Text("%i", i);
				ImGui::TableNextColumn();
				if (seq[i].ini != NULL)
				ImGui::Text("%s", seq[i].ini);
				else
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Empty");
				ImGui::TableNextColumn();
				ImGui::Text("%hi", seq[i].len);
			}

		}
		ImGui::EndTable();
	}
	ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Special")) {
			for (int i = 1; i < MAX_SEQUENCES; i++) {
				if (seq[i].is_active)
					for (int j = 0; j < MAX_FRAMES_PER_SEQUENCE; j++) {
						if (seq[i].special[j] > 0)
							ImGui::Text("Seq %d frame %u", i, seq[i].special[j]);
			}
		}
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
	}
	ImGui::End();
}

if (htiles)
{
    ImGui::Begin("Hard.dat Tile Viewer", &htiles, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::BeginTabBar("hdat"))
    {
        if (ImGui::BeginTabItem("Hard.dat squares"))
        {
            ImVec4 standard = ImVec4(0.97f, 0.86f, 0.38f, 1.0f);
            ImVec4 low = ImVec4(0.3f, 0.4f, 1.0f, 1.0f);
            ImVec4 fire = ImVec4(1.0f, 0.0f, 0.2f, 1.0f);
            ImVec4 col;
            ImGui::SliderInt("Tile", &htile, 0, 799);
            ImGui::BeginTable("table9", 50);
            int y = 0;
            for (y = 0; y < 50; y++)
            {
                int x = 0;
                for (x = 0; x < 50; x++)
                {
                    if (hmap.htile[htile].hm[x][y] == 1)
                        col = standard;
                    else if (hmap.htile[htile].hm[x][y] == 2)
                        col = low;
                    else if (hmap.htile[htile].hm[x][y] == 3)
                        col = fire;
                    if (hmap.htile[htile].hm[x][y] > 0)
                        ImGui::TextColored(col, "%d", hmap.htile[htile].hm[x][y]);
                    else
                        ImGui::Text("0");
                    ImGui::TableNextColumn();
                }
                ImGui::TableNextRow();
            }
            ImGui::EndTable();

			if (ImGui::Button("Remove tile data")) {
				memset(hmap.htile[htile].hm, 0, sizeof(hmap.htile[htile].hm));
				save_hard();
			}
			tooltippy("Blanks the tile, making it walkable");
			if (ImGui::Button("Remove all references to")) {
			for (int i = 0; i < 8000; i++) {
				if (hmap.btile_default[i] == htile)
					hmap.btile_default[i] = 0;
			}
			save_hard();
			}
			tooltippy("Does not remove manual stamping");

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Tile Index"))
        {
            ImGui::SetNextItemWidth(50);
            ImGui::DragInt("Select Tilescreen", &tindexscr, 0.2f, 1, GFX_TILES_NB_SETS);
            //ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "Warning: Will save to HARD.DAT");
            if (ImGui::Button("Clear indices from tilescreen"))
            {
                for (int i = 128 * (tindexscr - 1); i < 128 * tindexscr; i++)
                    hmap.btile_default[i] = 0;
                save_hard();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("For replacing an existing tilesheet");
            if (ImGui::Button("Reset bogus index values to zero"))
            {
                for (int i = 0; i < GFX_TILES_NB_SETS * 128; i++)
                {
                    if (hmap.btile_default[i] < 0 || hmap.btile_default[i] > 800)
                        hmap.btile_default[i] = 0;
                }
                save_hard();
            }
            if (ImGui::BeginTable("table9", 2, ImGuiTableRowFlags_Headers))
            {
                ImGui::TableSetupColumn("Screen Tile");
                ImGui::TableSetupColumn("Index");
                ImGui::TableHeadersRow();
                for (int i = (tindexscr - 1) * 128; i < ((tindexscr - 1) * 128) + 128; i++)
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", i);
                    ImGui::TableNextColumn();
                    ImGui::Text("%hd", hmap.btile_default[i]);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}

if (magicitems) {
	ImGui::Begin("Inventory", &magicitems, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("Green in the index column indicates the slot is in use");
	if (ImGui::BeginTabBar("magit")) {
	if (ImGui::BeginTabItem("Magic")) {
	float progress = (float)*pmagic_level / (float)*pmagic_cost;
	char buf[32];
	sprintf(buf, "%d/%d", *pmagic_level, *pmagic_cost);
	ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f), buf);
	tooltippy("Magic level/magic_cost");
		if (ImGui::Button("Kill current magic")) {
			kill_cur_magic();
			draw_status_all();
		}
		ImGui::Separator();
		//Table
		if (ImGui::BeginTable("magictable", 4, ImGuiTableRowFlags_Headers)) {
			ImGui::TableSetupColumn("Index");
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Sequence");
			ImGui::TableSetupColumn("Frame");
			ImGui::TableHeadersRow();

			for (int i = 1; i < NB_MITEMS + 1; i++) {
				ImGui::TableNextColumn();
				if (play.mitem[i].active)
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%d", i);
				else
				ImGui::Text("%d", i);
				ImGui::TableNextColumn();
				//make button for arming in used slots
				if (play.mitem[i].active) {
					ImGui::PushID(i * 5);
				if (ImGui::Button(play.mitem[i].name)) {
					if (*pcur_magic != 0) {
					//disarm old weapon
					scripting_run_proc(magic_script, "DISARM");
				}
				*pcur_magic = i;
				magic_script =
						scripting_load_script(play.mitem[*pcur_magic].name, 1000, 1);
				scripting_run_proc(magic_script, "ARM");
				scripting_run_proc(magic_script, "ARMMOVIE");
				draw_status_all();
				}
				ImGui::PopID();
				}
				else
				ImGui::Text("%s", play.mitem[i].name);
				ImGui::TableNextColumn();
				ImGui::Text("%d", play.mitem[i].seq);
				ImGui::TableNextColumn();
				ImGui::Text("%d", play.mitem[i].frame);
		}
		ImGui::EndTable();
		}
		ImGui::EndTabItem();
	}

	//Items
	if (ImGui::BeginTabItem("Items and/or Weapons")) {
		if (ImGui::Button("Kill current item/weapon")) {
			kill_cur_item();
			draw_status_all();
		}
		ImGui::Separator();
		//Table
		if (ImGui::BeginTable("itable", 4, ImGuiTableRowFlags_Headers)) {
			ImGui::TableSetupColumn("Index");
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Sequence");
			ImGui::TableSetupColumn("Frame");
			ImGui::TableHeadersRow();

			for (int i = 1; i < NB_ITEMS + 1; i++) {
				ImGui::TableNextColumn();
				if (play.item[i].active)
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%d", i);
				else
				ImGui::Text("%d", i);
				ImGui::TableNextColumn();
				if (play.item[i].active) {
					ImGui::PushID(10 * i);
					if (ImGui::Button(play.item[i].name)) {
						if (*pcur_weapon != 0) {
					//disarm old weapon
					scripting_run_proc(weapon_script, "DISARM");
				}

				*pcur_weapon = i;
				weapon_script =
						scripting_load_script(play.item[*pcur_weapon].name, 1000, 1);
				scripting_run_proc(magic_script, "ARM");
				scripting_run_proc(magic_script, "ARMMOVIE");
				draw_status_all();
					}
					ImGui::PopID();
				}
				else
				ImGui::Text("%s", play.item[i].name);
				ImGui::TableNextColumn();
				ImGui::Text("%d", play.item[i].seq);
				ImGui::TableNextColumn();
				ImGui::Text("%d", play.item[i].frame);

			}
			ImGui::EndTable();
		}
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
	}

	ImGui::End();
}

if (helpbox) {
	ImGui::Begin("A Brief Description of YeOldeDink", &helpbox);
	ImGui::TextWrapped("Welcome to YeOldeDink, a fork of Ghostknight's GNU Freedink fork with several bugfixes and enhancements. As a general reminder, most input boxes can be typed into by activating the console or by mousing over the relevant window, and slider/drag widgets can have their increment speed modified by holding either shift or alt while interacting.\n\n");
	ImGui::Spacing();
	ImGui::SeparatorText("File menu");
	ImGui::BulletText("Hide menubar and debug interface: This closes the ImGui interface and is equivalent to pressing Alt+D\n");
	ImGui::BulletText("Pause: This interrupts the execution of the engine until you choose to unpause\n");
	ImGui::BulletText("Restart: This will take you back to the title screen or similar\n");
	ImGui::BulletText("Quit: This will terminate the process and return you to your OS environment\n\n");
	ImGui::Spacing();
	ImGui::SeparatorText("Debug menu");
	ImGui::BulletText("Dev mode: switching this on makes visible many of the settings listed below.");
	ImGui::TextWrapped("- Info overlay: A transparent window that shows your FPS, along with the last debug log entry. The window may be moved to any corner of the screen or to a custom location by right-clicking. The DinkC console will appear here when activated.\n");
	ImGui::TextWrapped("- Game popout window: Dink within Dink. Useful if you want to go full-screen while using the debug log at the same time etc\n");
	ImGui::TextWrapped("- Ultimat(er) Cheat: Homage to the much-beloved script by Gary Hertel and Ted Shutes.\n");
	ImGui::TextWrapped("- Debug Log: See engine messages in real-time. Log level selects which messages are displayed, and some of the more useless log triggers have been switched off by default. The DinkC console will also appear in this window when enabled.\n");
	ImGui::TextWrapped("- DinkC Console: manually toggles the console. Equivalent to pressing Alt+C. It will also bypass keyboard input control for typing into text boxes etc\n");
	ImGui::TextWrapped("- Pause while console activated: The execution of the engine will be interrupted, and your DinkC command results apparent after deactivating the console while enabled\n");
	ImGui::TextWrapped("- Render game in main window: Toggles the display to the background. Designed for use with the popout window.\n");
	ImGui::TextWrapped("- Pink squares: Shows a pink hardbox overlay on touchable sprites, as well as bounding boxes for all sprites upon punching things.\n");
	ImGui::TextWrapped("- hard.dat overlay: Display tile and sprite collision for the screen without having to open DinkEdit.");
	//ImGui::TextWrapped("- Render tiles and background: Turns off the re-rendering of the background upon a screen change. Probably best to leave it on.\n");
	ImGui::TextWrapped("- Draw invisible sprites: Draws sprites that are either set to type 2, or are specified nodraw.\n");
	ImGui::TextWrapped("- Ignore sprite visions: Display all sprites on a screen regardless of which vision they're assigned.\n");
	//ImGui::TextWrapped("- Write to debug.txt: Chooses if messages are appended to debug.txt\n\n");
	ImGui::Spacing();
	ImGui::SeparatorText("Display menu");
	ImGui::BulletText("Texture filtering: Choose whether to filter the screen texture or use nearest-neighbour sampling");
	ImGui::BulletText("Ignore aspect ratio: fills the entire screen rather than using 4:3");
	ImGui::BulletText("Screen transition: Allows you to switch off the slide effect upon changing screens");
	ImGui::BulletText("Smooth shadows: A half-baked reimplementation of Redink1's Good Shadows. Doesn't work on Fastfiles.");
	ImGui::BulletText("Smooth fade: uses tweening algorithms to set the brightness during fades to look nicer");
	ImGui::BulletText("Set window to...: Changes the resolution in windowed mode.\n");
	ImGui::BulletText("Toggle full-screen display: Equivalent to pressing Alt+Enter.\n");
	ImGui::BulletText("Make window borderless: Removes the menu bar and outline provided by the operating system.");
	ImGui::Spacing();
	ImGui::SeparatorText("System menu");
	ImGui::BulletText("Gamepad and Touch: Alter your touch screen finger taps and check your controller works.\n");
	ImGui::BulletText("Audio: Adjusts a variety of sound-related settings such as volume and MIDI playback.\n");
	ImGui::BulletText("System information: Shows your linked SDL2 version along with the video/audio backend in use, plus paths.\n\n");
	ImGui::Spacing();
	ImGui::SeparatorText("Engine data menu");
	ImGui::TextWrapped("- Sprites: On the first tab, shows you active sprites on the current screen along with some parameters. The second tab will allow you to edit these parameters in real-time, and the third tab edits the sprite properties in MAP.DAT itself");
	ImGui::TextWrapped("- Variables: View and edit your local and global variables. Filter out globals with the checkbox.\n");
	ImGui::TextWrapped("- Scripts: Lists which scripts are running at the moment, along with which part of the file is being read.\n");
	ImGui::TextWrapped("- Editor Overrides: allows you to view and edit editor_frame/type/seq values on the current screen");
	ImGui::TextWrapped("- Map Index: An overview of which screens are in use, which screen you're on, with the ability to create new screens or edit MIDI/indoor. Mouse over to see the dink.dat screen number and the corresponding parameters");
	ImGui::TextWrapped("- Dink.ini: See which sequences are in use, and which are empty. Note that the list may not be entirely accurate due to SET_FRAME_FRAME commands etc.");
	ImGui::TextWrapped("- Colour palette: see the current palette in use and select a colour to fill the screen with. You may also edit individual colours on the second tab.\n\n");
	ImGui::Spacing();
	ImGui::SeparatorText("Speed");
	ImGui::BulletText("Normal speed: Equivalent to releasing the Tab key outside of debug mode after having held it down.\n");
	ImGui::BulletText("High speed: This will run the engine at twice the usual speed until chosen otherwise.");
	ImGui::BulletText("Slow speed: This runs the engine at half the normal speed. Unavailable outside of debug mode.");
	ImGui::Spacing();
	ImGui::SeparatorText("Quick save/load");
	ImGui::BulletText("QuickSave #Whatever: Saves the game into very high-numbered slots");
	ImGui::BulletText("Load: Reads a save file from a corresponding slot. Not available in mouse mode.\n");
	ImGui::Spacing();
	ImGui::SeparatorText("Other stuff");
	ImGui::BulletText("PNG support for both sprites and tiles in 24-bit mode");
	ImGui::BulletText("MP3 support for 1.08 Aural+ enjoyers, as well as FLAC and Opus (and SPC/VGZ) with playmidi()");
	ImGui::BulletText("MP3, Ogg, and FLAC sound effects with playsound()");
	//ImGui::BulletText("44.1kHz audio playback!");
	ImGui::BulletText("Pressing Tab in mouse mode will centre the cursor on the screen");
	ImGui::BulletText("Alt+U will unfreeze sprite 1 (player/mouse)");
	ImGui::BulletText("Controller disconnects will be autodetected, as will reconnects");
	ImGui::BulletText("DinkC: get_client_fork() returns 3");
	ImGui::BulletText("DinkC: get_platform() will return a value consistent with RTDink");
	ImGui::BulletText("DinkC: play_mod_order(int) will set the pattern for tracker music");
	ImGui::BulletText("DinkC: playavi(video.flv) will play back a video using ffplay. Requires FFmpeg to be installed.");
	ImGui::End();
}

if (aboutbox) {
	ImGui::Begin("About", &aboutbox, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::InvisibleButton("sneed", ImVec2(1.0f, 1.0f));
	ImGui::SameLine();
	ImGui::Text("YeOldeDink %s © 2022-2025 YeOldeToast", VERSION);
	ImGui::InvisibleButton("feed", ImVec2(1.0f, 1.0f));
	ImGui::SameLine();
	ImGui::Text("Based upon GNU Freedink by Beuc, Freedink-Lua by Phoenix (Alexander Krivács Schrøder), with improvements by Ghostknight and Seseler");
	ImGui::InvisibleButton("chuck", ImVec2(1.0f, 1.0f));
	ImGui::SameLine();
	ImGui::Text("Special thanks to drone1400, TealCool, Wojak, RobJ, Seseler, and Phoenix.");
	ImGui::Spacing();
	ImGui::TextWrapped("Uses Dear ImGui by Omar Cornut, Backward-cpp by bombela, Random for Modern C++ by effalkronium, Indicators and Glob by p-ranav, simpleini by brofield, Tweeny by Mobius3, Tinyfiledialogs by Guillaume Vareille, filesystem by gulrak, parallel-hashmap by greg7mdp, Lua5.4, luabridge3, and Cereal");
	ImGui::SeparatorText("Licenses");
	if (ImGui::TreeNode("GNU General Public License Version 3")) {
	ImGui::TextWrapped(
	"YeOldeDink is free software: you can redistribute it and/or modify "
    "it under the terms of the GNU General Public License as published by "
    "the Free Software Foundation, either version 3 of the License, or "
    "(at your option) any later version.");
	ImGui::Spacing();
	ImGui::TextWrapped(
		"YeOldeDink is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the "
    "GNU General Public License for more details.");
	ImGui::Spacing();
	ImGui::TextWrapped(
		"You should have received a copy of the GNU General Public License "
		"along with this program. If not, see <http://www.gnu.org/licenses/>.");
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("MIT License")) {
	ImGui::TextWrapped(
"Copyright (c) 2014-2024 Omar Cornut, Copyright (c) 1994–2019 Lua.org, PUC-Rio, Copyright (c) 2020-2022 Eduardo Bart (https://github.com/edubart), Copyright (c) 2019 Pranav, © 2017-2023 effolkronium (Illia Polishchuk), (c) 2006-2022 Brodie Thiesfield, 2013 Google Inc. All Rights Reserved.\n\n"
"Permission is hereby granted, free of charge, to any person obtaining a copy "
"of this software and associated documentation files (the \"Software\"), to deal "
"in the Software without restriction, including without limitation the rights "
"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
" copies of the Software, and to permit persons to whom the Software is "
"furnished to do so, subject to the following conditions:\n\n "
"The above copyright notice and this permission notice shall be included in all "
"copies or substantial portions of the Software.\n\n"
"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
"SOFTWARE.");
ImGui::TreePop();
	}
//ImGui::Spacing();
if (ImGui::TreeNode("BSD")) {
ImGui::TextWrapped(
	"SoLoud audio engine \n"
"Copyright (c) 2013-2018 Jari Komppa \n"
"Copyright (c) 2013-2022, Randolph Voorhies, Shane Grant\n\n"

"This software is provided 'as-is', without any express or implied "
"warranty. In no event will the authors be held liable for any damages "
"arising from the use of this software. \n\n"

"Permission is granted to anyone to use this software for any purpose, "
"including commercial applications, and to alter it and redistribute it "
"freely, subject to the following restrictions: \n\n"

"   1. The origin of this software must not be misrepresented; you must not  "
"claim that you wrote the original software. If you use this software "
"in a product, an acknowledgment in the product documentation would be "
"appreciated but is not required.\n\n"

"   2. Altered source versions must be plainly marked as such, and must not be "
"   misrepresented as being the original software. \n\n"

"   3. This notice may not be removed or altered from any source"
" distribution."
);
ImGui::TreePop();
}
if (ImGui::TreeNode("Apache 2.0")) {
	ImGui::TextWrapped(
	"Licensed under the Apache License, Version 2.0 (the \"License\");"
   "you may not use this file except in compliance with the License."
  " You may obtain a copy of the License at\n\n"
      " https://www.apache.org/licenses/LICENSE-2.0\n\n"
   "Unless required by applicable law or agreed to in writing, software"
   "distributed under the License is distributed on an \"AS IS\" BASIS,"
   "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."
  " See the License for the specific language governing permissions and"
   "limitations under the License."
	);

	ImGui::TreePop();
}
if (ImGui::TreeNode("RTSoft")) {
	ImGui::TextWrapped(
	"The Dink HD source code license (only applies to Seth's source code, not the media or third party/additional libraries used like DirectX)\n\n"

			"Copyright (C) 1997-2017 Seth A. Robinson All rights reserved.\n\n"

			"Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicence, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\n"

			"1. Redistributions of source code must retain the above copyright notice, this list of conditions, and the following disclaimer.\n\n"

			"2. Any software using source code from this distribution must contain the text \"This product includes software developed by Seth A. Robinson ( www.rtsoft.com )\" inside the app in an easily accessible position and sufficiently legible font.\n\n"

			"3. Except as contained in this notice, the name of Seth A. Robinson shall not be used in advertising or otherwise to promote the sale, use or other dealings in this Software without prior authorization from Seth A. Robinson.\n\n"

			"4. The name \"Dink Smallwood\" and \"Dink Smallwood HD\" are copyrighted by Robinson Technologies Corporation and shall not be used in advertising or otherwise to promote the sale, use or other dealings in this Software without prior authorization from Seth A. Robinson.\n\n"

			"5. Media such as graphics/audio are not included in this license.  Please ask Seth if you'd like to distribute Dink HD specific media in a port.\n\n"

			"THIS SOFTWARE IS PROVIDED 'AS IS' AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SETH A. ROBINSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
	);
	ImGui::TreePop();
}
if (ImGui::SmallButton("Okay")) {
	aboutbox = false;
	play_sneedle();
	}
	ImGui::End();
}

if (luaviewer) {
	ImGui::Begin("DinkLua Variables", &luaviewer);


	ImGui::End();
}

if (sproverride) {
	ImGui::Begin("Save data", &sproverride);
	if (ImGui::BeginTabBar("saveme")) {

	if (ImGui::BeginTabItem("Sprite data overrides")) {
	time_t ct;
	time(&ct);
	if (mode < 3)
	ImGui::BeginDisabled();
	ImGui::BulletText("Game timer: %f min", play.minutes + (difftime(ct, time_start) / 60));
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset"))
		play.minutes = 0;
	ImGui::BulletText("Screen timer: %" SDL_PRIs64 "ms", play.spmap[*pplayer_map].last_time);
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset"))
		play.spmap[*pplayer_map].last_time = 0;
	ImGui::InputText("Save slot string", save_game_info, sizeof(save_game_info));
	ImGui::Spacing();
	ImGui::SeparatorText("Editor Sprite Overrides");
	ImGui::Text("Selected sprite: %d", sproversel);
	ImGui::SameLine();
	if (ImGui::Button("Reset this sprite's override values to zero")) {
		play.spmap[*pplayer_map].type[sproversel] = 0;
		play.spmap[*pplayer_map].frame[sproversel] = 0;
		play.spmap[*pplayer_map].seq[sproversel] = 0;
	}

	ImGui::PushItemWidth(30.0f);
	ImGui::InputScalar("Type", ImGuiDataType_U8, &play.spmap[*pplayer_map].type[sproversel]);
	ImGui::InputScalar("Frame", ImGuiDataType_U8, &play.spmap[*pplayer_map].frame[sproversel]);
	ImGui::InputScalar("Sequence", ImGuiDataType_U16, &play.spmap[*pplayer_map].seq[sproversel]);
	ImGui::PopItemWidth();
	ImGui::Separator();
	ImGui::Checkbox("Show only modified", &sprovermod);
	ImGui::SameLine();
		if (ImGui::Button("Reset all values to zero")) {
		for (int j=1; j<100; j++) {
			play.spmap[*pplayer_map].type[j] = 0;
			play.spmap[*pplayer_map].seq[j] = 0;
			play.spmap[*pplayer_map].frame[j] = 0;
		}
	}
	if (ImGui::BeginTable("table88", 4, ImGuiTableFlags_Borders)) {
		ImGui::TableSetupColumn("Sprite");
		ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Frame");
		ImGui::TableSetupColumn("Sequence");
		ImGui::TableHeadersRow();
		for (int i=1; i<100; i++) {
			if ((sprovermod && play.spmap[*pplayer_map].type[i] != 0) || !sprovermod) {
			ImGui::TableNextColumn();
			ImGui::PushID(i);
			if (ImGui::Button("Select")) {
				sproversel = i;
			}
			ImGui::PopID();
			ImGui::SameLine();
			ImGui::Text("%d", i);
			ImGui::TableNextColumn();
			if (play.spmap[*pplayer_map].type[i] == 1)
			ImGui::Text("1: Killed completely");
			else if (play.spmap[*pplayer_map].type[i] == 2)
			ImGui::Text("2: Drawn nohard");
			else if (play.spmap[*pplayer_map].type[i] == 3)
			ImGui::Text("3: Background nohard");
			else if (play.spmap[*pplayer_map].type[i] == 4)
			ImGui::Text("4: Drawn hard");
			else if (play.spmap[*pplayer_map].type[i] == 5)
			ImGui::Text("5: Background hard");
			else if (play.spmap[*pplayer_map].type[i] == 6)
			ImGui::Text("6: Returns in 5 minutes");
			else if (play.spmap[*pplayer_map].type[i] == 7)
			ImGui::Text("7: Returns in 3 minutes");
			else if (play.spmap[*pplayer_map].type[i] == 8)
			ImGui::Text("8: Returns in 1 minute");
			else
			ImGui::Text("%d", play.spmap[*pplayer_map].type[i]);
			ImGui::TableNextColumn();
			ImGui::Text("%d", play.spmap[*pplayer_map].frame[i]);
			ImGui::TableNextColumn();
			ImGui::Text("%d", play.spmap[*pplayer_map].seq[i]);
			if (i == sproversel)
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.7f, 0.3f, 0.3f, 0.65f)));
			}
		}

		ImGui::EndTable();
	}
	if (mode < 3)
		ImGui::EndDisabled();

	ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Global function")) {
		if (ImGui::Button("Reset all global functions"))
			memset(play.func, 0, sizeof(play.func));

		if (ImGui::BeginTable("functable", 2, ImGuiTableFlags_Borders)) {
			ImGui::TableSetupColumn("Filename");
			ImGui::TableSetupColumn("Function");
			ImGui::TableHeadersRow();

			for (int i = 0; i < 100; i++) {
				ImGui::TableNextColumn();
				ImGui::Text("%s", play.func[i].file);
				ImGui::TableNextColumn();
				ImGui::Text("%s", play.func[i].func);
			}
			ImGui::EndTable();
		}
	ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Tilescreens")) {
		if (ImGui::BeginTable("tilesaves", 1, ImGuiTableFlags_Borders)) {
		for (int i = 1; i < 42; i++) {
			ImGui::Text("%s", play.tile[i].file);
		}
		ImGui::EndTable();
		}

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Other variables")) {
		ImGui::BulletText("Palette: %s", play.palette);
		ImGui::BulletText("last_map: %d", play.last_map);
		ImGui::BulletText("push_dir: %d", play.push_dir);

		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
	}

	ImGui::End();
}

if (paledit) {
	ImGui::Begin("Colour Palette", &paledit, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::BeginTabBar("colpal")) {
	if (ImGui::BeginTabItem("View")) {
		if (ImGui::InputText("Load palette from BMP", bmppal, IM_ARRAYSIZE(bmppal), ImGuiInputTextFlags_EnterReturnsTrue)) {
			gfx_palette_set_from_bmp(bmppal);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Set")) {
			gfx_palette_set_from_bmp(bmppal);
			bmppal[100] = {0};
		}
	ImGui::Text("Click on a colour to fill the background layer with it, equivalent to fill_screen()");
	SDL_Color palette[256];
	gfx_palette_get_phys(palette);
	//ImVec4 col = {palette[4].r / 255.0f, (float)palette[4].g / 255.0f, (float)palette[4].b / 255.0f, 1.0f};
	//ImGui::ColorPicker4("Edit", (float*)&col);
	for (int i=0; i< 256; i++) {
		ImGui::PushID(i * 1000000);
		if (ImGui::ColorButton("##col", ImVec4((float)palette[i].r / 255.0f, (float)palette[i].g / 255.0f, (float)palette[i].b / 255.0f, 1.0f), ImGuiColorEditFlags_NoAlpha)) {
		IOGFX_background->fill_screen(i, GFX_ref_pal);
		palcol = i;
		}
		ImGui::PopID();
		if ((i == 0) || ((i+1) % 32 != 0))
		ImGui::SameLine();
	}
	ImGui::Separator();
	ImGui::Text("Palette index number: %d", palcol);
	ImGui::SameLine();
	ImGui::ColorButton("Selected Palette Colour",ImVec4((float)palette[palcol].r / 255.0f, (float)palette[palcol].g / 255.0f, (float)palette[palcol].b / 255.0f, 1.0f));
	if (ImGui::Button("Copy fill_screen() command to clipboard")) {
		ImGui::LogToClipboard();
		ImGui::LogText("fill_screen(%d)", palcol);
		ImGui::LogFinish();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset background and status bar")) {
	draw_screen_game_background();
	draw_status_all();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset palette")) {
		//gfx_palette_reset();
		gfx_palette_set_from_bmp("TILES/SPLASH.BMP");
	}
	ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Edit")) {
		if (ImGui::Button("Copy from screen palette")) {
			memcpy(mypal, GFX_ref_pal, sizeof(mypal));
		}
		for (int i=0; i< 256; i++) {
			ImGui::PushID(i * 1000);
		if (ImGui::ColorButton("##col", ImVec4((float)mypal[i].r / 255.0f, (float)mypal[i].g / 255.0f, (float)mypal[i].b / 255.0f, 1.0f), ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel)) {
		//IOGFX_background->fill_screen(i, mypal);
		mycol = i;
		mypalcol = ImVec4((float)mypal[mycol].r / 255.0f, (float)mypal[mycol].g / 255.0f, (float)mypal[mycol].b / 255.0f, (float)mypal[mycol].a / 255.0f);
		}
		ImGui::PopID();
		if ((i == 0) || ((i+1) % 32 != 0))
		ImGui::SameLine();
	}
	//ImGui::Separator();
	//ImGui::ColorButton("Selected Palette Colour",ImVec4((float)mypal[mycol].r / 255.0f, (float)mypal[mycol].g / 255.0f, (float)mypal[mycol].b / 255.0f, (float)mypal[mycol].a / 255.0f));
	ImGui::Spacing();
	ImGui::SeparatorText("Palette operations");
	//ImGui::Checkbox("Auto-apply", &autopal);
	if (ImGui::Button("Convert to greyscale")) {
		for (int c = 0; c < 256; c++) {
			color::rgb<std::uint8_t> c1( {mypal[c].r, mypal[c].g, mypal[c].b} );
			color::gray<std::uint8_t> c2;
			c2 = c1;
			mypal[c].r = ::color::get::red(c2);
			mypal[c].g = ::color::get::green(c2);
			mypal[c].b = ::color::get::blue(c2);
	}
	}
	ImGui::SameLine();
	if (ImGui::Button("Invert")) {
		for (int c = 0; c < 256; c++) {
			color::rgb<std::uint8_t> c1( {mypal[c].r, mypal[c].g, mypal[c].b} );
			::color::operation::invert(c1);
			mypal[c].r = ::color::get::red(c1);
			mypal[c].g = ::color::get::green(c1);
			mypal[c].b = ::color::get::blue(c1);
		}
	}
	ImGui::Spacing();
	ImGui::Text("HSV/L/I of all colours");
	ImGui::Text("H: ");
	ImGui::SameLine();
	if (ImGui::ArrowButton("hue-", ImGuiDir_Left)) {
		changehsv(0, -1, 0);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("hue+", ImGuiDir_Right)) {
		changehsv(0, 1, 0);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	ImGui::Text("S: ");
	ImGui::SameLine();
	if (ImGui::ArrowButton("S-", ImGuiDir_Left)) {
		changehsv(1, -1, 0);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("S+", ImGuiDir_Right)) {
		changehsv(1, 1, 0);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	ImGui::Text("V: ");
	ImGui::SameLine();
	if (ImGui::ArrowButton("V-", ImGuiDir_Left)) {
		changehsv(2, -1, 0);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("V+", ImGuiDir_Right)) {
		changehsv(2, 1, 0);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	ImGui::Text("L: ");
	ImGui::SameLine();
	if (ImGui::ArrowButton("L-", ImGuiDir_Left)) {
		changehsv(2, -1, 1);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("L+", ImGuiDir_Right)) {
		changehsv(2, 1, 1);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	ImGui::Text("I: ");
	ImGui::SameLine();
	if (ImGui::ArrowButton("I-", ImGuiDir_Left)) {
		changehsv(2, -1, 2);
		gfx_palette_set_phys(mypal);
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("I+", ImGuiDir_Right)) {
		changehsv(2, 1, 2);
		gfx_palette_set_phys(mypal);
	}
	//Picker
	if (ImGui::TreeNode("Colour Picker")) {
		if (ImGui::ColorPicker4("Edit", (float*)&mypalcol)) {
			mypal[mycol].r = mypalcol.x * 255;
			mypal[mycol].g = mypalcol.y * 255;
			mypal[mycol].b = mypalcol.z * 255;
			mypal[mycol].a = mypalcol.w * 255;
	}
	ImGui::TreePop();
	}
	if (ImGui::Button("Apply palette to screen")) {
		gfx_palette_set_phys(mypal);
		//mypalcol = ImVec4((float)mypal[mycol].r / 255.0f, (float)mypal[mycol].g / 255.0f, (float)mypal[mycol].b / 255.0f, (float)mypal[mycol].a / 255.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Copy JASC PAL palette to clipboard")) {
		ImGui::LogToClipboard();
		ImGui::LogText("JASC-PAL\n");
		ImGui::LogText("0100\n");
		ImGui::LogText("256\n");
		for (int i = 0; i < 256; i++) {
			ImGui::LogText("%d %d %d\n", mypal[i].r, mypal[i].g, mypal[i].b);
		}
		ImGui::LogFinish();
	}

	ImGui::EndTabItem();
	}


	ImGui::EndTabBar();
	}
	ImGui::End();
}

if (iniconf) {
ImGui::Begin("yedink.ini file settings", &iniconf, ImGuiWindowFlags_AlwaysAutoResize);
if (ImGui::Button("Load config file data")) {
	CSimpleIniA ini;
	ini.SetUnicode();

	SI_Error rc = ini.LoadFile(paths_pkgdatafile("yedink.ini"));
	if (rc < 0) {
		log_warn("📋 Failed to open yedink.ini");
	} else {
		log_info("📋 Opened the ini file");
	}

	dispfont = ini.GetValue("fonts", "display");
	debugfont = ini.GetValue("fonts", "debugttf");
	strcpy(dispfon, dispfont);
	strcpy(debugfon, debugfont);
	debug_fontsize = atoi(ini.GetValue("fonts", "debug_pt_size", "172"));
	window_w = atoi(ini.GetValue("display", "window_w", "640"));
	window_h = atoi(ini.GetValue("display", "window_h", "480"));
	dispfilt = ini.GetBoolValue("display", "filtering", true);
	dispstretch = ini.GetBoolValue("display", "ignore_ratio", false);
	audio_samplerate = atoi(ini.GetValue("audio", "sameplerate", "48000"));
	audiobuf = atoi(ini.GetValue("audio", "buffer", "1024"));
	audiovol = atoi(ini.GetValue("audio", "music_volume", "128"));
	sfx_vol = atoi(ini.GetValue("audio", "sfx_volume", "128"));
	dbgtxtwrite = ini.GetBoolValue("debug", "write_debugtxt", false);
	debug_conpause = ini.GetBoolValue("debug", "console_pause", false);
}
ImGui::TextWrapped("These settings will only be apparent after saving and restarting");
ImGui::SeparatorText("Fonts");
ImGui::TextWrapped("The display font may either be a TTF file as part of resources, or something that Fontconfig can find. The debug interface font can only be a resources TTF file.");
ImGui::InputText("Display font", dispfon, IM_ARRAYSIZE(dispfon));
ImGui::InputText("Debug interface font", debugfon, IM_ARRAYSIZE(debugfon));
ImGui::SliderInt("Debug font size", &debug_fontsize, 8, 72);
ImGui::Spacing();
ImGui::SeparatorText("Display");
if (ImGui::SmallButton("640x480")) {
	window_w = 640;
	window_h = 480;
}
ImGui::SameLine();
if (ImGui::SmallButton("1280x960")) {
	window_w = 1280;
	window_h = 960;
}
ImGui::Text("CTRL+Click to type in your own values");
ImGui::DragInt("Window width", &window_w);
ImGui::DragInt("Window height", &window_h);
ImGui::Checkbox("Texture filtering", &dbg.debug_interp);
ImGui::Checkbox("Stretch beyond 4:3 aspect ratio", &dbg.debug_assratio);
ImGui::Spacing();
ImGui::SeparatorText("Audio");
if (ImGui::SmallButton("44.1Khz"))
audio_samplerate = 44100;
ImGui::SameLine();
if (ImGui::SmallButton("48kHz"))
audio_samplerate = 48000;
ImGui::SameLine();
if (ImGui::SmallButton("96kHz"))
audio_samplerate = 96000;
ImGui::Text("Set this to your sound card's actual rate for best performance");
ImGui::InputInt("Sampling rate", &audio_samplerate);
//ImGui::SliderInt("Buffer", &audiobuf, 32, 8192);
//ImGui::SliderInt("Music volume", &muvol, 1, 128);
//ImGui::SliderInt("SFX volume", &sfx_vol, 1, 128);
ImGui::Spacing();
ImGui::SeparatorText("Debug options");
ImGui::Checkbox("Show transparent info window", &dswitch);
//ImGui::Checkbox("Write to debug.txt", &dbg.debug_conwrite);
//ImGui::Checkbox("Pause while console active", &debug_conpause);
ImGui::Separator();

if (ImGui::Button("Save configuration to yedink.ini")) {
	CSimpleIniA ini;
	SI_Error rc = ini.LoadFile(paths_pkgdatafile("yedink.ini"));
	if (rc < 0) {
		log_warn("Failed to open yedink.ini");
	} else {
		log_info("Opened the ini file");
	}
	rc = ini.SetValue("fonts", "display", dispfon);
	rc = ini.SetValue("fonts", "debugttf", debugfon);
	char str[40];
	sprintf(str, "%d", debug_fontsize);
	rc = ini.SetValue("fonts", "debug_pt_size", str);
	sprintf(str, "%d", window_w);
	rc = ini.SetValue("display", "window_w", str);
	sprintf(str, "%d", window_h);
	rc = ini.SetValue("display", "window_h", str);
	rc = ini.SetBoolValue("display", "filtering", dbg.debug_interp);
	rc = ini.SetBoolValue("display", "ignore_ratio", debug_assratio);
	rc = ini.SetBoolValue("debug", "info_window", dswitch);
	rc = ini.SetBoolValue("debug", "write_debugtxt", dbg.debug_conwrite);
	sprintf(str, "%d", audio_samplerate);
	rc = ini.SetValue("audio", "samplerate", str);
	//sprintf(str, "%d", muvol);
	rc = ini.SetValue("audio", "music_volume", str);
	rc = ini.SaveFile(paths_pkgdatafile("yedink.ini"));
}
//if (ImGui::Button("Play video file")) {
//	unsigned int n = std::thread::hardware_concurrency();
//    log_info("%d Threads are supported", n);
//}
ImGui::End();
}

if (boxprev) {
	ImGui::Begin("Box previewer", &boxprev, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Checkbox("Preview bounds", &bprevview);
	ImGui::InputDouble("Left", &bprev.X.Min, 0, 640.0f);
	ImGui::InputDouble("Top", &bprev.Y.Min), 0, 400.0f;
	ImGui::InputDouble("Right", &bprev.X.Max);
	ImGui::InputDouble("Bottom", &bprev.Y.Max);
	if (ImPlot::BeginPlot("Box alignment", ImVec2(640, 480), ImPlotFlags_NoLegend)) {
		ImPlot::SetupAxesLimits(0, xmax, 0, ymax);
		ImPlot::SetupAxis(ImAxis_Y1, NULL, ImPlotAxisFlags_Invert | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
		ImPlot::SetupAxis(ImAxis_X1, NULL, ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_NoTickLabels);
		ImPlot::DragRect(0, &bprev.X.Min, &bprev.Y.Min, &bprev.X.Max, &bprev.Y.Max,ImVec4(1,0,1,1), ImPlotDragToolFlags_Delayed);

		ImPlot::EndPlot();
	}
	if (ImGui::Button("Copy inside_box() parameters to clipboard")) {
		ImGui::LogToClipboard();
		ImGui::LogText("%d, %d, %d, %d", bprev.X.Min, bprev.Y.Min, bprev.X.Max, bprev.Y.Max);
		ImGui::LogFinish();
	}

	ImGui::End();
}

if (callbinfo) {
	ImGui::Begin("Callbacks", &callbinfo, ImGuiWindowFlags_AlwaysAutoResize);
	//ImGui::Checkbox("Alternate callback system", &callbacksystem_new);
	//tooltippy("Allows for infinite slots");
	if (!callbacksystem_new) {
	ImGui::Checkbox("Show currently unused slots", &callbactive);
	if (ImGui::BeginTable("table43", 7, ImGuiTableFlags_Borders)) {
		ImGui::TableSetupColumn("No.");
		ImGui::TableSetupColumn("Owner");
		tooltippy("Script number");
		ImGui::TableSetupColumn("Active");
		//ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Name");
		//ImGui::TableSetupColumn("Offset");
		ImGui::TableSetupColumn("Min");
		ImGui::TableSetupColumn("Max");
		//ImGui::TableSetupColumn("Life");
		ImGui::TableSetupColumn("Timer");
		ImGui::TableHeadersRow();

		for (int i=0; i < MAX_CALLBACKS; i++) {
			if (callback[i].active || callbactive) {
			ImGui::TableNextColumn();
			ImGui::Text("%d", i);
			ImGui::SameLine();
			ImGui::PushID(i);
			if (ImGui::SmallButton("Kill"))
				scripting_kill_callback(i);
			ImGui::PopID();
			ImGui::TableNextColumn();
			ImGui::Text("%d", callback[i].owner);
			ImGui::TableNextColumn();
			ImGui::Text("%d", callback[i].active);
			ImGui::TableNextColumn();
			//ImGui::Text("%d", callback[i].type);
			//ImGui::TableNextColumn();
			ImGui::Text("%s", callback[i].name);
			ImGui::TableNextColumn();
			//ImGui::Text("%d", callback[i].offset);
			//ImGui::TableNextColumn();
			ImGui::Text("%ld", callback[i].min);
			ImGui::TableNextColumn();
			ImGui::Text("%ld", callback[i].max);
			ImGui::TableNextColumn();
			//ImGui::Text("%d", callback[i].lifespan);
			//ImGui::TableNextColumn();
			ImGui::Text("%ld", callback[i].timer);
		}
		}
		ImGui::EndTable();
	}
	}
	else {

	if (ImGui::BeginTable("cbacktable", 6, ImGuiTableFlags_Borders)) {
		ImGui::TableSetupColumn("ID");
		ImGui::TableSetupColumn("Owner script");
		ImGui::TableSetupColumn("Proc Name");
		ImGui::TableSetupColumn("Minimum time");
		ImGui::TableSetupColumn("Maximum");
		ImGui::TableSetupColumn("Timer expiry");
		ImGui::TableHeadersRow();

		auto view = callbackreg.view<ball_cack, callback_timer, callback_id>();
		view.each([](const auto entity, const auto &cb, auto &tim, auto &id) {
			ImGui::TableNextColumn();
			ImGui::Text("%d", id.ident);
			ImGui::TableNextColumn();
			ImGui::Text("%d", cb.owner);
			ImGui::TableNextColumn();
			ImGui::Text("%s", cb.name.c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%ld", cb.min);
			ImGui::TableNextColumn();
			ImGui::Text("%ld", cb.max);
			ImGui::TableNextColumn();
			ImGui::Text("%d", tim.timer);
		});

	ImGui::EndTable();
	}
	}

	ImGui::End();
}

if (dinkcinfo) {
	ImGui::Begin("DinkC Bindings", &dinkcinfo, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("There are %zu current DinkC bindings", dinkc_bindings.size());
	ImGui::TextWrapped("For parameters -1 means none at all, 0 means none for this slot, 1 means integer and 2 is string.");
	ImGui::Separator();
	if (ImGui::BeginTable("Table96", 8, ImGuiTableFlags_Borders)) {
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Param1");
		ImGui::TableSetupColumn("Param2");
		ImGui::TableSetupColumn("Param3");
		ImGui::TableSetupColumn("Param4");
		ImGui::TableSetupColumn("Param5");
		ImGui::TableSetupColumn("Param6");
		ImGui::TableSetupColumn("Bad Parameter Behaviour");
		ImGui::TableHeadersRow();
		for (const auto& binding: dinkc_bindings) {
			ImGui::TableNextColumn();
			ImGui::Text("%s", binding.first.c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%d", binding.second.params[0]);
			ImGui::TableNextColumn();
			ImGui::Text("%d", binding.second.params[1]);
			ImGui::TableNextColumn();
			ImGui::Text("%d", binding.second.params[2]);
			ImGui::TableNextColumn();
			ImGui::Text("%d", binding.second.params[3]);
			ImGui::TableNextColumn();
			ImGui::Text("%d", binding.second.params[4]);
			ImGui::TableNextColumn();
			ImGui::Text("%d", binding.second.params[5]);
			ImGui::TableNextColumn();
			if (binding.second.badparams_dcps == 0)
				ImGui::Text("Next line");
			else if (binding.second.badparams_dcps == 1)
				ImGui::Text("Continue");
			else if (binding.second.badparams_dcps == 2)
				ImGui::Text("Yield");
			else
				ImGui::Text("Doelse Once");
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

if (luainfo) {
	ImGui::Begin("DinkLua Bindings", &luainfo, ImGuiWindowFlags_AlwaysAutoResize);
	sol::state_view lua(luaVM);
	if (ImGui::BeginTabBar("Luatabs")) {
		if (ImGui::BeginTabItem("Bound functions")) {
			int bcount = lua.script("count = 0; for _ in pairs(dink) do count = count + 1 end; return count");
			ImGui::Text("There are %d functions bound to the 'dink' table", bcount);
			ImGui::Separator();
			sol::table bindings = lua["dink"];
			for (const auto& i: bindings) {
				auto name = i.first.as<std::optional<std::string>>();
				ImGui::Text("%s", name->c_str());
			}
			ImGui::EndTabItem();
		}

	if (ImGui::BeginTabItem("Volatile Table")) {
		//TODO: make a better property viewer
		int tcount = lua.script("count = 0; for _ in pairs(volatile) do count = count + 1 end; return count");
		ImGui::Text("There are %d entries in the volatile table", tcount);
		ImGui::Separator();
		sol::table entries = lua["volatile"];
		for (const auto& i: entries) {
			auto name = i.first.as<std::optional<std::string>>();
			ImGui::Text("%s", name->c_str());
		}
	ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}
	ImGui::End();
}

/*
ImGui::Begin("Extra testing stuff");
if (ImGui::Button("Set volume stream to 50%"))
	set_music_vol(64);
if (ImGui::Button("Set volume stream to 25"))
	set_music_vol(32);
if (ImGui::Button("Set volume stream to full"))
	set_music_vol(128);
ImGui::SliderInt("Model", &ts, 0, 11);
if (ImGui::Button("ADLMIDI Volume model 1"))
	Mix_ADLMIDI_setVolumeModel(ts);
#ifdef __EMSCRIPTEN__
if (ImGui::Button("Save file as base64")) {
	emscripten_run_script("OnSavegamesExport()");
}
#endif
if (ImGui::Button("Decompress"))
	bxz::ifstream mytar("sneed.tar", bxz::bz2);
ImGui::End();
/*
if (ImGui::Button("Pathfind")) {
	AStar::PathFinder mypathfinder;
	std::vector<uint8_t> myhardmap;
	int x1, y1;
	for (y1 = 0; y1 < 400; y1++)
		for (x1 = 0; x1 < 600; x1++) {
			if (screen_hitmap[x1][y1] == 0) {
				//free
				myhardmap.push_back(255);
			}
			else {
				//obstacle
				myhardmap.push_back(0);
			}
		}

		AStar::Coord2D startPos(spr[1].x, spr[1].y);
		AStar::Coord2D endPos(100, 100);
	mypathfinder.setWorldData(600, 399, myhardmap.data());
	auto path = mypathfinder.findPath(startPos, endPos);
	mypathfinder.exportPPM("test.ppm", &path);
	for (const auto& i : path) {
		log_info("coords %d %d", i.x, i.y);
		ImGui::Text("%d, %d", i.x, i.y);
	}
}

if (ImGui::Button("JSON Dink.dat output with modern json")) {
	json j;
	//dinkdat
	for (int i = 1; i <= 768; i++)
		j["screen"].push_back(g_dmod.map.loc[i]);

	for (int i = 1; i <= 768; i++)
		j["music"].push_back(g_dmod.map.music[i]);

	for (int i = 1; i <= 768; i++)
		j["indoor"].push_back(g_dmod.map.indoor[i]);

	std::ofstream os(paths_dmodfile("dinkdat.json"));
	os << std::setw(4) << j << std::endl;
}

if (ImGui::Button("JSON Savegame")) {
	save_game_json(1);
}

if (ImGui::Button("Export map.dat to json")) {
	json j;

	for (int i = 1; i<= 768; i++)
		if (g_dmod.map.loc[i] > 0) {
			//sprites
			for (int h1 = 1; h1 <= MAX_SPRITES_EDITOR; h1++) {

			}
		}
	std::ofstream os(paths_dmodfile("mapdat.json"));
	os << std::setw(4) << j << std::endl;
}

for (int j = 0; j < last_sprite_created; j++) {
	if (spr[j].que != 0)
		ImGui::Text("Sprite %d with cue %d", j, spr[j].que);
	else
		ImGui::Text("Sprite %d with Y co-ord %d", j, spr[j].y);
}

ImGui::SliderInt("Map width in screens", &map_width, 1, 100);
ImGui::End();

/*
ImGui::Begin("Sprite Ranking");

int rankings[MAX_SPRITES_AT_ONCE];
memset(rankings, 0, MAX_SPRITES_AT_ONCE * sizeof(int));
bool already_checked[MAX_SPRITES_AT_ONCE + 1];
memset(already_checked, 0, sizeof(already_checked));

for (int r1 = 0; r1 < last_sprite_created; r1++) {
		rankings[r1] = 0;

		int highest_sprite = 22000; //more than it could ever be

		for (int h1 = 1; h1 <= last_sprite_created; h1++) {
			if (!already_checked[h1] && spr[h1].active) {
				int height;
				if (spr[h1].que != 0)
					height = spr[h1].que;
				else
					height = spr[h1].y;

				if (height < highest_sprite) {
					highest_sprite = height;
					rankings[r1] = h1;
				}
			}
		}

		if (rankings[r1] != 0)
			already_checked[rankings[r1]] = true;
		ImGui::Text("Rank of index %d is sprite %d", r1, rankings[r1]);
	}
	ImGui::Separator();
	//New averaging system
	std::multimap<int, int> myrankings;
	int newrankings[MAX_SPRITES_AT_ONCE];
	memset(newrankings, 0, MAX_SPRITES_AT_ONCE * sizeof(int));
	//get akll sprites and use their Y if no que
	for (int i=0; i <= last_sprite_created; i++) {
			if (spr[i].que != 0) {
				myrankings.insert(std::make_pair(spr[i].que, i));
			}
			else {
				myrankings.insert(std::make_pair(spr[i].y, i));
			}
	}
	int kk = 0;
	for (auto const& entry: myrankings) {
		newrankings[kk] = entry.second;
		ImGui::Text("Rank of index %d is sprite %d", entry.first, entry.second);
		kk++;
	}
	ImGui::Separator();
	for (int i = 1; i <= last_sprite_created; i++) {
		ImGui::Text("Rank of index %d is sprite %d", i, newrankings[i]);
	}

ImGui::End();

}


ImGui::Begin("Fades");
if (ImGui::Button("Fade down")) {
	process_downcycle = 1;
	cycle_clock = game_GetTicks() + 4000;
	//CyclePalette();
}
if (ImGui::Button("Fade up")) {
	process_upcycle = 1;
	//up_cycle();
}
ImGui::Text("Downcycle: %d, Upcycle %d", process_downcycle, process_upcycle);
ImGui::Text("Brightness: %f", g_display->brightness);
ImGui::End();
*/

//Alternate text box drawing - doewsn't work with damage yet

//Stuff that runs each frame that isn't an imgui window
//To make it pause when console is shown and relevant setting is on
if (console_active == 0 && debug_conpause) {
	debug_paused = 0;
}
//Stolen from freedinkedit, toggled in display options, requested by drone1400. Runs really slow in GL mode
if (drawhard && !show_inventory) {
    int x1, y1;
	for (x1 = 0; x1 < 600; x1++)
		for (y1 = 0; y1 < 400; y1++) {
			if (screen_hitmap[x1][y1] == 1) {
				{
					SDL_Rect GFX_box_crap;
					GFX_box_crap.x = x1 + playl;
					GFX_box_crap.y = y1;
					GFX_box_crap.w = 1;
					GFX_box_crap.h = 1;
					IOGFX_backbuffer->fillRect(&GFX_box_crap, GFX_ref_pal[1].r,
											GFX_ref_pal[1].g,
											GFX_ref_pal[1].b);
				}
			}

			if (screen_hitmap[x1][y1] == 2) {
				{
					SDL_Rect GFX_box_crap;
					GFX_box_crap.x = x1 + playl;
					GFX_box_crap.y = y1;
					GFX_box_crap.w = 1;
					GFX_box_crap.h = 1;
					IOGFX_backbuffer->fillRect(
							&GFX_box_crap, GFX_ref_pal[128].r,
							GFX_ref_pal[128].g, GFX_ref_pal[128].b);
				}
			}

			if (screen_hitmap[x1][y1] == 3) {
				{
					SDL_Rect GFX_box_crap;
					GFX_box_crap.x = x1 + playl;
					GFX_box_crap.y = y1;
					GFX_box_crap.w = 1;
					GFX_box_crap.h = 1;
					IOGFX_backbuffer->fillRect(&GFX_box_crap, GFX_ref_pal[45].r,
											GFX_ref_pal[45].g,
											GFX_ref_pal[45].b);
				}
			}

			if (screen_hitmap[x1][y1] > 100) {

				if (cur_ed_screen.sprite[(screen_hitmap[x1][y1]) - 100]
							.is_warp == 1) {
					{
						SDL_Rect GFX_box_crap;
						GFX_box_crap.x = x1 + playl;
						GFX_box_crap.y = y1;
						GFX_box_crap.w = 1;
						GFX_box_crap.h = 1;
						IOGFX_backbuffer->fillRect(
								&GFX_box_crap, GFX_ref_pal[20].r,
								GFX_ref_pal[20].g, GFX_ref_pal[20].b);
					}
				} else {
					{
						SDL_Rect GFX_box_crap;
						GFX_box_crap.x = x1 + playl;
						GFX_box_crap.y = y1;
						GFX_box_crap.w = 1;
						GFX_box_crap.h = 1;
						IOGFX_backbuffer->fillRect(
								&GFX_box_crap, GFX_ref_pal[23].r,
								GFX_ref_pal[23].g, GFX_ref_pal[23].b);
					}
				}
			}
		}

}
//Draws a box around the sprite bounds so you can see which one you're editing. Toggled in the live sprite editor. Doesn't work too great
if (spr[spriedit].active == 1 && (sprinfo || spreditors)) {
if (spoutline) {
	rect box;
	rect_copy(&box, &k[getpic(spriedit)].box);
	//Now uses imgui windows rather than SDL itself
	//TODO: make it use SDL again
	ImGui::SetNextWindowPos(ImVec2((box.left + spr[spriedit].x - k[getpic(spriedit)].xoffset) * (g_display->w / 640), (spr[spriedit].y - k[getpic(spriedit)].yoffset) * (g_display->h / 480)));
	ImGui::SetNextWindowSize(ImVec2(box.right * (g_display->w / 640.0f), box.bottom * (g_display->h / 480.0f)));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.2f, 0.2f, 0.2f));
	ImGui::Begin("##spriteoutline", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize);
	ImGui::PopStyleColor(2);
	ImGui::End();
	//SDL_Rect r = {box.left + spr[spriedit].x - k[getpic(spriedit)].xoffset, box.top + spr[spriedit].y - k[getpic(spriedit)].yoffset, box.right - box.left,
	//								box.bottom - box.top};
	//					IOGFX_backbuffer->fillRect(&r, 255, 100, 0);
}
//hardbox preview for the sprite editor
if (hboxprev) {
	hboxprevoverlay(spriedit);
}

if (ddotprev)
	xyoffsetprev(spriedit);
}
//Depth dot
if (debug_boxanddot)
	for (int i = 1; i <= last_sprite_created; i++) {
		hboxprevoverlay(i);
		xyoffsetprev(i);
	}
//turns on info window when console activated
if (console_active && !dbg.dswitch && !dbg.logwindow)
	dbg.dswitch = 1;
        }
}

void alttext() {
		for (int j = 0; j <= last_sprite_created; j++) {
		if (spr[j].active && spr[j].brain == 8 && spr[j].damage == -1) {
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImVec2 window_pos, window_pivot;
			std::string textwidth = spr[j].text;
			int mytextwidth = textwidth.length();
			ImGui::SetNextWindowBgAlpha(0.5f);
			if (spr[j].text_owner != 1000) {
				ImGui::SetNextWindowSizeConstraints(ImVec2(110 * (viewport->WorkSize.x / 640.0f), 25 * (viewport->WorkSize.y / 480.0f)), ImVec2(viewport->WorkSize.x - spr[j].x, FLT_MAX));
				window_pos.x = (spr[j].x + 20) * (viewport->WorkSize.x / 640.0f);
				window_pos.y = (spr[j].y) * (viewport->WorkSize.y / 480.0f);
				//log_debug("Window pos X is %.2f and y is %.2f", window_pos.x, window_pos.y);
				ImGui::SetNextWindowPos(window_pos);
			}
			else {
				//say_xy
				window_pivot.x = 0.0f;
				window_pivot.y = 0.0f;
				ImGui::SetNextWindowSizeConstraints(ImVec2((mytextwidth * 7) * (viewport->WorkSize.x / 640.0f), 20 * (viewport->WorkSize.y / 480.0f)), ImVec2(viewport->WorkSize.x, FLT_MAX));
				ImGui::SetNextWindowPos(ImVec2((spr[j].x + 240) * (viewport->WorkSize.x / 640.0f), spr[j].y * (viewport->WorkSize.y / 480.0f)), ImGuiCond_Always, window_pivot);
			}
			//Text colouring
			char crap[512];
			int color = 14;

		//ye: could probably be optmized
		sprintf(crap, "%s", spr[j].text);
		std::string mytext = crap;
		//bool is for extra modes
		std::vector<std::tuple<std::string, SDL_Color, bool>> mytextmap;
		//scan through the entire text for backticks
		auto tick = mytext.find("`");
		//we found one, let's split
		if (tick != std::string::npos) {
			for (auto i = strtok(&mytext[0], "`"); i != NULL; i = strtok(NULL, "`")) {
				std::string colcode(1, i[0]);
				log_error("colcode %c len of %s is %d", i[0], strlen(i), i);
				bool extra = false;
				//get first character after code
				if (strlen(i) < 4) {
					std::string p(1, i[2]);
					log_error("p is %s", p.c_str());
					if (p == " ")
						extra = true;
				}

				try {
					color = textcolmap.at(colcode);
					//help grey text permaybe
					if (color == 8)
						ImGui::SetNextWindowBgAlpha(1.0f);
					//check previous one
						if (!mytextmap.empty()) {
							if (std::get<2>(mytextmap.back()) == true) {
								mytextmap.push_back({&i[1], {font_extra_colors[color].r, font_extra_colors[color].g, font_extra_colors[color].b}, extra});
							}
							else {
								mytextmap.push_back({&i[1], {font_colors[color].r, font_colors[color].g, font_colors[color].b}, extra});
							}
						}
						else {
							mytextmap.push_back({&i[1], {font_colors[color].r, font_colors[color].g, font_colors[color].b}, extra});
						}
				}
				catch (const std::exception &e) {
					//if (textcolmap2.find(colcode) != textcolmap2.end())
						//color = textcolmap2.at(colcode);
					mytextmap.push_back({&i[0], {font_colors[color].r, font_colors[color].g, font_colors[color].b}, false});
					log_debug("Chance `Zands strikes again!");
				}

			}

		} else {
			//no ticks found, use yellow (14)
			mytextmap.push_back({mytext, {font_colors[14].r, font_colors[14].g, font_colors[14].b}, false});
		}

		//debugging window
		//ImGui::Begin("Alttext debug", NULL, ImGuiWindowFlags_AlwaysAutoResize);
		for (const auto& textline: mytextmap) {
			//ImGui::Text("%s; colcode %d; extra %d", std::get<0>(textline).c_str(), std::get<1>(textline).r, std::get<2>(textline));
		}
		//ImGui::End();
			if (!showb.active && !show_inventory) {
			ImGui::Begin(spr[j].text, NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
			ImGui::SetWindowFontScale(1.0f * (viewport->WorkSize.x / 640.0f));
			for ( const auto& textline: mytextmap) {
				//if it's blank we'll skip, and use the next colour code if there is one
			char frst = std::get<0>(textline)[0];
			log_error("first is %c", frst);
			if (frst != ' ') {
			ImVec4 tcol = ImVec4(std::get<1>(textline).r / 255.0f, std::get<1>(textline).g / 255.0f, std::get<1>(textline).b / 255.0f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, tcol);
			ImGui::AlignTextToFramePadding();
			ImGui::TextWrapped("%s", std::get<0>(textline).c_str());
			ImGui::PopStyleColor();
			}
			//else {

			//}
			}
			ImGui::End();
			}
		}
	}
}
