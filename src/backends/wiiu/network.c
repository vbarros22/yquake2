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
 * Wii U network backend. For now this only implements the loopback
 * "network" between the local client and server, which is all single
 * player needs. Packets to real addresses are dropped.
 *
 * =======================================================================
 */

#include "../../common/header/common.h"

netadr_t net_local_adr;

#define MAX_LOOPBACK 4

typedef struct
{
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get, send;
} loopback_t;

static loopback_t loopbacks[2];

void
NET_Init(void)
{
	Com_Printf("Network: loopback only on the Wii U.\n");
}

void
NET_Shutdown(void)
{
}

void
NET_Config(qboolean multiplayer)
{
	(void)multiplayer;
}

qboolean
NET_CompareAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return false;
	}

	if (a.type == NA_LOOPBACK)
	{
		return true;
	}

	if (a.type == NA_IP)
	{
		return (memcmp(a.ip, b.ip, 4) == 0) && (a.port == b.port);
	}

	if (a.type == NA_IP6)
	{
		return (memcmp(a.ip, b.ip, 16) == 0) && (a.port == b.port);
	}

	return false;
}

qboolean
NET_CompareBaseAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return false;
	}

	if (a.type == NA_LOOPBACK)
	{
		return true;
	}

	if (a.type == NA_IP)
	{
		return memcmp(a.ip, b.ip, 4) == 0;
	}

	if (a.type == NA_IP6)
	{
		return memcmp(a.ip, b.ip, 16) == 0;
	}

	return false;
}

char *
NET_AdrToString(netadr_t a)
{
	static char s[64];

	if (a.type == NA_LOOPBACK)
	{
		Com_sprintf(s, sizeof(s), "loopback");
	}
	else
	{
		Com_sprintf(s, sizeof(s), "[%i.%i.%i.%i]:%d", a.ip[0], a.ip[1],
				a.ip[2], a.ip[3], (int)BigShort(a.port));
	}

	return s;
}

qboolean
NET_StringToAdr(const char *s, netadr_t *a)
{
	memset(a, 0, sizeof(*a));

	if (!strcmp(s, "localhost") || !strcmp(s, "loopback"))
	{
		a->type = NA_LOOPBACK;
		return true;
	}

	return false;
}

qboolean
NET_IsLocalAddress(netadr_t adr)
{
	return NET_CompareAdr(adr, net_local_adr);
}

qboolean
NET_GetPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int i;
	loopback_t *loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
	{
		loop->get = loop->send - MAX_LOOPBACK;
	}

	if (loop->get >= loop->send)
	{
		return false;
	}

	i = loop->get & (MAX_LOOPBACK - 1);
	loop->get++;

	memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	*net_from = net_local_adr;
	return true;
}

void
NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	int i;
	loopback_t *loop;

	if (to.type != NA_LOOPBACK)
	{
		return;
	}

	loop = &loopbacks[sock ^ 1];

	i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

void
NET_Sleep(int msec)
{
	(void)msec;
}
