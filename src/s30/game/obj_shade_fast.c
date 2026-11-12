/*
 * obj_shade_fast and calc_dynamic_lighting_for_object_2
 * (src/hasm/obj_shade_fast.s) in C, for the PocketGo S30: per-vertex
 * lighting of an object model's lit batches (those whose miscData isn't
 * BATCH_VTX_COL), from its normals, into the object's vertex colours.
 *
 * Float to integer conversions truncate: the assembly switches the FPU to
 * round towards zero around them.
 */
#include "camera.h"
#include "math_util.h"
#include "objects.h"
#include "structs.h"

/* The lit colour: an intensity in r, g and b, alpha opaque (two halfword
   stores on the N64: r g, then b and 0xFF) */
static void put_shade(Vertex *v, u32 shade) {
    u32 rg = shade | (shade << 8);

    v->r = (u8) (rg >> 8);
    v->g = (u8) rg;
    v->b = (u8) ((rg | 0xFF) >> 8);
    v->a = 0xFF;
}

/* The light's direction (s16 x, y, z) against each normal, 2.11 fixed point */
void obj_shade_fast(ObjectModel *model, Object *obj, f32 intensity) {
    ShadeProperties *shading = obj->shading;
    TriangleBatchInfo *batch, *end;
    Vertex *v;
    Vec3s *n;
    s32 base, lx, ly, lz;

    if (shading == NULL) {
        return;
    }
    base = (s32) (shading->unk0 * intensity * 160.0f);
    lx = shading->shadowDirX;
    ly = shading->shadowDirY;
    lz = shading->shadowDirZ;
    n = model->normals;
    v = obj->curVertData;
    batch = model->batches;
    end = batch + model->numberOfBatches;
    do {
        s32 count = batch[1].verticesOffset - batch[0].verticesOffset;

        if (batch->miscData == BATCH_VTX_COL) {
            v += count;
            if (batch->flags & 0x8000) {
                n += count;
            }
        } else {
            for (; count > 0; count--, n++, v++) {
                s32 dot = (n->x * lx + n->y * ly + n->z * lz) >> 11;
                u32 shade;

                if (dot > 0) {
                    shade = ((u32) (dot * base) >> 16) + base;
                    if ((s32) shade >= 0x100) {
                        shade = 0xFF;
                    }
                } else {
                    shade = base;
                }
                put_shade(v, shade);
            }
        }
        batch++;
    } while (batch < end);
}

/* As above, the light turned into the object's space (and the camera's, if
   inProjection): ambient plus diffuse */
void calc_dynamic_lighting_for_object_2(Object *obj, ObjectModel *model, s16 inProjection, f32 intensity) {
    ShadeProperties *shading = obj->shading;
    TriangleBatchInfo *batch, *end;
    ObjectTransform trans;
    MtxF mtx;
    Vec3f dir;
    Vertex *v;
    Vec3s *n;
    f32 level;
    s32 ambient, diffuse, lx, ly, lz;

    if (shading == NULL) {
        return;
    }
    dir.x = (f32) (shading->shadowDirX << 2);
    dir.y = (f32) (shading->shadowDirY << 2);
    dir.z = (f32) (shading->shadowDirZ << 2);
    if (inProjection) {
        mtxf_transform_dir(get_projection_matrix_f32(), &dir, &dir);
    }
    trans.rotation.s[0] = -obj->trans.rotation.s[0];
    trans.rotation.s[1] = -obj->trans.rotation.s[1];
    trans.rotation.s[2] = -obj->trans.rotation.s[2];
    trans.scale = 1.0f;
    trans.position.x = 0.0f;
    trans.position.y = 0.0f;
    trans.position.z = 0.0f;
    mtxf_from_inverse_transform(&mtx, &trans);
    mtxf_transform_dir(&mtx, &dir, &dir);

    level = shading->unk0 * intensity * 255.0f;
    ambient = (s32) (shading->ambient * level);
    diffuse = (s32) (shading->diffuse * level);
    lx = (s32) dir.x;
    ly = (s32) dir.y;
    lz = (s32) dir.z;
    n = model->normals;
    v = obj->curVertData;
    batch = model->batches;
    end = batch + model->numberOfBatches;
    do {
        s32 count = batch[1].verticesOffset - batch[0].verticesOffset;

        if (batch->miscData == BATCH_VTX_COL) {
            v += count;
            if (batch->flags & 0x8000) {
                n += count;
            }
        } else {
            for (; count > 0; count--, n++, v++) {
                s32 dot = (n->x * lx + n->y * ly + n->z * lz) >> 7;
                u32 shade;

                if (dot > 0) {
                    shade = ((u32) (dot * diffuse) >> 21) + ambient;
                    if ((s32) shade >= 0x100) {
                        shade = 0xFF;
                    }
                } else {
                    shade = ambient;
                }
                put_shade(v, shade);
            }
        }
        batch++;
    } while (batch < end);
}
