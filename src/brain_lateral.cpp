#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "brain.h"
#include "live_sprites_manager.h"
#include "game_engine.h"
#include "freedink.h"
#include "live_screen.h"
#include "gfx.h" /* GFX_RES_W -> a little off-screen ?? */
#include "log.h"
#include "random.hpp"

using Random = effolkronium::random_static;

/**
 * Enemy with lateral moves, e.g. dragon
 */
void dragon_brain(int h) {
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
				if (spr[h].brain == 10) {
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

	if (spr[h].target != 0)
		if ((unsigned)spr[h].attack_wait < thisTickCount) {
			if (spr[h].script != 0) {

				scripting_run_proc(spr[h].script, "ATTACK");
			}
		}

	if (spr[h].seq == 0) {
	recal:
		if (Random::get(1, 12) == 1) {
			hold = Random::get(1, 9);
			if ((hold != 1) && (hold != 3) && (hold != 7) && (hold != 9) &&
				(hold != 5)) {
				changedir(hold, h, spr[h].base_walk);
			}

		} else {
			//keep going the same way
			spr[h].seq = spr[h].seq_orig;
			if (spr[h].seq_orig == 0)
				goto recal;
		}
	}

	if (spr[h].y > playy)

	{
		changedir(8, h, spr[h].base_walk);
	}

	if (spr[h].x > GFX_RES_W) {
		changedir(4, h, spr[h].base_walk);
	}

	if (spr[h].y < 0) {
		changedir(2, h, spr[h].base_walk);
	}

	if (spr[h].x < 0) {
		changedir(6, h, spr[h].base_walk);
	}

	automove(h);

	if (check_if_move_is_legal(h) != 0)

	{

		int mydir = autoreverse(h);

		//	Msg("Real dir now is %d, autoresver changed to %d.",spr[h].dir, mydir);

		changedir(mydir, h, spr[h].base_walk);

		log_debug("🧭 real dir changed to %d", spr[h].dir);
	}
}
