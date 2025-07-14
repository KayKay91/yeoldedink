#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "brain.h"
#include "live_sprites_manager.h"
#include "gfx_sprites.h"
#include "log.h"
#include "freedink.h"
#include "game_engine.h"
#include "live_screen.h"
#include "random.hpp"
#include "debug.h"

using Random = effolkronium::random_static;

void process_target(int h) {
	if (spr[h].target > MAX_SPRITES_AT_ONCE) {
		log_error("❌ Sprite %d cannot 'target' sprite %d??", h, spr[h].target);
		return;
	}

	if (!spr[spr[h].target].active) {
		log_debug("Killing target");
		spr[h].target = 0;
		return;
	}

	int dir;
	int distance = get_distance_and_dir(h, spr[h].target, &dir);

	if (distance < spr[h].distance)
		return;

	changedir(dir, h, spr[h].base_walk);

	automove(h);
}

/**
 * Enemy with diagonal moves, e.g. pillbug
 */
void pill_brain(int h) {
	int hold;

	if (spr[h].damage > 0) {
		//got hit
		//SoundPlayEffect( 1,3000, 800 );
		if (spr[h].hitpoints > 0) {
			draw_damage(h);
			if (spr[h].damage > spr[h].hitpoints)
				spr[h].damage = spr[h].hitpoints;
			spr[h].hitpoints -= spr[h].damage;

			if (spr[h].hitpoints < 1) {
				//they killed it
				check_for_kill_script(h);

				if (spr[h].brain == 9) // i.e. not overriden in sprite's die()
				{
					if (spr[h].dir == 0)
						spr[h].dir = 3;
					change_dir_to_diag(&spr[h].dir);
					add_kill_sprite(h);
					lsm_remove_sprite(h);
				}
				return;
			}
		}
		spr[h].damage = 0;
	}

	if (spr[h].move_active) {
		process_move(h);
		return;
	}

	if (spr[h].freeze)
		return;

	if (spr[h].follow > 0) {
		process_follow(h);
	}

	if (spr[h].target != 0) {

		if (in_this_base(spr[h].seq, spr[h].base_attack)) {
			//still attacking
			return;
		}

		int dir;
		if (spr[h].distance == 0)
			spr[h].distance = 5;
		int distance = get_distance_and_dir(h, spr[h].target, &dir);

		if (distance < spr[h].distance)
			if ((unsigned)spr[h].attack_wait < thisTickCount) {
				//	Msg("base attack is %d.",spr[h].base_attack);
				if (spr[h].base_attack != -1) {
					//Msg("attacking with %d..", spr[h].base_attack+dir);

					/* Enforce lateral (not diagonal) attack,
			even in smooth_follow mode */
					int attackdir = 6; // arbitrary initialized default
					get_distance_and_dir_nosmooth(h, spr[h].target, &attackdir);
					spr[h].dir = attackdir;

					spr[h].seq = spr[h].base_attack + spr[h].dir;
					spr[h].frame = 0;

					if (spr[h].script != 0) {
						if (!scripting_run_proc(spr[h].script, "ATTACK"))
							spr[h].move_wait = thisTickCount + Random::get(10, 310);
					}
					return;
				}
			}

		if (spr[h].move_wait < thisTickCount) {
			process_target(h);
			spr[h].move_wait = thisTickCount + 200;

		} else {
			/*	automove(h);

		if (check_if_move_is_legal(h) != 0)
		{

			}
			*/

			goto walk_normal;
		}

		return;
	}

walk_normal:

	if (spr[h].base_walk != -1) {
		if (spr[h].seq == 0)
			goto recal;
	}

	if ((spr[h].seq == 0) && (spr[h].move_wait < thisTickCount)) {
	recal:
		if (Random::get(1,12) == 1) {
			hold = Random::get(1, 9);
			if ((hold != 4) && (hold != 6) && (hold != 2) && (hold != 8) &&
				(hold != 5)) {
				changedir(hold, h, spr[h].base_walk);
				spr[h].move_wait = thisTickCount + Random::get(200, 2200);
			}

		} else {
			//keep going the same way
			if (in_this_base(spr[h].seq_orig, spr[h].base_attack))
				goto recal;
			spr[h].seq = spr[h].seq_orig;
			if (spr[h].seq_orig == 0)
				goto recal;
		}
	}
	//ye: seseler's fix for edge-sticking
	if (dbg.monsteredgecol)
	{
		if (spr[h].y > (playy - 15) || spr[h].x > (playx - 15) || spr[h].y < 18 || spr[h].x < 18)
		{
			int dir = 1;
			hold = Random::get(20, 619);
			if (spr[h].x < hold)
			{
				dir += 2;
			}
			hold = Random::get(1, 399);
			if (spr[h].y > hold)
			{
				dir += 6;
			}
			changedir(dir, h, spr[h].base_walk);
		}
	}
	else
	{

	if (spr[h].y > (playy - 15))

	{
		changedir(9, h, spr[h].base_walk);
	}

	if (spr[h].x > (playx - 15))

	{
		changedir(1, h, spr[h].base_walk);
	}

	if (spr[h].y < 18) {
		changedir(1, h, spr[h].base_walk);
	}

	if (spr[h].x < 18) {
		changedir(3, h, spr[h].base_walk);
	}

	}

	automove(h);

	if (check_if_move_is_legal(h) != 0) {
		spr[h].move_wait = thisTickCount + 400;
		changedir(autoreverse_diag(h), h, spr[h].base_walk);
	}

	//				changedir(hold,h,spr[h].base_walk);
}
