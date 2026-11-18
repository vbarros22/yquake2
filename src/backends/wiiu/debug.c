/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * =======================================================================
 *
 * Wii U debugging aids, written to yquake2.log:
 *
 *  - a crash handler that logs where a DSI/ISI/program exception happened,
 *    with a backtrace, and shows it on screen;
 *  - a watchdog that logs where the main thread is when it hasn't made
 *    progress for a while, so hangs can be located too.
 *
 * Addresses come with the nearest symbol the system knows and their
 * offset from main(): add that to the address of main in yquake2.elf
 * (powerpc-eabi-addr2line -e yquake2.elf ...).
 *
 * =======================================================================
 */

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <coreinit/context.h>
#include <coreinit/debug.h>
#include <coreinit/exception.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include "wiiu.h"

int main(int argc, char **argv);

static OSThread *main_thread;
static volatile OSTime last_heartbeat;
static volatile int watchdog_paused;
static char crash_text[1536];

/*
 * "address name+offset". OSGetSymbolName() knows the system libraries;
 * for our own code it gives the nearest exported symbol, so the offset
 * from main() is printed too.
 */
static int
FormatAddress(char *out, size_t size, uint32_t addr)
{
	char name[64];

	name[0] = '\0';
	OSGetSymbolName(addr, name, sizeof(name));
	name[40] = '\0';

	return snprintf(out, size, "%08x %s (main%+d)\n", addr, name,
		(int)(addr - (uint32_t)main));
}

static void
FormatContext(char *out, size_t size, const char *what, OSContext *ctx)
{
	int len = snprintf(out, size, "%s\ndar %08x  sp %08x  main %08x\npc ",
		what, ctx->dar, ctx->gpr[1], (uint32_t)main);
	uint32_t sp = ctx->gpr[1];

	len += FormatAddress(out + len, size - len, ctx->srr0);
	if (len >= (int)size) return;
	len += snprintf(out + len, size - len, "lr ");
	if (len >= (int)size) return;
	len += FormatAddress(out + len, size - len, ctx->lr);

	/* PowerPC EABI frames: [sp] = back chain, [next + 4] = saved LR. */
	for (int i = 0; i < 10 && len < (int)size; i++)
	{
		if (sp < 0x10000000 || sp >= 0x50000000 || (sp & 3) != 0)
		{
			break;
		}

		uint32_t next = ((uint32_t *)sp)[0];

		if (next <= sp || next >= 0x50000000)
		{
			break;
		}

		len += snprintf(out + len, size - len, "  ");
		if (len >= (int)size) return;
		len += FormatAddress(out + len, size - len, ((uint32_t *)next)[1]);
		sp = next;
	}
}

static OSContext crash_context;
static const char *crash_what;

/*
 * Runs on the crashed thread, outside of the exception handler, where
 * writing files is safe.
 */
static void
CrashReport(void)
{
	static int reporting;

	/* A crash while reporting one: just stop. */
	if (reporting++)
	{
		while (1)
		{
			OSSleepTicks(OSSecondsToTicks(1));
		}
	}

	FormatContext(crash_text, sizeof(crash_text), crash_what, &crash_context);
	Sys_WiiU_LogRaw("\n*** CRASH ***\n");
	Sys_WiiU_LogRaw(crash_text);
	Sys_WiiU_LogRaw("\n");
	Sys_WiiU_SyncLog();
	OSFatal(crash_text);
}

static BOOL
CrashHandler(OSContext *ctx, const char *what)
{
	memcpy(&crash_context, ctx, sizeof(crash_context));
	crash_what = what;

	/* Resume the thread in CrashReport(), on a fresh frame below the
	   current one. Its back chain is zero, so OSFatal()'s backtrace
	   stops there instead of following garbage. */
	ctx->srr0 = (uint32_t)CrashReport;
	ctx->gpr[1] = (ctx->gpr[1] - 512) & ~15u;
	((uint32_t *)ctx->gpr[1])[0] = 0;
	((uint32_t *)ctx->gpr[1])[1] = 0;
	ctx->lr = 0;
	return TRUE;
}

static BOOL DSIHandler(OSContext *ctx) { return CrashHandler(ctx, "Yamagi Quake II crashed: DSI (bad memory access)"); }
static BOOL ISIHandler(OSContext *ctx) { return CrashHandler(ctx, "Yamagi Quake II crashed: ISI (bad jump)"); }
static BOOL ProgramHandler(OSContext *ctx) { return CrashHandler(ctx, "Yamagi Quake II crashed: program exception"); }

void
Sys_WiiU_Heartbeat(void)
{
	last_heartbeat = OSGetSystemTime();
}

void
Sys_WiiU_PauseWatchdog(int paused)
{
	watchdog_paused = paused;
	Sys_WiiU_Heartbeat();
}

static int
WatchdogThread(int argc, void *argv)
{
	int reported = 0;

	(void)argc;
	(void)argv;

	while (1)
	{
		OSSleepTicks(OSSecondsToTicks(1));

		/* Get the log onto the SD card, a crash or power off would
		   lose what's still cached. */
		Sys_WiiU_SyncLog();

		if (watchdog_paused)
		{
			continue;
		}

		if (OSTicksToSeconds(OSGetSystemTime() - last_heartbeat) < 8)
		{
			reported = 0;
			continue;
		}

		/* Report a stall once, then again if it lasts another 30s. */
		if (reported && reported++ < 30)
		{
			continue;
		}

		reported = 1;

		OSSuspendThread(main_thread);

		while (!OSIsThreadSuspended(main_thread))
		{
			OSSleepTicks(OSMillisecondsToTicks(1));
		}

		char text[1024];
		FormatContext(text, sizeof(text),
			"*** WATCHDOG: no progress for 8 seconds, main thread is at:",
			&main_thread->context);
		OSResumeThread(main_thread);

		Sys_WiiU_LogRaw("\n");
		Sys_WiiU_LogRaw(text);
		Sys_WiiU_LogRaw("\n");
		Sys_WiiU_SyncLog();
	}

	return 0;
}

void
Sys_WiiU_InitDebug(void)
{
	static OSThread watchdog;
	const uint32_t stack_size = 16 * 1024;
	uint8_t *stack = memalign(16, stack_size);
	char text[96];

	main_thread = OSGetCurrentThread();
	Sys_WiiU_Heartbeat();

	snprintf(text, sizeof(text), "main() is at %08x\n", (uint32_t)main);
	Sys_WiiU_LogRaw(text);

	OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES,
		OS_EXCEPTION_TYPE_DSI, DSIHandler);
	OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES,
		OS_EXCEPTION_TYPE_ISI, ISIHandler);
	OSSetExceptionCallbackEx(OS_EXCEPTION_MODE_GLOBAL_ALL_CORES,
		OS_EXCEPTION_TYPE_PROGRAM, ProgramHandler);

	if (stack && OSCreateThread(&watchdog, WatchdogThread, 0, NULL,
			stack + stack_size, stack_size, 16,
			OS_THREAD_ATTRIB_AFFINITY_CPU0 | OS_THREAD_ATTRIB_DETACHED))
	{
		OSSetThreadName(&watchdog, "yq2 watchdog");
		OSResumeThread(&watchdog);
	}
}
