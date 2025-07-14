#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include "brain.h"
#include "live_sprites_manager.h"
#include "live_screen.h"
#include "gfx_sprites.h"
//ye: makes sprite move around in a circle
//no known practical uses
void circle_brain(int h)
{
    //use brain parm to set radius
    //action to store tick
    float tick = spr[h].action / 100;
	spr[h].x = (int)(sin(tick) * spr[h].brain_parm) + (playx / 2);
    spr[h].y = (int)(cos(tick) * spr[h].brain_parm) + (playy / 2);
    spr[h].action += spr[h].brain_parm2;
}
