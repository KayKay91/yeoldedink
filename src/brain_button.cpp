#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "brain.h"
#include "live_sprites_manager.h"
#include "rect.h"
#include "gfx_sprites.h"
#include "dinkc.h"
#include "scripting.h"

void button_brain(int h) {
	rect box;
	if (spr[h].move_active) {
		process_move(h);
		return;
	}

	if (spr[h].script == 0)
		return;

	rect_copy(&box, &k[getpic(h)].hardbox);
	rect_offset(&box, spr[h].x, spr[h].y);

	if (spr[h].brain_parm == 0) {
		if (inside_box(spr[1].x, spr[1].y, box)) {
			spr[h].brain_parm = 1;

			if (scripting_proc_exists(spr[h].script, "BUTTONON")) 
			{
				scripting_run_proc(spr[h].script, "BUTTONON");
				
				return;
			}
		}

	} else {
		if (!inside_box(spr[1].x, spr[1].y, box)) {
			spr[h].brain_parm = 0;

			if (scripting_proc_exists(spr[h].script, "BUTTONOFF")) 
			{
				
				scripting_run_proc(spr[h].script, "BUTTONOFF");
				return;
			}
		}
	}
}
