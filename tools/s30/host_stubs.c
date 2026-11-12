/*
 * The headless host build (make -f Makefile.s30 host): the game, libultra's
 * OS (n64/src/os.c) and the audio microcode (n64/src/mixer.c) as a 32-bit
 * x86 program, with no display, renderer, input or audio output. For
 * reproducing crashes under gdb or ASan on the PC; nothing here runs on the
 * S30.
 *
 * Environment:
 *   DKR_ROM          the US 1.0 ROM (required)
 *   DKR_HOSTPRESS    "frame:buttons,frame:buttons,..." (buttons as S30_N64_*
 *                    hex bits, held for 6 frames): scripted input
 *   DKR_HOSTFRAMES   exit after this many display lists
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "s30_audio.h"
#include "s30_gfx.h"
#include "s30_input.h"

typedef unsigned int GLuint;

int gS30EnableAudio = 1;
static const char* sPrefix = "DKR";
static uint32_t* sSegments;
static unsigned sFrames;

void S30_SetEnvPrefix(const char* prefix) {
    sPrefix = prefix;
}

const char* S30_Getenv(const char* name) {
    char buf[128];

    snprintf(buf, sizeof(buf), "%s_%s", sPrefix, name);
    return getenv(buf);
}

const char* S30_TmpPath(const char* name) {
    char* p = malloc(256);

    snprintf(p, 256, "/tmp/dkr_%s", name);
    return p;
}

/* Display */
void S30Display_SetRefresh(float hz) { (void)hz; }
void S30Display_Init(void) {}
void S30Display_BeginFrame(void) {}
int S30Display_Bind3D(void) { return 0; }
void S30Display_Bind2D(void) {}
void S30Display_SaveBackground(void) {}
void S30Display_ReadFrame(unsigned char* rgba) { (void)rgba; }
void S30Display_RestoreBackground(void) {}
void S30Display_CopyRect(GLuint* tex, GLuint* fbo, int w, int h, float x0, float y0, float x1, float y1) {}
void S30Display_VisMono(const float prim[4], const float env[4]) {}
void S30Display_MotionBlurBlend(float alpha) {}
void S30Display_MotionBlurSave(void) {}
void S30Display_QueueProbe(int slot, int x, int y, float z) {}
int S30Display_ReadProbes(unsigned char* out, int max) { return 0; }
void S30Display_Unbind(void) {}
void S30Display_EndFrame(void) {}
void S30Display_TakeLateFrames(int* frames, int* vblanks) { *frames = 0; *vblanks = 0; }

/* Renderer: the display list is only counted */
void S30Gfx_Configure(const S30GfxConfig* config) {
    sSegments = config->segments;
    S30_SetEnvPrefix(config->envPrefix);
}
void S30Gfx_SetVi(float x0, float y0, float kx, float ky, int stride) {}
void S30Gfx_Init(void) {}

void* pc_resolve_addr(uint32_t addr) {
    if (addr < 0x10000000u && sSegments != NULL) {
        return (void*)(uintptr_t)(sSegments[(addr >> 24) & 0xF] + (addr & 0xFFFFFF));
    }
    return (void*)(uintptr_t)addr;
}

/* F3DDKR display list statistics: commands, vertex loads, triangles,
   texture rectangles, and addresses that don't resolve */
static unsigned sCmds, sVtx, sTris, sRects, sBad;

static uint32_t sBadSample[4];

static int bad_ptr(uint32_t addr) {
    uintptr_t p = (uintptr_t)pc_resolve_addr(addr);

    if (p < 0x08000000u || p > 0xF0000000u) {
        sBadSample[0] = addr;
        return 1;
    }
    return 0;
}

static void walk(const uint32_t* dl, int depth, int max) {
    if (depth > 16 || dl == NULL) {
        sBad++;
        return;
    }
    for (; max != 0; dl += 2, max--) {
        uint32_t w0 = dl[0], w1 = dl[1];

        sCmds++;
        sBadSample[1] = w0;
        if (sCmds > 200000) {
            return;
        }
        switch (w0 >> 24) {
            case 0x04:
                sVtx += ((w0 >> 19) & 0x1F) + 1;
                sBad += bad_ptr(w1);
                break;
            case 0x05:
                sTris += (w0 >> 4) & 0xFFF;
                sBad += bad_ptr(w1);
                break;
            case 0x01:
            case 0x03:
                sBad += bad_ptr(w1);
                break;
            case 0x06:
                if (bad_ptr(w1)) {
                    sBad++;
                } else {
                    walk(pc_resolve_addr(w1), depth + 1, -1);
                }
                if ((w0 >> 16) & 0xFF) {
                    return;
                }
                break;
            case 0x07:
                if (bad_ptr(w1)) {
                    sBad++;
                } else {
                    walk(pc_resolve_addr(w1), depth + 1, (w0 >> 16) & 0xFF);
                }
                break;
            case 0xB8:
                return;
            case 0xE4:
            case 0xE5:
                sRects++;
                break;
            case 0xFD:
                if (bad_ptr(w1)) {
                    static int reported;

                    sBad++;
                    if (reported++ < 5) {
                        printf("host: frame %u: bad G_SETTIMG %08x %08x at %p, before:", sFrames, w0, w1, (void*)dl);
                        for (int k = -6; k < 0; k++) {
                            printf(" %08x:%08x", dl[2 * k], dl[2 * k + 1]);
                        }
                        printf("\n");
                    }
                }
                break;
        }
        if (sBadSample[0] != 0 && sBadSample[2] == 0) {
            sBadSample[2] = sBadSample[1];
            sBadSample[3] = sBadSample[0];
        }
    }
}

