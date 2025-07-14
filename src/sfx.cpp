/**
 * Sound effects (not music) management

 * Copyright (C) 1997, 1998, 1999, 2002, 2003  Seth A. Robinson
 * Copyright (C) 2003  Shawn Betts
 * Copyright (C) 2005, 2007, 2008, 2009, 2014, 2015  Sylvain Beucler
 * Copyright (C) 2022 CRTS
 * Copyright (C) 2023-2024 Yeoldetoast

 * This file is part of Yeoldedink

 * Yeoldedink is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.

 *  Yeoldedink is distributed in the hope that it will be useful, but
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

#include <stdlib.h>
#include <string.h> /* memset, memcpy */
#include <errno.h>

#include "SDL.h"
#if defined SDL_MIXER_X && !defined DINKEDIT
#include <SDL2/SDL_mixer_ext.h>
#else
#include "SDL_mixer.h"
#undef SDL_MIXER_X
#endif

#include "dinklua.h"

#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_freeverbfilter.h"
#include "soloud_bassboostfilter.h"
#include "soloud_speech.h"
#include "soloud_midi.h"
#include "random.hpp"

#include "live_sprites_manager.h"
#include "debug_imgui.h"
#include "debug.h"
#include "game_engine.h"
#include "live_screen.h"
#include "str_util.h"
#include "io_util.h"

#include "paths.h"
#include "log.h"
#include "math.h"
#include "sfx.h"
#include "gfx_sprites.h"
#include "log.h"

//for pitch randomisation
using Random = effolkronium::random_static;

SoLoud::Soloud gSoloud;
SoLoud::Wav gWave;
SoLoud::Bus gBus;
SoLoud::FreeverbFilter gVerb;
SoLoud::BassboostFilter gBassBoost;
SoLoud::Speech speech;
SoLoud::Midi mymidi;
SoLoud::SoundFont mysoundfont;

/* Sound - BGM */
int sound_on = 1;

/* Channel metadata */
int buf_samples = 512;
int audio_samplerate;
int adlemu, opnemu;
char soundfont[200];
char* soundnames[MAX_SOUNDS];
//"3d sound" parameters
float atten = 0.8f;
float dist_min = 20.0f;
float dist_max = 610.0f;
int dist_model = 2;
int distmult = 100;
time_t ct;
float sfx_bass_boost = 0.0f;

//traditionally unalterable SFX settings
const int sfx_baserate = 22050;
int sfx_ducksound = 1;
//ye: duckhz is played at different speeds depending on if the duck is big or little
int sfx_duckhz = 18000;
//other hurtsound must be in slot+1
int sfx_hurtsound = 15;
int sfx_hurthz = 25050;
int sfx_thwacksound = 9;
int sfx_thwackhz = sfx_baserate;
int sfx_mousesound = 19;
int sfx_mousehz = sfx_baserate;
// and +1, 2, 3
int sfx_pigsound = 2;
//same as the ducks in terms of size
int sfx_pighz = 18000;
int sfx_spritehz = sfx_baserate;
int sfx_choicescrollsound = 11;
int sfx_choicescrollhz = sfx_baserate;
int sfx_choiceselectsound = 17;
int sfx_choiceselecthz = sfx_baserate;
int sfx_inventoryopensound = 18;
int sfx_inventoryopenhz = sfx_baserate;
int sfx_inventorymovesound = 11;
int sfx_inventorymovehz = sfx_baserate;
int sfx_inventoryselectsound = 18;
int sfx_inventoryselecthz = 42050;
int sfx_inventoryexitsound = 17;
int sfx_inventoryexithz = sfx_baserate;
int sfx_statusstatsound = 22;
int sfx_statusstathz = sfx_baserate;
int sfx_statusgoldsound = 14;
int sfx_statusgoldhz = sfx_baserate;
int sfx_statusexpticksound = 13;
int sfx_statusexptickhz = 15050;
int sfx_statusexpcountsound = 13;
int sfx_statusexpcounthz = 29050;
//"door"
int sfx_warpsound = 7;
int sfx_warphz = 12000;


