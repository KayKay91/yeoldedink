/**
 * Compute and store the search paths

 * Copyright (C) 2007, 2008, 2009, 2014, 2015  Sylvain Beucler

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
#include <stdlib.h>
#include <string.h> /* strdup */
#include <unistd.h> /* getcwd */
#include <errno.h>
#include <filesystem>
#include <vector>

#include "io_util.h"
#include "paths.h"
#include "log.h"

#if defined _WIN32 || defined __WIN32__ || defined __CYGWIN__
#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0401
#include <windows.h>
#include <shlobj.h>
#endif

/* mkdir */
#include <sys/stat.h>
#include <sys/types.h>
#if defined _WIN32 || defined __WIN32__ || defined __CYGWIN__
#include <direct.h>
#define mkdir(name, mode) mkdir(name)
#endif

#include "str_util.h" /* asprintf_append */

#ifdef __ANDROID__
#include <unistd.h> /* chdir */
#include <errno.h>
#include "SDL.h"
#endif

#ifndef __EMSCRIPTEN__
#include "whereami.h"
#include "tinyfiledialogs.h"
#endif

#ifdef __APPLE__
#include <sysdir.h>
#include <limits.h>
#include <glob.h>
#endif

namespace fs = std::filesystem;

/* Pleases gnulib's error module */
char* program_name = PACKAGE;

static char* pkgdatadir = NULL;
static char* fallbackdir = NULL;
static char* dmoddir = NULL;
static char* dmodname = NULL;
static char* userappdir = NULL;
std::vector<std::string> dmoddir_vec;

/**
 * getcwd without size restriction
 * aka get_current_dir_name(3)
 */
char* paths_getcwd() {
	char* cwd = NULL;
	int cwd_size = PATH_MAX;
	do {
		cwd = (char*)realloc(cwd, cwd_size);
		getcwd(cwd, cwd_size);
		cwd_size *= 2;
	} while (errno == ERANGE);
	return cwd;
}

static char* br_strcat(const char* str1, const char* str2) {
	char* result;
	size_t len1, len2;

	if (str1 == NULL)
		str1 = "";
	if (str2 == NULL)
		str2 = "";

	len1 = strlen(str1);
	len2 = strlen(str2);

	result = (char*)malloc(len1 + len2 + 1);
	memcpy(result, str1, len1);
	memcpy(result + len1, str2, len2);
	result[len1 + len2] = '\0';

	return result;
}

static char* br_build_path(const char* dir, const char* file) {
	char *dir2, *result;
	size_t len;
	int must_free = 0;

	len = strlen(dir);
	if (len > 0 && dir[len - 1] != '/') {
		dir2 = br_strcat(dir, "/");
		must_free = 1;
	} else {
		dir2 = (char*)dir;
	}

	result = br_strcat(dir2, file);
	if (must_free)
		free(dir2);
	return result;
}

#ifdef __APPLE__
//https://stackoverflow.com/questions/5123361/finding-library-application-support-from-c
std::string expandTilde(const char* str) {
    if (!str) return {};

    glob_t globbuf;
    if (glob(str, GLOB_TILDE, nullptr, &globbuf) == 0) {
        std::string result(globbuf.gl_pathv[0]);
        globfree(&globbuf);
        return result;
    } else {
        log_error("Unable to get config path");
    }
}

std::string settingsPath() {
    char path[PATH_MAX];
    auto state = sysdir_start_search_path_enumeration(SYSDIR_DIRECTORY_APPLICATION_SUPPORT, SYSDIR_DOMAIN_MASK_USER);
    if ((state = sysdir_get_next_search_path_enumeration(state, path))) {
        return expandTilde(path);
    } else {
        log_error("Failed to get macOS settings path");
    }
}
#endif

/**
 * Allow using paths in test suite until we implement in-memory mocked
 * filesystem, using std::stream rather than hard-to-mock FILE*
 */
