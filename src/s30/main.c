/*
 * Diddy Kong Racing on the PocketGo S30: process entry.
 *
 * Environment:
 *   DKR_ROM       the US 1.0 ROM (.z64); the game reads its assets from it
 *                 (default: data/dkr.z64 next to the binary)
 *   DKR_SAVE      EEPROM save (default: $XDG_DATA_HOME/dkr/dkr.eep, i.e.
 *                 ~/.local/share/dkr/dkr.eep; else dkr.eep next to the binary)
 *   DKR_PAK       Controller Pak image, the time trial ghosts (default: dkr.pak
 *                 beside the save)
 *
 *   DKR_STATS     RCP tasks and the thread on the CPU every 2 s (n64/src/os.c)
 *   DKR_MENUDBG   each menu as it starts (src/menu.c menu_init), to follow a
 *                 scripted host run (tools/s30/host_stubs.c DKR_HOSTPRESS)
 *
 * Crash reports also go to crash.log next to the binary (on the SD card;
 * stderr is the launcher's log in /tmp, lost on reboot). kill -QUIT prints
 * where the game's running thread is.
 */
#define _GNU_SOURCE
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/input.h>

#include "s30_app.h"
#include "s30_audio.h"
#include "s30_display.h"
#include "s30_gfx.h"
#include "s30_input.h"
#include "s30_os.h"
#include "s30_platform.h"

extern void mainproc(void);

/* The RSP's segment table (G_MW_SEGMENT) */
static uint32_t sSegments[16];

/* Texture memory as the RDP reads it: the main pool holds the assets as the
   ROM has them (textures and palettes in the N64's byte order); the game's
   own arrays are u16 (framebuffers, the palettes it builds) */
static int tex_elem_size(uintptr_t addr) {
    return addr >= (uintptr_t)gS30Ram && addr < (uintptr_t)gS30Ram + S30_RAM_SIZE ? 1 : 2;
}

/* The S30's Nintendo face buttons: A -> A (accelerate), B -> B (brake),
   L1 -> Z (item), R1 -> R (hop and drift), L2 -> L, R2 -> Z, X -> C-up,
   Y -> C-left, Select -> C-right, Start -> Start, D-pad -> N64 D-pad */
static const S30ButtonMap kButtons[] = {
    { BTN_EAST, S30_N64_A },         { BTN_SOUTH, S30_N64_B },          { BTN_TL, S30_N64_Z },
    { BTN_TR, S30_N64_R },           { BTN_TL2, S30_N64_L },            { BTN_TR2, S30_N64_Z },
    { BTN_NORTH, S30_N64_CUP },      { BTN_WEST, S30_N64_CLEFT },       { BTN_SELECT, S30_N64_CRIGHT },
    { BTN_START, S30_N64_START },    { BTN_DPAD_UP, S30_N64_DUP },      { BTN_DPAD_DOWN, S30_N64_DDOWN },
    { BTN_DPAD_LEFT, S30_N64_DLEFT }, { BTN_DPAD_RIGHT, S30_N64_DRIGHT },
};

int main(int argc, char** argv) {
    char* exe = strdup(argv[0]);
    const char* exeDir = dirname(exe);

    (void)argc;
    setvbuf(stdout, NULL, _IOLBF, 0); // logs survive a crash
    S30App_InstallCrashHandler(S30App_PathIn(exeDir, "crash.log"));
    {
        // The game's own threads, the RCP on this one, the VI's retraces
        S30OsConfig os = {
            .romPath = getenv("DKR_ROM") ? getenv("DKR_ROM") : S30App_PathIn(S30App_PathIn(exeDir, "data"), "dkr.z64"),
            .savePath = getenv("DKR_SAVE") ? getenv("DKR_SAVE") : S30App_SavePath("dkr", "dkr.eep", exeDir),
            .saveType = S30_SAVE_EEPROM_16K,
            .pakPath = getenv("DKR_PAK") ? getenv("DKR_PAK") : S30App_SavePath("dkr", "dkr.pak", exeDir),
            .threads = 1,
            .vi = 1,
            // The audio manager sizes each frame from the AI length
            .aiLength = 1,
        };

        printf("dkr: ROM %s, save %s\n", os.romPath, os.savePath);
        S30Os_Init(&os);
    }
    {
        // Diddy Kong Racing: Rare's F3DDKR, fixed-point Mtx, texture data
        // as tex_elem_size says
        S30GfxConfig gfx = { S30_UCODE_F3DDKR, sSegments, 0, tex_elem_size, "DKR" };
        S30Gfx_Configure(&gfx);
    }
    S30Display_Init();
    S30Gfx_Init();
    S30Input_Init(kButtons, sizeof(kButtons) / sizeof(kButtons[0]));
    // The game's audio thread passes a frame per retrace to osAiSetNextBuffer,
    // at 22050 Hz (src/audiomgr.h OUTPUT_RATE)
    S30Audio_Configure(22050, 60.0f);
    S30Audio_Open();
    mainproc(); // starts the game's threads
    S30Os_RunRcp();
    return 0;
}