//register our soloud sound slots to load into
SoLoud::Wav gSFX[MAX_SOUNDS];
//create groups so we can get repeating and survive
SoLoud::handle group_loop = gSoloud.createVoiceGroup();
SoLoud::handle group_survive = gSoloud.createVoiceGroup();
//Main queue
SoLoud::Queue gQueue;

/* Hardware soundcard information */
int hw_freq, hw_channels;
static Uint16 hw_format;

/**
 * Display a SDL audio-format in human-readable form
 **/
static const char* format2string(Uint16 format) {
	char* format_str = "Unknown";
	switch (format) {
	case AUDIO_U8:
		format_str = "U8";
		break;
	case AUDIO_S8:
		format_str = "S8";
		break;
	case AUDIO_U16LSB:
		format_str = "U16LSB";
		break;
	case AUDIO_S16LSB:
		format_str = "S16LSB";
		break;
	case AUDIO_U16MSB:
		format_str = "U16MSB";
		break;
	case AUDIO_S16MSB:
		format_str = "S16MSB";
		break;
	case AUDIO_S32LSB:
		format_str = "S32LSB";
		break;
	case AUDIO_S32MSB:
		format_str = "S32MSB";
		break;
	case AUDIO_F32LSB:
		format_str = "F32LSB";
		break;
	case AUDIO_F32MSB:
		format_str = "F32MSB";
		break;
	}
	return format_str;
}
/**
 * Load sounds from the standard paths. Sound is converted to the
 * current hardware (soundcard) format so that sound rate can be
 * altered with minimal overhead when playing the sound.
 */
int CreateBufferFromWaveFile(char* filename, int index) {
	/* Open the wave file */
	char path[150];

	if (index >= MAX_SOUNDS || index < 0) {
		log_error("SCRIPTING ERROR: sound index %d is too big.", index);
		return 0;
	}
	if (soundnames[index] != NULL && compare(soundnames[index], filename) && dbg.sfxspeed) {
		return 1;
	}
	sprintf(path, "sound/%s", filename);
	soundnames[index] = strdup(filename);
	//Loading first from main data, then d-mod. Seems to work
	if (sound_on) {
	gSFX[index].load(paths_fallbackfile(path));
	gSFX[index].load(paths_dmodfile(path));
	//Setting this improves the blast that occurs upon changing screens, but may annoy some
	gSFX[index].set3dDistanceDelay(dbg.audiodelay);
	}

	return 1;
}

/**
 * Is the specified sound currently playing?
 *
 * Only used in pig_brain(), could be removed maybe.
 */
int playing(int sound) {
	return gSoloud.isValidVoiceHandle(sound);
}

/**
 * Kill repeating sounds except the ones that survive
 */
void kill_repeat_sounds(void) {
	if (!sound_on)
		return;

	log_info("🔇 Halting non-surviving repeating sounds");
	//Get rid of all those not set to survive
	gSoloud.fadeVolume(group_loop, 0, 0.3f);
	gSoloud.setVolume(group_survive, 1.0f);
	gSoloud.scheduleStop(group_loop, 1);
	gSoloud.setPause(group_survive, false);
}

/**
 * Kill all repeating sounds, even the ones that survive (used from
 * scripting's restart_game() and load_game())
 */
void kill_repeat_sounds_all(void) {
	if (!sound_on)
		return;

	gSoloud.stop(group_loop);
}

/**
 * Kill one sound
 */
void kill_this_sound(int channel) {
	gSoloud.stop(channel);
}

//yeolde: Actually just halt all SFX
void halt_all_sounds() {
	gSoloud.stopAll();
}

/**
 * Called by update_frame()
 *
 * If sound is active, refreshed pan&vol for 3D effect.
 *
 * If repeating and sprite.sound==0 and sprite.owner!=0 -> stop sound
 * If sprite.active==0 and sprite.owner!=0 -> stop sound
 */
