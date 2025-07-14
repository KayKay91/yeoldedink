/**
 * DinkC script engine

 * Copyright (C) 1997, 1998, 1999, 2002, 2003  Seth A. Robinson
 * Copyright (C) 2005, 2007, 2008, 2014, 2015  Sylvain Beucler

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

#ifndef _BGM_H
#define _BGM_H

#ifdef SDL_MIXER_X
#include <SDL2/SDL_mixer_ext.h>
#else
#include "SDL_mixer.h"
#endif

#include <string>

extern /*bool*/ int midi_active;

extern bool something_playing(void);
extern int PlayMidi(char* sFileName, int fadein);
extern int PauseMidi();
extern int ResumeMidi();
extern int StopMidi();
extern void check_midi();
extern void bgm_init(void);
extern void bgm_quit(void);
extern void loopmidi(int loop_midi);
extern int play_modorder(int order);
extern void set_music_tempo(double tempo);
extern double get_music_tempo();
extern void set_music_speed(double speed);
extern void set_music_pitch(double pitch);
extern void set_music_vol(int volume);
extern int get_music_vol();
extern void fade_music(int time);
extern double get_music_position();
extern void set_music_position(double pos);
extern void set_music_track(int track);
extern int get_music_tracks();
extern void load_music_soundfont(std::string filename);

extern void dinklua_object_playmidi(std::string bgm, int fadein);

extern const char *tag_title;
extern const char *tag_artist;
extern const char *tag_album;
extern double mus_duration;
extern float mytempo;
extern Mix_MusicType mus_type;
extern bool loop_midi;

#endif
