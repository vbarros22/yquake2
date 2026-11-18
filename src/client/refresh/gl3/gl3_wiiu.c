/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016 Daniel Gibson
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
 * The Wii U replacement for gl3_sdl.c: the OpenGL 3.3 context comes from
 * gx2gl (https://github.com/ExpansionPak/gx2gl), which draws with GX2 and
 * mirrors the TV image to the GamePad.
 *
 * =======================================================================
 */

#include "header/local.h"

#include <gx2gl/present.h>
#include <gx2gl/proc.h>
#include <whb/gfx.h>

static GX2GL_Context context = NULL;
static qboolean vsyncActive = false;
qboolean IsHighDPIaware = false;

void GL3_SwapWindow(void)
{
	GX2GL_Present();
}

qboolean GL3_IsVsyncActive(void)
{
	return vsyncActive;
}

void GL3_SetVsync(void)
{
	/* GX2 has no adaptive vsync: r_vsync 2 means plain vsync. */
	int interval = r_vsync->value ? 1 : 0;

	GX2GL_SetSwapInterval(interval);
	vsyncActive = GX2GL_GetSwapInterval() != 0;
}

int GL3_PrepareForWindow(void)
{
	/* gx2gl's default framebuffer has no stencil buffer, which only
	   costs the stencil shadows (gl_shadows). No MSAA either. */
	gl3config.stencil = false;

	if (gl_msaa_samples->value)
	{
		Com_Printf("MSAA is not supported on the Wii U.\n");
		ri.Cvar_SetValue("r_msaa_samples", 0);
	}

	return 0;
}

static void *
GetProcAddress(const char *name)
{
	return GX2GL_GetProcAddress(name);
}

int GL3_InitContext(void* win)
{
	(void)win;

	context = GX2GL_CreateContext();

	if (context == NULL)
	{
		Com_Printf("GL3_InitContext(): GX2GL_CreateContext() failed!\n");
		return false;
	}

	GX2GL_MakeCurrent(context);

	if (!gladLoadGLLoader(GetProcAddress))
	{
		Com_Printf("GL3_InitContext(): ERROR: loading OpenGL function pointers failed!\n");
		return false;
	}
	else if (GLVersion.major < 3 || (GLVersion.major == 3 && GLVersion.minor < 2))
	{
		Com_Printf("GL3_InitContext(): ERROR: glad only got GL version %d.%d!\n",
			GLVersion.major, GLVersion.minor);
		return false;
	}

	Com_Printf("Successfully loaded OpenGL function pointers using glad, got version %d.%d!\n",
		GLVersion.major, GLVersion.minor);

	gl3config.debug_output = false;
	gl3config.anisotropic = GLAD_GL_EXT_texture_filter_anisotropic != 0;
	gl3config.major_version = GLVersion.major;
	gl3config.minor_version = GLVersion.minor;

	GL3_SetVsync();

	return true;
}

void GL3_GetDrawableSize(int* width, int* height)
{
	GX2ColorBuffer *tv = WHBGfxGetTVColourBuffer();

	if (tv && tv->surface.width > 0)
	{
		*width = (int)tv->surface.width;
		*height = (int)tv->surface.height;
	}
}

void GL3_ShutdownContext()
{
	if (context)
	{
		GX2GL_DeleteContext(context);
		context = NULL;
	}
}

int GL3_GetSDLVersion()
{
	/* The client checks this against GLimp_GetFrameworkVersion(). */
	return 2;
}
