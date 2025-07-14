#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "brain.h"
#include "freedink.h"
#include "live_sprites_manager.h"
#include "game_engine.h"
#include "live_screen.h"
#include "log.h"
#include "random.hpp"

using Random = effolkronium::random_static;
int timer_npc_wait_min = 400;
int timer_npc_wait_max = 3400;

void find_action(int h) {

	spr[h].action = Random::get(1,2);

	if (spr[h].action == 1) {
		//sit and think
		spr[h].move_wait = thisTickCount + Random::get(timer_npc_wait_min, timer_npc_wait_max);
		if (spr[h].base_walk != -1) {
			int dir = Random::get({1, 3, 7, 9});
			spr[h].pframe = 1;
			spr[h].pseq = spr[h].base_walk + dir;
		}

		//ye: base idle
		/*
		if (spr[h].base_idle != -1) {
			log_error("Base idle is %d", spr[h].base_idle);
			spr[h].move_wait = thisTickCount + Random::get(timer_npc_wait_min, timer_npc_wait_max);
			int dir = Random::get(1, 9);
			spr[h].wait = 0;
			if (dir == 5)
				dir++;
			changedir(dir, h, spr[h].base_idle);
			spr[h].idle = 1;
		}
			*/

		return;
	}

	if (spr[h].action == 2) {
		//move
		spr[h].move_wait = thisTickCount + Random::get(500, 3500);
		int dir = Random::get(1, 4);
		spr[h].pframe = 1;
		if (dir == 1)
			changedir(1, h, spr[h].base_walk);
		if (dir == 2)
			changedir(3, h, spr[h].base_walk);
		if (dir == 3)
			changedir(7, h, spr[h].base_walk);
		if (dir == 4)
			changedir(9, h, spr[h].base_walk);
		return;
	}

	log_error("❌ Internal error:  Brain 16, unknown action.");
}

void people_brain(int h) {
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
				//ye: why is there a check here???
				if (spr[h].brain == 16) {
					if (spr[h].dir == 0)
						spr[h].dir = 3;
					spr[h].brain = 0;
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
		return;
	}

	if ((spr[h].move_wait < thisTickCount) && (spr[h].seq == 0)) {

		spr[h].action = 0;
	}

	if (spr[h].action == 0)
		find_action(h);

	if (spr[h].action != 2) {
		spr[h].seq = 0;
		return;
	}
	if (spr[h].seq_orig != 0)
		if (spr[h].seq == 0)
			spr[h].seq = spr[h].seq_orig;

	if (spr[h].y > playy)

	{

		if (Random::get<bool>())
			changedir(9, h, spr[h].base_walk);
		else
			changedir(7, h, spr[h].base_walk);
	}

	if (spr[h].x > playx)

	{
		if (Random::get<bool>())
			changedir(1, h, spr[h].base_walk);
		else
			changedir(7, h, spr[h].base_walk);
	}

	if (spr[h].y < 20) {
		if (Random::get<bool>())
			changedir(1, h, spr[h].base_walk);
		else
			changedir(3, h, spr[h].base_walk);
	}

	if (spr[h].x < 30) {
		if (Random::get<bool>())
			changedir(3, h, spr[h].base_walk);
		else
			changedir(9, h, spr[h].base_walk);
	}

	automove(h);

	if (check_if_move_is_legal(h) != 0) {
		if (Random::get(0, 2) == 2) {
			changedir(autoreverse_diag(h), h, spr[h].base_walk);

		} else {
			spr[h].move_wait = 0;
			spr[h].pframe = 1;
			spr[h].seq = 0;
		}
	}

	//				changedir(hold,h,spr[h].base_walk);
}
