
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <math.h>

#include "brain.h"
#include "live_sprites_manager.h"
#include "freedink.h"
#include "game_engine.h"
#include "gfx.h"
#include "gfx_sprites.h"
#include "sfx.h"
#include "log.h"
#include "debug_imgui.h"
#include "debug.h"
#include "random.hpp"

using Random = effolkronium::random_static;

void make_missile(int x1, int y1, int dir, int speed, int seq, int frame,
				int strength) {
	int crap = add_sprite(x1, y1, 11, seq, frame);
	spr[crap].speed = speed;
	spr[crap].seq = seq;
	spr[crap].timing = 0;
	spr[crap].strength = strength;
	spr[crap].flying = /*true*/ 1;
	changedir(dir, crap, 430);
}

void missile_brain(int h, /*bool*/ int repeat) {
	rect box;
	int j;
	automove(h);

	*pmissle_source = h;
	int hard = check_if_move_is_legal(h);
	if (repeat && spr[h].seq == 0)
		spr[h].seq = spr[h].seq_orig;
	spr[1].hitpoints = *plife;

	if (hard > 0 && hard != 2) {
		//lets check to see if they hit a sprites hardness
		if (hard > 100) {
			int ii;
			for (ii = 1; ii < last_sprite_created; ii++) {
				if (spr[ii].sp_index == hard - 100) {
					//autopause on missile hit

					if (spr[ii].script > 0) {
						*pmissile_target = 1;
						*penemy_sprite = 1;

						if (scripting_proc_exists(spr[ii].script, "HIT"))
						{
							scripting_kill_callbacks(spr[ii].script);
							scripting_run_proc(spr[ii].script, "HIT");
						}
					}

					if (spr[h].script > 0) {
						*pmissile_target = ii;
						*penemy_sprite = 1;
						if (scripting_proc_exists(spr[h].script, "DAMAGE"))
						{
							scripting_kill_callbacks(spr[h].script);
							scripting_run_proc(spr[h].script, "DAMAGE");
						}
					} else {
						if (spr[h].attack_hit_sound == 0) {
							sfx_play_hit_thwack();
							log_debug("💥 Playing default attack sound after missile intersected with hardsprite");
						}
						else {
							SoundPlayEffect(spr[h].attack_hit_sound, spr[h].attack_hit_sound_speed, 0, 0, 0);
						}

						lsm_remove_sprite(h);
					}

					//run missile end
					return;
				}
			}
		}
		//run missile end

		if (spr[h].script > 0) {
			*pmissile_target = 0;
			scripting_run_proc(spr[h].script, "DAMAGE");
		} else {
			if (spr[h].attack_hit_sound == 0) {
				sfx_play_hit_thwack();
				log_debug("💥 Playing default attack sound at missile end");
			}
			else
				SoundPlayEffect(spr[h].attack_hit_sound,
								spr[h].attack_hit_sound_speed, 0, 0, 0);

			lsm_remove_sprite(h);
			return;
		}
	}

	if (spr[h].x > 1000)
		lsm_remove_sprite(h);
	if (spr[h].y > 700)
		lsm_remove_sprite(h);
	if (spr[h].y < -500)
		lsm_remove_sprite(h);
	if (spr[h].x < -500)
		lsm_remove_sprite(h);

	//did we hit anything that can die?

	for (j = 1; j <= last_sprite_created; j++) {
		if (spr[j].active && h != j && spr[j].nohit != 1 &&
			spr[j].notouch == /*false*/ 0)
			if (spr[h].brain_parm != j && spr[h].brain_parm2 != j)
			//if (spr[j].brain != 15) if (spr[j].brain != 11)
			{
				rect_copy(&box, &k[getpic(j)].hardbox);
				rect_offset(&box, spr[j].x, spr[j].y);

				if (spr[h].range != 0)
					rect_inflate(&box, spr[h].range, spr[h].range);
				//ye: paint screen orange
				//orange screen good
				if (debug_mode && debug_missilesquares) {
					SDL_Rect r = {box.left, box.top, box.right - box.left,
								box.bottom - box.top};
					IOGFX_backbuffer->fillRect(&r, 255, 93, 10);
				}

				if (debug_mode && debug_pausemissile) {
						debug_paused = true;
						pause_everything();
					}

				if (inside_box(spr[h].x, spr[h].y, box)) {
					spr[j].notouch = /*true*/ 1;
					spr[j].notouch_timer = thisTickCount + 100;
					spr[j].target = 1;
					*penemy_sprite = 1;

					//missile hit pause
					if (debug_mode && debug_pausemissilehit) {
						debug_paused = true;
						pause_everything();
					}
					//change later to reflect REAL target
					if (spr[h].script > 0) {
						*pmissile_target = j;
						scripting_run_proc(spr[h].script, "DAMAGE");
					} else {
						if (spr[h].attack_hit_sound == 0) {
							sfx_play_hit_thwack();
							log_debug("💥 Playing default attack sound after missile hit something that may die");
						}
						else
							SoundPlayEffect(spr[h].attack_hit_sound,
											spr[h].attack_hit_sound_speed, 0, 0,
											0);
					}

					if (spr[j].hitpoints > 0 && spr[h].strength != 0) {
						int hit = 0;
						if (spr[h].strength == 1)
							hit = spr[h].strength - spr[j].defense;
						else {
							int top = Random::get(1, (int)ceil(spr[h].strength / 2.0f));
							if (debug_maxdam)
								top = (int)ceil(spr[h].strength / 2.0f);
							hit = ((spr[h].strength / 2) + top) - spr[j].defense;

						}

						if (hit < 0)
							hit = 0;
						spr[j].damage += hit;
						if (hit > 0)
							random_blood(spr[j].x, spr[j].y - 40, j);
						spr[j].last_hit = 1;
						//Msg("Damage done is %d..", spr[j].damage);
					}

					if (spr[j].script > 0) {
						//CHANGED did = h
						*pmissile_target = 1;

						if (scripting_proc_exists(spr[j].script, "HIT"))
						{
							scripting_kill_callbacks(spr[j].script);
							scripting_run_proc(spr[j].script, "HIT");
						}
					}
				}
				//run missile end
			}
	}
}

void missile_brain_expire(int h) {
	missile_brain(h, /*false*/ 0);
	if (spr[h].seq == 0)
		lsm_remove_sprite(h);
}
