/*
 * The game's handwritten math library (src/hasm/ido/math_util.s) in C, for
 * the PocketGo S30. Written from the assembly, not from the decomp's
 * NON_MATCHING C in src/hasm/math_util.c: same arithmetic, same order of
 * float operations, same fixed-point rounding.
 *
 * Mtx is the N64's s15.16 matrix as 16-bit elements: integer halves
 * (s16 [4][4]) then fraction halves (u16 [4][4]), each where its index puts
 * it in native byte order (as gfx.c and libultra/src/gu/mtxutil.c read and
 * write it on this build).
 *
 * The sine and arctangent tables are the assembly's own data, extracted at
 * build time (tools/s30/math_tables.py).
 */
#include <math.h>
#include <string.h>

#include "math_util.h"

extern u16 gSineTable[1025];
extern u16 gArcTanTable[1025];

u8 gIntDisFlag = 0;
s32 gCurrentRNGSeed = 0x5141564D; // 'QAVM'
s32 gPrevRNGSeed = 0x5141564D;

/* ---------------------------------------------------------------------------
 * Interrupts: one N64 thread runs at a time here and nothing interrupts it
 * mid-instruction (n64/src/os.c), so the critical sections need nothing
 * ------------------------------------------------------------------------- */

u32 interrupts_disable(void) {
    return 0;
}

void interrupts_enable(UNUSED u32 flags) {
}

void set_gIntDisFlag(u8 setting) {
    gIntDisFlag = setting;
}

u8 get_gIntDisFlag(void) {
    return gIntDisFlag;
}

void set_breakpoint(UNUSED u32 address) {
}

/* ---------------------------------------------------------------------------
 * Matrix formats
 * ------------------------------------------------------------------------- */

#define MTX_INT(m) ((s16 *) (m))
#define MTX_FRAC(m) ((u16 *) (m) + 16)

/* Mtx to 16.16 integers */
void mtx_to_mtxs(Mtx *m, MtxS *mi) {
    s32 *out = (s32 *) mi;
    s32 i;

    for (i = 0; i < 16; i++) {
        out[i] = (s32) (((u32) (u16) MTX_INT(m)[i] << 16) | MTX_FRAC(m)[i]);
    }
}

void mtx_to_mtxs_2(Mtx *m, MtxS *mi) {
    mtx_to_mtxs(m, mi);
}

/* trunc.w.s */
void mtxf_to_mtxs(MtxF *mf, MtxS *mi) {
    s32 i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            (*mi)[i][j] = (s32) ((*mf)[i][j] * 65536.0f);
        }
    }
}

