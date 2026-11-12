/*
 * Diddy Kong Racing on the PocketGo S30: the ROM's assets in this CPU's byte
 * order.
 *
 * The game reads its assets straight from the ROM (big-endian) and then
 * through its own structs; each loader calls in here once the data is in RAM
 * (after any gzip_inflate), and the multi-byte fields of each struct are
 * swapped by what the game's struct says they are. Byte arrays stay as they
 * are, and so does texture and palette data: the renderer reads that in the
 * N64's byte order (src/s30/main.c, tex_elem_size).
 *
 * Unions that the game reads both as a word and as bytes are left alone here
 * and their readers fixed instead (TARGET_S30 in the game's code).
 */
#include "s30_swap.h"

#include "asset_enums.h"
#include "font.h"
#include "particles.h"
#include "objects.h"
#include "structs.h"

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

void s30_swap16(void *p, s32 count) {
    u16 *h = p;

    for (; count > 0; count--, h++) {
        *h = __builtin_bswap16(*h);
    }
}

void s30_swap32(void *p, s32 count) {
    u32 *w = p;

    for (; count > 0; count--, w++) {
        *w = __builtin_bswap32(*w);
    }
}

#define SWAP16(base, ofs) s30_swap16((u8 *) (base) + (ofs), 1)
#define SWAP32(base, ofs) s30_swap32((u8 *) (base) + (ofs), 1)

/* A struct's multi-byte fields: the offsets of its 16-bit fields and of its
   32-bit fields, each list ended by -1 */
typedef struct {
    s16 halves[40];
    s16 words[24];
} Layout;

static void swap_layout(void *base, const Layout *layout) {
    s32 i;

    for (i = 0; layout->halves[i] >= 0; i++) {
        SWAP16(base, layout->halves[i]);
    }
    for (i = 0; layout->words[i] >= 0; i++) {
        SWAP32(base, layout->words[i]);
    }
}

/* ---------------------------------------------------------------------------
 * Asset tables: arrays of offsets (s32) or of ids (s16), by section
 * ------------------------------------------------------------------------- */

void s30_swap_table(s32 section, void *data, s32 size) {
    switch (section) {
        case ASSET_HUD_ELEMENT_IDS:
        case ASSET_MENU_ELEMENT_IDS:
        case ASSET_ANIMATION_IDS:
        case ASSET_LEVEL_OBJECT_TRANSLATION_TABLE:
        case ASSET_DUMMY_PARTICLE_IDS:
            s30_swap16(data, size / 2);
            break;
        case ASSET_FONTS:
            s30_swap_fonts(data);
            break;
        case ASSET_PARTICLES:
            s30_swap_particles(data, size);
            break;
        case ASSET_PARTICLE_BEHAVIORS:
            s30_swap_particle_behaviours(data, size);
            break;
        case ASSET_TTGHOSTS_TABLE: {
            TTGhostTable *t = data;
            s32 n = size / (s32) sizeof(TTGhostTable);

            for (; n > 0; n--, t++) {
                SWAP32(t, 4); // ghostOffset; mapId, defaultVehicleId are bytes
            }
            break;
        }
        case ASSET_MISC:
            break; // typed as it is used (s30_misc_asset)
        case ASSET_JAPANESE_FONTS_TABLE:
            break; // the Japanese version only
        default:
            // Offset tables (*_TABLE, ASSET_WEATHER_PARTICLES' texture ids)
            s30_swap32(data, size / 4);
            break;
    }
}

/* ---------------------------------------------------------------------------
 * Textures and sprites
 * ------------------------------------------------------------------------- */

static const Layout kTextureHeader = {
    { 0x06, 0x08, 0x0A, 0x12, 0x14, 0x16, -1 },
    { 0x0C, -1 },
};

void s30_swap_texture_header(TextureHeader *tex) {
    swap_layout(tex, &kTextureHeader);
}

/* Every frame's header (each frame is textureSize bytes, header included) */
void s30_swap_texture(TextureHeader *tex) {
    s32 frames;

    s30_swap_texture_header(tex);
    frames = tex->numOfTextures >> 8;
    while (--frames > 0) {
        tex = (TextureHeader *) ((u8 *) tex + tex->textureSize);
        s30_swap_texture_header(tex);
    }
}

void s30_swap_sprite(SpriteAsset *sprite) {
    s30_swap16(sprite, 4); // baseTextureId, numberOfFrames, anchor x, y
    SWAP32(sprite, 0x08);  // frameTexOffsets are bytes
}

