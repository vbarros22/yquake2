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

#include <coreinit/time.h>
#include "header/local.h"

#include <gx2gl/present.h>
#include <gx2gl/stats.h>
#include <gx2gl/proc.h>
#include <whb/gfx.h>

static GX2GL_Context context = NULL;
void Sys_WiiU_LogRaw(const char *string); /* backends/wiiu */

static qboolean vsyncActive = false;
qboolean IsHighDPIaware = false;

void GL3_SwapWindow(void)
{
	/* Log the first GL errors: gx2gl reports what it can't do this way. */
	static int frame, reported, reported_slow;
	GLenum err;

	while ((err = glGetError()) != GL_NO_ERROR && reported < 40)
	{
		Com_Printf("GL error 0x%04x in frame %d\n", err, frame);
		reported++;
	}

	frame++;

	GX2GL_Present();

	/* Log what gx2gl did in slow frames (and in one frame every 10 s,
	   for comparison). */
	static OSTime last, lastBaseline;
	OSTime now = OSGetTime();
	int ms = last ? (int)OSTicksToMilliseconds(now - last) : 0;
	int baseline = OSTicksToMilliseconds(now - lastBaseline) >= 10000;
	last = now;

	if ((ms >= 40 || baseline) && reported_slow < 3000)
	{
		GX2GL_FrameStats st;
		char line[512];

		GX2GL_GetFrameStats(&st);
		snprintf(line, sizeof(line),
			"%s frame %d: %d ms | draw %u/%ums buf %u(%uKB)/%ums unif %u/%ums "
			"bind %u/%ums tex %u/%ums mip %u/%ums shd %u+%u/%ums prog %u/%ums "
			"clr %u/%ums | gpu wait %ums\n",
			baseline ? "sample" : "SLOW", frame, ms,
			st.draws, st.draw_us / 1000, st.buffer_uploads, st.buffer_bytes / 1024,
			st.buffer_us / 1000, st.uniform_calls, st.uniform_us / 1000,
			st.texture_binds, st.texture_bind_us / 1000,
			st.texture_uploads, st.texture_upload_us / 1000,
			st.mipmap_generations, st.mipmap_us / 1000,
			st.shader_compiles, st.program_links, st.shader_us / 1000,
			st.program_switches, st.program_switch_us / 1000,
			st.clears, st.clear_us / 1000, st.finish_us / 1000);
		Sys_WiiU_LogRaw(line);
		reported_slow++;

		if (baseline)
		{
			lastBaseline = now;
		}
	}
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
