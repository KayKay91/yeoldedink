/**
 * Different ways to give information to the user

 * Copyright (C) 1997, 1998, 1999, 2002, 2003  Seth A. Robinson
 * Copyright (C) 2007, 2008, 2009, 2014  Sylvain Beucler
 * Copyright (C) 2022 CRTS
 * Copyright (C) 2024 Yeoldetoast

 * This file is part of YeOldeDink

 * YeOldeDink is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.

 * YeOldeDink is distributed in the hope that it will be useful, but
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

#include <iostream>
#include <string.h> /* strerror */
#include <errno.h>
#include "log.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/ringbuffer_sink.h"
#include "game_engine.h"
#include "debug_imgui.h"
#include "debug.h"

int debug_mode = 0;
static std::string lastLog;
static SDL_LogOutputFunction sdlLogger;
static std::string filename;
static FILE* out = NULL;

static char* init_error_msg = NULL;

void log_output(void* userdata, int category, SDL_LogPriority priority,
				const char* message) {
	lastLog = message;
	//Yeolde: added this for imgui debug window
	Buf.append(message);
	Buf.append("\n");
	debug_autoscroll[0] = true;

	//Coloured text instead of the default SDL stuff. Should refactor evnetually
	if (dbg.debug_coltext)
	{
		if (priority == SDL_LOG_PRIORITY_ERROR || priority == SDL_LOG_PRIORITY_CRITICAL)
		{
			spdlog::error(message);
			if (debug_errorpause) {
				debug_paused = true;
				pause_everything();
				dbg.logwindow = true;
			}
			errBuf.append(message);
			errBuf.append("\n");
			debug_autoscroll[1] = true;
		}
		else if (priority == SDL_LOG_PRIORITY_WARN)
		{
			spdlog::warn(message);
			errBuf.append(message);
			errBuf.append("\n");
		}
		else if (priority == SDL_LOG_PRIORITY_INFO)
		{
			spdlog::info(message);
			infBuf.append(message);
			infBuf.append("\n");
			debug_autoscroll[2] = true;
		}
		else if (priority == SDL_LOG_PRIORITY_DEBUG)
		{
			spdlog::debug(message);
			dbgBuf.append(message);
			dbgBuf.append("\n");
			debug_autoscroll[3] = true;
		}
		else
		{
			spdlog::trace(message);
			traceBuf.append(message);
			traceBuf.append("\n");
			debug_autoscroll[4] = true;
		}
	}
	else
	{
		//Use the existing SDL logger if they don't like colours - might get rid of
		sdlLogger(userdata, category, priority, message);
	}

	if (debug_mode && dbg.debug_conwrite) {
		// display message on screen in debug mode
		// also write to DEBUG.TXT if option is on

		//if (out != NULL) {
		//	fputs(message, out);
		//	fputc('\n', out);
		//}
		auto l = spdlog::get("file_sink");
		l->set_level(spdlog::level::debug);
	}
}

void log_init() {
	/* Decorate default SDL log */
	SDL_LogGetOutputFunction(&sdlLogger, NULL);
	SDL_LogSetOutputFunction(log_output, NULL);
	//Create our console output and lastlog sink
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_level(spdlog::level::debug);
	auto lastlog_sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(5);
	spdlog::sinks_init_list sink_list = { lastlog_sink, console_sink };
	spdlog::logger logger("multi_sink", sink_list.begin(), sink_list.end());
	spdlog::set_default_logger(std::make_shared<spdlog::logger>("", sink_list));
}

void log_quit() {
	SDL_LogSetOutputFunction(sdlLogger, NULL);
	if (init_error_msg != NULL)
		free(init_error_msg);
}

void log_debug_on(int level) {
	debug_mode = 1;
	/*SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);*/
	switch (level) {
	case 0:
		SDL_LogSetAllPriority(SDL_LOG_PRIORITY_ERROR);
		spdlog::set_level(spdlog::level::err);
		break;
	case 1:
		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
		spdlog::set_level(spdlog::level::debug);
		break;
	case 2:
		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
		spdlog::set_level(spdlog::level::info);
		break;
	default:
		SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
		spdlog::set_level(spdlog::level::trace);
		break;
	}

}

void log_debug_off() {
	debug_mode = 0;
	spdlog::set_level(spdlog::level::off);
	spdlog::enable_backtrace(5);
	SDL_LogResetPriorities();
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_ERROR);
	/* Avoid startup error about X11 missing symbols */
	SDL_LogSetPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_CRITICAL);
	/* If you need to debug early: */
	/* SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG); */
}

const char* log_getLastLog() {
	return lastLog.c_str();
}

void log_set_output_file(const char* fn) {
	filename = fn;
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
	spdlog::get("")->sinks().push_back(file_sink);
}