/* ---------------------------------------------------------------------------
 * Fonts: a count, then FontData each
 * ------------------------------------------------------------------------- */

void s30_swap_fonts(void *data) {
    s32 n, i;
    FontData *font;

    s30_swap32(data, 1);
    n = *(s32 *) data;
    font = (FontData *) ((s32 *) data + 1);
    for (i = 0; i < n; i++, font++) {
        SWAP32(font, 0x00);
        s30_swap16(&font->x, 4);
        s30_swap16(font->textureID, 32);
        s30_swap32(font->texturePointers, 32);
    }
}

/* ---------------------------------------------------------------------------
 * Particles
 * ------------------------------------------------------------------------- */

static const Layout kParticleDescriptor = {
    { 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0E, -1 },
    { 0x10, -1 },
};

void s30_swap_particles(void *data, s32 size) {
    u8 *p;

    for (p = data; p + sizeof(ParticleDescriptor) <= (u8 *) data + size; p += sizeof(ParticleDescriptor)) {
        swap_layout(p, &kParticleDescriptor);
    }
}

static const Layout kParticleBehaviour = {
    { 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x40, 0x42, 0x44, 0x46,
      0x48, 0x4A, 0x4C, 0x4E, 0x64, 0x66, 0x68, 0x6A, 0x6C, 0x6E, 0x80, 0x82, 0x84, 0x86, 0x88, 0x8A, -1 },
    { 0x00, 0x04, 0x08, 0x0C, 0x10, 0x30, 0x34, 0x38, 0x3C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x70, 0x74, 0x78, 0x7C,
      0x8C, 0x90, 0x94, 0x9C, -1 },
};

void s30_swap_particle_behaviours(void *data, s32 size) {
    u8 *p;

    for (p = data; p + sizeof(ParticleBehaviour) <= (u8 *) data + size; p += sizeof(ParticleBehaviour)) {
        swap_layout(p, &kParticleBehaviour);
    }
}

/* ---------------------------------------------------------------------------
 * Levels
 * ------------------------------------------------------------------------- */

/* 0x20 (AILevelTable) and 0x70 (wavesXlu...) hold bytes */
static const Layout kLevelHeader = {
    { 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E, 0x40, 0x42, 0x54, 0x5A, 0x5E, 0x60, 0x62, 0x64, 0x66, 0x68, 0x6E, 0x90,
      0x92, 0x96, 0x98, 0x9A, 0xA8, 0xAA, 0xB0, 0xBA, -1 },
    { 0x08, 0x74, 0x78, 0x7C, 0x80, 0x84, 0x88, 0x8C, 0xA4, 0xAC, -1 },
};

void s30_swap_level_header(LevelHeader *header) {
    swap_layout(header, &kLevelHeader);
}

static const Layout kLevelModel = {
    { 0x18, 0x1A, 0x1C, 0x1E, 0x24, 0x26, 0x30, 0x32, 0x34, 0x36, 0x3C, 0x3E, 0x40, 0x42, 0x44, 0x46, -1 },
    { 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x20, 0x28, 0x2C, 0x38, 0x48, -1 },
};

static const Layout kLevelModelSegment = {
    { 0x1C, 0x1E, 0x20, 0x28, 0x30, 0x32, 0x38, -1 },
    { 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x2C, 0x34, 0x3C, -1 },
};

static void swap_vertices(Vertex *v, s32 n) {
    for (; n > 0; n--, v++) {
        s30_swap16(v, 3); // x, y, z; the colour is bytes
    }
}

static void swap_triangles(Triangle *t, s32 n) {
    for (; n > 0; n--, t++) {
        s30_swap16(&t->uv0, 6); // flags and vertex indices are bytes
    }
}

static void swap_batches(TriangleBatchInfo *b, s32 n) {
    for (; n > 0; n--, b++) {
        SWAP16(b, 0x02);
        SWAP16(b, 0x04);
        SWAP32(b, 0x08);
    }
}

static void swap_textures(TextureInfo *t, s32 n) {
    for (; n > 0; n--, t++) {
        SWAP32(t, 0x00); // the texture id; width, height, format, surface are bytes
    }
}