void pc_process_displaylist(void* displayList) {
    const char* max = S30_Getenv("HOSTFRAMES");

    sFrames++;
    walk(displayList, 0, -1);
    if (sFrames % 60 == 0) {
        printf("host: frame %u: cmds %u vtx %u tris %u rects %u bad %u (%08x %08x)\n", sFrames, sCmds / 60, sVtx / 60,
               sTris / 60, sRects / 60, sBad, sBadSample[2], sBadSample[3]);
        memset(sBadSample, 0, sizeof(sBadSample));
        sCmds = sVtx = sTris = sRects = sBad = 0;
    }
    if (max != NULL && sFrames >= (unsigned)atoi(max)) {
        printf("host: %u frames, exiting\n", sFrames);
        exit(0);
    }
}

void gfx_texture_cache_invalidate(void* addr) { (void)addr; }
void flush_texture_cache_external(void) {}
void S30Gfx_TextureOverride(const void* src, int w, int h, int rgb565, void* pixels) { free(pixels); }
void S30Gfx_FrameCopy(const void* dst, int x, int y, int w, int h) {}
void S30Gfx_BeginDepthTests(void) {}
int S30Gfx_DepthVisible(float x, float y, float z) { return 1; }
void S30Gfx_ReadFrame(void* dst) { (void)dst; }

/* Input: DKR_HOSTPRESS */
void S30Input_Init(const S30ButtonMap* map, int count) { (void)map; (void)count; }

void S30Input_Read(S30Pad* pad) {
    const char* script = S30_Getenv("HOSTPRESS");

    memset(pad, 0, sizeof(*pad));
    while (script != NULL && *script) {
        unsigned frame = strtoul(script, (char**)&script, 10), buttons = 0;

        if (*script == ':') {
            buttons = strtoul(script + 1, (char**)&script, 16);
        }
        if (sFrames >= frame && sFrames < frame + 6) {
            pad->button |= buttons;
        }
        if (*script == ',') {
            script++;
        }
    }
}

/* Audio: frames are dropped; the AI reads empty */
int S30Audio_Open(void) { return 0; }
void S30Audio_Configure(int32_t rate, float updateHz) {}
void S30Audio_Start(void (*update)(void)) {}
void S30Audio_Init(void (*update)(void)) {}
/* A device draining the queue at 22050 Hz in real time, with the paced
   mode's latency kept (n64/src/audio.c), so the game sizes its frames as it
   does on the S30 */
#include <time.h>
static double sQueued;
static double sLastNs;

static void drain(void) {
    struct timespec ts;
    double now;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = ts.tv_sec * 1e9 + ts.tv_nsec;
    if (sLastNs != 0) {
        sQueued -= (now - sLastNs) * 22050e-9;
        if (sQueued < 0) {
            sQueued = 0;
        }
    }
    sLastNs = now;
}

uint32_t S30Audio_QueuedFrames(void) {
    drain();
    return sQueued > 1024 ? (uint32_t)(sQueued - 1024) : 0;
}
int S30Audio_Underruns(void) { return 0; }
void S30Audio_Play(const int16_t* left, const int16_t* right, int32_t frames) {}
/* DKR_HOSTAUDIO=file: the frames, raw s16 stereo */
void S30Audio_PlayInterleaved(const int16_t* samples, int32_t frames) {
    static FILE* f;
    const char* path = S30_Getenv("HOSTAUDIO");

    if (path != NULL && f == NULL) {
        f = fopen(path, "wb");
    }
    if (f != NULL) {
        fwrite(samples, 4, frames, f);
    }
    drain();
    sQueued += frames;
    if (sQueued > 8192 + 2050) {
        sQueued = 8192 + 2050;
    }
}
void S30Audio_Lock(void) {}
void S30Audio_Unlock(void) {}

/* The app: no crash handler (gdb, ASan); paths as on the S30 */
#include <stdarg.h>

void S30App_InstallCrashHandler(const char* crashLog) { (void)crashLog; }
void S30App_SignalStack(void) {}

void S30App_Fatal(const char* fmt, va_list ap) {
    vfprintf(stderr, fmt, ap);
    abort();
}

const char* S30App_PathIn(const char* dir, const char* name) {
    char* p = malloc(strlen(dir) + strlen(name) + 2);

    sprintf(p, "%s/%s", dir, name);
    return p;
}

const char* S30App_SavePath(const char* game, const char* file, const char* fallbackDir) {
    char* p = malloc(256);

    (void)fallbackDir;
    snprintf(p, 256, "/tmp/%s_%s", game, file);
    return p;
}
void S30App_PrintWhere(const char* who, void* ctx) { (void)who; (void)ctx; }
