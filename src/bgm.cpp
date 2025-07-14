/**
 * Background music (currently .midi's and audio CDs)

 * Copyright (C) 1997, 1998, 1999, 2002, 2003  Seth A. Robinson
 * Copyright (C) 2005, 2007, 2008, 2009, 2010, 2014, 2015  Sylvain Beucler
 * Copyright (C) 2022-2024 Yeoldetoast, Seseler

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

/* CD functions */
#include "SDL.h"
/* MIDI functions */
#if defined SDL_MIXER_X && !defined DINKEDIT
#include <SDL2/SDL_mixer_ext.h>
#include "dinklua.h"
#else
#include "SDL_mixer.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */
#include <optional>

#include "bgm.h"
#include "io_util.h"
#include "str_util.h"
#include "game_engine.h"
#include "paths.h"
#include "log.h"
#include "sfx.h" /* sound_on */

/* Current background music (not cd) */
static Mix_Music* music_data = NULL;
//for crossfading
static Mix_Music* music_data2 = NULL;
static char* last_midi = NULL;
bool loop_midi = false;
int midi_active = 1;
float mytempo = 1.0;

const char *tag_title = NULL;
const char *tag_album = NULL;
const char *tag_artist = NULL;
double mus_duration;
Mix_MusicType mus_type;

/*
 * MIDI functions
 */

/**
 * Returns whether the background music is currently playing
 */
bool something_playing() {
	#ifdef SDL_MIXER_X
	return (bool)Mix_PlayingMusicStream(music_data);
	#else
	return (bool)Mix_PlayingMusic();
	#endif
}

/**
 * Clean-up music when it's finished or manually halted
 */
static void callback_HookMusicFinished() {
	if (music_data != NULL)
		Mix_FreeMusic(music_data);
}

/**
 * Thing to play the midi
 */
int PlayMidi(char* midi_filename, int fadein) {
	char relpath[256];
	char* fullpath = NULL;

	/* no midi stuff right now */
	if (sound_on == /*false*/ 0)
		return 1;

	/* Do nothing if the same midi is already playing */
	/* TODO: Does not differentiate midi and ./midi, qsf\\midi and
qsf/midi... Ok, midi is supposed to be just a number, but..*/
	//ye: I don't know what the above means
	if (last_midi != NULL && compare(last_midi, midi_filename) &&
		something_playing()) {
		log_info("🤔 I think %s is already playing, I should skip it...", midi_filename);
		return 0;
	}

	// Attempt to play .ogg in addition to .mid, if playing a ".*\.mid$"
	// yeolde: also add MP3
	char* oggv_filename = NULL;
	int pos = strlen(midi_filename) - strlen(".mid");
	if (strcasecmp(midi_filename + pos, ".mid") == 0) {
		oggv_filename = strdup(midi_filename);
		strcpy(oggv_filename + pos, ".ogg");
	}

	char* mp3_filename = NULL;
	if (strcasecmp(midi_filename + pos, ".mid") == 0) {
		mp3_filename = strdup(midi_filename);
		strcpy(mp3_filename + pos, ".mp3");
	}

	/* Try to load the ogg vorbis or midi in the DMod or the main game */
	int exists = 0;
	fullpath = (char*)malloc(1);
	if (!exists && oggv_filename != NULL) {
		free(fullpath);
		sprintf(relpath, "sound/%s", oggv_filename);
		fullpath = paths_dmodfile(relpath);
		exists = exist(fullpath);
	}
	if (!exists && mp3_filename != NULL) {
		free(fullpath);
		sprintf(relpath, "sound/%s", mp3_filename);
		fullpath = paths_dmodfile(relpath);
		exists = exist(fullpath);
	}
	if (!exists) {
		free(fullpath);
		sprintf(relpath, "sound/%s", midi_filename);
		fullpath = paths_dmodfile(relpath);
		exists = exist(fullpath);
	}
	if (!exists && oggv_filename != NULL) {
		free(fullpath);
		sprintf(relpath, "sound/%s", oggv_filename);
		fullpath = paths_fallbackfile(relpath);
		exists = exist(fullpath);
	}
	if (!exist(fullpath)) {
		free(fullpath);
		sprintf(relpath, "sound/%s", midi_filename);
		fullpath = paths_fallbackfile(relpath);
		exists = exist(fullpath);
	}
	free(oggv_filename);
	free(mp3_filename);

	if (!exist(fullpath)) {
		free(fullpath);
		log_warn("🚫 Error playing music %s, doesn't exist in any dir.",
				midi_filename);
		return 0;
	}

	/* Save the midi currently playing */
	if (last_midi != NULL)
		free(last_midi);
	last_midi = strdup(midi_filename);

	/* Stop whatever is playing before we play something else. */
	#ifdef SDL_MIXER_X
	Mix_HaltMusicStream(music_data);
	#else
	Mix_HaltMusic();
	#endif

	/* Load the file */
	if ((music_data = Mix_LoadMUS(fullpath)) == NULL) {
		log_warn("🚫 Unable to play '%s': %s", fullpath, Mix_GetError());
		free(fullpath);
		return 0;
	}

	/* Play it */
	#ifdef SDL_MIXER_X
	Mix_SetFreeOnStop(music_data, 1);
	#else
	Mix_HookMusicFinished(callback_HookMusicFinished);
	#endif

	#ifdef SDL_MIXER_X
	if (fadein > 0)
		Mix_FadeInMusicStream(music_data, (loop_midi == true) ? -1 : 1, fadein);
		else
		Mix_PlayMusicStream(music_data, (loop_midi == true) ? -1 : 1);

	if (high_speed == 1 && !dinklua_enabled)
		Mix_SetMusicTempo(music_data, 3.0);
	#else
	Mix_PlayMusic(music_data, (loop_midi == true) ? -1 : 1);
	#endif

	tag_album = Mix_GetMusicAlbumTag(music_data);
	tag_artist = Mix_GetMusicArtistTag(music_data);
	tag_title = Mix_GetMusicTitle(music_data);
	mus_duration = Mix_MusicDuration(music_data);
	mus_type = Mix_GetMusicType(music_data);

	free(fullpath);
	return 1;
}

