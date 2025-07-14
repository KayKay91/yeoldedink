/**
 * DinkC script engine

 * Copyright (C) 1997, 1998, 1999, 2002, 2003  Seth A. Robinson
 * Copyright (C) 2005, 2006  Dan Walma
 * Copyright (C) 2005, 2007, 2008, 2009, 2010, 2011, 2014, 2015  Sylvain Beucler
 * Copyright (C) 2013  Alexander Krivács Schrøder
 * Copyright (C) 2022 CRTS
 * Copyright (c) 2024 Yeoldetoast

 * This file is part of GNU FreeDink

 * GNU FreeDink is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.

 * GNU FreeDink is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>
#include <unordered_map>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* compare */
#include "soloud.h"
#include "random.hpp"

#include "libintl.h"
#define _(String) gettext(String)

#include "dinkc.h"
#include "dinkc_bindings.h"
#include "game_engine.h"
#include "live_sprites_manager.h"
#include "input.h"
#include "paths.h"
#include "str_util.h"
#include "log.h"
#include "sfx.h"
#include "editor_screen.h"
#include "game_choice.h"
#include "i18n.h"
#include "debug.h"
#include "debug_imgui.h"
#include "scripting.h"
#include "gnulib.h"

using Random = effolkronium::random_static;
//for the scripting backend.
int dinkc_enabled = 1;

/* store current procedure arguments expanded values of type 'int' (see get_parms) */
static int nlist[10];
/* store current procedure arguments of type 'string' (idem) */
static char* slist[10];
char* cur_funcname = NULL;
//&return in 1.08
int returnint = 0;
int bKeepReturnInt = 0;
char returnstring[200];
/* Used to tell decipher_string about the currently selected savegame
in a choice menu; also abuse to tell which key is selected in
joystick remapping */
unsigned short decipher_savegame = 0;

/* Number of reserved ASCII indexes in .d BPE compression format */
#define NB_PAIRS_MAX 128

//struct refinfo* rinfo[MAX_SCRIPTS];
char* rinfo_code[MAX_SCRIPTS];
#define rinfo(script) ((struct refinfo*)(sinfo[script]->data))

int weapon_script = 0;
int magic_script = 0;

static enum dinkc_parser_state process_line(int script, char* s,
											/*bool*/ int doelse);

/**
 * Decompress a .d DinkC script; also clean newlines. Check
 * contrib/d2c.c for more explanation about the decompression process.
 */
static char* byte_pair_decompress_stream(FILE* in) {
	int step = 512;
	int nb_read = 0;
	char* ret = (char*)xmalloc(step);
	ret[0] = '\0';

	unsigned char stack[NB_PAIRS_MAX + 1], pairs[NB_PAIRS_MAX][2];
	short c, top = -1;
	int nb_pairs = 0;

	/* Check for optional pair count and pair table */
	if ((c = fgetc(in)) > 127) {
		/* Read pairs table */
		nb_pairs = c - 128;
		int i, j;
		for (i = 0; i < nb_pairs; i++) {
			for (j = 0; j < 2; j++) {
				int c = fgetc(in);
				if (c == EOF) {
					log_error("🗜️ decompress: invalid header: truncated pair table");
					free(ret);
					return NULL;
				}
				if (c > i + 128) {
					log_error("🗜️ decompress: invalid header: reference to a pair that is not registered yet");
					free(ret);
					return NULL;
				}
				pairs[i][j] = c;
			}
		}
	} else {
		/* Non-compressed file, put back the character we read */
		ungetc(c, in);
	}

	for (;;) {
		/* Pop byte from stack or read byte from file */
		if (top >= 0)
			c = stack[top--];
		else if ((c = fgetc(in)) == EOF)
			break;

		/* Push pair on stack or output byte to file */
		if (c > 127) {
			if ((c - 128) >= nb_pairs) {
				log_error("🗜️ decompress: invalid body: references non-existent pair %d", c);
				break;
			}
			stack[++top] = pairs[c - 128][1];
			stack[++top] = pairs[c - 128][0];
		} else {
			ret[nb_read] = c;
			nb_read++;
			if ((nb_read % step) == 0)
				ret = (char*)xrealloc(ret, nb_read + step);
		}
	}
	ret[nb_read] = '\0'; /* safety */
	ret = (char*)xrealloc(ret, nb_read + 1);

	return ret;
}

static char* read_stream(FILE* in) {
	int step = 512;
	int nb_read = 0;
	char* ret = (char*)xmalloc(step);
	ret[0] = '\0';

	int c;
	while ((c = getc(in)) != EOF) {
		ret[nb_read] = c;
		nb_read++;
		if ((nb_read % step) == 0)
			ret = (char*)xrealloc(ret, nb_read + step);
	}
	ret[nb_read] = '\0'; /* safety */
	ret = (char*)xrealloc(ret, nb_read + 1);

	return ret;
}

/**
 * Only load game metadata (timetime). Used when displaying the list
 * of saved games (see decipher_string).
 */
//static /*bool*/ int load_game_small(int num, char line[196], int* mytime) {
//	FILE* f = paths_savegame_fopen(num, "rb");
//	if (f == NULL) {
//		log_info("💾 Couldn't quickload save game %d", num);
//		return /*false*/ 0;
//	} else {
//		//int version = read_lsb_int(f);
//		fseek(f, 4, SEEK_CUR);
//
//		fread(line, 196, 1, f);
//		line[195] = '\0';
//		*mytime = read_lsb_int(f);
//		fclose(f);
//
//		return /*true*/ 1;
//	}
//}

/* Find available script slot */
int script_find_slot() {
	log_enter("script_find_slot");
	int k = 1;
	for (k = 1; k < MAX_SCRIPTS; k++)
		if (sinfo[k] == NULL)
			break;

	if (k < MAX_SCRIPTS) {
		log_exit("script_find_slot: %d", k);
		return k;
	} else {
		log_exit("script_find_slot: %d", -1);
		return -1;
	}
}

/**
 * Initialize script structure
 */
static int dinkc_allocate_data(void **data)
{
  *data = XZALLOC(struct refinfo);
  if (*data == NULL)
  {
    return 0;
	log_error("🚛 Failed to allocate DinkC Data");
  }
  memset(*data, 0, sizeof(struct refinfo));
  return 1;
}

static int script_init(const char* name, char* code, int sprite) {
	log_enter("script_init: name: %s, code: %s", name, code);
	int script = sprite;
	//if (script <= 0) {
	//	log_exit("script_init: %d", -1);
	//	return -1;
	//}

	//rinfo(script) = XZALLOC(struct refinfo);


	//if (rinfo[script] == NULL) {
	//	log_exit("script_init: %d", -1);
	//	return -1;
	//}

	//memset(rinfo(script), 0, sizeof(struct refinfo));

	//sinfo[script]->name = strdup(name);
	/* For clarity: */
	//rinfo(script)->current = 0;
	//rinfo(script)->cur_line = 1;
	//rinfo(script)->cur_col = 0;
	//rinfo(script)->debug_line = 1;

	rinfo_code[script] = code;
	rinfo(script)->end = strlen(code);

	log_exit("script_init: %d", script);
	return script;
}

/* export (non-static) for the test suite */
int ts_script_init(const char* name, char* code) {
	return script_init(name, code, 1);
}

/**
 * Start a simple script, usually from the script console
 */
int dinkc_execute_one_liner(char* line) {
	log_enter("dinkc_execute_one_liner: line: %s", line);
	char* code = strdup(line);
	//int script = script_init("one-liner", code);
	int script = script_find_slot();
	if (script <= 0) {
		free(code);
		log_exit("dinkc_execute_one_liner: %d", -1);
		return -1;
	}

	struct script_engine *script_engine;
    //find the index of DinkC in the engines list
	for (int i = 0; (script_engine = &script_engines[i])->active; i++)
  	{
		//log_error("Scrip engine name is %s", &script_engine->name);
	if (!strcmp(script_engine->name, "DinkC"))
    {
      script_engine = &script_engines[i];
	  break;
    }
  }

    //allocate
    sinfo[script] = XZALLOC(struct scriptinfo);
    if (sinfo[script] == NULL)
    {
        log_error("🚛 Couldn't allocate script %d.", script);
        return -1;
    }
    memset(sinfo[script], 0, sizeof(struct scriptinfo));

	void *data;
    data = XZALLOC(struct refinfo);
    sinfo[script]->data = data;


    sinfo[script]->engine = script_engine;
    sinfo[script]->name = strdup("one-liner");
	//attach it to the screen instead of running permanently?
    sinfo[script]->sprite = 0;
    rinfo_code[script] = strdup(code);
    rinfo(script)->end = strlen(code);

	//rinfo(script)->sprite = 1000; /* survive screen change */
	rinfo(script)->level = 1; /* skip 'void main(void) {' parsing */

	process_line(script, rinfo_code[script], 0);
	log_exit("dinkc_execute_one_liner: %d", returnint);
	return returnint;
}

static FILE* locate_script(char* script_name, int* compressed) {
	log_enter("locate_script: script_name: %s, compressed: %d",
			script_name, *compressed);
	char temp[100];
	FILE* in = NULL;

	log_info("🗄️ LOADING %s", script_name);

	//sprintf(temp, "story/%s.d", script_name);
	sprintf(temp, "%s", script_name);
	//in = paths_dmodfile_fopen(temp, "rb");
	in = fopen(temp, "rb");
	if (in == NULL) {
		sprintf(temp, "story/%s.c", script_name);
		in = paths_dmodfile_fopen(temp, "rb");
		if (in == NULL) {
			sprintf(temp, "story/%s.d", script_name);
			in = paths_fallbackfile_fopen(temp, "rb");
			if (in == NULL) {
				sprintf(temp, "story/%s.c", script_name);
				in = paths_fallbackfile_fopen(temp, "rb");
				if (in == NULL) {
					log_exit("locate_script: NULL");
					return NULL;
				}
			}
		}
	}

	strtoupper(temp);
	log_debug("🔎 Located %s", temp);
	if (temp[strlen(temp) - 1] == 'D')
		*compressed = 1;
	else
		*compressed = 0;

	log_exit("locate_script");
	return in;
}

/**
 * Load script from 'filename', save it in the first available script
 * slot, attach to game sprite #'sprite' if 'set_sprite' is 1.
 **/
int dinkc_load_script(char* script_name, int sprite) {
	log_enter("load_script: script_name: %s, sprite: %d", script_name, sprite);
	// Locate script
	int compressed = 1;
	FILE* in = locate_script(script_name, &compressed);
	if (in == NULL) {
		log_warn("⛔ Script %s not found. (checked for .C and .D) (requested by "
				"%d?)",
				script_name, sprite);
		log_exit("load_script: %d", 0);
		return 0;
	}

	// Read script
	char* script_code = NULL;
	if (compressed) {
		log_debug("🗜️ Decompressing...");
		script_code = byte_pair_decompress_stream(in);
	} else {
		log_debug("📖 Reading from disk...");
		script_code = read_stream(in);
	}
	fclose(in);

	if (script_code == NULL) {
		log_error("❗ Error reading %s", script_name);
		log_exit("load_script: %d", 0);
		return 0;
	}

	// Create script
	//int script = script_init(script_name, script_code, sprite);
	//hacked this up from the above
	rinfo_code[sprite] = script_code;
	rinfo(sprite)->end = strlen(script_code);
	int script = sprite;
	log_info("📂 Loading script %s.. (slot %d)", script_name, script);
	if (script < 0) {
		log_error("❗ Couldn't find unused buffer for script.");
		free(script_code);
		log_exit("load_script: %d", 0);
		return 0;
	}
	//defined in scripting.cpp
	//sinfo[script]->sprite = sprite;

	log_exit("load_script: %d", script);
	return script;
}

