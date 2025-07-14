#include <SDL.h>

#ifndef DEBUG_H
#define DEBUG_H

extern int fps;

struct debugsettings {
    bool drawhard;
    bool dswitch;
    bool logwindow;
    bool debug_meminfo, debug_pinksquares, debug_drawsprites, debug_drawgame,
        debug_gamewindow, debug_paused, debug_conpause, debug_scripboot,
        debug_interp, debug_infhp, debug_magcharge;
    bool debug_invspri, debug_noclip, debug_vision, debug_screenlock,
        debug_badpic, debug_conwrite, debug_assratio, debug_walkoff;
    bool screentrans, debug_audiopos, debug_speechsynth, debug_returnint,
        debug_coltext, debug_warpfade, debug_goodshadows, dshowfps,
        dshowinfo, dshowlog, dshowclock, germantext, dtradinfo;
    bool devmode, pause_on_console_show, progressbars, savejson;
    int corner = 1;
    bool altfade = true;
    int musvol = 100;
    float sfxvol = 1.0;
    float speevol, dopacity;
    bool audiodelay = true;
    bool jrumble;
    // for the colour shader
    bool shader_colour, shader_colcycle;
    float shader_r = 1.0f, shader_g = 1.0f, shader_b = 1.0f;
    bool framelimit, autosave, showplswait;
    int autosaveslot;
    // Touch stuff
    bool logtouch, touchcont, fingswipe, tooltips;
    int touchfing[5];
    // Joystick fake button bind
    int fakekey, savetime, uistyle, joythresh;
    bool jquit, focuspause, lockspeed, punchedit, errorscript, varreplace = true,
        monsteredgecol, luaerrorpause, invexit, mouseloc, statlerp, warpfix,
        logautoscroll, clickmouse, gfxspeed, sfxspeed;
    int loglevel, transition_speed;
    // for retaining settings using cereal
    template <class Archive> void serialize(Archive &ar) {
        ar(drawhard, dswitch, logwindow, debug_meminfo, debug_pinksquares,
           debug_drawsprites, debug_drawgame, debug_gamewindow, debug_paused,
           debug_conpause, debug_scripboot, debug_interp, debug_infhp,
           debug_magcharge, debug_invspri, debug_noclip, debug_vision,
           debug_screenlock, debug_badpic, debug_conwrite, debug_assratio,
           debug_walkoff, screentrans, debug_audiopos, debug_speechsynth,
           debug_returnint, debug_coltext, debug_warpfade, debug_goodshadows,
           devmode, corner, altfade, musvol, sfxvol, speevol, audiodelay,
           jrumble, framelimit, autosave, logtouch, touchcont, transition_speed,
           touchfing, fakekey, jquit, loglevel, focuspause, lockspeed, tooltips,
           punchedit, errorscript, varreplace, monsteredgecol, savejson,
           savetime, uistyle, pause_on_console_show, dshowlog, dshowinfo, dtradinfo,
           dshowfps, dopacity, dshowclock, luaerrorpause, germantext, progressbars,
           autosaveslot, invexit, mouseloc, joythresh, statlerp, warpfix, logautoscroll, showplswait, clickmouse, gfxspeed, sfxspeed);
    }
};

extern debugsettings dbg;

extern void debug_logic();
extern void save_user_settings();
extern void load_user_settings();
extern void pause_everything();
extern bool game_paused();
extern void copy_to_clipboard(const char *text);
extern void set_window_res(int w, int h);
// imgui stuff
extern void load_console_font();
extern bool check_window_res(int w, int h);
extern void xyoffsetprev(int sprite);
extern void hboxprevoverlay(int sprite);
extern void edit_script(char *script);
extern void edit_dinkini();
#endif
