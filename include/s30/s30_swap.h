#ifndef S30_SWAP_H
#define S30_SWAP_H

/* The ROM's assets in this CPU's byte order (src/s30/game/swap.c): each
   loader calls in here once the asset is in RAM, under TARGET_S30. */

#include "types.h"
#include "structs.h"

void s30_swap16(void *p, s32 count);
void s30_swap32(void *p, s32 count);

/* asset_table_load: a whole section (offset and id tables, fonts, particles) */
void s30_swap_table(s32 section, void *data, s32 size);

void s30_swap_texture_header(TextureHeader *tex);
void s30_swap_texture(TextureHeader *tex);
void s30_swap_sprite(SpriteAsset *sprite);
void s30_swap_fonts(void *data);
void s30_swap_particles(void *data, s32 size);
void s30_swap_particle_behaviours(void *data, s32 size);
void s30_swap_level_header(LevelHeader *header);
void s30_swap_level_model(LevelModel *model);
void s30_swap_object_model(ObjectModel *model);
void s30_swap_object_header(ObjectHeader *header);
void s30_swap_object_map(u8 *entries, s32 size, s16 *translationTable, s32 *headersTable);
void s30_swap_ghost(GhostHeader *ghost, s32 size);

/* The misc section's assets, by how the game reads them (swapped once, the
   first time) */
typedef enum {
    S30_MISC_BYTES,
    S30_MISC_WORDS,       // s32 or f32
    S30_MISC_HALVES,      // s16 or u16
    S30_MISC_COLOUR_LOOP, // ColorLoopEntry
    S30_MISC_LIGHT_ANIM,  // LevelHeader_70, MiscAssetObjectHeader24
    S30_MISC_PULSE,       // PulsatingLightData
    S30_MISC_BOOST,       // Object_Boost
    S30_MISC_SHIELD,      // RacerShieldGfx
} S30MiscType;

void *s30_misc_asset(s32 index, S30MiscType type);

#endif
