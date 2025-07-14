/**
 * Sound (not music) management

 * Copyright (C) 1997, 1998, 1999, 2002, 2003  Seth A. Robinson
 * Copyright (C) 2003  Shawn Betts
 * Copyright (C) 2005, 2007  Sylvain Beucler
 * Copyright (C) 2023-2024 Yeoldetoast

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

#ifndef _SFX_H
#define _SFX_H

#ifdef SDL_MIXER_X
#include <SDL2/SDL_mixer_ext.h>
#else
#include "SDL_mixer.h"
#endif
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_speech.h"

/* Sound metadata */
//ye: this is arbitrary
#define MAX_SOUNDS 400
//while this can be 4096 tops
#define NUM_CHANNELS 128

extern /*bool*/ int sound_on;
extern int buf_samples, audio_samplerate, hw_channels;
//For MixerX settings from ini
extern int adlemu, opnemu;
extern char soundfont[200];
//sfx
extern SoLoud::Soloud gSoloud;
extern SoLoud::Wav gWave;
extern SoLoud::Speech speech;
extern SoLoud::Bus gBus;
extern float sfx_bass_boost;

extern void sfx_init();
extern void sfx_quit();
extern int CreateBufferFromWaveFile(char* filename, int dwBuf);
extern int SoundPlayEffect(int sound, int min, int plus, int sound3d,
						/*bool*/ int repeat);
extern void EditorSoundPlayEffect(int sound);
extern int playing(int sound);
extern int SoundStopEffect(int sound);
extern void kill_repeat_sounds(void);
extern void kill_repeat_sounds_all(void);
extern void sfx_log_meminfo(void);
extern void update_sound(void);
extern void halt_all_sounds();
extern void sfx_set_bassboost(float boost);

/* DinkC procedures */
extern int playsound(int sound, int min, int plus, int sound3d, int repeat);
extern void sound_set_kill(int soundbank);
extern void sound_set_survive(int soundbank, int survive);
extern void sound_set_vol(int soundbank, int volume);
extern int playsfx(char* filename, int speed, int pan);

//Lua sound properties
extern void lua_set_vol(int soundbank, int volume);
extern int sound_get_vol(int soundbank);
extern int sound_get_overall_vol(int soundbank);
extern bool sound_get_survive(int soundbank);
extern void sound_set_pan(int soundbank, int pan);
extern int sound_get_pan(int soundbank);
extern void sound_set_hz(int soundbank, int hz);
extern int sound_get_hz(int soundbank);
extern int sound_get_speed(int soundbank);
extern void sound_set_speed(int soundbank, int speed);
extern void sound_set_pause(int soundbank, bool paused);
extern bool sound_get_pause(int soundbank);
extern void sound_set_loop_point(int soundbank, int looppoint);
extern int sound_get_loop_point(int soundbank);
extern int sound_get_loop_count(int soundbank);
extern int sound_get_pos(int soundbank);
extern void sound_set_looping(int soundbank, bool repeat);
extern bool sound_get_looping(int soundbank);
//Extra Lua functions for sounds
extern void sound_fade_vol(int soundbank, int vol, int time);
extern void sound_fade_pan(int soundbank, int pan, int time);
extern void sound_fade_speed(int soundbank, int speed, int time);
extern void sound_schedule_pause(int soundbank, int time);
extern void sound_schedule_stop(int soundbank, int time);
extern void sound_oscillate_vol(int soundbank, int from, int to, int time);
extern void sound_oscillate_pan(int soundbank, int from, int to, int time);
extern void sound_oscillate_speed(int soundbank, int from, int to, int time);

//sfx 3d sound proeprties
extern float atten;
extern float dist_min;
extern float dist_max;
extern int dist_model;

//test, sounds horrid
extern void play_midi_as_sfx(char* file, char* sf2);

//debug mode
extern bool sound_slot_occupied(int slot);
extern double sound_slot_duration(int slot);
extern void sound_slot_preview(int slot);
extern char* soundnames[MAX_SOUNDS];
extern void pause_sfx();

//hard-coded sfx playback
extern void sfx_stop_sprite_sound(int sprite);
extern void sfx_play_mouse();
extern void sfx_play_duck(int size, int sprite);
extern int sfx_play_pig(int size, int sprite);
extern void sfx_play_warp();

extern void sfx_play_hit_thwack();

extern void sfx_play_choice_scroll();
extern void sfx_play_choice_select();

extern void sfx_play_sprite_sound(int sound, int sprite, int loop);

extern void sfx_play_inventory_move();
extern void sfx_play_inventory_open();
extern void sfx_play_inventory_exit();
extern void sfx_play_inventory_select();

extern void sfx_play_status_gold();
extern void sfx_play_status_stats();
extern void sfx_play_status_exp_tick();
extern void sfx_play_status_exp_count();

extern void sfx_play_player_hurt();

//default sample rate for sprite sounds
extern int sfx_spritehz;
extern int sfx_ducksound;
extern int sfx_duckhz;
extern int sfx_hurtsound;
extern int sfx_hurthz;
extern int sfx_inventorysound;
extern int sfx_inventoryhz;
extern int sfx_thwacksound;
extern int sfx_thwackhz;
extern int sfx_mousesound;
extern int sfx_mousehz;
extern int sfx_pigsound;
extern int sfx_pighz;
extern int sfx_choicescrollsound;
extern int sfx_choicescrollhz;
extern int sfx_choiceselectsound;
extern int sfx_choiceselecthz;
extern int sfx_inventoryopensound;
extern int sfx_inventoryopenhz;
extern int sfx_inventorymovesound;
extern int sfx_inventorymovehz;
extern int sfx_inventoryselectsound;
extern int sfx_inventoryselecthz;
extern int sfx_inventoryexitsound;
extern int sfx_inventoryexithz;
extern int sfx_statusstatsound;
extern int sfx_statusstathz;
extern int sfx_statusgoldsound;
extern int sfx_statusgoldhz;
extern int sfx_statusexpticksound;
extern int sfx_statusexptickhz;
extern int sfx_statusexpcountsound;
extern int sfx_statusexpcounthz;
extern int sfx_warpsound;
extern int sfx_warphz;



#endif