#ifdef _WIN32
#include <shellapi.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
void ts_paths_init() {
	char* cwd = paths_getcwd();
	pkgdatadir = br_build_path(cwd, "tmp_ts/pkgdatadir");
	fallbackdir = br_build_path(cwd, "tmp_ts/fallbackdir");
	dmoddir = br_build_path(cwd, "tmp_ts/dmoddir");
	dmodname = br_build_path(cwd, "dmoddir");
	userappdir = br_build_path(cwd, "tmp_ts/useradddir");
#ifdef _WIN32
	SHFILEOPSTRUCT fileop;
	fileop.hwnd = NULL;
	fileop.wFunc = FO_DELETE;
	fileop.pFrom = br_build_path(cwd, "tmp_ts\0");
	fileop.pTo = NULL;
	fileop.fFlags = FOF_NOCONFIRMATION | FOF_SILENT; // do not prompt the user
	fileop.lpszProgressTitle = NULL;
	fileop.hNameMappings = NULL;
	SHFileOperation(&fileop);
#else
	{
		pid_t pid = 0;
		if ((pid = fork()) < 0) {
			perror("fork");
		} else if (pid == 0) {
			if (execlp("rm", "rm", "-rf", "tmp_ts/", NULL) < 0)
				perror("execlp");
			exit(EXIT_FAILURE);
		} else {
			int status = 0;
			waitpid(pid, &status, 0);
		}
	}
#endif
	free(cwd);
	if (mkdir("tmp_ts", 0777) < 0)
		perror("mkdir");
	if (mkdir(pkgdatadir, 0777) < 0)
		perror("mkdir");
	if (mkdir(fallbackdir, 0777) < 0)
		perror("mkdir");
	if (mkdir(dmoddir, 0777) < 0)
		perror("mkdir");
	if (mkdir(dmoddir, 0777) < 0)
		perror("mkdir");
	if (mkdir(userappdir, 0777) < 0)
		perror("mkdir");
	// map.dat is always opened in "r" or "r+b", pre-create it:
	FILE* in = paths_dmodfile_fopen("map.dat", "w");
	fclose(in);
}

