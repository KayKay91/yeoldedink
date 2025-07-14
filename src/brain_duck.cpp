#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "brain.h"
#include "game_engine.h"
#include "live_sprites_manager.h"
#include "freedink.h"
#include "game_engine.h"
#include "live_screen.h"
#include "gfx_sprites.h"
#include "sfx.h"
#include "dinkc.h"
#include "random.hpp"

using Random = effolkronium::random_static;

int gfx_duck_death_seq = 164;
int timer_duck_min = 200;
int timer_duck_max = 500;

/*bool*/ int check_for_duck_script(int i) {

	if (spr[i].script > 0) {
		//if (  (spr[i].brain == 0) | (spr[i].brain == 5) | (spr[i].brain == 6) | (spr[i].brain == 7))

		scripting_run_proc(spr[i].script, "DUCKDIE");

		return (/*true*/ 1);
	}

	return (/*false*/ 0);
}

void kill_sprite_all(int sprite) {
	lsm_remove_sprite(sprite);
	kill_text_owned_by(sprite);
	scripting_kill_scripts_owned_by(sprite);
}

void duck_brain(int h) {
	int hold;

	if ((spr[h].damage > 0) && (in_this_base(spr[h].pseq, 110))) {

		check_for_duck_script(h);

		//hit a dead duck
		//ye: explosion created
		int crap2 = add_sprite(spr[h].x, spr[h].y, 7, gfx_duck_death_seq, 1);
		/* TODO: add_sprite might return 0, and the following
would trash spr[0] - cf. bugs.debian.org/688934 */
		spr[crap2].speed = 0;
		spr[crap2].base_walk = 0;
		spr[crap2].seq = gfx_duck_death_seq;

		if (debug_nohitfix)
			spr[crap2].nohit = 1;
		draw_damage(h);
		spr[h].damage = 0;
		add_exp(spr[h].exp, h);

		kill_sprite_all(h);

		return;
	}

	if ((spr[h].damage > 0) && (in_this_base(spr[h].pseq, spr[h].base_walk))) {
		//SoundPlayEffect( 1,3000, 800 );
		//ye: duck made headless?
		draw_damage(h);
		add_exp(spr[h].exp, h);
		spr[h].damage = 0;

		//lets kill the duck here, ha.
		check_for_kill_script(h);
		spr[h].follow = 0;
		int crap = add_sprite(spr[h].x, spr[h].y, 5, 1, 1);
		/* TODO: add_sprite might return 0, and the following
would trash spr[0] - cf. bugs.debian.org/688934 */
		spr[crap].speed = 0;
		spr[crap].base_walk = 0;
		spr[crap].size = spr[h].size;
		spr[crap].speed = Random::get(1,3);

		if (debug_nohitfix)
			spr[crap].nohit = 1;

		spr[h].base_walk = 110;
		spr[h].speed = 1;
		spr[h].timing = 0;
		spr[h].wait = 0;
		spr[h].frame = 0;

		if (spr[h].dir == 0)
			spr[h].dir = 1;
		if (spr[h].dir == 4)
			spr[h].dir = 7;
		if (spr[h].dir == 6)
			spr[h].dir = 3;

		changedir(spr[h].dir, h, spr[h].base_walk);
		spr[crap].dir = spr[h].dir;
		spr[crap].base_walk = 120;
		changedir(spr[crap].dir, crap, spr[crap].base_walk);

		automove(h);
		return;
	}

	if (spr[h].move_active) {
		process_move(h);
		return;
	}

	if (spr[h].freeze) {
		return;
	}

	if (spr[h].follow > 0) {
		process_follow(h);
		return;
	}

	if (spr[h].base_walk == 110) {
		if (Random::get(1, 100) == 1)
			random_blood(spr[h].x, spr[h].y - 18, h);
		goto walk;
	}

	if (spr[h].seq == 0) {

		if (Random::get(1, 12) == 1) {
			hold = Random::get(1, 9);

			if ((hold != 2) && (hold != 8) && (hold != 5)) {

				//Msg("random dir change started.. %d", hold);
				changedir(hold, h, spr[h].base_walk);

			} else {
				//int junk = spr[h].size;
			/*
				if (junk >= 100)
					junk = 18000 - (junk * 50);

				if (junk < 100)
					junk = 16000 + (junk * 100);

				*/

				sfx_play_duck(spr[h].size, h);
				spr[h].mx = 0;
				spr[h].my = 0;
				spr[h].wait = thisTickCount + Random::get(timer_duck_min, timer_duck_max);
			}
			return;
		}

		if ((spr[h].mx != 0) || (spr[h].my != 0))

		{
			spr[h].seq = spr[h].seq_orig;
		}
	}

walk:
	if (spr[h].y > playy)

	{
		changedir(9, h, spr[h].base_walk);
	}

	if (spr[h].x > playx - 30)

	{
		changedir(7, h, spr[h].base_walk);
	}

	if (spr[h].y < 10) {
		changedir(1, h, spr[h].base_walk);
	}

	if (spr[h].x < 30) {
		changedir(3, h, spr[h].base_walk);
	}

	//   Msg("Duck dir is %d, seq is %d.", spr[h].dir, spr[h].seq);
	automove(h);

	if (check_if_move_is_legal(h) != 0)

	{
		if (spr[h].dir != 0)
			changedir(autoreverse_diag(h), h, spr[h].base_walk);
	}
}
