#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "brain.h"
#include "live_sprites_manager.h"
#include "live_screen.h"
#include "gfx_sprites.h"
#include "gfx.h" /* GFX_RES_W -> a little off-screen ?? */

void bounce_brain(int h) {
	if (spr[h].y > (playy - k[getpic(h)].box.bottom)) {
		spr[h].my -= (spr[h].my * 2);
	}

	if (spr[h].x > (GFX_RES_W - k[getpic(h)].box.right)) {
		spr[h].mx -= (spr[h].mx * 2);
	}

	if (spr[h].y < 0) {
		spr[h].my -= (spr[h].my * 2);
	}

	if (spr[h].x < 0) {
		spr[h].mx -= (spr[h].mx * 2);
	}

	spr[h].x += spr[h].mx;
	spr[h].y += spr[h].my;
}

//ye: like the normal brain except stops according to hardbox
//also adjusts for noclip
void bounce_brain_superior(int h) {
    int ybounds = playy;
    int xbounds = playx;
    int xmin = playl;

    if (spr[h].noclip) {
        ybounds = GFX_RES_H;
        xbounds = GFX_RES_W;
        xmin = 0;
    }
	if (spr[h].y > (ybounds - k[getpic(h)].hardbox.bottom)) {
		spr[h].my -= (spr[h].my * 2);
	}

	if (spr[h].x > (xbounds - k[getpic(h)].hardbox.right)) {
		spr[h].mx -= (spr[h].mx * 2);
	}

	if (spr[h].y < 0 - k[getpic(h)].hardbox.top) {
		spr[h].my -= (spr[h].my * 2);
	}

	if (spr[h].x < 0 - (k[getpic(h)].hardbox.left - xmin)) {
		spr[h].mx -= (spr[h].mx * 2);
	}

	spr[h].x += spr[h].mx;
	spr[h].y += spr[h].my;
}
