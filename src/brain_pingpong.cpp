#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "brain.h"
#include "live_sprites_manager.h"
#include "live_screen.h"
//plays the sequence forwards and backwards sort of like brain 6
//TODO: refactor this into brain_repeat using brain_parm
void pingpong_brain(int h) {

	if (spr[h].move_active) {
		process_move(h);
		//		return;
	}

	if (spr[h].seq_orig == 0) {
		if (spr[h].sp_index != 0) {
			spr[h].seq_orig = cur_ed_screen.sprite[spr[h].sp_index].seq;
			spr[h].frame = cur_ed_screen.sprite[spr[h].sp_index].frame;
			spr[h].wait = 0;

			//cur_ed_screen.sprite[spr[h].sp_index].frame;
		}
		else {
			spr[h].seq_orig = spr[h].pseq;
		}
    }

	if (spr[h].seq == 0) {
		spr[h].seq = spr[h].seq_orig;
		spr[h].reverse = !(bool)spr[h].reverse;
    }
}