bool paths_init(char* argv0, char* refdir_opt, char* dmoddir_opt) {
	char* refdir = NULL;

	/** pkgdatadir **/
	{
#if defined __ANDROID__ || defined _WIN32 || defined __WIN32__ || defined __EMSCRIPTEN__ || defined __CYGWIN__
		/* "./freedink" */
		pkgdatadir = strdup("./" PACKAGE);
#elif defined __APPLE__
		pkgdatadir = br_build_path(BUILD_DATA_DIR, PACKAGE);
		#ifdef WELL_BEHAVED_DINK
		//library application support whatever dest
		fs::path d = br_build_path(settingsPath().c_str(), PACKAGE);
		//log_error("Destination is %s", d.c_str());
		//Only copy if it doesn't exist
		if (!fs::exists(d)) {
		//Source path
		int dirname_length;
		int length = wai_getExecutablePath(NULL,0, NULL);
		pkgdatadir = (char*)malloc(length + 1);
		wai_getExecutablePath(pkgdatadir, length, &dirname_length);
		pkgdatadir[dirname_length] = '\0';
		pkgdatadir = br_build_path(pkgdatadir, BUILD_DATA_DIR);
		pkgdatadir = br_build_path(pkgdatadir, PACKAGE);
		fs::path p = pkgdatadir;
		fs::copy(p, d, fs::copy_options::recursive);
		}
		pkgdatadir = strdup(d.c_str());
		#endif
		/* (e.g. "/usr/share/freedink") */
		//yeolde: since we're releasing mainly appimages let's make it search locally
		//actually let's make it use XDG paths, and leave this for MacOS
		//int length = wai_getExecutablePath(NULL,0, NULL);
		//int dirname_length;
		//pkgdatadir = (char*)malloc(length + 1);
		//wai_getExecutablePath(pkgdatadir, length, &dirname_length);
		//pkgdatadir[dirname_length] = '\0';
		//pkgdatadir = br_build_path(pkgdatadir, PACKAGE);
#else
	//GNU/Linux and stuff
	#ifdef WELL_BEHAVED_DINK
	//Check if it's already done
	fs::path d = br_build_path(br_build_path(getenv("HOME"), ".config"), PACKAGE);
	//log_error("Config path is: %s", d.c_str());
	if (!fs::exists(d)) {
		//It isn't, so we copy
		int dirname_length;
		//Get source path
		int length = wai_getExecutablePath(NULL,0, NULL);
		pkgdatadir = (char*)malloc(length + 1);
		wai_getExecutablePath(pkgdatadir, length, &dirname_length);
		pkgdatadir[dirname_length] = '\0';
		pkgdatadir = br_build_path(pkgdatadir, BUILD_DATA_DIR);
		pkgdatadir = br_build_path(pkgdatadir, PACKAGE);
		fs::path p = pkgdatadir;
		//log_error("Copying from %s to %s", p.c_str(), d.c_str());
		fs::copy(p, d, fs::copy_options::recursive);
	}
	#endif
	pkgdatadir = br_build_path(br_build_path(getenv("HOME"), ".config"), PACKAGE);
#endif
	}

	/** refdir  (e.g. "/usr/share/dink") **/
	{
		/** => refdir **/
		char* match = NULL;
		int nb_dirs = 8;
		char* lookup[nb_dirs];
		int i = 0;
		if (refdir_opt == NULL)
			lookup[0] = NULL;
		else
			lookup[0] = refdir_opt;

		/* Use absolute filename, otherwise SDL_rwops fails on Android (java.io.FileNotFoundException) */
		lookup[1] = paths_getcwd();

		/* FHS mentions optional 'share/games' which some Debian packagers
seem to be found of */
		/* PACKAGERS: don't alter these paths. FreeDink must run in a
_consistent_ way across platforms. If you need an alternate
path, consider using ./configure --prefix=..., or contact
bug-freedink@gnu.org to discuss it. */
		lookup[2] = br_build_path(BUILD_DATA_DIR, "dink");
		lookup[3] = "/usr/local/share/games/dink";
		lookup[4] = "/usr/local/share/dink";
		lookup[5] = "/usr/share/games/dink";
		lookup[6] = "/usr/share/dink";
		//TODO: add nicer paths to windows
		#ifdef _WIN32
		lookup[3] = "C:\\Program Files (x86)\\Dink Smallwood";
		#endif

		//for appimages and macos
		#ifdef WELL_BEHAVED_DINK
		int dirname_length;
		int length = wai_getExecutablePath(NULL,0, NULL);
		lookup[7] = (char*)malloc(length + 1);
		wai_getExecutablePath(lookup[7], length, &dirname_length);
		lookup[7][dirname_length] = '\0';
		lookup[7] = br_build_path(lookup[7], BUILD_DATA_DIR);
		#else
		lookup[7] = paths_getcwd();
		#endif

		for (; i < nb_dirs; i++) {
			char *dir_graphics_ci = NULL, *dir_tiles_ci = NULL;
			if (lookup[i] == NULL)
				continue;
			dir_graphics_ci = br_build_path(lookup[i], "dink/graphics");
			dir_tiles_ci = br_build_path(lookup[i], "dink/tiles");
			ciconvert(dir_graphics_ci);
			ciconvert(dir_tiles_ci);
			if (is_directory(dir_graphics_ci) && is_directory(dir_tiles_ci)) {
				match = lookup[i];
			}

			if (match == NULL && i == 0) {
				log_error("Invalid --refdir option: %s and/or %s are not "
						"accessible.",
						dir_graphics_ci, dir_tiles_ci);
				return false;
			}

			free(dir_graphics_ci);
			free(dir_tiles_ci);
			if (match != NULL)
				break;
		}
		refdir = match;
		if (refdir != NULL) {
			refdir = strdup(refdir);
		} else {
			//Dialogue box for refdir selection in case it can't find it.
			#ifndef __EMSCRIPTEN__
			refdir = strdup(tinyfd_selectFolderDialog("Please select your main data location", NULL));
			#endif
			if (refdir == NULL) {
			log_error("Error: cannot find reference directory (--refdir). I "
					"looked in:\n"
					// lookup[0] already treated above
					"- %s [current dir]\n"
					"- %s [build prefix]\n"
					"- %s [hard-coded prefix]\n"
					"- %s [hard-coded prefix]\n"
					"- %s [hard-coded prefix]\n"
					"- %s [hard-coded prefix]\n"
					"The reference directory contains among others the "
					"'dink/graphics/' and 'dink/tiles/' directories (as well "
					"as D-Mods).",
					lookup[1], lookup[2], lookup[3], lookup[4], lookup[5],
					lookup[6]);
			return false;
			}
		}

		free(lookup[1]); // paths_getcwd
		free(lookup[2]); // br_build_path
	}

	/** fallbackdir (e.g. "/usr/share/dink/dink") **/
	/* (directory used when a file cannot be found in a D-Mod) */
	{ fallbackdir = br_strcat(refdir, "/dink"); }

	/** dmoddir (e.g. "/usr/share/dink/island") **/
	{
		if (dmoddir_opt != NULL && is_directory(dmoddir_opt)) {
			/* Use path given on the command line, either a full path or a
	path relative to the current directory. */
			/* Note: don't search for "dink" in the default dir if no
	'-game' option was given */
			dmoddir = strdup(dmoddir_opt);
		} else {
			/* Use path given on the command line, relative to $refdir */
			char* subdir = dmoddir_opt;
			if (subdir == NULL || strlen(subdir) == 0) /* no opt, or -g '' */
				subdir = "dink";
			dmoddir = (char*)malloc(strlen(refdir) + 1 + strlen(subdir) + 1);
			strcpy(dmoddir, refdir);
			strcat(dmoddir, "/");
			strcat(dmoddir, subdir);
			if (!is_directory(dmoddir)) {
				char* msg = NULL;

				asprintf_append(&msg,
								"Error: D-Mod directory '%s' doesn't exist. I "
								"looked in:\n",
								subdir);
				if (dmoddir_opt != NULL)
					asprintf_append(&msg, "- ./%s\n", dmoddir_opt);
				asprintf_append(&msg, "- %s (refdir is '%s')", dmoddir, refdir);

				log_error("%s", msg);
				free(msg);
				return false;
			}
		}
		/* Strip slashes */
		while (strlen(dmoddir) > 0 && dmoddir[strlen(dmoddir) - 1] == '/')
			dmoddir[strlen(dmoddir) - 1] = '\0';
	}

	/** dmodname (e.g. "island") **/
	/* Used to save games in ~/.dink/<dmod>/... */
	{
		/* Don't accept '.' or '' or '/..' (nameless) but accept 'island' (relative path) */
		int len = strlen(dmoddir);
		char* start = dmoddir + len;
		for (; start >= dmoddir; start--) /* basename(1) */
			if (*start == '\\' || *start == '/')
				break;
		start++; // we're either -1 or at the last slash, start is right after
		if (strcmp(start, ".") == 0 || strcmp(start, "..") == 0 ||
			strcmp(start, "") == 0) {
			log_error("Error: not loading nameless D-Mod '%s'", start);
			return false;
		}
		int dmodname_len = start - dmoddir;
		dmodname = (char*)malloc(dmodname_len + 1);
		strncpy(dmodname, start, dmodname_len);
		dmodname[dmodname_len] = '\0';
	}

	/** userappdir (e.g. "~/.dink") **/
	{
#if defined _WIN32 || defined __WIN32__ || defined __CYGWIN__
		userappdir = (char*)malloc(MAX_PATH);
		/* C:\Documents and Settings\name\Application Data */
		SHGetSpecialFolderPath(NULL, userappdir, CSIDL_APPDATA, 1);
#else
		char* envhome = getenv("HOME");
		if (envhome != NULL)
			userappdir = strdup(getenv("HOME"));
#endif
		if (userappdir != NULL) {
			userappdir = (char*)realloc(userappdir,
										strlen(userappdir) + 1 + 1 +
												strlen(PACKAGE) + 1);
			strcat(userappdir, "/");
#if defined _WIN32 || defined __WIN32__ || defined __CYGWIN__
#else
			strcat(userappdir, ".");
#endif
			strcat(userappdir, "dink");
		} else {
			// No detected home directory - saving in the reference
			// directory
			userappdir = strdup(refdir);
		}
	}

	log_info("datadir = %s", BUILD_DATA_DIR);
	log_info("pkgdatadir = %s", pkgdatadir);
	log_info("refdir = %s", refdir);
	log_info("fallbackdir = %s", fallbackdir);
	log_info("dmoddir = %s", dmoddir);
	log_info("dmodname = %s", dmodname);
	log_info("userappdir = %s", userappdir);

	free(refdir);
	return true;
}

