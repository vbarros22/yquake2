/*
 * Wii U backend helpers shared by main.c, system.c and the engine.
 */

#ifndef WIIU_BACKEND_H
#define WIIU_BACKEND_H

/* Where the game lives on the SD card. baseq2/ goes inside. */
#define WIIU_DATADIR "fs:/vol/external01/wiiu/apps/yquake2"

void Sys_WiiU_OpenLog(const char *path);

/* Called once per main loop iteration: handles the HOME menu. */
void Sys_WiiU_Frame(void);

/* Writes to yquake2.log (and the WHB log) without going through Com_Printf. */
void Sys_WiiU_LogRaw(const char *string);

/* Crash handler and hang watchdog, see debug.c. */
void Sys_WiiU_InitDebug(void);
void Sys_WiiU_Heartbeat(void);
void Sys_WiiU_PauseWatchdog(int paused);

#endif