void update_sound(void) {

	if (!sound_on)
		return;

	for (int i = 1; i < MAX_SPRITES_AT_ONCE; i++) {
			if (spr[i].active && gSoloud.isVoiceGroup(spr[i].sounds)) {
			//This sprite is alive. Let's update its positioning.
			gSoloud.set3dSourceParameters(spr[i].sounds, spr[i].x, spr[i].y, 0, spr[i].mx, spr[i].my, 0);
			//sound speedup
			#ifndef DINKEDIT
			if (!dinklua_enabled) {
			if (high_speed == 1)
				gSoloud.setRelativePlaySpeed(spr[i].sounds, 3.0);
			else if (high_speed == -1)
				gSoloud.setRelativePlaySpeed(spr[i].sounds, 0.5);
			else if (high_speed == -2)
				gSoloud.setRelativePlaySpeed(spr[i].sounds, 0.1);
			else
				gSoloud.setRelativePlaySpeed(spr[i].sounds, 1.0);
			}
			#endif

			}
		}
	//Set the player's listening pos
	gSoloud.set3dListenerPosition(spr[1].x, spr[1].y, 0);
	gSoloud.set3dListenerVelocity(spr[1].mx, spr[1].my * -1, 0);
	gSoloud.set3dListenerUp(0, -1, 0);

	gSoloud.update3dAudio();
}

static int SoundPlayEffectChannel(int sound, int min, int plus, int sound3d,
								/*bool*/ int repeat, int explicit_channel);

/**
 * Play a sound previously loaded to memory (in registered_sounds)
 * - sound: sound index
 * - min: frequency (Hz)
 * - plus: max random frequency, to add to min
 * - sound3d: if != 0, sprite number whose location will be used for
 *   pseudo-3d effects (volume, panning)
 * - repeat: is sound looping?
 **/
int SoundPlayEffect(int sound, int min, int plus, int sound3d,
					/*bool*/ int repeat) {
	return SoundPlayEffectChannel(sound, min, plus, sound3d, repeat, -1);
}
/**
 * SoundPlayEffect_Channel_ allows to specify an explicit audio
 * channel, which in turns allows the editor to only use one channel
 * for everything (when you move the mouse with the keyboard, you'll
 * hear a series of close 'ticks', but they won't overlap each
 * others). The rest of the time, the game will just pass '-1' for the
 * channel, so the first available channel (among NUM_CHANNELS useable
 * simultaneously) will be selected.
 */