/**
 * Pause midi file if we're not already paused
 */
/* should be used when player hits 'n' or alt+'n' - but I never
got it to work in the original game */
int PauseMidi() {
	#ifdef SDL_MIXER_X
	Mix_PauseMusicStream(music_data);
	#else
	Mix_PauseMusic();
	#endif
	return 1;
}

/**
 * Resumes playing of a midi file
 */
/* should be used when player hits 'b' or alt+'b' - but I never
got it to work in the original game */
int ResumeMidi() {
	#ifdef SDL_MIXER_X
	Mix_ResumeMusicStream(music_data);
	#else
	Mix_ResumeMusic();
	#endif
	return 1;
}

/**
 * Stops a midi file playing
 */
// DinkC binding: stopmidi()
int StopMidi() {
	#ifdef SDL_MIXER_X
	Mix_HaltMusicStream(music_data);
	#else
	Mix_HaltMusic(); // return always 0
	#endif
	return 1;
}

/**
 * Initialize BackGround Music (currently nothing to do, MIDI init is
 * done with SDL_INIT_AUDIO in sfx.c).
 */
void bgm_init(void) {
}

void bgm_quit(void) {
	#ifdef SDL_MIXER_X
	Mix_HaltMusicStream(music_data);
	Mix_HaltMusicStream(music_data2);
	#else
	Mix_HaltMusic();
	#endif

	if (last_midi != NULL)
		free(last_midi);
	last_midi = NULL;
}

/** DinkC procedures **/
//ye: and lua
void loopmidi(int arg_loop_midi) {
	if (arg_loop_midi > 0)
		loop_midi = true;
	else
		loop_midi = false;
}

int play_modorder(int order) {
	#ifdef SDL_MIXER_X
	return Mix_ModMusicStreamJumpToOrder(music_data, order);
	#else
	return Mix_ModMusicJumpToOrder(order);
	#endif
}

void set_music_tempo(double tempo) {
	#ifdef SDL_MIXER_X
	if ((Mix_SetMusicTempo(music_data, tempo)) == 0) {
		log_info("Now that's my tempo!");
	}
	else {
		log_info("🤭 Tempo not implemented for this format");
	}
	#else
	log_error("❌ Build lacks MixerX");
	#endif
}

double get_music_tempo() {
	#ifdef SDL_MIXER_X
		return Mix_GetMusicTempo(music_data);
	#else
		log_error("❌Build lacks MixerX");
	#endif
}

//Doesn't seem to work with anything except Ogg STB rather than libogg which we're not using (probably)
void set_music_speed(double speed) {
	#ifdef SDL_MIXER_X
	if (Mix_SetMusicSpeed(music_data, speed) == 0) {
		log_info("🏃 Set the music speed");
	} else {
		log_info("🍲 No speed for you!");
	}
	#else
	log_error("❌ Build lacks MixerX");
	#endif
}

void set_music_vol(int volume) {
	#ifdef SDL_MIXER_X
	if (volume < 0 || volume > 128) {
		log_error("📶 Volume must be from 1-128");
		return;
		}
	if (Mix_GetMidiPlayer() != MIDI_Native)
		Mix_VolumeMusicStream(music_data, volume);
	else
		log_error("📶 System MIDI output does not support volume control");
	#else
	log_error("❌ Not available in non-MixerX builds");
	#endif
}

int get_music_vol() {
	#ifdef SDL_MIXER_X
	return Mix_GetVolumeMusicStream(music_data);
	#else
	return Mix_VolumeMusic(-1);
	#endif
}

void set_music_pitch(double pitch) {
	#ifdef SDL_MIXER_X
	if (Mix_SetMusicPitch(music_data, pitch) == 0) {
		log_info("⚾ Setting pitch");
	} else {
		log_error("⚾ Could not set pitch");
	}
	#else
	log_error("❌ Build lacks MixerX");
	#endif
}

void fade_music(int time) {
	#ifdef SDL_MIXER_X
	if (Mix_FadeOutMusicStream(music_data, time) == 0) {
		log_error("☁️ Could not fade music");
	} else {
		log_info("☁️ Fading out music over %d ms", time);
	}
	#else
	log_error("❌ Build lacks MixerX");
	#endif

}

double get_music_position() {
	return Mix_GetMusicPosition(music_data);
}

void set_music_position(double pos) {
	#ifdef SDL_MIXER_X
	Mix_SetMusicPositionStream(music_data, pos);
	#else
	Mix_SetMusicPosition(pos);
	#endif
}

void dinklua_object_playmidi(std::string bgm, int fadein = 0) {
	PlayMidi((char*)bgm.c_str(), fadein);
}

void set_music_track(int track) {
	Mix_StartTrack(music_data, track);
}

int get_music_tracks() {
	return Mix_GetNumTracks(music_data);
}

void load_music_soundfont(std::string filename) {
	#ifdef SDL_MIXER_X
	Mix_SetMidiPlayer(MIDI_Fluidsynth);
	#endif
	if (Mix_SetSoundFonts(paths_dmodfile(filename.c_str())) == 0)
		log_error("🎶 Failed to load SoundFont");
}