/**
 * Remove leading spaces by shifting 'str' to the left, as much as
 * there is leading spaces.
 */
void strip_beginning_spaces(char* str) {
	char* pc = str;
	int diff = 0;

	/* Find first non-space character (pos) */
	while (*pc == ' ')
		pc++;
	diff = pc - str;

	/* Shift string to the left from pos */
	/* Don't use str(str, pc) to avoid memory overlap */
	while (*pc != '\0') {
		*(pc - diff) = *pc;
		pc++;
	}
	*(pc - diff) = '\0';
}

/**
 * Locate a procedure (such as "void hit()")
 */
/*bool*/ int locate(int script, char* lookup_proc) {
	log_enter("locate: script: %d, lookup_proc: %s", script, lookup_proc);
	if (rinfo(script) == NULL) {
		log_exit("locate: %d", 0);
		return 0;
	}

	int save_current = rinfo(script)->current;
	int save_cur_line = rinfo(script)->cur_line;
	int save_cur_col = rinfo(script)->cur_col;
	int save_debug_line = rinfo(script)->debug_line;
	rinfo(script)->current = 0;
	rinfo(script)->cur_line = 1;
	rinfo(script)->cur_col = 0;
	rinfo(script)->debug_line = 1;

	char* line = NULL;
	char* word = NULL;

	while ((line = read_next_line(script)) != NULL) {
		strip_beginning_spaces(line);

		int is_proc = 0;
		word = get_word(line, 1);
		if (compare(word, "VOID"))
			is_proc = 1;
		free(word);
		if (is_proc) {
			char* cur_proc = NULL;
			word = get_word(line, 2);
			cur_proc = separate_string(word, 1, '(');
			free(word);

			int is_right_proc = 0;
			if (compare(cur_proc, lookup_proc))
				is_right_proc = 1;
			free(cur_proc);

			if (is_right_proc) {
				//clean up vars so it is ready to run
				if (sinfo[script]->sprite != 1000) {
					// TODO: move out so we don't depend on 'spr'
					// reset move/move_stop moves
					spr[sinfo[script]->sprite].move_active = 0;
					if (dversion >= 108)
						// also reinit move_nohard as in brain.cpp:done_move
						spr[sinfo[script]->sprite].move_nohard = 0;
				}
				rinfo(script)->skipnext = /*false*/ 0;
				rinfo(script)->onlevel = 0;
				rinfo(script)->level = 0;

				free(line);
				log_exit("locate: %d", 1);
				return 1;
				//this is desired proc
			}
		}
		free(line);
	}

	// Not found, restoring position
	rinfo(script)->current = save_current;
	rinfo(script)->cur_line = save_cur_line;
	rinfo(script)->cur_col = save_cur_col;
	rinfo(script)->debug_line = save_debug_line;
	log_exit("locate: %d", 0);
	return 0;
}

/**
 * Look for the 'label' label (e.g. 'loop:'), that is used by a "goto"
 * instruction. This sets the script->current field appropriately.
 **/
/*bool*/ int locate_goto(char* expr, int script) {
	//log_enter("locate_goto: expr: %s, script: %d, scriptname: %s", expr,
	//		script, RINFO_NAME(script));
	replace_norealloc(";", "", expr);
	char* label = (char*)calloc(1, strlen(expr) + 1 + 1);
	sprintf(label, "%s:", expr);

	char* line = NULL;
	rinfo(script)->current = 0;
	rinfo(script)->cur_line = 1;
	while ((line = read_next_line(script)) != NULL) {
		strip_beginning_spaces(line);

		int is_right_label = 0;
		char* word = get_word(line, 1);
		replace_norealloc("\n", "", word);
		if (compare(word, label))
			is_right_label = 1;
		free(word);

		if (is_right_label) {
			log_debug("↩️ Found goto : Line is %s, word is %s.", line, label);

			rinfo(script)->skipnext = /*false*/ 0;
			rinfo(script)->onlevel = 0;
			rinfo(script)->level = 0;

			free(label);
			free(line);
			log_exit("locate_goto: %d", 1);
			return 1;
			//this is desired label
		}
		free(line);
	}

	log_warn("↩️ %s: cannot goto %s", sinfo[script]->name, label);
	free(label);
	log_exit("locate_goto: %d", 0);
	return 0;
}

/**
 * Lookup variable with this precise scope
 */
int lookup_var(char* variable, int scope) {
	log_enter("lookup_var: variable: %s, scope: %d", variable, scope);
	int i;
	for (i = 1; i < MAX_VARS; i++)
		if (play.var[i].active && compare(play.var[i].name, variable) &&
				scope == play.var[i].scope) {
			log_exit("lookup_var: %d", i);
			return i;
		}
	log_exit("lookup_var: %d", 0);
	return 0;
}

/**
 * v1.07-style scope. This function is buggy: the first memory slot
 * has precedence (independently of local/global scope).
 *
 * Return -1 if not found, slot index >1 if found. Slot 0 isn't
 * currently used by the engine.
 */
static int lookup_var_local_global_107(char* variable, int var_scope) {
	log_enter("lookup_var_local_global_107: variable: %s, var_scope: %d", variable, var_scope);
	int i;
	for (i = 1; i < MAX_VARS; i++)
		if (play.var[i].active == 1 &&
				((play.var[i].scope == DINKC_GLOBAL_SCOPE) ||
				(play.var[i].scope == var_scope)) &&
				(compare(play.var[i].name, variable))) {
			log_exit("lookup_var_local_global_107: %d", i);
			return i;
		}
	log_exit("lookup_var_local_global_107: %d", -1);
	return -1; /* not found */
}

/**
 * v1.08-style scope: local variables are searched before global
 * variables.
 *
 * Return -1 if not found, slot index >1 if found. Slot 0 isn't
 * currently used by the engine.
 */
static int lookup_var_local_global_108(char* variable, int var_scope) {
	log_enter("lookup_var_local_global_108: variable: %s, var_scope: %d", variable, var_scope);
	int search_scope[2];
	search_scope[0] = var_scope; /* first local scope */
	search_scope[1] = DINKC_GLOBAL_SCOPE; /* then global scope */

	int i;
	for (i = 0; i < 2; i++) {
		//We'll start going through every var, starting at one
		int v;
		for (v = 1; v < MAX_VARS; v++) {
			//Okay... make sure the var is active,
			//The scope should match the script,
			//Then make sure the name is the same.
			if (play.var[v].active && play.var[v].scope == search_scope[i] &&
					compare(play.var[v].name, variable)) {
				log_exit("lookup_var_local_global_108: %d", v);
				return v;
			}
		}
	}
	log_exit("lookup_var_local_global_108: %d", -1);
	return -1;
}

/**
 * Lookup variable with local->global nested scope
 */
static int lookup_var_local_global(char* variable, int scope) {
	if (dversion >= 108)
		return lookup_var_local_global_108(variable, scope);
	else
		return lookup_var_local_global_107(variable, scope);
}

/* Test suite proxy */
int ts_lookup_var_local_global(char* variable, int scope) {
	return lookup_var_local_global(variable, scope);
}

/**
 * Expand 'variable' in the scope of 'script' and return the integer
 * value. Only used in function 'get_parms'.
 */
long decipher(char* variable, int script) {
	//log_enter("decipher: variable: %s, script: %d, scriptname: %s",
	//		variable, script, RINFO_NAME(script));
	// Special vars: &current_sprite and &current_script
	if (compare(variable, "&current_sprite")) {
		log_exit("decipher: %d", sinfo[script]->sprite);
		return sinfo[script]->sprite;
	}
	if (compare(variable, "&current_script")) {
		log_exit("decipher: %d", script);
		return script;
	}

	//v1.08 special variables.
	if (dversion >= 108) {
		if (compare(variable, "&return")) {
			log_exit("decipher: %d", returnint);
			return returnint;
		}
		if (compare(variable, "&arg1")) {
			log_exit("decipher: %d", rinfo(script)->arg1);
			return rinfo(script)->arg1;
		}
		if (compare(variable, "&arg2")) {
			log_exit("decipher: %d", rinfo(script)->arg2);
			return rinfo(script)->arg2;
		}
		if (compare(variable, "&arg3")) {
			log_exit("decipher: %d", rinfo(script)->arg3);
			return rinfo(script)->arg3;
		}
		if (compare(variable, "&arg4")) {
			log_exit("decipher: %d", rinfo(script)->arg4);
			return rinfo(script)->arg4;
		}
		if (compare(variable, "&arg5")) {
			log_exit("decipher: %d", rinfo(script)->arg5);
			return rinfo(script)->arg5;
		}
		if (compare(variable, "&arg6")) {
			log_exit("decipher: %d", rinfo(script)->arg6);
			return rinfo(script)->arg6;
		}
		if (compare(variable, "&arg7")) {
			log_exit("decipher: %d", rinfo(script)->arg7);
			return rinfo(script)->arg7;
		}
		if (compare(variable, "&arg8")) {
			log_exit("decipher: %d", rinfo(script)->arg8);
			return rinfo(script)->arg8;
		}
		if (compare(variable, "&arg9")) {
			log_exit("decipher: %d", rinfo(script)->arg9);
			return rinfo(script)->arg9;
		}
	}

	// Check in local and global variables
	int i = lookup_var_local_global(variable, script);
	if (i != -1) {
 		log_exit("decipher: %d", play.var[i].var);
 		return play.var[i].var;
	} else {
		log_exit("decipher: %d", 0);
		return 0; // compatibility
	}
}

/**
 * Replace all variables in a string; try longest variables
 * first. Known bug: may replace shorter variables (e.g. &gold instead
 * of &golden).
 */
void var_replace_107(char** line_p, int scope) {
	log_enter("var_replace_107: line_p: %s, scope: %d", *line_p, scope);
	char crap[20];
	int i;
	for (i = 1; i < MAX_VARS; i++)
		if ((play.var[i].active == 1) &&
			((play.var[i].scope == DINKC_GLOBAL_SCOPE) ||
			(play.var[i].scope == scope))) {
			sprintf(crap, "%d", play.var[i].var);
			replace(play.var[i].name, crap, line_p);
		}
	log_exit("var_replace_107: void");
}

/**
 * Replace all variables in a string; try longest variables first.
 *
 * Possible improvements:
 *
 * - Copy play.var[] and sort it by variable length (and avoid the
 *   recursion)
 *
 * - find vars in the string and replace them as-needed (requires
 *   understanding what exactly is an end-of-variable delimiter, if
 *   such a thing exists)
 */

/**
 * Similar to decipher, plus:
 * - expand special choice variables &savegameinfo and &buttoninfo
 * - it can replace several variables in the same string
 * - with v1.07 it has a prefix bug (see var_replace_107)
 */