static int SoundPlayEffectChannel(int sound, int min, int plus, int sound3d,
								/*bool*/ int repeat, int explicit_channel) {
	log_enter("SoundPlayEffectChannel: sound: %d, min: %d, plus: %d, "
			"sound3d: %d, repeat: %d, explicit_channel: %d", sound, min,
			plus, sound3d, repeat, explicit_channel);

	//Safety check. Don't know if it's needed due to Soloud's virtual voices
	//yes it is...
	if (gSoloud.getActiveVoiceCount() == gSoloud.getMaxActiveVoiceCount()) {
		halt_all_sounds();
		log_error("😶 Out of voices for playsound!");
	}
	//Although we do in fact need a different safety check
	if (sound < 0 || sound > MAX_SOUNDS) {
		log_error("🔕 Sound index out of range!");
		return 1;
	}

	//Our audio handle number to return so they can do stuff with it
	//TODO: give it a better name than ecks
	int x;
	//Set our listener before playing a sound
	gSoloud.set3dListenerPosition(spr[1].x, spr[1].y, spr[1].size / 100.0f);
	/* Sample rate / frequency */
	{
		int play_freq;

		if (plus == 0)
			play_freq = min;
		else if (min > 0) {
			play_freq = Random::get(min, min + plus -1);
		}

		if (sound3d > 0) {
			//Sound3d is a sprite number that we'll use for positioning
			//ye: Dink is a 2D engine...
			x = gSoloud.play3d(gSFX[sound], spr[sound3d].x, spr[sound3d].y, 0, spr[sound3d].mx, spr[sound3d].my, 0, 1.0f, true);
			//Make it attenuate as we walk away from it. Higher values increase the rolloff
			gSoloud.set3dSourceMinMaxDistance(x, dist_min, dist_max);
			gSoloud.set3dSourceAttenuation(x, dist_model, atten);
			//use an extra field in the sprite to store the handle(s)
			if (gSoloud.isVoiceGroup(spr[sound3d].sounds)) {
				gSoloud.addVoiceToGroup(spr[sound3d].sounds, x);
			}
			else {
				//Create the group for the sprite if it doesn't already exist
				spr[sound3d].sounds = gSoloud.createVoiceGroup();
				gSoloud.addVoiceToGroup(spr[sound3d].sounds, x);
			}
			//unpause after positioning applied
			gSoloud.setPause(x, false);
		} else {
			//A normal, non-positional sound not attached to a sprite we'll play clocked to avoid bunching up
			#ifndef DINKEDIT
			x = gSoloud.playClocked(thisTickCount / 1000.0f, gSFX[sound]);
			#else
			x = gSoloud.play(gSFX[sound], 1.0f, 0.0f, true);
			#endif
		}
		//Kill the sound if it becomes inaudible
		gSoloud.setInaudibleBehavior(x, false, true);
		//Set the speed in hertz, or else use defaults if set to zero
		if (min > 0)
			gSoloud.setSamplerate(x, play_freq);
		else if (plus > 0) {
			float myspeed = Random::get<float>(100, 100 + plus) / 100.0f;
			gSoloud.setRelativePlaySpeed(x, myspeed);
		}
		#ifndef DINKEDIT
		if (repeat == 1) {
		gSoloud.setLooping(x, true);
		//Keep the sound ticking even if it's inaudible
		gSoloud.setInaudibleBehavior(x, true, false);
		if (!sound3d)
			gSoloud.addVoiceToGroup(group_loop, x);
		} else if (high_speed == 1 && !dinklua_enabled) {
			//For fast and slow modes
			gSoloud.setRelativePlaySpeed(x, 3.0);
		} else if (high_speed == -1 && !dinklua_enabled) {
			gSoloud.setRelativePlaySpeed(x, 0.5);
		} else if (high_speed == -2 && !dinklua_enabled) {
			gSoloud.setRelativePlaySpeed(x, 0.1);
		}
		#endif
	}
	log_exit("SoundPlayEffectChannel: %d", x);
	return x;
}

/**
 * Just play a sound, do not try to update sprites info or apply
 * effects
 */
void EditorSoundPlayEffect(int sound) {
	/* Don't print warning if the sound isn't present - as sounds are
played continuously when arrow keys are pressed */
		SoundPlayEffectChannel(sound, 0,
							0, 0, 0, 0);
}

/**
 * SoundStopEffect
 *
 * Stops the sound effect specified.
 * Returns TRUE if succeeded.
 */
int SoundStopEffect(int sound) {

	if (sound >= MAX_SOUNDS) {
		log_error("🎸 Attempting to get stop sound %d (> MAX_SOUNDS=%d)", sound, MAX_SOUNDS);
		return 0;
	} else {
		gSoloud.stopAudioSource(gSFX[sound]);
		return 1;
	}

}

/**
 * InitSound
 *
 * Sets up the DirectSound object and loads all sounds into secondary
 * DirectSound buffers.  Returns -1 on error, or 0 if successful
 */