/* The model as gzip_inflate left it, its pointers still offsets */
void s30_swap_level_model(LevelModel *model) {
    u8 *base = (u8 *) model;
    s32 i;

    swap_layout(model, &kLevelModel);
    swap_textures((TextureInfo *) (base + (s32) model->textures), model->numberOfTextures);
    for (i = 0; i < model->numberOfSegments; i++) {
        LevelModelSegment *seg = (LevelModelSegment *) (base + (s32) model->segments) + i;

        swap_layout(seg, &kLevelModelSegment);
        swap_vertices((Vertex *) (base + (s32) seg->vertices), seg->numberOfVertices);
        swap_triangles((Triangle *) (base + (s32) seg->triangles), seg->numberOfTriangles);
        swap_batches((TriangleBatchInfo *) (base + (s32) seg->batches), seg->numberOfBatches + 1);
        if ((s32) seg->collisionFacets != 0) {
            s30_swap16(base + (s32) seg->collisionFacets, seg->numberOfTriangles * 4);
        }
    }
    s30_swap16(base + (s32) model->segmentsBoundingBoxes, model->numberOfSegments * 6);
    // The BSP tree: a node per split between segments, its leaves the
    // segments (left, right, split value)
    {
        BspTreeNode *node = (BspTreeNode *) (base + (s32) model->segmentsBspTree);

        for (i = 0; i < model->numberOfSegments - 1; i++, node++) {
            SWAP16(node, 0x00);
            SWAP16(node, 0x02);
            SWAP16(node, 0x06);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Object models and animations
 * ------------------------------------------------------------------------- */

static const Layout kObjectModel = {
    { 0x18, 0x1A, 0x20, 0x22, 0x24, 0x26, 0x28, 0x30, 0x32, 0x48, 0x4A, 0x50, 0x52, -1 },
    { 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x1C, 0x2C, 0x38, 0x3C, 0x40, 0x44, 0x4C, -1 },
};

/* The model as gzip_inflate left it, its pointers still offsets */
void s30_swap_object_model(ObjectModel *model) {
    u8 *base = (u8 *) model;

    swap_layout(model, &kObjectModel);
    swap_textures((TextureInfo *) (base + (s32) model->textures), model->numberOfTextures);
    swap_vertices((Vertex *) (base + (s32) model->vertices), model->numberOfVertices);
    swap_triangles((Triangle *) (base + (s32) model->triangles), model->numberOfTriangles);
    swap_batches((TriangleBatchInfo *) (base + (s32) model->batches), model->numberOfBatches + 1);
    if (model->numberOfAttachPoints > 0) {
        s30_swap16(base + (s32) model->attachPoints, model->numberOfAttachPoints); // vertex indices
    }
    if (model->collisionSpheresSize > 0) {
        s30_swap16(base + (s32) model->collisionSpheres, model->collisionSpheresSize * 2);
    }
    if ((s32) model->animatedVertexIndices != 0) {
        // One per vertex: its index among the animated ones, or -1
        s30_swap16(base + (s32) model->animatedVertexIndices, model->numberOfVertices);
    }
}

/* ---------------------------------------------------------------------------
 * Object headers and object maps
 * ------------------------------------------------------------------------- */

static const Layout kObjectHeader = {
    { 0x30, 0x32, 0x34, 0x36, 0x38, 0x3E, 0x40, 0x42, 0x44, 0x46, 0x48, 0x4A, 0x4C, 0x4E, 0x50, -1 },
    { 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, -1 },
};

/* The header as loaded, its pointers still offsets */
void s30_swap_object_header(ObjectHeader *header) {
    u8 *base = (u8 *) header;
    s32 i;

    swap_layout(header, &kObjectHeader);
    s30_swap32(base + (s32) header->modelIds, header->numberOfModelIds);
    s30_swap32(base + (s32) header->objectParticles, header->particleCount * 2);
    // Vehicle parts: s32 ids, s8 indices (attach points)
    s30_swap32(base + (s32) header->vehiclePartIds, header->attachPointCount);
    // Lights: u16 misc asset id, then (past the u32 that is read as bytes)
    // the home position, radius and two u16s
    for (i = 0; i < header->numLightSources; i++) {
        u8 *light = base + (s32) header->unk24 + i * sizeof(ObjectHeader24);

        SWAP16(light, 0x06);
        s30_swap16(light + 0x0C, 6);
    }
}

extern const unsigned char gS30EntryWideFields[128][2][32];

/* Each entry's fields by its object's behaviour, through the level-object
   translation table and the object's header */
void s30_swap_object_map(u8 *entries, s32 size, s16 *translationTable, s32 *headersTable) {
    u8 *p = entries, *end = entries + size;
    ObjectHeader header;

    while (p < end) {
        s32 entrySize = p[1] & 0x7F;
        s32 objectID = p[0] | ((p[1] & 0x80) << 1);
        s32 behavior, i;

        if (entrySize < 8) {
            break;
        }
        // The object header's behaviorId (offset 0x54, a byte)
        asset_load(ASSET_OBJECTS, (u32) &header, headersTable[translationTable[objectID]], sizeof(header));
        behavior = header.behaviorId & 0x7F;
        for (i = 0; gS30EntryWideFields[behavior][0][i] != 0; i++) {
            if (gS30EntryWideFields[behavior][0][i] + 2 <= entrySize) {
                SWAP16(p, gS30EntryWideFields[behavior][0][i]);
            }
        }
        for (i = 0; gS30EntryWideFields[behavior][1][i] != 0; i++) {
            if (gS30EntryWideFields[behavior][1][i] + 4 <= entrySize) {
                SWAP32(p, gS30EntryWideFields[behavior][1][i]);
            }
        }
        p += entrySize;
    }
}

/* ---------------------------------------------------------------------------
 * Time trial ghosts
 * ------------------------------------------------------------------------- */

/* The header's time and node count (the rest of it is read as bytes), then
   the nodes (s16s) */
void s30_swap_ghost(GhostHeader *ghost, s32 size) {
    SWAP16(ghost, 4);
    SWAP16(ghost, 6);
    s30_swap16((u8 *) ghost + sizeof(GhostHeader), (size - (s32) sizeof(GhostHeader)) / 2);
}

/* ---------------------------------------------------------------------------
 * The misc section: assets of many kinds, swapped as they are first used
 * ------------------------------------------------------------------------- */

#define MISC_ASSETS 128
static u8 sMiscSwapped[MISC_ASSETS];

extern s32 *gAssetsMiscSection;
extern s32 *gAssetsMiscTable;
extern s32 gAssetsMiscTableLength;

/* A misc asset's size in bytes (up to the next one's start) */
static s32 misc_size(s32 index) {
    if (index + 1 >= gAssetsMiscTableLength) {
        return 0;
    }
    return (gAssetsMiscTable[index + 1] - gAssetsMiscTable[index]) * 4;
}

void *s30_misc_asset(s32 index, S30MiscType type) {
    u8 *asset = (u8 *) &gAssetsMiscSection[gAssetsMiscTable[index]];
    s32 size, i;

    if (index < 0 || index >= MISC_ASSETS || sMiscSwapped[index] || type == S30_MISC_BYTES) {
        return asset;
    }
    sMiscSwapped[index] = 1;
    size = misc_size(index);
    switch (type) {
        case S30_MISC_WORDS:
            s30_swap32(asset, size / 4);
            break;
        case S30_MISC_HALVES:
            s30_swap16(asset, size / 2);
            break;
        case S30_MISC_COLOUR_LOOP:
            // ColorLoopEntry each: an s32 (the count, in the first), then a colour
            for (i = 0; i + 8 <= size; i += 8) {
                SWAP32(asset, i);
            }
            break;
        case S30_MISC_LIGHT_ANIM:
            // LevelHeader_70 (MiscAssetObjectHeader24 is the same data): s32s
            // at 0-0xC, two colours, then {s32, colour} for each of unk0
            s30_swap32(asset, 4);
            for (i = 0; i < *(s32 *) asset && 0x18 + i * 8 + 4 <= size; i++) {
                SWAP32(asset, 0x18 + i * 8);
            }
            break;
        case S30_MISC_PULSE:
            // PulsatingLightData: u16 x4, an s32, then frames of u16 x2
            s30_swap16(asset, 4);
            SWAP32(asset, 8);
            s30_swap16(asset + 12, (size - 12) / 2);
            break;
        case S30_MISC_BOOST:
            // Object_Boost: 27 floats, s16 x2, bytes, then a float and two words
            s30_swap32(asset, 27);
            s30_swap16(asset + 0x6C, 2);
            s30_swap32(asset + 0x74, 3);
            break;
        case S30_MISC_SHIELD:
            // RacerShieldGfx each: s16 x4, f32 x2
            for (i = 0; i + 16 <= size; i += 16) {
                s30_swap16(asset + i, 4);
                s30_swap32(asset + i + 8, 2);
            }
            break;
        default:
            break;
    }
    return asset;
}
