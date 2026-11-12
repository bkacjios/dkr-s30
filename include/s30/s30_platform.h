#ifndef S30_PLATFORM_H
#define S30_PLATFORM_H

/* What the platform layer (src/s30) and the game share. Plain C types: the
   platform layer doesn't include the game's headers. */

/* The game's main memory pool (src/memory.h: S30_RAM_SIZE) */
#define S30_RAM_SIZE 0x400000
extern unsigned char gS30Ram[S30_RAM_SIZE];

#endif
