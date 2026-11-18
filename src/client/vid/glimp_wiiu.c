/*
 * Copyright (C) 2010 Yamagi Burmeister
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
 * The client side of the render backend on the Wii U. There is no
 * window: the renderer draws to the TV through gx2gl, which mirrors it
 * to the GamePad, so this only reports the TV resolution. SDL is used
 * for input and sound only, its video subsystem stays off because it
 * would take over GX2.
 *
 * =======================================================================
 */

#include <gx2/display.h>

#include "../../common/header/common.h"
#include "header/ref.h"

float glimp_refreshRate = -1.0f;

static const char *displayindices[] = {"TV", NULL};

/*
 * The resolution WHBGfx renders the TV at for the console's TV setting.
 */
static void
GetTVSize(int *width, int *height)
{
	switch (GX2GetSystemTVScanMode())
	{
		case GX2_TV_SCAN_MODE_480I:
		case GX2_TV_SCAN_MODE_480P:
			*width = 854;
			*height = 480;
			break;

		case GX2_TV_SCAN_MODE_1080I:
		case GX2_TV_SCAN_MODE_1080P:
			*width = 1920;
			*height = 1080;
			break;

		case GX2_TV_SCAN_MODE_720P:
		default:
			*width = 1280;
			*height = 720;
			break;
	}
}

qboolean
GLimp_Init(void)
{
	int width, height;

	GetTVSize(&width, &height);

	Com_Printf("-------- vid initialization --------\n");
	Com_Printf("Wii U TV: %ix%i, mirrored to the GamePad\n", width, height);
	Com_Printf("------------------------------------\n\n");

	return true;
}

void
GLimp_Shutdown(void)
{
	GLimp_ShutdownGraphics();
}

static qboolean initialized = false;

qboolean
GLimp_InitGraphics(int fullscreen, int *pwidth, int *pheight)
{
	int width, height;

	(void)fullscreen;

	/* A restart (vid_restart, changed settings) tears the old
	   context down first, like closing the SDL window would. */
	if (initialized)
	{
		re.ShutdownContext();
		initialized = false;
	}

	if (re.PrepareForWindow() == -1)
	{
		return false;
	}

	/* The TV has one resolution: whatever r_mode asks for, use it.
	   The window argument only has to be non-NULL. */
	GetTVSize(&width, &height);
	*pwidth = width;
	*pheight = height;

	if (!re.InitContext((void *)1))
	{
		return false;
	}

	re.GetDrawableSize(&viddef.width, &viddef.height);
	Com_Printf("Drawable size: %ix%i\n", viddef.width, viddef.height);

	initialized = true;

	return true;
}

void
GLimp_ShutdownGraphics(void)
{
	if (initialized)
	{
		re.ShutdownContext();
		initialized = false;
	}
}

void
GLimp_GrabInput(qboolean grab)
{
	(void)grab;
}

float
GLimp_GetRefreshRate(void)
{
	if (glimp_refreshRate <= 0)
	{
		glimp_refreshRate = 60;
	}

	return glimp_refreshRate;
}

qboolean
GLimp_GetDesktopMode(int *pwidth, int *pheight)
{
	GetTVSize(pwidth, pheight);
	return true;
}

const char **
GLimp_GetDisplayIndices(void)
{
	return displayindices;
}

int
GLimp_GetNumVideoDisplays(void)
{
	return 1;
}

int
GLimp_GetWindowDisplayIndex(void)
{
	return 0;
}

int
GLimp_GetFrameworkVersion(void)
{
	/* Must match GL3_GetSDLVersion() in the renderer. */
	return 2;
}