void sfx_init() {
	if (!sound_on) {
		//Init soloud with no sound output
		gSoloud.init(SoLoud::Soloud::FLAGS::LEFT_HANDED_3D, SoLoud::Soloud::BACKENDS::NULLDRIVER, SoLoud::Soloud::AUTO, SoLoud::Soloud::AUTO, SoLoud::Soloud::AUTO);
		log_info("🔇 SoLoud initted with null driver");
		return;
	}

	sound_on = 0;

	log_info("📻 Initting sound");

	#ifdef _WIN32
	SDL_setenv("SDL_AUDIODRIVER", "wasapi", 1);
	#endif

	if (SDL_Init(SDL_INIT_AUDIO) == -1) {
		log_error("📻 SDL_Init(SDL_INIT_AUDIO): %s", SDL_GetError());
		return;
	}

	/* Work-around to disable fluidsynth and fallback to TiMidity++: */
	/* SDL_setenv("SDL_SOUNDFONTS="); */
	/* SDL_setenv("SDL_FORCE_SOUNDFONTS=1"); */
	//Yeolde: set in ini file
	if (strcmp(soundfont, "0"))
		Mix_SetSoundFonts(paths_pkgdatafile(soundfont));
	/* To specify an alternate timidity.cfg */
	/* Cf. SDL2_mixer/timidity/config.h for defaults */
	/* SDL_setenv("TIMIDITY_CFG", "somewhere/timidity.cfg", 0); */

	/* MIX_DEFAULT_FREQUENCY is ~22kHz are considered a good default,
44kHz is considered too CPU-intensive on older computers */
//yeolde: Thankfully I don't care about them.
	/* MIX_DEFAULT_FORMAT is 16bit adapted to current architecture
(little/big endian) */
	/* MIX_DEFAULT_CHANNELS is 2 => stereo, allowing panning effects */
	/* 1024 (chunk on which effects are applied) seems a good default,
4096 is considered too big for SFX */

	//yeolde: even with SoLoud we still need to init Mixer for bgm
	if (Mix_OpenAudio(audio_samplerate, MIX_DEFAULT_FORMAT,
					hw_channels, buf_samples) == -1) {
		log_error("Mix_OpenAudio: %s", Mix_GetError());
		return;
	}

	/* Done with initialization */
	/* Avoid calling SDL_PauseAudio when using SDL_Mixer */
	/* SDL_PauseAudio(0); */

	/* Dump audio info */
	{
		log_info("📻 Audio driver: %s", SDL_GetCurrentAudioDriver());

		int numtimesopened;
		numtimesopened = Mix_QuerySpec(&hw_freq, &hw_format, &hw_channels);
			log_info("📻 Audio hardware info: "
					"frequency=%dHz\tformat=%s\tchannels=%d\topened=%d times",
					hw_freq, format2string(hw_format), hw_channels,
					numtimesopened);
	}

	sound_on = 1;
	//Set our saved parameters
	//TODO: move this out to bgm
	#if defined SDL_MIXER_X && !defined DINKEDIT
	Mix_VolumeMusicGeneral(dbg.musvol);
	Mix_SetMidiPlayer(midiplayer);
	Mix_OPNMIDI_setEmulator(opnemu);
	Mix_ADLMIDI_setEmulator(adlemu);
	Mix_ADLMIDI_setFullPanStereo(1);
	//hopefully prevents it from clipping
	Mix_ADLMIDI_setVolumeModel(ADLMIDI_VM_DMX_FIXED);
	#else
	Mix_VolumeMusic(dbg.musvol);
	#endif
	//init soloud
	//Backend defined in soloud.h - could be made user-configurable
	#ifdef WITH_MINIAUDIO
	gSoloud.init(SoLoud::Soloud::FLAGS::LEFT_HANDED_3D, SoLoud::Soloud::BACKENDS::MINIAUDIO, audio_samplerate, SoLoud::Soloud::AUTO, hw_channels);
	#elif defined WITH_SDL2_STATIC
	gSoloud.init(SoLoud::Soloud::FLAGS::LEFT_HANDED_3D, SoLoud::Soloud::BACKENDS::SDL2, audio_samplerate, buf_samples, hw_channels);
	#elif defined WITH_COREAUDIO
	gSoloud.init(SoLoud::Soloud::FLAGS::LEFT_HANDED_3D, SoLoud::Soloud::BACKENDS::COREAUDIO, audio_samplerate, buf_samples, hw_channels);
	#endif
	log_info("📻 SoLoud initted at %dHz with buffer of %d using %s with %d channels", gSoloud.getBackendSamplerate(), gSoloud.getBackendBufferSize(), gSoloud.getBackendString(), gSoloud.getBackendChannels());
	//let's give them lots of voices
	gSoloud.setMaxActiveVoiceCount(NUM_CHANNELS);
	//For our waveform viewer
	gSoloud.setVisualizationEnable(true);
	//Set our 3d vector, sound speed, and clipper
	gSoloud.set3dListenerUp(1, -1, 0);
	gSoloud.setPostClipScaler(0.95);
	//Set to 10x the speed of sound, assuming that 1 in-game unit is 100m or so
	gSoloud.set3dSoundSpeed(3430);
	gVerb.setParams(0, 2, 5, 5);
	//gSoloud.setGlobalFilter(2, &gVerb);
	speech.setParams(2000, 6, 2, KW_TRIANGLE);
	//Set our loaded sfx volume
	gSoloud.setGlobalVolume(dbg.sfxvol);
	//Set our speech bus bass boost
	//gSoloud.setGlobalFilter(0, &gBassBoost);
	gBus.setFilter(1, &gBassBoost);
}