void decipher_string(char** line_p, int script) {
	//log_enter("decipher_string: line_p: %s, script: %d, scriptname: %s",
	//		*line_p, script, RINFO_NAME(script));
	char buffer[20 + 1];

	/* Replace all valid variables in 'line' */
	var_replace(line_p, script);

	if ((strchr(*line_p, '&') != NULL) && (script != 0)) {
		sprintf(buffer, "%d", sinfo[script]->sprite);
		replace("&current_sprite", buffer, line_p);
		sprintf(buffer, "%d", script);
		replace("&current_script", buffer, line_p);

		if (dversion >= 108) {
			//v1.08 special variables.
			sprintf(buffer, "%d", returnint);
			replace("&return", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg1);
			replace("&arg1", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg2);
			replace("&arg2", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg3);
			replace("&arg3", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg4);
			replace("&arg4", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg5);
			replace("&arg5", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg6);
			replace("&arg6", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg7);
			replace("&arg7", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg8);
			replace("&arg8", buffer, line_p);
			sprintf(buffer, "%d", rinfo(script)->arg9);
			replace("&arg9", buffer, line_p);
		}

		if (decipher_savegame != 0) {
			// TODO: break dep on input, replace input_get_button_action by a callback
			int button_action = input_get_button_action(decipher_savegame - 1);
			if (button_action == ACTION_ATTACK)
				replace("&buttoninfo", _("Attack"), line_p);
			else if (button_action == ACTION_TALK)
				replace("&buttoninfo", _("Talk/Examine"), line_p);
			else if (button_action == ACTION_MAGIC)
				replace("&buttoninfo", _("Magic"), line_p);
			else if (button_action == ACTION_INVENTORY)
				replace("&buttoninfo", _("Item Screen"), line_p);
			else if (button_action == ACTION_MENU)
				replace("&buttoninfo", _("Main Menu"), line_p);
			else if (button_action == ACTION_MAP)
				replace("&buttoninfo", _("Map"), line_p);
			else if (button_action == ACTION_BUTTON7)
				replace("&buttoninfo", _("Please don't use"), line_p);
			else if (button_action == ACTION_BUTTON8)
				replace("&buttoninfo", _("Unused"), line_p);
			else if (button_action == ACTION_BUTTON9)
				replace("&buttoninfo", _("Unused"), line_p);
			else if (button_action == ACTION_BUTTON10)
				replace("&buttoninfo", _("Unused"), line_p);
			else if (button_action == ACTION_DOWN)
				replace("&buttoninfo", _("Down"), line_p);
			else if (button_action == ACTION_LEFT)
				replace("&buttoninfo", _("Left"), line_p);
			else if (button_action == ACTION_RIGHT)
				replace("&buttoninfo", _("Right"), line_p);
			else if (button_action == ACTION_UP)
				replace("&buttoninfo", _("Up"), line_p);
			else
				replace("&buttoninfo", _("Error: not mapped"), line_p);
		}
	}

	if ((decipher_savegame != 0) && compare(*line_p, "&savegameinfo")) {
		char gameinfo[196] = "";
		int mytime = 0;

		free(*line_p);
		if (load_game_small(decipher_savegame, gameinfo, &mytime) == 1)
			asprintf(line_p, _("Slot %d - %d:%02d - %s"), decipher_savegame,
					mytime / 60, mytime % 60, gameinfo);
		else
			asprintf(line_p, _("Slot %d - Empty"), decipher_savegame);
	}

	log_exit("decipher_string: void");
}

/**
 *
 * name: name of the procedure() to call
 * n1: wait at least n1 milliseconds before callback
 * n2: wait at most n1+n2 milliseconds before callback
 * script: number of the script currently running
**/
int add_callback(char name[20], int n1, int n2, int script) {
	//log_enter("add_callback: name %s, n1: %d, n2: %d, script: %d, scriptname: %s",
	//		name, n1,n2, script, RINFO_NAME(script));
	int k;
	for (k = 1; k < MAX_CALLBACKS; k++) {
		if (callback[k].active == /*false*/ 0) {
			memset(&callback[k], 0, sizeof(callback[k]));

			callback[k].active = /*true*/ 1;
			callback[k].min = n1;
			callback[k].max = n2;
			callback[k].owner = script;
			strcpy(callback[k].name, name);

			log_debug("☎️ Callback added to %d.", k);
			log_exit("add_callback: %d", k);
			return (k);
		}
	}

	log_error("❗ Couldn't add callback, all out of space");
	log_exit("add_callback: %d", 0);
	return 0;
}

void kill_callbacks_owned_by_script(int script) {
	//log_enter("kill_callbacks_owned_by_script: script: %d, scriptname: %s",
	//		script, RINFO_NAME(script));
	int i = 1;
	for (; i < MAX_CALLBACKS; i++) {
		if (callback[i].owner == script) {
			log_debug("☎️ Kill_all_callbacks just killed %d for script %d", i,
					script);
			//killed callback
			callback[i].active = /*false*/ 0;
		}
	}
	log_exit("kill_callbacks_owned_by_script: void");
}

/**
 * Free resources used by this script.
 *
 * Caution: this makes references from spr[].script dangling
 */
void dinkc_kill_script(int k) {
	log_enter("kill_script: script: %d", k);
	//This is handled by phoenix's scripting abstractor so we just need to release rinfo_code buffer
		if (rinfo_code[k] != NULL)
			free(rinfo_code[k]);
		rinfo_code[k] = NULL;

	log_exit("kill_script: void");
}

/**
 * Kill all scripts except those attached to pseudo-sprite 1000, which
 * is meant to survive across screen changes
 * (kill_all_scripts_for_real(...) is more brutal)
 *
 * Used by gfx_tiles only
 */
void kill_all_scripts(void) {
	log_enter("kill_all_scripts");
	/* Kill scripts (except if attached to pseudo-sprite 1000) */
	int k = 1;
	for (; k < MAX_SCRIPTS; k++) {
		if (sinfo[k] != NULL)
			if (sinfo[k]->sprite != 1000)
				scripting_kill_script(k);
	}

	/* Kill pending callbacks (except if attached to pseudo-sprite 1000) */
	for (k = 1; k < MAX_CALLBACKS; k++) {
		if (callback[k].active &&
			(!(sinfo[callback[k].owner] != NULL) &&
			(sinfo[callback[k].owner]->sprite == 1000))) {
			log_debug("☎️ Killed callback %d.  (was attached to script %d)", k,
					callback[k].owner);
			callback[k].active = 0;
		}
	}
	log_exit("kill_all_scripts: void");
}

/**
 * Kill all scripts including those attached to pseudo-sprite 1000

void kill_all_scripts_for_real(void) {
	log_enter("kill_all_scripts_for_real");
	int k = 1;
	for (k = 1; k < MAX_SCRIPTS; k++) {
		if (rinfo[k] != NULL)
			kill_script(k);
	}

	for (k = 1; k < MAX_CALLBACKS; k++) {
		callback[k].active = 0;
	}
	log_exit("kill_all_scripts_for_real: void");
}
*/
/**
 * Reset all vars.
 * Used in test suite.
 */
void kill_all_vars() {
	log_enter("kill_all_vars");
	memset(&play.var, 0, sizeof(play.var));
	log_exit("kill_all_vars: void");
}

/**
 * Return the next single line from rinfo_code[script], starting at
 * rinfo(script)->current. Update line/column counters.
 */
char* read_next_line(int script) {
	//TODO: put a breakpoint thingy here
	log_enter("read_next_line: script: %d", script);
	if (sinfo[script] == NULL) {
		log_error("❗ Tried to read script %d, it doesn't exist.", script);
		log_exit("read_next_line: %s", "NULL");
		return NULL;
	}

	if (rinfo(script)->current >= rinfo(script)->end) {
		//at end of buffer
		log_exit("read_next_line: %s", "NULL");
		return NULL;
	}

	/* remember the beginning of the line to be parsed, we'll use it in
	the debugging messages */
	rinfo(script)->debug_line = rinfo(script)->cur_line;

	int k = rinfo(script)->current;
	int start = k;
	for (; k < rinfo(script)->end; k++) {
		rinfo(script)->current++;
		rinfo(script)->cur_col++;

		if (rinfo_code[script][k] == '\n') {
			rinfo(script)->cur_line++;
			rinfo(script)->cur_col = 0;
		}
		if (rinfo_code[script][k] == '\n' || rinfo_code[script][k] == '\r')
			break;
	}

	if (k < rinfo(script)->end) {
		int len = rinfo(script)->current - start;
		char* buf = (char*)xmalloc(len + 1);

		char* pc = buf;
		int k = start;
		for (; k < rinfo(script)->current; k++, pc++) {
			*pc = rinfo_code[script][k];

			/* Compatibility substitutions, important when parsing
	title_start/title_end, namely */
			if (*pc == '\t')
				*pc = ' ';
			if (*pc == '\r')
				*pc = '\n';
		}
		*pc = '\0'; /* for safety */
		log_exit("read_next_line: %s", buf);
		return buf;
	} else {
		//at end of buffer
		log_exit("read_next_line: %s", "NULL");
		return NULL;
	}
}

void kill_scripts_with_inactive_sprites() {
	log_enter("kill_scripts_with_inactive_sprites");
	for (int i = 1; i < MAX_SCRIPTS; i++) {
		if (sinfo[i] != NULL) {
			// TODO: move out so we don't depend on 'spr'
			if (sinfo[i]->sprite > 0 && sinfo[i]->sprite != 1000 &&
				!spr[sinfo[i]->sprite].active) {
				//kill this script, owner is dead
				log_debug("🥀 Killing script %s, owner sprite %d is dead.",
						sinfo[i]->name, sinfo[i]->sprite);
				scripting_kill_script(i);
			}
		}
	}
	log_exit("kill_scripts_with_inactive_sprites: void");
}

/**
 * Run callbacks, order by index. Sets the activation delay if
 * necessary. Kill obsolete callbacks along the way.
 *
 * Callbacks are set by wait() and set_callback_random().
 *
 * spawn()/external()/etc. use other mechanisms. say_stop*() also use
 * callbacks, but implemented differently (spr[x].callback, processed
 * in updateFrame()).
 **/
//void dinkc_process_callbacks(Uint32 now) {
//	log_enter("process_callbacks: now: %u", now);
//	for (int k = 1; k < MAX_CALLBACKS; k++) {
//		if (callback[k].active) {
			//if (callback[k].owner > 0 && rinfo[callback[k].owner] == NULL) {
				//kill this process, it's owner sprite is 'effin dead.
			//	log_debug("🥀 Killed callback %d because script %d is dead.", k,
			//			callback[k].owner);
			//	callback[k].active = /*false*/ 0;
			//} else {
			//	if (callback[k].timer == 0) {
					//set timer

			//		if (callback[k].max > 0)
			//			callback[k].timer = now + Random::get(callback[k].min, callback[k].max);
			//		else
			//			callback[k].timer = now + callback[k].min;
			//	} else {
			//		if (callback[k].timer < now) {
			//			callback[k].timer = 0;

			//			if (compare(callback[k].name, "")) {
							//callback defined no proc name, so lets assume they want to start the script where it
							//left off
							//kill this callback
			//				callback[k].active = /*false*/ 0;
							//run_script(callback[k].owner);
			//				log_debug("☎️ Called script %d from callback %d.",
			//						callback[k].owner, k);
			//			} else {
			//				log_debug("☎️ Called proc %s from callback %d.",
			//						callback[k].name, k);

							//callback defined a proc name
							//if (locate(callback[k].owner, callback[k].name)) {
								//found proc, lets run it
							//	run_script(callback[k].owner);
							//}
