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
 * Hunk allocator for the Wii U. There is no virtual memory to reserve,
 * so a hunk is one heap block of its maximum size. The callers size
 * their hunks from the file they load, so the maximum is never far from
 * what ends up used.
 *
 * =======================================================================
 */

#include <malloc.h>
#include <stdlib.h>

#include "../../common/header/common.h"

static byte *membase;
static size_t maxhunksize;
static size_t curhunksize;

void *
Hunk_Begin(int maxsize)
{
	/* plus 32 bytes for cacheline */
	maxhunksize = maxsize + 32;
	curhunksize = 0;

	membase = memalign(32, maxhunksize);

	if (membase == NULL)
	{
		Sys_Error("unable to allocate %d bytes", maxsize);
	}

	memset(membase, 0, maxhunksize);

	return membase;
}

void *
Hunk_Alloc(int size)
{
	byte *buf;

	/* round to cacheline */
	size = (size + 31) & ~31;

	if (curhunksize + size > maxhunksize)
	{
		Sys_Error("%s: overflow %d > %d",
			__func__, (int)(curhunksize + size), (int)maxhunksize);
	}

	buf = membase + curhunksize;
	curhunksize += size;
	return buf;
}

int
Hunk_End(void)
{
	/* The block is not trimmed: realloc() is free to move it, and the
	   callers size their hunks from the file they load anyway. */
	return curhunksize;
}

void
Hunk_Free(void *base)
{
	free(base);
}
