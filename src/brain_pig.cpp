#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "brain.h"
#include "game_engine.h"
#include "live_sprites_manager.h"
#include "freedink.h"
#include "live_screen.h"
#include "gfx_sprites.h"
#include "gfx.h" /* GFX_RES_W -> a little off-screen ?? */
#include "sfx.h"
#include "random.hpp"

using Random = effolkronium::random_static;
int gfx_pig_death_seq = 164;
int timer_pig_min = 200;
int timer_pig_max = 500;

void pig_brain(int h) {
	int hold;

	if (spr[h].move_active) {
		process_move(h);
		return;
	}

	if ((spr[h].damage > 0)) {
		//SoundPlayEffect( 1,3000, 800 );
		draw_damage(h);
		spr[h].hitpoints -= spr[h].damage;
		spr[h].damage = 0;
		if (spr[h].hitpoints < 1) {
			add_exp(spr[h].exp, h);
			spr[h].damage = 0;
			//lets kill the duck here, ha.
			check_for_kill_script(h);
			spr[h].speed = 0;
			spr[h].base_walk = -1;
			spr[h].seq = gfx_pig_death_seq;
			spr[h].brain = 7;
			if (debug_nohitfix)
				spr[h].nohit = 1;
		}

		return;
	}

	if (spr[h].freeze)
		return;

	if (spr[h].seq == 0) {

		if (Random::get(1, 12) == 1) {
			hold = Random::get(1, 9);

			if ((hold != 4) && (hold != 6) && (hold != 2) && (hold != 8) &&
				(hold != 5)) {
				changedir(hold, h, spr[h].base_walk);

			} else {
				//int junk = spr[h].size;

				//if (junk >= 100)
				//	junk = 18000 - (junk * 50);

				//if (junk < 100)
				//∂	junk = 16000 + (junk * 100);

				//hold = Random::get(1, 4);

				if (!playing(spr[h].last_sound))
					spr[h].last_sound = 0;

				if (spr[h].last_sound == 0) {
					spr[h].last_sound = sfx_play_pig(spr[h].size, h);
					/*
					if (hold == 1)
						spr[h].last_sound = SoundPlayEffect(2, junk, 800, h, 0);
					if (hold == 2)
						spr[h].last_sound = SoundPlayEffect(3, junk, 800, h, 0);
					if (hold == 3)
						spr[h].last_sound = SoundPlayEffect(4, junk, 800, h, 0);
					if (hold == 4)
						spr[h].last_sound = SoundPlayEffect(5, junk, 800, h, 0);
						*/
				}

				spr[h].mx = 0;
				spr[h].my = 0;
				spr[h].wait = thisTickCount + Random::get(timer_pig_min, timer_pig_max);
			}

		} else {

			if ((spr[h].mx != 0) || (spr[h].my != 0))

			{
				spr[h].seq = spr[h].seq_orig;
			}
		}
	}

	if (spr[h].y > (playy - k[getpic(h)].box.bottom / 4)) {
		changedir(9, h, spr[h].base_walk);
	}

	if (spr[h].x > (GFX_RES_W - k[getpic(h)].box.right - 10)) {
		changedir(1, h, spr[h].base_walk);
	}

	if (spr[h].y < 10) {
		changedir(1, h, spr[h].base_walk);
	}

	if (spr[h].x < 10) {
		changedir(3, h, spr[h].base_walk);
	}

	automove(h);

	if (check_if_move_is_legal(h) != 0)

	{
		changedir(autoreverse_diag(h), h, spr[h].base_walk);
	}
}