//						}
//					}
//				}
//			}
//		}
//	}
//	log_exit("process_callbacks: void");
//}


/**
 * Kill all scripts attached to given sprite
 */
void dinkc_kill_scripts_owned_by(int sprite) {
	log_enter("dinkc_kill_scripts_owned_by: sprite: %d", sprite);

	int i;
	for (i = 1; i < MAX_SCRIPTS; i++)
		if (sinfo[i] != NULL && sinfo[i]->sprite == sprite)
			scripting_kill_script(i);

	log_exit("dinkc_kill_scripts_owned_by: void");
}

void dinkc_run_script(int script) {
	log_enter("run_script: script: %d", script);
	char* line = NULL;

	/* keep 'return' value? */
	if (dversion >= 108) {
		if (bKeepReturnInt == 1) {
			bKeepReturnInt = 0;
		} else {
			returnint = 0;
		}
	} else {
		returnint = 0;
	}
	returnstring[0] = 0;

	if (sinfo[script] != NULL) {
		log_debug("➡️ Script %s is entered at %d:%d (offset %ld).",
				sinfo[script]->name, rinfo(script)->cur_line,
				rinfo(script)->cur_col, rinfo(script)->current);
	} else {
		log_error("❗ Tried to run a script that doesn't exist in memory.  Nice "
				"work.");
	}

	int doelse_once = 0;
	while ((line = read_next_line(script)) != NULL) {
		while (1) {
			strip_beginning_spaces(line);
			if (strcmp(line, "\n") == 0)
				break;

			int doelse = 0;
			if (doelse_once == 1) {
				doelse = 1;
				doelse_once = 0;
			}
			int result = process_line(script, line, doelse);

			if (result == DCPS_DOELSE_ONCE) {
				doelse_once = 1;
				/* now process the rest of the line */
			}

			if (result == DCPS_YIELD) {
				/* Quit script: */
				if (dbg.debug_scripboot)
					log_debug("👢 giving script the boot");
				free(line);
				log_exit("run_script: void");
				return;
			}

			if (result == DCPS_GOTO_NEXTLINE)
				break;

			/* else result == DCPS_CONTINUE */
		}
		free(line);
	}

	if (sinfo[script] != NULL && rinfo(script)->proc_return != 0) {
		scripting_resume_script(rinfo(script)->proc_return);
		scripting_kill_script(script);
	}
	log_exit("run_script: void");
}

/**
 * Make new global functions (v1.08)
 */
void make_function(char file[10], char func[20]) {
	log_enter("make_function: file: %s, func: %s", file, func);
	//See if it already exists

	int exists = 0;
	int i;
	for (i = 0; strlen(play.func[i].func) > 0 && i < 100; i++) {
		if (compare(func, play.func[i].func)) {
			exists = 1;
			break;
		}
	}
	if (exists == 1) {
		strncpy(play.func[i].file, file, 10);
	} else {
		//ye: make it behave as per what was intended
		if (debug_funcfix) {
			strncpy(play.func[i].file, file, 10);
			strncpy(play.func[i].func, func, 20);
		} else {
			strncpy(play.func[0].file, file, 10);
			strncpy(play.func[0].func, func, 20);
		}
	}
	log_exit("make_function: void");
}

/**
 * Declare a new variable
 * - script: used for debug messages
 */
//TODO: get rid of all references and use scripting_make_int directly
void dinkc_make_int(char* name, int value, int scope, int script) {
	log_enter("make_int: name: %s, value: %d, scope: %d, script: %d, scriptname: %s",
			name, value, scope, script, RINFO_NAME(script));
	/* int dupe;
	if (strlen(name) > 19) {
		log_error("❗ [DinkC] %s:%d: varname %s is too long", rinfo(script)->name,
				rinfo(script)->debug_line, name);
		log_exit("make_int: void");
		return;
	}
	dupe = lookup_var(name, scope);

	if (dupe > 0) {
		if (scope != DINKC_GLOBAL_SCOPE) {
			log_warn("📊 [DinkC] %s:%d: Local var %s already used in this "
					"procedure",
					rinfo(script)->name, rinfo(script)->debug_line, name);

			play.var[dupe].var = value;
		} else {
			log_warn("🔮 [DinkC] %s:%d: var %s is already a global, not changing "
					"value",
					rinfo(script)->name, rinfo(script)->debug_line, name);
		}
		*/
	if (!scripting_make_int(name, value, scope))
	{
		//log_warn("[DinkC] %s:%d: %s",
		//		  sinfo[script]->name, rinfo(script)->debug_line, scripting_error);
	}
		log_exit("make_int: void");
		return;

	//make new var
	//int i;
	//for (i = 1; i < MAX_VARS; i++) {
	//	if (play.var[i].active == /*false*/ 0) {
	//		play.var[i].active = /*true*/ 1;
	//		play.var[i].scope = scope;
	//		strcpy(play.var[i].name, name);
			//g("var %s created, used slot %d ", name,i);
	//		play.var[i].var = value;
	//		log_exit("make_int: void");
	//		return;
	//	}
	//}

	//log_error("❗ [DinkC] %s:%d: out of var space, all %d used",
	//		rinfo(script)->name, rinfo(script)->debug_line, MAX_VARS);
}

/**
 * (re)Define variable
 *
 * name: variable name
 * newname: new value (unless that's a function call, cf. 'rest')
 * math: operator (one of '=', '+', '-', '*', '/')
 * script: in-memory script identifier
 * rest: text of the script after the operator (left-trimmed)
 */
void var_equals(char* name, char* newname, char math, int script,
				char rest[200]) {
	log_enter("var_equals: name: %s, newname: %s, math: %c, script: %d, "
			"scriptname: %s, rest: %s", name, newname, math, script,
			RINFO_NAME(script), rest);
	int newval = 0;
	struct varman* lhs_var = NULL;

	/** Ensure left-hand side is an existing variable **/
	if (name[0] != '&') {
		log_error("❗ [DinkC] %s:%d:[var_equals]: unknown var %s",
				sinfo[script]->name, rinfo(script)->debug_line, name);
		log_exit("var_equals: void");
		return;
	}
	/* Find the variable slot */
	{
		int k = lookup_var_local_global(name, script);
		if (k != -1)
			lhs_var = &(play.var[k]);

		if (lhs_var == NULL) /* not found */
		{
			log_error("❗ [DinkC] %s:%d:[var_equals]: unknown var %s",
					sinfo[script]->name, rinfo(script)->debug_line, name);
			log_exit("var_equals: void");
			return;
		}
	}

	/** Analyse right-hand side **/
	/* check if right-hand side is a function */
	if (strchr(rest, '(') != NULL) {
		process_line(script, rest, /*false*/ 0);
		newval = returnint;
		goto next2;
	}

	/* check if right-hand side is a variable to copy */
	/* remove trailing ';' */
	if (strchr(newname, ';') != NULL)
		replace_norealloc(";", "", newname);
	/* look for existing variable */
	{
		int k2 = lookup_var_local_global(newname, script);
		if (k2 != -1) {
			newval = play.var[k2].var;
			//found var
			goto next2;
		}
	}
	/* also check special variables */
	if (compare(newname, "&current_sprite")) {
		newval = sinfo[script]->sprite;
		goto next2;
	}
	if (compare(newname, "&current_script")) {
		newval = script;
		goto next2;
	}
	if (dversion >= 108) {
		//v1.08 special variables.
		if (compare(newname, "&return")) {
			newval = returnint;
			goto next2;
		}
		if (compare(newname, "&arg1")) {
			newval = rinfo(script)->arg1;
			goto next2;
		}
		if (compare(newname, "&arg2")) {
			newval = rinfo(script)->arg2;
			goto next2;
		}
		if (compare(newname, "&arg3")) {
			newval = rinfo(script)->arg3;
			goto next2;
		}
		if (compare(newname, "&arg4")) {
			newval = rinfo(script)->arg4;
			goto next2;
		}
		if (compare(newname, "&arg5")) {
			newval = rinfo(script)->arg5;
			goto next2;
		}
		if (compare(newname, "&arg6")) {
			newval = rinfo(script)->arg6;
			goto next2;
		}
		if (compare(newname, "&arg7")) {
			newval = rinfo(script)->arg7;
			goto next2;
		}
		if (compare(newname, "&arg8")) {
			newval = rinfo(script)->arg8;
			goto next2;
		}
		if (compare(newname, "&arg9")) {
			newval = rinfo(script)->arg9;
			goto next2;
		}
	}
	/* otherwise, assume right-hand side is an integer */
	newval = atol(newname);

next2:
	/* Apply the right operation */
	if (math == '=')
		lhs_var->var = newval;
	if (math == '+')
		lhs_var->var += newval;
	if (math == '-')
		lhs_var->var -= newval;
	if (math == '/' && newval != 0)
		// TODO: do something if newval == 0 (crashes in Dink v1.08)
		//yeolde: golly gee
		lhs_var->var = lhs_var->var / newval;
	if (math == '*')
		lhs_var->var = lhs_var->var * newval;

	log_exit("var_equals: void");
}

/**
 * Evaluate a value (variable, int, or maths), in the context of
 * 'script'.
 */
int var_figure(char* h, int script) {
	log_enter("var_figure: h: %s, script: %d, scriptname: %s", h, script,
			RINFO_NAME(script));
	char* word = NULL;
	int ret = 0;
	int n1 = 0, n2 = 0;

	int is_one_word_equation = 0;
	word = get_word(h, 2);
	if (compare(word, ""))
		is_one_word_equation = 1;
	free(word);
	if (is_one_word_equation) {
		// variable -> integer
		if (h[0] == '&')
			decipher_string(&h, script);

		// integer
		ret = atol(h);
		log_exit("var_figure: %d", ret);
		return ret;
	}

	word = get_word(h, 1);
	decipher_string(&word, script);
	n1 = atol(word);
	free(word);

	word = get_word(h, 3);
	replace_norealloc(")", "", word);
	decipher_string(&word, script);
	n2 = atol(word);
	free(word);

	word = get_word(h, 2);
	log_debug("🧮 Compared %d to %d", n1, n2);

	if (compare(word, "==")) {
		if (n1 == n2)
			ret = 1;
		else
			ret = 0;
		free(word);
		log_exit("var_figure: %d", ret);
		return ret;
	}

	if (compare(word, ">")) {
		if (n1 > n2)
			ret = 1;
		else
			ret = 0;
		free(word);
		log_exit("var_figure: %d", ret);
		return ret;
	}

	if (compare(word, ">=")) {
		if (n1 >= n2)
			ret = 1;
		else
			ret = 0;
		free(word);
		log_exit("var_figure: %d", ret);
		return ret;
	}

	if (compare(word, "<")) {
		if (n1 < n2)
			ret = 1;
		else
			ret = 0;
		free(word);
		log_exit("var_figure: %d", ret);
		return ret;
	}

	if (compare(word, "<=")) {
		if (n1 <= n2)
			ret = 1;
		else
			ret = 0;
		free(word);
		log_exit("var_figure: %d", ret);
		return ret;
	}

	if (compare(word, "!=")) {
		if (n1 != n2)
			ret = 1;
		else
			ret = 0;
		free(word);
		log_exit("var_figure: %d", ret);
		return ret;
	}

	free(word);
	log_exit("var_figure: %d", ret);
	return ret;
}

/**
 * Check if 'line' is a valid variable declaration, and define the
 * variable it to 0 (via make_int(...)). 'line' is modified.
 */
