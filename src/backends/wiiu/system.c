/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * System specific stuff for the Wii U: time, files, the console log,
 * the HOME menu, and the game and renderer, which are linked in
 * statically instead of being loaded as libraries.
 *
 * =======================================================================
 */

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <coreinit/exit.h>
#include <coreinit/foreground.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <whb/log.h>
#include <whb/proc.h>

#include "../../common/header/common.h"
#include "../../common/header/glob.h"
#include "wiiu.h"

/* Entry points of the statically linked game and renderer. Their other
   symbols are made local at build time, so these are the only names they
   share with the engine. */
void *GetGameAPI(void *import);
void *GetRefAPI(void *import);

qboolean stdin_active = false;

extern FILE *logfile;

char cfgdir[MAX_OSPATH] = CFGDIRNAME;

static FILE *wiiu_log;
static qboolean wiiu_leaving_foreground;

/* ================================================================ */

void
Sys_WiiU_OpenLog(const char *path)
{
	wiiu_log = fopen(path, "w");
}

void
Sys_WiiU_SyncLog(void)
{
	if (wiiu_log)
	{
		fflush(wiiu_log);
		fsync(fileno(wiiu_log));
	}
}

void
Sys_WiiU_LogRaw(const char *string)
{
	WHBLogWrite(string);

	if (wiiu_log)
	{
		fputs(string, wiiu_log);
		fflush(wiiu_log);
	}
}

/*
 * Shuts down and returns to the Wii U menu.
 */
static YQ2_ATTR_NORETURN void
WiiU_Exit(int status)
{
	Sys_WiiU_PauseWatchdog(1);

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif
	Qcommon_Shutdown();

	if (logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}

	if (wiiu_log)
	{
		Sys_WiiU_SyncLog();
		fclose(wiiu_log);
		wiiu_log = NULL;
	}

	if (!wiiu_leaving_foreground)
	{
		/* Quitting from the game's menu or an error: ask for the Wii U
		   menu and run ProcUI until it has released the foreground. The
		   renderer is gone, so drop WHBGfx's callbacks, they would wait
		   on the GPU that has been shut down. */
		ProcUIClearCallbacks();
		SYSLaunchMenu();

		while (WHBProcIsRunning())
		{
		}
	}

	WHBProcShutdown();
	exit(status);
}

void
Sys_WiiU_Frame(void)
{
	Sys_WiiU_Heartbeat();

	/* WHBProcIsRunning() blocks while the HOME menu is open. */
	Sys_WiiU_PauseWatchdog(1);
	int running = WHBProcIsRunning();
	Sys_WiiU_PauseWatchdog(0);

	if (!running)
	{
		/* HOME menu "close software", or a power down. */
		wiiu_leaving_foreground = true;
		WiiU_Exit(0);
	}
}

void
Sys_Error(const char *error, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, error);
	vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	Sys_WiiU_LogRaw("Error: ");
	Sys_WiiU_LogRaw(string);
	Sys_WiiU_LogRaw("\n");

	WiiU_Exit(1);
}

void
Sys_Quit(void)
{
	Sys_WiiU_LogRaw("------------------------------------\n");
	WiiU_Exit(0);
}

void
Sys_Init(void)
{
}

/* ================================================================ */

char *
Sys_ConsoleInput(void)
{
	return NULL;
}

void
Sys_ConsoleOutput(char *string)
{
	/* 0x01 and 0x02 mark red and green lines. */
	if ((string[0] == 0x01) || (string[0] == 0x02))
	{
		string++;
	}

	Sys_WiiU_Heartbeat();
	Sys_WiiU_LogRaw(string);
}

/* ================================================================ */

long long
Sys_Microseconds(void)
{
	static OSTime first;
	OSTime now = OSGetSystemTime();

	if (first == 0)
	{
		/* Set back by 1ms so neither this function nor
		   Sys_Milliseconds() ever returns 0. */
		first = now - OSMillisecondsToTicks(1);
	}

	return (long long)OSTicksToMicroseconds(now - first);
}

int
Sys_Milliseconds(void)
{
	return (int)(Sys_Microseconds() / 1000ll);
}

void
Sys_Nanosleep(int nanosec)
{
	OSSleepTicks(OSNanosecondsToTicks(nanosec));
}

/* ================================================================ */

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static char findpattern[MAX_OSPATH];
static DIR *fdir;

static char *
WiiU_FindMatch(void)
{
	struct dirent *d;

	while ((d = readdir(fdir)) != NULL)
	{
		if (!*findpattern || glob_match(findpattern, d->d_name))
		{
			if ((strcmp(d->d_name, ".") != 0) && (strcmp(d->d_name, "..") != 0))
			{
				Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase,
					d->d_name);
				return findpath;
			}
		}
	}

	return NULL;
}

