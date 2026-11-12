/*
 * Diddy Kong Racing on the PocketGo S30: what of the N64 is the game's own.
 * libultra's OS is the shared library's (n64/src/os.c, s30_os.h).
 *
 * - The main memory pool (gS30Ram; gMainMemoryPool is linked to its start).
 * - The libultra data and the microcode symbols the game references.
 */
#include <stdint.h>
#include <stdio.h>

#include "s30_platform.h"

/* On the N64, from the end of the game's bss to the end of RAM */
unsigned char gS30Ram[S30_RAM_SIZE] __attribute__((aligned(16)));

/* libultra's VI modes (the VI is the display's here) */
uint32_t osViModeTable[56 * 20];
uint32_t osViModeNtscLan1[20], osViModePalLan1[20], osViModeMpalLan1[20];
uint32_t osViModeNtscLpn1[20], osViModePalLpn1[20], osViModeMpalLpn1[20];

/* The microcodes the game names in its tasks: the RCP is n64/src/gfx.c and
   n64/src/mixer.c */
uint64_t rspbootTextStart[1], rspbootTextEnd[1];
uint64_t rspF3DDKRXbusStart[1], rspF3DDKRXbusEnd[1], rspF3DDKRDataXbusStart[1], rspF3DDKRDataXbusEnd[1];
uint64_t rspF3DDKRDramStart[1], rspF3DDKRDramEnd[1];
uint64_t rspF3DDKRFifoStart[1], rspF3DDKRFifoEnd[1], rspF3DDKRDataFifoStart[1], rspF3DDKRDataFifoEnd[1];
uint64_t aspMainTextStart[1], aspMainTextEnd[1], aspMainDataStart[1], aspMainDataEnd[1];

/* Controller Pak repair: no pak here (n64/src/os.c: osPfsIsPlug finds none) */
int32_t osPfsReFormat(void* pfs, void* mq, int32_t channel) {
    (void)pfs;
    (void)mq;
    (void)channel;
    return 1; // PFS_ERR_NOPACK
}

/* libultra's audio asserts (debug builds only call it) */
void __assert(const char* exp, const char* file, int line) {
    fprintf(stderr, "dkr: assert %s at %s:%d\n", exp, file, line);
}
