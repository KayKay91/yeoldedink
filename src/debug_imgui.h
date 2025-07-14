#pragma once

#include "imgui.h"
//Called from IOGfxDisplaySW/GL2
extern void dbg_imgui();
//Ran at start to update jukebox list
extern void jukelist();
//Important for testing audio
extern void play_sneedle();
extern bool get_cheat();
//imgui info window, for showing console outside debug mode
extern void infowindow();
extern void alttext();
extern void dinktheme();
//For the debug log
extern ImGuiTextBuffer Buf, errBuf, dbgBuf, infBuf, traceBuf;
extern Uint32 debug_engine_cycles;
extern bool debug_autoscroll[5];
//For the tile toggle and info window
extern bool render_tiles, drawhard, debug_hardenedmode, debug_tileanims, debug_divequals, debug_alttextlock, debug_debugpause;
//saves
extern bool debug_savestates;
//For the pink squares, sprites, and mem info, and so on and so on *sniffs*
extern bool debug_meminfo, debug_pinksquares, debug_drawsprites, debug_drawgame, debug_gamewindow, debug_paused, debug_conpause, debug_infhp, debug_magcharge, debug_missilesquares, debug_hitboxsquares;
extern bool debug_invspri, debug_noclip, debug_vision, debug_screenlock, debug_badpic, debug_conwrite, debug_assratio, debug_walkoff, debug_ranksprites, debug_pausetag, debug_pausehit, debug_pausehits;
extern bool debug_talksquares, debug_pausetalk, debug_hitboxfix, debug_talkboxexp, debug_framepause, debug_pausemissile, debug_pausemissilehit, debug_funcfix, debug_pushsquares, debug_pausepush, debug_idledirs;
extern bool debug_errorpause, debug_alttext, debug_extratextcols, debug_fullfade, debug_maxdam, debug_scriptkill, debug_overignore;
extern int debug_fontsize, debug_base_timing, debug_authorkey;
extern int midiplayer, spriedit;
//Ini stuff
extern const char* dispfont;
extern char debugfon[200];
extern char debug_editor[20];
extern ImFont* confont;
//TODO: get rid of most of the above