const char* paths_getpkgdatadir(void) {
	return pkgdatadir;
}
const char* paths_getdmoddir(void) {
	return dmoddir;
}
const char* paths_getdmodname(void) {
	return dmodname;
}

const char* paths_getfallbackdir(void) {
	return fallbackdir;
}

char* paths_getluarequire(void) {
	char* fullpath = br_build_path(pkgdatadir, "/libraries/?.lua");
	//char* fallbackpath = br_build_path(fallbackdir, "/STORY/?.lua");
	ciconvert(fullpath);
	return fullpath;
}

char* paths_dmodfile(const char* file) {
	char* fullpath = br_build_path(dmoddir, file);
	ciconvert(fullpath);
	return fullpath;
}

FILE* paths_dmodfile_fopen(const char* file, const char* mode) {
	char* fullpath = paths_dmodfile(file);
	FILE* result = fopen(fullpath, mode);
	free(fullpath);
	return result;
}

char* paths_fallbackfile(const char* file) {
	char* fullpath = br_build_path(fallbackdir, file);
	ciconvert(fullpath);
	return fullpath;
}

FILE* paths_fallbackfile_fopen(const char* file, const char* mode) {
	char* fullpath = paths_fallbackfile(file);
	FILE* result = fopen(fullpath, mode);
	free(fullpath);
	return result;
}

char* paths_pkgdatafile(const char* file) {
	char* fullpath = br_build_path(pkgdatadir, file);
	ciconvert(fullpath);
	return fullpath;
}