/**
 * Undoes everything that was done in a sfx_init call
 */
void sfx_quit(void) {
	if (!sound_on)
		return;

	/* Stops all SFX channels */
	//gSoloud.stopAll();
	gSoloud.deinit();
	Mix_Quit();
	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/**
 * Print SFX memory usage
 */
void sfx_log_meminfo() {
	int sum = sizeof(gSFX);
	log_debug("Sounds   = %8d", sum);
}

void sfx_set_bassboost(float boost) {
	log_info("🎸 Bass boost set to %.2f", boost);
	gBassBoost.setParams(boost);
	gSoloud.setGlobalFilter(1, &gBassBoost);
}


/**
 * Set volume; dx_volume is [-10000;10000] in hundredth of dB
 */
static int SetVolume(int channel, int dx_volume) {
	// SFX
	/* See doc/sound.txt for details */
	if (!gSoloud.isValidVoiceHandle(channel)) {
		log_warn("👹 Attempted to set the volume of invalid voice handle!");
		return 0;
	}
	double exp = ((dx_volume / 1000.0f) + 10);
	double num = 100.0f * (pow(2, exp) - 1);
	//gSoloud.setVolume(channel, 1.0f * pow(10, ((double)dx_volume / 100) / 20));
	gSoloud.setVolume(channel, (num / 1023.0f) / 100.0f);
	//log_debug("📶 Volume for %d to change to is %d, set to %.2f", channel, dx_volume, 1.0f * pow(10, ((double)dx_volume / 100) / 20));
	log_debug("📶 Volume for handle %d to change to is %d, set to %.2f", channel, dx_volume, (num / 1023.0f) / 100.0f);
	//return Mix_Volume(channel, MIX_MAX_VOLUME * pow(10, ((double)dx_volume / 100) / 20));
	return 1;
}

/** DinkC procedures **/
/* BIG FAT WARNING: in DinkC, soundbank is channel+1
(a.k.a. non-zero), and 0 means failure. */
//ye: this no longer applies
int playsound(int sound, int min, int plus, int sound3d, int repeat) {
	log_enter("playsound: sound: %d, min: %d, plus: %d, sound3d: %d, repeat: %d",
			sound, min, plus, sound3d, repeat);
	int channel = SoundPlayEffect(sound, min, plus, sound3d, repeat);
	log_debug("🎵 Sound index %d played on handle: %d", sound, channel);

	log_exit("playsound: %d", channel);
	return channel;
}

void sound_set_kill(int soundbank) {
	gSoloud.stop(soundbank);
}

void sound_set_survive(int soundbank, int survive) {
	if (survive) {
	gSoloud.setProtectVoice(soundbank, true);
	gSoloud.addVoiceToGroup(group_survive, soundbank);
	}
	else {
		gSoloud.setProtectVoice(soundbank, false);
		gSoloud.setLooping(soundbank, false);
	}
}

void sound_set_vol(int soundbank, int volume) {
	SetVolume(soundbank, volume);
}


//For Lua only
void lua_set_vol(int soundbank, int volume) {
	if (!gSoloud.isValidVoiceHandle(soundbank) || volume == 0)
		log_warn("👹 Attempted to set the volume of invalid voice handle!");
	gSoloud.setVolume(soundbank, volume / 100.0f);
}

int sound_get_vol(int soundbank) {
	return gSoloud.getVolume(soundbank) * 100;
}

int sound_get_overall_vol(int soundbank) {
	return gSoloud.getOverallVolume(soundbank) * 100;
}

void sound_fade_vol(int soundbank, int vol, int time) {
	gSoloud.fadeVolume(soundbank, vol / 100.0f, time);
}

bool sound_get_survive(int soundbank) {
	//Only using voice protection for survived sounds I think
	return gSoloud.getProtectVoice(soundbank);
}

void sound_set_pan(int soundbank, int pan) {
	gSoloud.setPan(soundbank, pan / 100.0f);
}

void sound_fade_pan(int soundbank, int pan, int time) {
	gSoloud.fadePan(soundbank, pan / 100.0f, time);
}

int sound_get_pan(int soundbank) {
	return (int)gSoloud.getPan(soundbank * 100);
}

void sound_set_hz(int soundbank, int hz) {
	gSoloud.setSamplerate(soundbank, hz);
}

int sound_get_hz(int soundbank) {
	return (int)gSoloud.getSamplerate(soundbank);
}

int sound_get_speed(int soundbank) {
	return (int)gSoloud.getRelativePlaySpeed(soundbank) * 100;
}

void sound_set_speed(int soundbank, int speed) {
	//Sound speed default is 100
	if (speed > 0)
		gSoloud.setRelativePlaySpeed(soundbank, speed / 100.0f);
}

void sound_fade_speed(int soundbank, int speed, int time) {
	if (speed > 0)
	gSoloud.fadeRelativePlaySpeed(soundbank, speed / 100.0f, time);
}

void sound_set_pause(int soundbank, bool paused) {
	gSoloud.setPause(soundbank, paused);
}

bool sound_get_pause(int soundbank) {
	return gSoloud.getPause(soundbank);
}

void sound_schedule_pause(int soundbank, int time) {
	gSoloud.schedulePause(soundbank, time);
}

void sound_schedule_stop(int soundbank, int time) {
	gSoloud.scheduleStop(soundbank, time);
}

void sound_set_loop_point(int soundbank, int looppoint) {
	gSoloud.setLoopPoint(soundbank, looppoint / 1.0f);
}

int sound_get_loop_point(int soundbank) {
	return (int)gSoloud.getLoopPoint(soundbank);
}

int sound_get_loop_count(int soundbank) {
	return gSoloud.getLoopCount(soundbank);
}

int sound_get_pos(int soundbank) {
	return gSoloud.getStreamPosition(soundbank);
}

void sound_oscillate_vol(int soundbank, int from, int to, int time) {
	gSoloud.oscillateVolume(soundbank, from / 100.0f, to / 100.0f, time);
}

void sound_oscillate_pan(int soundbank, int from, int to, int time) {
	gSoloud.oscillatePan(soundbank, from / 100.0f, to / 100.0f, time);
}

void sound_oscillate_speed(int soundbank, int from, int to, int time) {
	gSoloud.oscillateRelativePlaySpeed(soundbank, from / 100.0f, to / 100.0f, time);
}

void sound_set_looping(int soundbank, bool repeat) {
	gSoloud.setLooping(soundbank, repeat);
}

bool sound_get_looping(int soundbank) {
	return gSoloud.getLooping(soundbank);
}

//yeolde: use soloud to one-shot play a wav/ogg/flac
int playsfx(char* filename, int speed, int pan) {
	gWave.load(paths_dmodfile(filename));
	//gWave.load(filename);
	int x = gSoloud.playBackground(gWave);
	if (speed != 0) {
		gSoloud.setRelativePlaySpeed(x, speed / 100.0f);
	}
	if (pan != 0) {
		gSoloud.setPan(x, pan / 100.0f);
	}
	return x;
}
//TODO: remove
void play_midi_as_sfx(char* filename, char* sf2) {
	//todo: check files exist first
	mysoundfont.load(paths_pkgdatafile(sf2));
	mymidi.load(paths_dmodfile(filename), mysoundfont);
	int x = gSoloud.play(mymidi);
}

//Called from debug interface
bool sound_slot_occupied(int slot) {
	if (gSFX[slot].mData != NULL)
		return true;
	else
		return false;
}

double sound_slot_duration(int slot) {
	return gSFX[slot].getLength();
}
//called from debug mode window
void sound_slot_preview(int slot) {
	gSoloud.play(gSFX[slot]);
}
//pause and resume feature
void pause_sfx() {
	game_paused() == false ? gSoloud.setPauseAll(false) : gSoloud.setPauseAll(true);
}

//hard-coded sfx playback

//nono.wav, used on title screen
void sfx_play_mouse() {
	SoundPlayEffect(sfx_mousesound, sfx_mousehz, 0, 0, 0);
}

//door sound
void sfx_play_warp() {
	SoundPlayEffect(sfx_warpsound, sfx_warphz, 0, 0, 0);
}

void sfx_play_hit_thwack() {
	SoundPlayEffect(sfx_thwacksound, sfx_thwackhz, 0, 0, 0);
}

void sfx_play_choice_scroll() {
	SoundPlayEffect(sfx_choicescrollsound, sfx_choicescrollhz, 0, 0, 0);
}

void sfx_play_choice_select() {
	SoundPlayEffect(sfx_choiceselectsound, sfx_choiceselecthz, 0, 0, 0);
}

void sfx_play_sprite_sound(int sound, int sprite, int loop) {
	SoundPlayEffect(sound, sfx_spritehz, 0, sprite, loop);
}

void sfx_stop_sprite_sound(int sprite) {
	if (gSoloud.isVoiceGroup(spr[sprite].sounds))
		gSoloud.stop(spr[sprite].sounds);
}

void sfx_play_inventory_move() {
	SoundPlayEffect(sfx_inventorymovesound, sfx_inventorymovehz, 0, 0, 0);
}

void sfx_play_inventory_open() {
	SoundPlayEffect(sfx_inventoryopensound, sfx_inventoryopenhz, 0,0,0);
}

void sfx_play_inventory_exit() {
	SoundPlayEffect(sfx_inventoryexitsound, sfx_inventoryexithz, 0, 0, 0);
}

void sfx_play_inventory_select() {
	SoundPlayEffect(sfx_inventoryselectsound, sfx_inventoryselecthz, 0, 0, 0);
}

void sfx_play_status_gold() {
	SoundPlayEffect(sfx_statusgoldsound, sfx_statusgoldhz, 0, 0, 0);
}

void sfx_play_status_stats() {
	SoundPlayEffect(sfx_statusstatsound, sfx_statusstathz, 0, 0, 0);
}

void sfx_play_status_exp_tick() {
	SoundPlayEffect(sfx_statusexpticksound, sfx_statusexptickhz, 0, 0, 0);
}
//counting up upon level achieved
void sfx_play_status_exp_count() {
	SoundPlayEffect(sfx_statusexpcountsound, sfx_statusexpcounthz, 0, 0, 0);
}

void sfx_play_duck(int size, int sprite) {
	//log_error("Size of sprite %d is %d with 100 sized hz being %d", sprite, size, sfx_duckhz - (size * 50));
	if (size >= 100)
		SoundPlayEffect(sfx_ducksound, sfx_duckhz - (size * 50), 0, sprite, 0);
	else
		SoundPlayEffect(sfx_ducksound, (sfx_duckhz - 2000) + (size * 100), 0, sprite, 0);
}

int sfx_play_pig(int size, int sprite) {
	int randsound = Random::get(sfx_pigsound, sfx_pigsound + 3);
	if (size >= 100)
		return SoundPlayEffect(randsound, sfx_pighz - (size * 50), 800, sprite, 0);
	else
		return SoundPlayEffect(randsound, (sfx_pighz - 2000) + (size * 100), 800, sprite, 0);
}

void sfx_play_player_hurt() {
	int mysound = Random::get<int>(0,1);
	SoundPlayEffect(sfx_hurtsound + mysound, sfx_hurthz, 0, 0, 0);
	//TODO: put rumble back in here
}
