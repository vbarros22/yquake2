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
 * Addresses are logged relative to main(): add that offset to the address
 * of main in yquake2.elf (powerpc-eabi-addr2line -e yquake2.elf ...).
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
static char crash_text[1024];

static uint32_t
Rel(uint32_t addr)
{
	return addr - (uint32_t)main;
}

static int
FormatBacktrace(char *out, size_t size, uint32_t sp)
{
	int len = 0;

	/* PowerPC EABI frames: [sp] = back chain, [sp + 4] = saved LR. */
	for (int i = 0; i < 12 && sp >= 0x10000000 && sp < 0x50000000 && (sp & 3) == 0; i++)
	{
		uint32_t *frame = (uint32_t *)sp;
		uint32_t next = frame[0];

		if (next <= sp)
		{
			break;
		}

		len += snprintf(out + len, size - len, " %08x", Rel(((uint32_t *)next)[1]));

		if (len >= (int)size)
		{
			break;
		}

		sp = next;
	}

	return len;
}

static void
FormatContext(char *out, size_t size, const char *what, OSContext *ctx)
{
	int len = snprintf(out, size,
		"%s\npc %08x  lr %08x  (relative to main)\ndar %08x  sp %08x\nbacktrace:",
		what, Rel(ctx->srr0), Rel(ctx->lr), ctx->dar, ctx->gpr[1]);

	if (len < (int)size)
	{
		FormatBacktrace(out + len, size - len, ctx->gpr[1]);
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
	FormatContext(crash_text, sizeof(crash_text), crash_what, &crash_context);
	Sys_WiiU_LogRaw("\n*** CRASH ***\n");
	Sys_WiiU_LogRaw(crash_text);
	Sys_WiiU_LogRaw("\n");
	OSFatal(crash_text);
}

static BOOL
CrashHandler(OSContext *ctx, const char *what)
{
	memcpy(&crash_context, ctx, sizeof(crash_context));
	crash_what = what;

	/* Resume the thread in CrashReport(), below its current frame. */
	ctx->srr0 = (uint32_t)CrashReport;
	ctx->gpr[1] -= 256;
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

		char text[512];
		FormatContext(text, sizeof(text),
			"*** WATCHDOG: no progress for 8 seconds, main thread is at:",
			&main_thread->context);
		OSResumeThread(main_thread);

		Sys_WiiU_LogRaw("\n");
		Sys_WiiU_LogRaw(text);
		Sys_WiiU_LogRaw("\n");
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