char *
Sys_FindFirst(const char *path, unsigned musthave, unsigned canhave)
{
	char *p;

	if (fdir)
	{
		Sys_Error("Sys_BeginFind without close");
	}

	Q_strlcpy(findbase, path, sizeof(findbase));

	if ((p = strrchr(findbase, '/')) != NULL)
	{
		*p = 0;
		Q_strlcpy(findpattern, p + 1, sizeof(findpattern));
	}
	else
	{
		strcpy(findpattern, "*");
	}

	if (strcmp(findpattern, "*.*") == 0)
	{
		strcpy(findpattern, "*");
	}

	if ((fdir = opendir(findbase)) == NULL)
	{
		return NULL;
	}

	return WiiU_FindMatch();
}

char *
Sys_FindNext(unsigned musthave, unsigned canhave)
{
	if (fdir == NULL)
	{
		return NULL;
	}

	return WiiU_FindMatch();
}

void
Sys_FindClose(void)
{
	if (fdir != NULL)
	{
		closedir(fdir);
	}

	fdir = NULL;
}

/* ================================================================ */

void
Sys_UnloadGame(void)
{
}

void *
Sys_GetGameAPI(void *parms)
{
	Com_Printf("Using the built-in game.\n");
	return GetGameAPI(parms);
}

/* ================================================================ */

void
Sys_Mkdir(const char *path)
{
	char dir[MAX_OSPATH];
	size_t len;

	/* mkdir() on the Wii U doesn't like a trailing slash. */
	Q_strlcpy(dir, path, sizeof(dir));
	len = strlen(dir);

	while (len > 1 && dir[len - 1] == '/')
	{
		dir[--len] = '\0';
	}

	/* FS_CreatePath() walks the whole path. "fs:", "fs:/vol" and
	   "fs:/vol/external01" can't be created (or stat()ed), and exist. */
	{
		static const char datadir_path[] = WIIU_DATADIR;
		size_t dirlen = strlen(dir);

		if (strchr(dir, '/') == NULL ||
		    (dirlen < sizeof(datadir_path) &&
		     strncmp(datadir_path, dir, dirlen) == 0 &&
		     (datadir_path[dirlen] == '/' || datadir_path[dirlen] == '\0')))
		{
			return;
		}
	}

	if (!Sys_IsDir(dir))
	{
		if (mkdir(dir, 0755) != 0)
		{
			Com_Error(ERR_FATAL, "Couldn't create dir %s\n", dir);
		}
	}
}

qboolean
Sys_IsDir(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) != -1)
	{
		if (S_ISDIR(sb.st_mode))
		{
			return true;
		}
	}

	return false;
}

qboolean
Sys_IsFile(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) != -1)
	{
		if (S_ISREG(sb.st_mode))
		{
			return true;
		}
	}

	return false;
}

char *
Sys_GetHomeDir(void)
{
	/* Portable mode: configs and saves live next to the game data. */
	return NULL;
}

void
Sys_Remove(const char *path)
{
	if (remove(path) == -1 && errno != ENOENT)
	{
		Com_Printf("%s: remove %s: %s\n",
			__func__, path, strerror(errno));
	}
}

int
Sys_Rename(const char *from, const char *to)
{
	/* "to" exists now: Q_fopen() may remember it as missing. */
	Q_FileCacheInvalidate();
	return rename(from, to);
}

void
Sys_RemoveDir(const char *path)
{
	char filepath[MAX_OSPATH];
	struct dirent *file;
	DIR *directory;

	if (Sys_IsDir(path))
	{
		directory = opendir(path);

		if (directory)
		{
			while ((file = readdir(directory)) != NULL)
			{
				snprintf(filepath, MAX_OSPATH, "%s/%s", path, file->d_name);
				Sys_Remove(filepath);
			}

			closedir(directory);
			Sys_Remove(path);
		}
	}
}

qboolean
Sys_Realpath(const char *in, char *out, size_t size)
{
	/* The Wii U has no symlinks, and the paths we use are absolute. */
	Q_strlcpy(out, in, size);
	return true;
}

/* ================================================================ */

void *
Sys_GetProcAddress(void *handle, const char *sym)
{
	return NULL;
}

void
Sys_FreeLibrary(void *handle)
{
}

void *
Sys_LoadLibrary(const char *path, const char *sym, void **handle)
{
	*handle = NULL;

	/* Only the GL3 renderer is linked in. */
	if (sym && strcmp(sym, "GetRefAPI") == 0)
	{
		*handle = (void *)GetRefAPI;
		return (void *)GetRefAPI;
	}

	Com_Printf("%s: %s is not available on the Wii U\n", __func__, path);
	return NULL;
}

/* ================================================================ */

void
Sys_GetWorkDir(char *buffer, size_t len)
{
	if (getcwd(buffer, len) != 0)
	{
		return;
	}

	buffer[0] = '\0';
}

qboolean
Sys_SetWorkDir(char *path)
{
	return chdir(path) == 0;
}

/* ================================================================ */

/*
 * newlib declares basename() in <libgen.h> but wut doesn't provide it.
 * Like POSIX basename(), but never modifies path.
 */
char *
basename(char *path)
{
	static char dot[] = ".";
	char *slash;

	if (path == NULL || *path == '\0')
	{
		return dot;
	}

	slash = strrchr(path, '/');

	/* "dir/" style paths aren't passed in by the engine. */
	return (slash && slash[1]) ? slash + 1 : path;
}