FILE* paths_pkgdatafile_fopen(const char* file, const char* mode) {
	char* fullpath = paths_pkgdatafile(file);
	FILE* result = fopen(fullpath, mode);
	free(fullpath);
	return result;
}

FILE* paths_savegame_fopen(int num, const char* mode) {
	char* fullpath_in_dmoddir = NULL;
	char* fullpath_in_userappdir = NULL;
	FILE* fp = NULL;

	/* 20 decimal digits max for 64bit integer - should be enough :) */
	char file[4 + 20 + 4 + 1];
	sprintf(file, "save%d.dat", num);

	/** fullpath_in_userappdir **/
	char* savedir = strdup(userappdir);
	savedir = (char*)realloc(savedir,
							strlen(userappdir) + 1 + strlen(dmodname) + 1);
	strcat(savedir, "/");
	strcat(savedir, dmodname);
	/* Create directories if needed */
	if (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL)
		/* Note: 0777 & umask => 0755 in common case */
		if ((!is_directory(userappdir) && (mkdir(userappdir, 0777) < 0)) ||
			(!is_directory(savedir) && (mkdir(savedir, 0777) < 0))) {
			free(savedir);
			return NULL;
		}
	fullpath_in_userappdir = br_build_path(savedir, file);
	ciconvert(fullpath_in_userappdir);
	free(savedir);

	/** fullpath_in_dmoddir **/
	fullpath_in_dmoddir = paths_dmodfile(file);
	ciconvert(fullpath_in_dmoddir);

	/* Try ~/.dink (if present) when reading - but don't try that first
when writing - except in Emscripten where we need a stable,
exclusive mount point. */
#ifndef __EMSCRIPTEN__
	if (strchr(mode, 'r') != NULL)
#endif
		fp = fopen(fullpath_in_userappdir, mode);

	/* Try in the D-Mod dir */
	if (fp == NULL)
		fp = fopen(fullpath_in_dmoddir, mode);

	/* Then try in ~/.dink */
	if (fp == NULL)
		fp = fopen(fullpath_in_userappdir, mode);

	free(fullpath_in_dmoddir);
	free(fullpath_in_userappdir);

	return fp;
}

//ye: A cut-down variation of the above for Lua serialisation path only, everything else handled by Lua
//TODO: rewrite this in cpp
const char* paths_luasave(const char* filename, const char* mode) {
	char* fullpath_in_dmoddir = NULL;
	char* fullpath_in_userappdir = NULL;
	FILE* fp = NULL;

	/* 20 decimal digits max for 64bit integer - should be enough :) */
	char file[4 + 20 + 4 + 1];
	sprintf(file, filename);

	/** fullpath_in_userappdir **/
	char* savedir = strdup(userappdir);
	savedir = (char*)realloc(savedir, strlen(userappdir) + 1 + strlen(dmodname) + 1);
	strcat(savedir, "/");
	strcat(savedir, dmodname);
	/* Create directories if needed */
	if (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL)
		/* Note: 0777 & umask => 0755 in common case */
		if ((!is_directory(userappdir) && (mkdir(userappdir, 0777) < 0)) ||
			(!is_directory(savedir) && (mkdir(savedir, 0777) < 0))) {
			free(savedir);
			return NULL;
		}
	fullpath_in_userappdir = br_build_path(savedir, file);
	ciconvert(fullpath_in_userappdir);
	free(savedir);

	/** fullpath_in_dmoddir **/
	fullpath_in_dmoddir = paths_dmodfile(file);
	ciconvert(fullpath_in_dmoddir);

	/* Try ~/.dink (if present) when reading - but don't try that first
when writing - except in Emscripten where we need a stable,
exclusive mount point. */
#ifndef __EMSCRIPTEN__
	if (strchr(mode, 'r') != NULL)
#endif
		fp = fopen(fullpath_in_userappdir, mode);

	/* Try in the D-Mod dir */
	if (fp == NULL) {
		fp = fopen(fullpath_in_dmoddir, mode);
		fclose(fp);
		return fullpath_in_dmoddir;
	}

	/* Then try in ~/.dink */
	if (fp == NULL) {
		fp = fopen(fullpath_in_userappdir, mode);
		fclose(fp);
		return fullpath_in_userappdir;
	}

	free(fullpath_in_dmoddir);
	free(fullpath_in_userappdir);

}

void paths_quit(void) {
	free(pkgdatadir);
	free(fallbackdir);
	free(dmoddir);
	free(dmodname);
	free(userappdir);

	pkgdatadir = NULL;
	fallbackdir = NULL;
	dmoddir = NULL;
	dmodname = NULL;
	userappdir = NULL;
}
