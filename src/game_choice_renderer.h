#ifndef GAME_CHOICE_RENDERER_H
#define GAME_CHOICE_RENDERER_H
#include <inttypes.h>

extern void game_choice_renderer_render();
extern void scripting_set_choice_colour(uint8_t r, uint8_t g, uint8_t b);
extern void scripting_set_choice_hover_colour(uint8_t r, uint8_t g, uint8_t b);

#endif