void int_prepare(char* line, int script) {
	log_enter("int_prepare: line: %s, script: %d, scriptname: %s", line,
			script, RINFO_NAME(script));
	char* hold = strdup(line);

	char* name = NULL;
	char* temp = NULL;
	replace_norealloc("=", " ", line);
	temp = separate_string(line, 1, ';');
	strcpy(line, temp); // safe as strlen(line) <= strlen(temp)
	free(temp);
	name = get_word(line, 2);

	if (name[0] != '&') {
		log_error("❗ [DinkC] %s:%d: can't create var %s, should be &%s.",
				sinfo[script]->name, rinfo(script)->debug_line, name, name);
	} else {
		dinkc_make_int(name, 0, script, script);

		strcpy(line, hold);
	}
	free(name);
	free(hold);
	log_exit("int_prepare: void");
}

/**
 * Process DinkC dialog choice stanza
 * Globals: game_choice, game_choice_start, game_choice_clear, i18n_translate
 */
/*bool*/ int dinkc_get_choices(int script) {
	log_enter("dinkc_get_choices: script: %d, scriptname: %s", script,
			RINFO_NAME(script));
	char* line = NULL;
	int cur = 1; // TODO: make it stay <= 20
	int retnum = 0;
	game_choice_clear();
	game_choice.newy = -5000;
	while (1) {
	redo:
		line = read_next_line(script);
		if (line == NULL)
			line = strdup(""); // compatibility

		strip_beginning_spaces(line);
		//Msg("Comparing to %s.", line);

		char* word = get_word(line, 1);
		if (compare(word, "set_y")) {
			free(word);
			word = get_word(line, 2);
			game_choice.newy = atol(word);
			free(word);
			free(line);
			goto redo;
		}

		if (compare(word, "set_title_color")) {
			free(word);
			word = get_word(line, 2);
			game_choice.color = atol(word);
			free(word);
			free(line);
			goto redo;
		}
		free(word);

		strip_beginning_spaces(line);
		if (compare(line, "\n")) {
			free(line);
			goto redo;
		}

		char* directive = NULL;
	morestuff:
		directive = separate_string(line, 1, '(');
		strip_beginning_spaces(directive);

		if (compare(directive, "title_start")) {
			free(line);
			while ((line = read_next_line(script)) != NULL) {
				strip_beginning_spaces(line);
				free(directive);

				directive = separate_string(line, 1, '(');
				if (directive != NULL) {
					strip_beginning_spaces(directive);

					if (compare(directive, "title_end")) {
						replace_norealloc("\n\n\n\n", "\n \n",
										game_choice.buffer);
						replace_norealloc("\n\n", "\n", game_choice.buffer);
						free(directive);
						free(line);
						goto redo;
					}
				}

				/* drop '\n', this messes translations */
				line[strlen(line) - 1] = '\0';
				/* Translate text (before variable substitution) */
				char* translation = i18n_translate(
						sinfo[script]->name, rinfo(script)->debug_line, line);
				decipher_string(&translation, script);
				int cur_len = strlen(game_choice.buffer);
				strncat(game_choice.buffer, translation,
						TALK_TITLE_BUFSIZ - 1 - cur_len - 1);
				free(translation);
				/* put '\n' back */
				strcat(game_choice.buffer, "\n");
				game_choice.buffer[TALK_TITLE_BUFSIZ - 1] = '\0';
				free(line);
			}

			free(directive);
			goto redo;
		}

		if (compare(directive, "choice_end")) {
			if (cur - 1 == 0) {
				log_debug("⚖️ Error: choice() has 0 options in script %s, offset "
						"%ld.", sinfo[script]->name, rinfo(script)->current);

				free(directive);
				free(line);
				log_exit("dinkc_get_choices: (bool) %d", 0);
				return /*false*/ 0;
			}
			game_choice_start(script, cur - 1);
			free(directive);
			free(line);
			log_exit("dinkc_get_choices: (bool) %d", 1);
			return /*true*/ 1;
		}
		free(directive);

		char* condition = separate_string(line, 1, '"');
		strip_beginning_spaces(condition);

		if (strlen(condition) > 2) {
			//found conditional statement
			if (strchr(condition, '(') == NULL) {
				log_error("❗ [DinkC] Error with choice() statement in script %s,"
						" offset %ld. (%s?)",
						sinfo[script]->name, rinfo(script)->current,
						condition);

				free(condition);
				free(line);
				log_exit("dinkc_get_choices: (bool) %d", 0);
				return /*false*/ 0;
			}

			char* temp = separate_string(condition, 2, '(');
			free(condition);
			condition = separate_string(temp, 1, ')');
			free(temp);

			//Msg("Running %s through var figure..", check);
			if (var_figure(condition, script) == 0) {
				log_debug("👎 Answer is no.");
				retnum++;

				free(condition);
				free(line);
				goto redo;
				//said NO to statement
			}
			//Msg("Answer is yes.");
			free(condition);

			/* Resume processing stripping the first condition (there
	may be several conditions on a single dialog ligne, which
	are AND'ed) */
			char* p = strchr(line, ')') + 1;
			int i = 0;
			for (; *p != '\0'; i++, p++)
				line[i] = *p;
			line[i] = '\0';
			goto morestuff;
		}
		free(condition);

		retnum++;
		char* text = separate_string(line, 2, '"');
		if (strlen(text) > 0) {
			/* Translate text (before variable substitution) */
			char* translation = i18n_translate(sinfo[script]->name,
											rinfo(script)->debug_line, text);
			strip_beginning_spaces(translation);

			decipher_savegame = retnum;
			decipher_string(&translation, script);
			decipher_savegame = 0;
			strncpy(game_choice.line[cur], translation, TALK_LINE_BUFSIZ - 1);
			game_choice.line[cur][TALK_LINE_BUFSIZ - 1] = '\0';
			free(translation);
		} else {
			/* Handle empty text separately because _("") has a special
	meaning (returns .mo meta-data). */
			strcpy(game_choice.line[cur], "");
		}
		free(text);
		game_choice.line_return[cur] = retnum;
		cur++;
		free(line);
	}

	log_error("❗ dinkc_get_choices: Implicit return value!");
	log_exit("dinkc_get_choices: ? (implicit return)");
}

/**
 * Utility function for 'process_line', to separate and store the current procedure arguments.
 *
 * proc_name: named of the called function
 * script: script id
 * str_params: string to parse (what was after the function name)
 * spec: describe the function's parameters:
 *    1=int
 *    2=string
 *    0=no more args (10 args max)
 *
 * Known compatibility issue: passing no argument to a function
 * expecting 1 int argument is considered valid..
 *
 * Return: 0 if parse error, 1 if success
 */
int get_parms(char proc_name[20], int script, char* str_params, int* spec) {
	log_enter("get_parms: proc_name: %s, script: %d, scriptname: %s, "
			"str_params: %s, spec: %d", proc_name, script, RINFO_NAME(script),
			str_params, *spec);
	/* Clean-up parameters */
	memset(nlist, 0, 10 * sizeof(int));
	{
		int i = 0;
		for (; i < 10; i++)
			slist[i][0] = '\0';
	}

	/* Safety */
	char* limit = str_params + strlen(str_params);

	strip_beginning_spaces(str_params);
	if (str_params[0] == '(') {
		//Msg("Found first (.");
		str_params++;
	} else {
		log_error("❗ [DinkC] Missing '(' in %s, offset %ld.", sinfo[script]->name,
				rinfo(script)->current);
		log_exit("get_parms: %d", 0);
		return 0;
	}

	int i = 0;
	for (; i < 10; i++) {
		strip_beginning_spaces(str_params);

		if (spec[i] == 1) // type=int
		{
			// Get next parameter (until ',' or ')' is reached)
			char* parm = NULL;
			if (strchr(str_params, ',') != NULL)
				parm = separate_string(str_params, 1, ',');
			else if (strchr(str_params, ')') != NULL)
				parm = separate_string(str_params, 1, ')');
			else
				parm = strdup("");

			// move to next param
			str_params += strlen(parm);

			int intval = -1;
			if (parm[0] == '&') {
				replace_norealloc(" ", "", parm);
				intval = decipher(parm, script);
			} else {
				intval = atol(parm);
			}
			// store parameter of type 'int'
			nlist[i] = intval;
			free(parm);
		} else if (spec[i] == 2) // type=string
		{
			// Checking for string
			char* parm = NULL;
			parm = separate_string(str_params, 2, '"');

			// replace DinkC string parameter
			free(slist[i]);
			slist[i] = parm;

			// move to next param
			str_params += strlen(parm) + 2; // 2x"
			if (str_params > limit)
				str_params = limit;
		}

		if ((i + 1) == 10 || spec[i + 1] == 0) // this was the last arg
		{
			//finish
			strip_beginning_spaces(str_params);

			if (str_params[0] == ')') {
				str_params++;
			} else {
				log_exit("get_parms: %d", 0);
				return 0;
			}
			strip_beginning_spaces(str_params);
			log_exit("get_parms: %d", 1);
			return 1;
		}

		//got a parm, but there is more to get, lets make sure there is a comma there
		strip_beginning_spaces(str_params);

		if (str_params[0] == ',') {
			str_params++;
		} else {
			log_exit("get_parms: %d", 0);
			return 0;
		}
	}
	log_exit("get_parms: %d", 1);
	return 1;
}

/**
 * Are these 2 function signatures identical?
 */
static int signatures_eq_p(int* params1, int* params2) {
	log_enter("signatures_eq_p");
	int i = 0;
	for (; i < 10; i++)
		if (params1[i] != params2[i]) {
			log_exit("signatures_eq_p: %d", 0);
			return 0;
		}
	log_exit("signatures_eq_p: %d", 1);
	return 1;
}

/**
 * Process one line of DinkC and returns directive to the DinkC
 * interpreter.
 *
 * Cf. doc/HACKING_dinkc.txt for understanding in progress ;)
 * Globals: kill_text_owned_by
 **/
