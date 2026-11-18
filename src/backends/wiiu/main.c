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
 * Wii U entry point. The game data and everything the game writes
 * (configs, saves, screenshots, the log) live in WIIU_DATADIR on the
 * SD card.
 *
 * =======================================================================
 */

#include <sys/stat.h>
#include <unistd.h>

#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

#include "../../common/header/common.h"
#include "wiiu.h"

int
main(int argc, char **argv)
{
	/* The HOME menu needs ProcUI running before anything else. */
	WHBProcInit();
	WHBLogCafeInit();
	WHBLogUdpInit();

	mkdir(WIIU_DATADIR, 0755);
	chdir(WIIU_DATADIR);
	Sys_WiiU_OpenLog(WIIU_DATADIR "/yquake2.log");

	/* Keep everything in one folder on the SD card. */
	is_portable = true;
	Q_strlcpy(datadir, WIIU_DATADIR, sizeof(datadir));

	/* The loader may pass no arguments at all. */
	static char *default_argv[] = {"yquake2", NULL};

	if (argc < 1 || argv == NULL)
	{
		argc = 1;
		argv = default_argv;
	}

	// Initialize the game.
	// Never returns.
	Qcommon_Init(argc, argv);

	return 0;
}