/* cvt.w.s: the FPU's rounding mode, round to nearest even */
void mtxf_to_mtx(MtxF *mf, Mtx *m) {
    s32 i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            u32 v = (u32) (s32) lrintf((*mf)[i][j] * 65536.0f);
            MTX_INT(m)[i * 4 + j] = (s16) (v >> 16);
            MTX_FRAC(m)[i * 4 + j] = (u16) v;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Float matrices
 * ------------------------------------------------------------------------- */

void mtxf_transform_point(float mf[4][4], float x, float y, float z, float *ox, float *oy, float *oz) {
    *ox = mf[3][0] + ((mf[0][0] * x + mf[1][0] * y) + mf[2][0] * z);
    *oy = mf[3][1] + ((mf[0][1] * x + mf[1][1] * y) + mf[2][1] * z);
    *oz = mf[3][2] + ((mf[0][2] * x + mf[1][2] * y) + mf[2][2] * z);
}

void mtxf_transform_dir(MtxF *mf, Vec3f *in, Vec3f *out) {
    f32 x = in->x, y = in->y, z = in->z;

    out->x = (x * (*mf)[0][0] + y * (*mf)[1][0]) + z * (*mf)[2][0];
    out->y = (x * (*mf)[0][1] + y * (*mf)[1][1]) + z * (*mf)[2][1];
    out->z = (x * (*mf)[0][2] + y * (*mf)[1][2]) + z * (*mf)[2][2];
}

/* output = mat1 * mat2, a row at a time: each row of mat1 is read before
   the same row of output is written, mat2 again for every row (so output
   may be mat1, as on the N64) */
void mtxf_mul(MtxF *mat1, MtxF *mat2, MtxF *output) {
    s32 i, j;

    for (i = 0; i < 4; i++) {
        f32 a0 = (*mat1)[i][0], a1 = (*mat1)[i][1], a2 = (*mat1)[i][2], a3 = (*mat1)[i][3];

        for (j = 0; j < 4; j++) {
            (*output)[i][j] = (a1 * (*mat2)[1][j] + a2 * (*mat2)[2][j]) + (a0 * (*mat2)[0][j] + a3 * (*mat2)[3][j]);
        }
    }
}

#define S16_TO_F (1.0f / 65536.0f)

void mtxf_from_transform(MtxF *mtx, ObjectTransform *trans) {
    f32 sr = sins_s16(trans->rotation.y_rotation) * S16_TO_F;
    f32 cr = coss_s16(trans->rotation.y_rotation) * S16_TO_F;
    f32 sp = sins_s16(trans->rotation.x_rotation) * S16_TO_F;
    f32 cp = coss_s16(trans->rotation.x_rotation) * S16_TO_F;
    f32 sy = sins_s16(trans->rotation.z_rotation) * S16_TO_F;
    f32 cy = coss_s16(trans->rotation.z_rotation) * S16_TO_F;
    f32 scale = trans->scale;

    (*mtx)[0][3] = 0.0f;
    (*mtx)[1][3] = 0.0f;
    (*mtx)[2][3] = 0.0f;
    (*mtx)[0][0] = (sp * sr * sy + cy * cr) * scale;
    (*mtx)[0][1] = (sy * cp) * scale;
    (*mtx)[0][2] = (sp * cr * sy - cy * sr) * scale;
    (*mtx)[1][0] = (sp * sr * cy - sy * cr) * scale;
    (*mtx)[1][1] = (cy * cp) * scale;
    (*mtx)[1][2] = (sp * cr * cy + sy * sr) * scale;
    (*mtx)[2][0] = (cp * sr) * scale;
    (*mtx)[2][1] = -(sp * scale);
    (*mtx)[2][2] = (cp * cr) * scale;
    (*mtx)[3][0] = trans->position.x;
    (*mtx)[3][1] = trans->position.y;
    (*mtx)[3][2] = trans->position.z;
    (*mtx)[3][3] = 1.0f;
}

void mtxf_from_inverse_transform(MtxF *mtx, ObjectTransform *trans) {
    f32 sr = sins_s16(trans->rotation.y_rotation) * S16_TO_F;
    f32 cr = coss_s16(trans->rotation.y_rotation) * S16_TO_F;
    f32 sp = sins_s16(trans->rotation.x_rotation) * S16_TO_F;
    f32 cp = coss_s16(trans->rotation.x_rotation) * S16_TO_F;
    f32 sy = sins_s16(trans->rotation.z_rotation) * S16_TO_F;
    f32 cy = coss_s16(trans->rotation.z_rotation) * S16_TO_F;
    f32 x, y, z;

    (*mtx)[0][3] = 0.0f;
    (*mtx)[1][2] = sp;
    (*mtx)[1][3] = 0.0f;
    (*mtx)[2][3] = 0.0f;
    (*mtx)[0][0] = cr * cy - sp * sy * sr;
    (*mtx)[0][1] = sp * cy * sr + cr * sy;
    (*mtx)[0][2] = -(sr * cp);
    (*mtx)[1][0] = -(cp * sy);
    (*mtx)[1][1] = cp * cy;
    (*mtx)[2][0] = sp * sy * cr + sr * cy;
    (*mtx)[2][1] = sr * sy - sp * cy * cr;
    (*mtx)[2][2] = cr * cp;
    x = trans->position.x;
    y = trans->position.y;
    z = trans->position.z;
    (*mtx)[3][0] = ((*mtx)[1][0] * y + (*mtx)[0][0] * x) + (*mtx)[2][0] * z;
    (*mtx)[3][1] = ((*mtx)[1][1] * y + (*mtx)[0][1] * x) + (*mtx)[2][1] * z;
    (*mtx)[3][2] = ((*mtx)[1][2] * y + (*mtx)[0][2] * x) + (*mtx)[2][2] * z;
    (*mtx)[3][3] = 1.0f;
}

/* Only the w column of the rotation matrix it never finished */
void func_80070058(MtxF *mtx, UNUSED RPYAngles *angles) {
    (*mtx)[0][3] = 0.0f;
    (*mtx)[1][3] = 0.0f;
    (*mtx)[2][3] = 0.0f;
    (*mtx)[3][3] = 1.0f;
}

void mtxf_scale_y(MtxF *input, f32 scale) {
    (*input)[1][0] *= scale;
    (*input)[1][1] *= scale; // AVOID_UB: the N64 read [1][1] from a3
    (*input)[1][2] *= scale;
}

void mtxf_translate_y(MtxF *input, f32 offset) {
    (*input)[3][0] = (*input)[1][0] * offset + (*input)[3][0];
    (*input)[3][1] = (*input)[1][1] * offset + (*input)[3][1];
    (*input)[3][2] = (*input)[1][2] * offset + (*input)[3][2];
}

void mtxf_billboard(MtxF *mtx, s32 angle, f32 scale, f32 scaleY) {
    f32 s = sins_s16(angle) * S16_TO_F;
    f32 c = coss_s16(angle) * S16_TO_F;

    c *= scale;
    s *= scale;
    memset(mtx, 0, sizeof(MtxF));
    (*mtx)[0][0] = c;
    (*mtx)[0][1] = s;
    (*mtx)[2][2] = scale;
    (*mtx)[1][0] = -s;
    (*mtx)[1][1] = c * scaleY;
    (*mtx)[3][3] = 1.0f;
}

void mtxf_from_translation(MtxF *mtx, f32 x, f32 y, f32 z) {
    memset(mtx, 0, sizeof(MtxF));
    (*mtx)[0][0] = 1.0f;
    (*mtx)[1][1] = 1.0f;
    (*mtx)[2][2] = 1.0f;
    (*mtx)[3][3] = 1.0f;
    (*mtx)[3][0] = x;
    (*mtx)[3][1] = y;
    (*mtx)[3][2] = z;
}

void mtxf_from_scale(MtxF *mtx, f32 scaleX, f32 scaleY, f32 scaleZ) {
    memset(mtx, 0, sizeof(MtxF));
    (*mtx)[3][3] = 1.0f;
    (*mtx)[0][0] = scaleX;
    (*mtx)[1][1] = scaleY;
    (*mtx)[2][2] = scaleZ;
}

/* ---------------------------------------------------------------------------
 * Fixed-point matrices and vectors (32-bit products, wrapping)
 * ------------------------------------------------------------------------- */

void mtxs_transform_point(MtxS *mi, Vec3s *vec) {
    s32 x = vec->x, y = vec->y, z = vec->z;
    s32 i;

    for (i = 0; i < 3; i++) {
        (&vec->x)[i] = (s16) ((x * (*mi)[0][i] + y * (*mi)[1][i] + z * (*mi)[2][i] + (*mi)[3][i]) >> 16);
    }
}

void mtxs_transform_dir(MtxS *mi, Vec3s *vec) {
    s32 x = vec->x, y = vec->y, z = vec->z;
    s32 i;

    for (i = 0; i < 3; i++) {
        (&vec->x)[i] = (s16) ((x * (*mi)[0][i] + y * (*mi)[1][i] + z * (*mi)[2][i]) >> 16);
    }
}

/* Incident vector at vec[0], reflection written to vec[1] */
void vec3s_reflect(Vec3s *vec, Vec3s *n) {
    s32 x = vec[0].x, y = vec[0].y, z = vec[0].z;
    s32 dot = (x * n->x + y * n->y + z * n->z) >> 12;

    vec[1].x = (s16) (((dot * n->x) >> 13) - x);
    vec[1].y = (s16) (((dot * n->y) >> 13) - y);
    vec[1].z = (s16) (((dot * n->z) >> 13) - z); // AVOID_UB: the N64 subtracted x
}

void vec3s_rotate_rpy(RPYAngles *rotation, Vec3s *vec) {
    s32 x = vec->x, y = vec->y, z = vec->z;
    s32 s, c, a, b;
    s16 *angle = (s16 *) rotation;

    s = sins_s16(angle[0]);
    c = coss_s16(angle[0]);
    a = (x * c - y * s) >> 16;
    b = (y * c + x * s) >> 16;
    x = a;
    y = b;

    s = sins_s16(angle[1]);
    c = coss_s16(angle[1]);
    a = (y * c - z * s) >> 16;
    b = (z * c + y * s) >> 16;
    y = a;
    z = b;

    s = sins_s16(angle[2]);
    c = coss_s16(angle[2]);
    a = (x * c + z * s) >> 16;
    b = (z * c - x * s) >> 16;
    x = a;
    z = b;

    vec->x = x;
    vec->y = y;
    vec->z = z;
}

/* Rotation angles at 0 (x), 2 (y), 4 (z) */
void vec3f_rotate(Vec3s *rotation, Vec3f *vec) {
    s16 *angle = (s16 *) rotation;
    f32 x = vec->x, y = vec->y, z = vec->z;
    f32 s, c, a, b;

    s = sins_f(angle[2]);
    c = coss_f(angle[2]);
    a = x * c - y * s;
    b = y * c + x * s;
    x = a;
    y = b;

    s = sins_f(angle[1]);
    c = coss_f(angle[1]);
    a = y * c - z * s;
    b = z * c + y * s;
    y = a;
    z = b;

    s = sins_f(angle[0]);
    c = coss_f(angle[0]);
    a = x * c + z * s;
    b = z * c - x * s;
    vec->x = a;
    vec->y = y;
    vec->z = b;
}

void vec3f_rotate_ypr(Vec3s *rotation, Vec3f *vec) {
    s16 *angle = (s16 *) rotation;
    f32 x = vec->x, y = vec->y, z = vec->z;
    f32 s, c, a, b;

    s = sins_f(angle[0]);
    c = coss_f(angle[0]);
    a = x * c + z * s;
    b = z * c - x * s;
    x = a;
    z = b;

    s = sins_f(angle[1]);
    c = coss_f(angle[1]);
    a = y * c - z * s;
    b = z * c + y * s;
    y = a;
    z = b;

    s = sins_f(angle[2]);
    c = coss_f(angle[2]);
    a = x * c - y * s;
    b = y * c + x * s;
    vec->x = a;
    vec->y = b;
    vec->z = z;
}

/* A length along z turned by pitch (angle 1), then yaw (angle 0) */
void vec3f_rotate_py(Vec3s *rotation, Vec3f *vec) {
    s16 *angle = (s16 *) rotation;
    f32 z = vec->z;
    f32 x, y;

    y = -(z * sins_f(angle[1]));
    z = z * coss_f(angle[1]);
    x = z * sins_f(angle[0]);
    z = z * coss_f(angle[0]);
    vec->x = x;
    vec->y = y;
    vec->z = z;
}

s32 tri2d_xz_contains_point(s32 x, s32 z, Vec3s *pointA, Vec3s *pointB, Vec3s *pointC) {
    s32 ax = pointA->x, az = pointA->z, bx = pointB->x, bz = pointB->z, cx = pointC->x, cz = pointC->z;
    s32 ab = (x - ax) * (bz - az) - (bx - ax) * (z - az) >= 0;
    s32 bc = (x - bx) * (cz - bz) - (cx - bx) * (z - bz) >= 0;
    s32 ca = (x - cx) * (az - cz) - (ax - cx) * (z - cz) >= 0;

    return ab == bc && ca == bc;
}

f32 area_triangle_2d(f32 x0, f32 z0, f32 x1, f32 z1, f32 x2, f32 z2) {
    f32 dx0 = x1 - x0, dz0 = z1 - z0;
    f32 dx1 = x2 - x1, dz1 = z2 - z1;
    f32 dx2 = x0 - x2, dz2 = z0 - z2;
    f32 d0 = sqrtf(dx0 * dx0 + dz0 * dz0);
    f32 d1 = sqrtf(dx1 * dx1 + dz1 * dz1);
    f32 d2 = sqrtf(dx2 * dx2 + dz2 * dz2);
    f32 s = 0.5f * ((d0 + d1) + d2);
    f32 a = ((s - d0) * s) * ((s - d1) * (s - d2));

    if (a < 0.0f) {
        a = 0.0f;
    }
    return sqrtf(a);
}

/* ---------------------------------------------------------------------------
 * Random numbers
 * ------------------------------------------------------------------------- */

void set_rng_seed(s32 num) {
    gCurrentRNGSeed = num;
}

void save_rng_seed(void) {
    gPrevRNGSeed = gCurrentRNGSeed;
}

void load_rng_seed(void) {
    gCurrentRNGSeed = gPrevRNGSeed;
}

s32 get_rng_seed(void) {
    return gCurrentRNGSeed;
}

/* The 64-bit shift register of the assembly: only its low 32 bits are kept */
s32 rand_range(s32 min, s32 max) {
    u32 s = (u32) gCurrentRNGSeed;
    u32 t = ((u32) ((s32) s >> 1)) ^ ((s & 0xFFFFF) << 12);
    u32 range = (u32) (max - min + 1);
    u32 r;

    r = t ^ ((t >> 20) & 0xFFF);
    gCurrentRNGSeed = (s32) r;
    r -= (u32) min;
    return (s32) ((range != 0 ? r % range : r) + (u32) min); // divu by 0 leaves the dividend in HI
}

/* ---------------------------------------------------------------------------
 * Trigonometry: angles are 0x10000 to a turn; sines and cosines are 16.16
 * ------------------------------------------------------------------------- */

/* Interpolates between the quarter-wave table's entries */
s32 sins_s16(s16 angle) {
    u32 a = (u32) (s32) angle;
    u32 i, lo, hi;
    s32 v;

    if ((a << 17) & 0x80000000) {
        a ^= 0x7FFF;
    }
    i = (a >> 4) & 0x3FF;
    lo = gSineTable[i];
    hi = gSineTable[i + 1];
    v = (s32) ((lo << 1) + (((hi - lo) * (a & 0xF)) >> 3));
    return (a & 0x8000) ? -v : v;
}

s32 coss_s16(s16 angle) {
    return sins_s16((s16) (angle + 0x4000));
}

/* Without the interpolation */
s32 sins_2(s16 angle) {
    u32 a = (u32) (s32) angle;
    s32 v;

    if ((a << 17) & 0x80000000) {
        a ^= 0x7FFF;
    }
    v = (s32) gSineTable[(a >> 4) & 0x3FF] << 1;
    return (a & 0x8000) ? -v : v;
}

s32 coss_2(s16 angle) {
    return sins_2((s16) (angle + 0x4000));
}

f32 sins_f(s16 angle) {
    return (f32) sins_s16(angle) * S16_TO_F;
}

f32 coss_f(s16 angle) {
    return (f32) coss_s16(angle) * S16_TO_F;
}

s32 atan2s(s32 xDelta, s32 zDelta) {
    u32 x = (u32) xDelta, y = (u32) zDelta, base, t;

    if ((xDelta | zDelta) == 0) {
        return 0;
    }
    if (xDelta >= 0) {
        if (zDelta >= 0) {
            base = 0;
        } else {
            y = -y;
            base = 0x4000;
            t = x, x = y, y = t;
        }
    } else {
        x = -x;
        if (zDelta < 0) {
            y = -y;
            base = 0x8000;
        } else {
            base = 0xC000;
            t = x, x = y, y = t;
        }
    }
    // dsll/ddivu on the sign-extended registers
    if ((s32) (x - y) >= 0) {
        t = (u32) ((u64) ((s64) (s32) y << 11) / (u64) (s64) (s32) x) & 0xFFE;
        return (base + 0x4000 - (s32) (s16) gArcTanTable[t / 2]) & 0xFFFF;
    }
    t = (u32) ((u64) ((s64) (s32) x << 11) / (u64) (s64) (s32) y) & 0xFFE;
    return (base + (s32) (s16) gArcTanTable[t / 2]) & 0xFFFF;
}

/* cvt.w.s: round to nearest */
#ifdef AVOID_UB
u16 arctan2_f(f32 y, f32 x) {
#else
s16 arctan2_f(f32 y, f32 x) {
#endif
    return atan2s((s32) lrintf(y * 255.0f), (s32) lrintf(x * 255.0f));
}

s32 fix32_sqrt(s32 x) {
    return (s32) lrintf(sqrtf((f32) x / 65536.0f) * 65536.0f);
}

/* The bug kept: the integer's bits taken as a float */
s32 bad_int_sqrt(s32 x) {
    union {
        s32 i;
        f32 f;
    } u;

    u.i = x;
    u.f = sqrtf(u.f);
    return u.i;
}

/* ---------------------------------------------------------------------------
 * Memory
 * ------------------------------------------------------------------------- */

/* 16 bytes at a time until dst reaches end (at least once) */
void dmacopy_doubleword(void *src, void *dst, u32 end) {
    u8 *s = src, *d = dst;

    do {
        memcpy(d, s, 16);
        s += 16;
        d += 16;
    } while ((u32) (uintptr_t) d != end);
}