static enum dinkc_parser_state process_line(int script, char* s,
											/*bool*/ int doelse) {
	log_enter("process_line: script: %d, scriptname: %s, s: %s, doelse: %d",
			script, RINFO_NAME(script), s, doelse);
	char *h, *p;
	char* ev[3];

	memset(&ev, 0, sizeof(ev));

	if (rinfo(script)->level < 1)
		rinfo(script)->level = 1;

	h = s;
	if (h[0] == '\0')
		return DCPS_GOTO_NEXTLINE;

	if ((h[0] == '/') && (h[1] == '/')) {
		//Msg("It was a comment!");
		goto bad;
	}

	/* Cut line */
	ev[0] = separate_string(h, 1, ' ');
	ev[1] = separate_string(h, 2, ' ');
	ev[2] = separate_string(h, 3, ' ');
	/* Prepare free on return */
#define PL_RETURN(intval)                                                      \
	{                                                                          \
		free(ev[0]), free(ev[1]), free(ev[2]);                                 \
		log_exit("process_line: %d", intval);                                  \
		return intval;                                                         \
	}

	if (compare(ev[0], "VOID")) {
		if (rinfo(script)->proc_return != 0) {
			scripting_resume_script(rinfo(script)->proc_return);
			scripting_kill_script(script);
		}
		PL_RETURN(DCPS_YIELD);
	}

	/* goto label? */
	if (ev[0][strlen(ev[0]) - 1] == ':' && strlen(ev[1]) < 2) {
		if (dversion >= 108) {
			/* Attempt to avoid considering:
			say("bonus: 5 points", 1); // would not display any text at all!
			as a label */
			if (strncmp(ev[0], "say", 3) != 0) {
				PL_RETURN(DCPS_GOTO_NEXTLINE); //its a label
			}
		} else {
			PL_RETURN(DCPS_GOTO_NEXTLINE); //its a label
		}
	}

	/** Expression between parenthesis **/
	if (ev[0][0] == '(') {
		//this procedure has been passed a conditional statement finder
		//what kind of conditional statement is it?
		p = h;
		char* temp = separate_string(h, 2, ')');
		free(ev[0]);
		ev[0] = separate_string(h, 1, ')');

		// Msg("Ok, turned h %s to  ev1 %s.",h,ev[0]);
		p += strlen(ev[0]) + 1;

		strip_beginning_spaces(p);

		if (strchr(temp, '=') != NULL) {
			h++;
			strip_beginning_spaces(h);
			process_line(script, h, /*false*/ 0);
			replace_norealloc("==", "", temp);
			char* expr = (char*)xmalloc(20 + 4 + strlen(temp) + 1);
			sprintf(expr, "%d == %s", returnint, temp);
			returnint = var_figure(expr, script);
			strcpy(h, "\n");
			free(expr);
			free(temp);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}

		if (strchr(temp, '>') != NULL) {
			h++;
			strip_beginning_spaces(h);
			process_line(script, h, /*false*/ 0);
			replace_norealloc("==", "", temp);
			char* expr = (char*)xmalloc(20 + 3 + strlen(temp) + 1);
			sprintf(expr, "%d > %s", returnint, temp);
			returnint = var_figure(expr, script);
			strcpy(h, "\n");
			free(expr);
			free(temp);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}

		if (strchr(temp, '<') != NULL) {
			h++;
			strip_beginning_spaces(h);
			process_line(script, h, /*false*/ 0);
			replace_norealloc("==", "", temp);
			char* expr = (char*)xmalloc(20 + 3 + strlen(temp) + 1);
			sprintf(expr, "%d < %s", returnint, temp);
			returnint = var_figure(expr, script);
			strcpy(h, "\n");
			free(expr);
			free(temp);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}

		/* Beuc: This should be converted to a set of "if ... else
* if... else if ..." and multi-character constants should be
* removed. However, this may cause the interpreter to behave
* differently, so be careful. */
		/* For now, I'll rewrite the code in an equivalent warning-free
* inelegant way: strchr(str, 'ab') <=> strchr(str, 'b') */
		/* if (strchr (temp, '<=') != NULL) */
		if (strchr(temp, '=') != NULL) {
			h++;
			strip_beginning_spaces(h);
			process_line(script, h, /*false*/ 0);
			replace_norealloc("==", "", temp);
			char* expr = (char*)xmalloc(20 + 4 + strlen(temp) + 1);
			sprintf(expr, "%d <= %s", returnint, temp);
			returnint = var_figure(expr, script);
			strcpy(h, "\n");
			free(expr);
			free(temp);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}
		/* if (strchr (temp, '>=') != NULL) */
		if (strchr(temp, '=') != NULL) {
			h++;
			strip_beginning_spaces(h);
			process_line(script, h, /*false*/ 0);
			replace_norealloc("==", "", temp);
			char* expr = (char*)xmalloc(20 + 4 + strlen(temp) + 1);
			sprintf(expr, "%d >= %s", returnint, temp);
			returnint = var_figure(expr, script);
			strcpy(h, "\n");
			free(expr);
			free(temp);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}
		/* if (strchr (temp, '!=') != NULL) */
		if (strchr(temp, '=') != NULL) {
			h++;
			strip_beginning_spaces(h);
			process_line(script, h, /*false*/ 0);
			replace_norealloc("==", "", temp);
			char* expr = (char*)xmalloc(20 + 4 + strlen(temp) + 1);
			sprintf(expr, "%d != %s", returnint, temp);
			returnint = var_figure(expr, script);
			strcpy(h, "\n");
			free(expr);
			free(temp);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}
		free(temp);

		if (p[0] == ')') {
			//its a procedure in the if statement!!!
			h++;
			p++;
			char* line_copy = strdup(p);
			process_line(script, h, /*false*/ 0);
			log_debug("🔢 Returned %d for the returnint", returnint);
			strcpy(s, line_copy); /* strlen(s) >= strlen(line_copy) */
			free(line_copy);
			h = s;

			PL_RETURN(DCPS_GOTO_NEXTLINE);
		} else {
			h++;

			char* expr = separate_string(h, 1, ')');
			h += strlen(expr) + 1;
			returnint = var_figure(expr, script);
			if (expr == NULL)
				free(expr);

			strcpy_nooverlap(s, h);

			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}

		strip_beginning_spaces(h);
		strip_beginning_spaces(ev[0]);

		s = h;
	} /* END expression between parenthesis */

	if (strchr(ev[0], '(') != NULL) {
		//Msg("Has a (, lets change it");
		free(ev[0]);
		ev[0] = separate_string(h, 1, '(');
		//Msg("Ok, first is now %s",ev[0]);
	}

	/** { Bloc } **/
	{
		char first = ev[0][0];
		if (first == '{') {
			rinfo(script)->level++;
			//Msg("Went up level, now at %d.", rinfo(script)->level);
			h++;
			if (rinfo(script)->skipnext) {
				/* Skip the whole { section } */
				rinfo(script)->skipnext = /*false*/ 0;
				rinfo(script)->onlevel = (rinfo(script)->level - 1);
			}
			goto good;
		}

		if (first == '}') {
			rinfo(script)->level--;
			//Msg("Went down a level, now at %d.", rinfo(script)->level);
			h++;

			if (rinfo(script)->onlevel > 0 &&
				rinfo(script)->level == rinfo(script)->onlevel) {
				/* Finished skipping the { section }, preparing to run 'else' */
				strip_beginning_spaces(h);
				strcpy_nooverlap(s, h);
				PL_RETURN(DCPS_DOELSE_ONCE);
			}
			goto good;
		}
	}

	/* Fix if there are too many closing '}' */
	if (rinfo(script)->level < 0) {
		rinfo(script)->level = 0;
	}

	/* Note: that's the 2nd time we compare with "VOID" -
cf. above. However ev[0] was modified in between, so this
section may still be called if the first comparison didn't
match. */
	if (compare(ev[0], "void")) {
		//     Msg("Next procedure starting, lets quit");
		strcpy_nooverlap(s, h);
		if (rinfo(script)->proc_return != 0) {
			scripting_resume_script(rinfo(script)->proc_return);
			scripting_kill_script(script);
		}

		PL_RETURN(DCPS_YIELD);
	}

	/* Stop processing if we're skipping the current { section } */
	if (rinfo(script)->onlevel > 0 &&
		rinfo(script)->level > rinfo(script)->onlevel) {
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	rinfo(script)->onlevel = 0;

	/* Skip the current line if the previous 'if' or 'else' said so */
	if (rinfo(script)->skipnext) {
		//sorry, can't do it, you were told to skip the next thing
		rinfo(script)->skipnext = /*false*/ 0;
		strcpy(s, "\n"); /* jump to next line */
		//PL_RETURN(3);
		PL_RETURN(DCPS_DOELSE_ONCE);
	}

	if (compare(ev[0], "void")) {
		log_error("❗ [DinkC] Missing } in %s, offset %ld.", sinfo[script]->name,
				rinfo(script)->current);
		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_YIELD);
	}

	/** if **/
	if (compare(ev[0], "if")) {
		h += strlen(ev[0]);
		strip_beginning_spaces(h);

		process_line(script, h, /*false*/ 0);
		// Result is 'returnint'

		if (returnint != 0) {
			log_debug("▶️ If returned true");
		} else {
			//don't do it!
			rinfo(script)->skipnext = /*true*/ 1;
			log_debug("⏩ If returned false, skipping next thing");
		}

		strcpy_nooverlap(s, h);
		//g("continuing to run line %s..", h);

		//PL_RETURN(5);
		PL_RETURN(DCPS_DOELSE_ONCE);
		/* state 5 should actually be state DCPS_CONTINUE, but keeping
	it that way (e.g. with doelse=1 for the next line) for
	compatibility, just in case somebody abused it */
	}

	if (compare(ev[0], "else")) {
		//Msg("Found else!");
		h += strlen(ev[0]);

		if (doelse) {
			// Yes to else
		} else {
			// No to else...
			// they shouldn't run the next thing
			rinfo(script)->skipnext = /*true*/ 1;
		}
		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_CONTINUE);
	}

	/** Dialog **/
	if (compare(ev[0], "choice_start")) {
		kill_text_owned_by(1);
		if (dinkc_get_choices(script)) {
			// Question(s) gathered successfully
			PL_RETURN(DCPS_YIELD);
		}
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	/** Jump **/
	if (compare(ev[0], "goto")) {
		locate_goto(ev[1], script);
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	/** Definition **/
	if (compare(ev[0], "int")) {
		int_prepare(h, script);
		h += strlen(ev[0]);

		if (strchr(h, '=') != NULL) {
			strip_beginning_spaces(h);
			//Msg("Found =...continuing equation");
			strcpy_nooverlap(s, h);
			PL_RETURN(DCPS_CONTINUE);
		} else {
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}
	}

	/** "return;" and "return something;" **/
	if (compare(ev[0], "return;")) {
		log_debug("↪️ Found return; statement");

		if (rinfo(script)->proc_return != 0) {
			bKeepReturnInt = 1; /* v1.08 */
			scripting_resume_script(rinfo(script)->proc_return);
			scripting_kill_script(script);
		}

		PL_RETURN(DCPS_YIELD);
	}

	if (dversion >= 108) {
		/* "return", without trailing ';' */
		/* added so we can have return values and crap. */
		/* see also "return;" above */
		if (compare(ev[0], "return")) {
			log_debug("↪️ Found return; statement");
			h += strlen(ev[0]);
			strip_beginning_spaces(h);
			process_line(script, h, 0);
			if (rinfo(script)->proc_return != 0) {
				bKeepReturnInt = 1;
				scripting_resume_script(rinfo(script)->proc_return);
				scripting_kill_script(script);
			}
			PL_RETURN(DCPS_YIELD);
		}
	}

	/********************/
	/*  DinkC bindings  */
	/*                  */
	/********************/

	/** Lookup bindings **/
	{
		char* funcname = ev[0];
		char* str_args = h + strlen(ev[0]);
		struct binding* pbd = NULL;
		pbd = dinkc_bindings_lookup(funcname);

		if (pbd != NULL) {
			/* Common arguments */
			int* yield = (int*)alloca(sizeof(int) * 1);
			yield[0] = 0; /* don't yield by default) */

			/* Specific arguments */
			int* params = pbd->params;
			if (params[0] != -1) /* no args == no checks*/
			{
				if (!get_parms(funcname, script, str_args, params)) {
					/* Invalid parameters in the DinkC script - output an
		error message */
					int i = 0;
					while (params[i] != 0 && i < 10)
						i++;
					log_error(
							"❗ [DinkC] %s:%d: procedure '%s' takes %d parameters",
							sinfo[script]->name, rinfo(script)->debug_line,
							funcname, i);

					/* Set 'returnint' if necessary */
					if (pbd->badparams_returnint_p == 1)
						returnint = pbd->badparams_returnint;
					/* Fallback parser state */
					PL_RETURN(pbd->badparams_dcps);
				}
			}

			/* Call C function */
			cur_funcname = pbd->funcname; /* for error messages */
			int sig_void[10] = {-1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			int sig_int[10] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			int sig_str[10] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			int sig_int_int[10] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
			int sig_int_str[10] = {1, 2, 0, 0, 0, 0, 0, 0, 0, 0};
			int sig_str_int[10] = {2, 1, 0, 0, 0, 0, 0, 0, 0, 0};
			int sig_str_str[10] = {2, 2, 0, 0, 0, 0, 0, 0, 0, 0};
			int sig_int_int_int[10] = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
			int sig_str_int_int[10] = {2, 1, 1, 0, 0, 0, 0, 0, 0, 0};
			int sig_int_int_int_int[10] = {1, 1, 1, 1, 0, 0, 0, 0, 0, 0};
			int sig_int_int_int_int_int[10] = {1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
			int sig_int_int_int_int_int_int[10] = {1, 1, 1, 1, 1,
												1, 0, 0, 0, 0};

			log_trace("process_line: Determine function signature for: %s",
					cur_funcname);
			/* {-1,0,0,0,0,0,0,0,0,0} */
			if (signatures_eq_p(pbd->params, sig_void)) {
				log_trace("process_line: signature: %s",
						"{-1,0,0,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*) =
						(void (*)(int, int*, int*))pbd->func;
				(*pf)(script, yield, &returnint);
			}
			/* {1,0,0,0,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_int)) {
				log_trace("process_line: signature: %s",
						"{1,0,0,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, int) =
						(void (*)(int, int*, int*, int))pbd->func;
				(*pf)(script, yield, &returnint, nlist[0]);
			}
			/* {2,0,0,0,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_str)) {
				log_trace("process_line: signature: %s",
						"{2,0,0,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, char*) =
						(void (*)(int, int*, int*, char*))pbd->func;
				(*pf)(script, yield, &returnint, slist[0]);
			}
			/* {1,1,0,0,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_int_int)) {
				log_trace("process_line: signature: %s",
						"{1,1,0,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, int, int) =
						(void (*)(int, int*, int*, int, int))pbd->func;
				(*pf)(script, yield, &returnint, nlist[0], nlist[1]);
			}
			/* {1,2,0,0,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_int_str)) {
				log_trace("process_line: signature: %s",
						"{1,2,0,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, int, char*) =
						(void (*)(int, int*, int*, int, char*))pbd->func;
				(*pf)(script, yield, &returnint, nlist[0], slist[1]);
			}
			/* {2,1,0,0,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_str_int)) {
				log_trace("process_line: signature: %s",
						"{2,1,0,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, char*, int) =
						(void (*)(int, int*, int*, char*, int))pbd->func;
				(*pf)(script, yield, &returnint, slist[0], nlist[1]);
			}
			/* {2,2,0,0,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_str_str)) {
				log_trace("process_line: signature: %s",
						"{2,2,0,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, char*, char*) =
						(void (*)(int, int*, int*, char*, char*))pbd->func;
				(*pf)(script, yield, &returnint, slist[0], slist[1]);
			}
			/* {1,1,1,0,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_int_int_int)) {
				log_trace("process_line: signature: %s",
						"{1,1,1,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, int, int, int) =
						(void (*)(int, int*, int*, int, int, int))pbd->func;
				(*pf)(script, yield, &returnint, nlist[0], nlist[1], nlist[2]);
			}
			/* {2,1,1,0,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_str_int_int)) {
				log_trace("process_line: signature: %s",
						"{2,1,1,0,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, char*, int, int) =
						(void (*)(int, int*, int*, char*, int, int))pbd->func;
				(*pf)(script, yield, &returnint, slist[0], nlist[1], nlist[2]);
			}
			/* {1,1,1,1,0,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_int_int_int_int)) {
				log_trace("process_line: signature: %s",
						"{1,1,1,1,0,0,0,0,0,0}");
				void (*pf)(int, int*, int*, int, int, int, int) = (void (*)(
						int, int*, int*, int, int, int, int))pbd->func;
				(*pf)(script, yield, &returnint, nlist[0], nlist[1], nlist[2],
					nlist[3]);
			}
			/* {1,1,1,1,1,0,0,0,0,0} */
			else if (signatures_eq_p(pbd->params, sig_int_int_int_int_int)) {
				log_trace("process_line: signature: %s",
						"{1,1,1,1,1,0,0,0,0,0}");
				void (*pf)(int, int*, int*, int, int, int, int, int) =
						(void (*)(int, int*, int*, int, int, int, int,
								int))pbd->func;
				(*pf)(script, yield, &returnint, nlist[0], nlist[1], nlist[2],
					nlist[3], nlist[4]);
			}
			/* {1,1,1,1,1,1,0,0,0,0} */
			else if (signatures_eq_p(pbd->params,
									sig_int_int_int_int_int_int)) {
				log_trace("process_line: signature: %s",
						"{1,1,1,1,1,1,0,0,0,0}");
				void (*pf)(int, int*, int*, int, int, int, int, int, int) =
						(void (*)(int, int*, int*, int, int, int, int, int,
								int))pbd->func;
				(*pf)(script, yield, &returnint, nlist[0], nlist[1], nlist[2],
					nlist[3], nlist[4], nlist[5]);
			} else {
				log_fatal("Internal error: DinkC function %s has unknown "
						"signature",
						pbd->funcname);
				exit(EXIT_FAILURE);
			}
			if (dbg.debug_returnint)
				log_debug("🔢 Value of returnint after '%s': %d", cur_funcname, returnint);
			cur_funcname = "";
			/* the function can manipulation returnint through argument #3 */

			if (*yield == 0) {
				PL_RETURN(DCPS_GOTO_NEXTLINE);
			} else if (*yield == 1) {
				PL_RETURN(DCPS_YIELD);
			} else {
				log_fatal("Internal error: DinkC function %s requested invalid "
						"state %d",
						pbd->funcname, *yield);
				exit(EXIT_FAILURE);
			}
		}
	}

	/***************/
	/** Operators **/
	/**           **/
	/***************/

	/* Beware: this works on ev[1], not ev[0]; position in the code is
critical! */

	if (compare(ev[1], "=")) {
		h += strlen(ev[0]);
		strip_beginning_spaces(h);
		h++;
		strip_beginning_spaces(h);
		var_equals(ev[0], ev[2], '=', script, h);
		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	if (compare(ev[1], "+=")) {
		h += strlen(ev[0]);
		strip_beginning_spaces(h);
		h += 2;
		strip_beginning_spaces(h);
		var_equals(ev[0], ev[2], '+', script, h);
		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	if (compare(ev[1], "*=")) {
		h += strlen(ev[0]);
		strip_beginning_spaces(h);
		h += 2;
		strip_beginning_spaces(h);
		var_equals(ev[0], ev[2], '*', script, h);
		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	if (compare(ev[1], "-=")) {
		h += strlen(ev[0]);
		strip_beginning_spaces(h);
		h += 2;
		strip_beginning_spaces(h);

		var_equals(ev[0], ev[2], '-', script, h);

		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	if (compare(ev[1], "/") || (dversion >= 108 && compare(ev[1], "/=") && !debug_divequals)) {
		h += strlen(ev[0]);
		strip_beginning_spaces(h);
		h++;
		strip_beginning_spaces(h);

		var_equals(ev[0], ev[2], '/', script, h);

		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	if ((compare(ev[1], "/=") && debug_divequals && dversion >= 108)) {
		h += strlen(ev[0]);
		strip_beginning_spaces(h);
		h += 2;
		strip_beginning_spaces(h);

		var_equals(ev[0], ev[2], '/', script, h);
		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	if (compare(ev[1], "*")) {
		h += strlen(ev[0]);
		strip_beginning_spaces(h);
		h++;
		strip_beginning_spaces(h);

		var_equals(ev[0], ev[2], '*', script, h);

		strcpy_nooverlap(s, h);
		PL_RETURN(DCPS_GOTO_NEXTLINE);
	}

	/***************************************/
	/** New DinkC user-defined procedures **/
	/**                                   **/
	/***************************************/
	if (dversion >= 108) {
		if (compare(ev[0], "external")) {
			h += strlen(ev[0]);
			int p[20] = {2, 2, 1, 1, 1, 1, 1, 1, 1, 1};
			{
				int i = 0;
				for (; i < 10; i++)
					slist[i][0] = '\0';
			}
			get_parms(ev[0], script, h, p);
			if (strlen(slist[0]) > 0 && strlen(slist[1]) > 0) {
				int myscript1 = scripting_load_script(slist[0], sinfo[script]->sprite, 0);
				if (myscript1 == 0) {
					log_error("❗ [DinkC] external: Couldn't find %s.c (for "
							"procedure %s)",
							slist[0], slist[1]);
					PL_RETURN(DCPS_GOTO_NEXTLINE);
				}
				log_trace("process_line: assign arguments for myscript1: %d, "
						"(%s)", myscript1, RINFO_NAME(myscript1));
				log_trace("process_line: rinfo[%d]->arg1 = %d", myscript1,
						nlist[2]);
				log_trace("process_line: rinfo[%d]->arg2 = %d", myscript1,
						nlist[3]);
				log_trace("process_line: rinfo[%d]->arg3 = %d", myscript1,
						nlist[4]);
				log_trace("process_line: rinfo[%d]->arg4 = %d", myscript1,
						nlist[5]);
				log_trace("process_line: rinfo[%d]->arg5 = %d", myscript1,
						nlist[6]);
				log_trace("process_line: rinfo[%d]->arg6 = %d", myscript1,
						nlist[7]);
				log_trace("process_line: rinfo[%d]->arg7 = %d", myscript1,
						nlist[8]);
				log_trace("process_line: rinfo[%d]->arg8 = %d", myscript1,
						nlist[9]);

				rinfo(myscript1)->arg1 = nlist[2];
				rinfo(myscript1)->arg2 = nlist[3];
				rinfo(myscript1)->arg3 = nlist[4];
				rinfo(myscript1)->arg4 = nlist[5];
				rinfo(myscript1)->arg5 = nlist[6];
				rinfo(myscript1)->arg6 = nlist[7];
				rinfo(myscript1)->arg7 = nlist[8];
				rinfo(myscript1)->arg8 = nlist[9];
				if (scripting_proc_exists(myscript1, slist[1])) {
					log_trace("process_line: rinfo[%d]->proc_return = %d (%s)",
							myscript1, script, RINFO_NAME(script));
					rinfo(myscript1)->proc_return = script;
					scripting_run_proc(myscript1, slist[1]);
					PL_RETURN(DCPS_YIELD);
				} else {
					log_error("❗ [DinkC] external: Couldn't find procedure %s in "
							"%s.",
							slist[1], slist[0]);
					scripting_kill_script(myscript1);
				}
			}
			strcpy_nooverlap(s, h);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}

		if (strchr(h, '(') != NULL) {
			//lets attempt to run a procedure
			int myscript =
					scripting_load_script(sinfo[script]->name, sinfo[script]->sprite, 0);
			h += strlen(ev[0]);
			int p[20] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
			get_parms(ev[0], script, h, p);
			if (scripting_proc_exists(myscript, ev[0])) {
				log_trace(
						"process_line: assign arguments for myscript: %d, (%s)",
						myscript, RINFO_NAME(myscript));
				log_trace("process_line: rinfo[%d]->arg1 = %d", myscript,
						nlist[0]);
				log_trace("process_line: rinfo[%d]->arg2 = %d", myscript,
						nlist[1]);
				log_trace("process_line: rinfo[%d]->arg3 = %d", myscript,
						nlist[2]);
				log_trace("process_line: rinfo[%d]->arg4 = %d", myscript,
						nlist[3]);
				log_trace("process_line: rinfo[%d]->arg5 = %d", myscript,
						nlist[4]);
				log_trace("process_line: rinfo[%d]->arg6 = %d", myscript,
						nlist[5]);
				log_trace("process_line: rinfo[%d]->arg7 = %d", myscript,
						nlist[6]);
				log_trace("process_line: rinfo[%d]->arg8 = %d", myscript,
						nlist[7]);
				log_trace("process_line: rinfo[%d]->arg9 = %d", myscript,
						nlist[8]);
				log_trace("process_line: rinfo[%d]->proc_return = %d (%s)",
						myscript, script, RINFO_NAME(script));

				/* Custom procedure in the current script */
				rinfo(myscript)->arg1 = nlist[0];
				rinfo(myscript)->arg2 = nlist[1];
				rinfo(myscript)->arg3 = nlist[2];
				rinfo(myscript)->arg4 = nlist[3];
				rinfo(myscript)->arg5 = nlist[4];
				rinfo(myscript)->arg6 = nlist[5];
				rinfo(myscript)->arg7 = nlist[6];
				rinfo(myscript)->arg8 = nlist[7];
				rinfo(myscript)->arg9 = nlist[8];
				rinfo(myscript)->proc_return = script;
				scripting_run_proc(myscript, ev[0]);
				PL_RETURN(DCPS_YIELD);
			} else {
				/* Try custom global procedure */
				int i = 0;
				for (; i < 100; i++) {
					/* Skip empty slots */
					if (strlen(play.func[i].func) == 0)
						continue;

					if (compare(play.func[i].func, ev[0])) {
						myscript = scripting_load_script(play.func[i].file,
											sinfo[script]->sprite, 0);

						log_trace(
								"process_line: assign arguments for myscript: %d, (%s)",
								myscript, RINFO_NAME(myscript));
						log_trace("process_line: rinfo[%d]->arg1 = %d", myscript,
								nlist[0]);
						log_trace("process_line: rinfo[%d]->arg2 = %d", myscript,
								nlist[1]);
						log_trace("process_line: rinfo[%d]->arg3 = %d", myscript,
								nlist[2]);
						log_trace("process_line: rinfo[%d]->arg4 = %d", myscript,
								nlist[3]);
						log_trace("process_line: rinfo[%d]->arg5 = %d", myscript,
								nlist[4]);
						log_trace("process_line: rinfo[%d]->arg6 = %d", myscript,
								nlist[5]);
						log_trace("process_line: rinfo[%d]->arg7 = %d", myscript,
								nlist[6]);
						log_trace("process_line: rinfo[%d]->arg8 = %d", myscript,
								nlist[7]);
						log_trace("process_line: rinfo[%d]->arg9 = %d", myscript,
								nlist[8]);

						rinfo(myscript)->arg1 = nlist[0];
						rinfo(myscript)->arg2 = nlist[1];
						rinfo(myscript)->arg3 = nlist[2];
						rinfo(myscript)->arg4 = nlist[3];
						rinfo(myscript)->arg5 = nlist[4];
						rinfo(myscript)->arg6 = nlist[5];
						rinfo(myscript)->arg7 = nlist[6];
						rinfo(myscript)->arg8 = nlist[7];
						rinfo(myscript)->arg9 = nlist[8];
						if (scripting_proc_exists(myscript, ev[0])) {
							log_trace("process_line: rinfo[%d]->proc_return = %d (%s)",
									myscript, script, RINFO_NAME(script));
							rinfo(myscript)->proc_return = script;
							scripting_run_proc(myscript, ev[0]);
							PL_RETURN(DCPS_YIELD);
						}
						break;
					}
				}
				log_error("❗ [DinkC] Procedure void %s( void ); not found in "
						"script %s."
						" (word 2 was \"%s\")",
						ev[0],
						sinfo[myscript] != NULL ? sinfo[myscript]->name : "",
						ev[1]);
				scripting_kill_script(myscript);
			}

			/*seperate_string(h, 1,'(',line);

	int myscript = load_script(rinfo(script)->name, rinfo(script)->sprite, false);

	if (locate( myscript, line))
	{
	rinfo[myscript]->proc_return = script;
	run_script(myscript);
	PL_RETURN(DCPS_YIELD);
	} else
	{
	Msg("ERROR:  Procedure void %s( void ); not found in script %s. (word 2 was %s) ", line,
	ev[1], rinfo[myscript]->name);
	kill_script(myscript);
	} */
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}
	} else {
		/* v1.07 function that are implemented differently than in v1.08 */
		if (compare(ev[0], "external")) {
			h += strlen(ev[0]);
			int p[20] = {2, 2, 0, 0, 0, 0, 0, 0, 0, 0};
			if (get_parms(ev[0], script, h, p)) {
				int myscript1 = scripting_load_script(slist[0], sinfo[script]->sprite, 0);
				if (myscript1 == 0) {
					log_error("❗ [DinkC] external: Couldn't find %s.c (for "
							"procedure %s)",
							slist[0], slist[1]);
					PL_RETURN(DCPS_GOTO_NEXTLINE);
				}
				if (scripting_proc_exists(myscript1, slist[1])) {
					rinfo(myscript1)->proc_return = script;
					scripting_run_proc(myscript1, slist[1]);
					PL_RETURN(DCPS_YIELD);
				} else {
					log_error("❗ [DinkC] external: Couldn't find procedure %s in "
							"%s.",
							slist[1], slist[0]);
					scripting_kill_script(myscript1);
				}
			} else {
				log_error("❗ [DinkC] %s: procedure 'external' takes 2 parameters"
						" (offset %ld)",
						sinfo[script]->name, rinfo(script)->current);
			}
			strcpy_nooverlap(s, h);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}

		if (strchr(h, '(') != NULL) {
			//lets attempt to run a procedure
			char* proc = separate_string(h, 1, '(');
			int myscript = scripting_load_script(sinfo[script]->name, sinfo[script]->sprite, 0);

			if (scripting_proc_exists(myscript, proc)) {
				rinfo(myscript)->proc_return = script;
				scripting_run_proc(myscript, proc);
				free(proc);
				PL_RETURN(DCPS_YIELD);
			} else {
				log_error("❗ [DinkC] Procedure void %s( void ); not found in "
						"script %s. (word 2 was %s)", proc, sinfo[script]->name, ev[1]);
				scripting_kill_script(myscript);
			}
			free(proc);
			PL_RETURN(DCPS_GOTO_NEXTLINE);
		}

		log_error("❗ [DinkC] \"%s\" unknown in %s, offset %ld.", ev[0],
				sinfo[script]->name, rinfo(script)->current);
		//in a thingie, ready to go
	}

bad:
	strcpy(s, "\n"); /* jump to next line */
	//PL_RETURN(DCPS_GOTO_NEXTLINE);
	PL_RETURN(DCPS_CONTINUE);

good:
	strcpy_nooverlap(s, h);
	//s = h
	//Msg("ok, continuing with running %s..",s);
	PL_RETURN(DCPS_CONTINUE);
}

/****************/
/*  Hash table  */
/*              */
/****************/

/* Hash table of bindings, build dynamically (depending on 'dversion',
not statically) */
//Hash_table* bindings = NULL;
//New bindings system
std::unordered_map<std::string, binding> dinkc_bindings;

/* Auxiliary functions for hash */
//static size_t dinkc_bindings_hasher(const void* x, size_t tablesize) {
//	return hash_string(((struct binding*)x)->funcname, tablesize);
	// We could also call 'hash_pjw' from module 'hash-pjw'
//}

//static bool dinkc_bindings_comparator(const void* a, const void* b) {
//	return !strcmp(((struct binding*)a)->funcname,
//				((struct binding*)b)->funcname);
//}

/**
 * Search a binding by function name
 */
struct binding* dinkc_bindings_lookup(char* funcname) {
	log_enter("dinkc_bindings_lookup: funcname: %s", funcname);
	struct binding search;
	struct binding* result;
	char* lcfuncname = strdup(funcname);
	char* pc;
	for (pc = lcfuncname; *pc != '\0'; pc++)
		*pc = tolower(*pc);
	search.funcname = lcfuncname;

	//result = (struct binding*)hash_lookup(hash, &search);
	try {
	result = &dinkc_bindings.at(lcfuncname);
	}
	catch (const std::exception&) {
		result = NULL;
	}
	//log_debug("DinkC binding for '%s': %s", funcname, result != NULL ? result->funcname : "NULL");
	free(lcfuncname);

	log_exit("dinkc_bindings_lookup");
	return result;
}

/**
 * Add a new binding to hash table 'hash'.
 */
//no longer needed probably
void dinkc_bindings_add(struct binding* pbd) {
	log_enter("dinkc_bindings_add");
	void* slot = dinkc_bindings_lookup(pbd->funcname);
	if (slot != NULL) {
		log_fatal("Internal error: attempting to redeclare DinkC function %s",
				pbd->funcname);
		exit(EXIT_FAILURE);
	}

	/* Copy uninitialized binding in hash table */
	//if (hash_insert(hash, newslot) == NULL) {
	//	log_fatal("Not enough memory to declare DinkC function %s",
	//			pbd->funcname);
	//	exit(EXIT_FAILURE);
	//}
	log_exit("dinkc_bindings_add");
}

static void dinkc_free_data(void *data)
{
  free(data);
}

int dinkc_run_script_proc(int script, char *proc)
{
  if (!locate(script, proc)) {
	return 0;
  }


  dinkc_run_script(script);
  return 1;
}

void dinkc_init(struct script_engine *script_engine) {
	log_enter("dinkc_init");
	//Phoenix stuff
	script_engine->active = 1;
	script_engine->name = strdup("DinkC");
	script_engine->extensions = XNMALLOC(3, char *);
	script_engine->extensions[0] = strdup("c");
	script_engine->extensions[1] = strdup("d");
	script_engine->extensions[2] = NULL;

	script_engine->allocate_data = dinkc_allocate_data;
	script_engine->free_data = dinkc_free_data;

	script_engine->load_script = dinkc_load_script;
	script_engine->kill_script = dinkc_kill_script;
	script_engine->script_proc_exists = locate;
	script_engine->run_script_proc = dinkc_run_script_proc;
	script_engine->resume_script = dinkc_run_script;
	//memset(&rinfo, 0, sizeof(rinfo));
	game_choice_clear();

	/* Set all string params pointers to NULL */
	int i = 0;
	for (; i < 10; i++) {
		/* alloc empty strings; will be replaced as needed in
	get_parm(...) */
		slist[i] = strdup("");
	}

	log_exit("dinkc_init");
}

void dinkc_quit() {
	log_enter("dinkc_quit");
	int i = 0;
	for (; i < 10; i++) {
		if (slist[i] != NULL)
			free(slist[i]);
		slist[i] = NULL;
	}
	kill_all_vars();
	dinkc_bindings_quit();
	log_exit("dinkc_quit");
}
