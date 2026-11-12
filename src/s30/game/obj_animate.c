/*
 * obj_animate (src/hasm/obj_animate.s) in C, for the PocketGo S30: an
 * object model's vertex animation, from its keyframes.
 *
 * An animation (ObjectModel_44.anim, a byte stream read a byte at a time, so
 * the same in either byte order) holds, with n animated vertices and a
 * keyframe stride of 3n + 12 bytes:
 *   - 0x00: frame 0's 12-byte header, then at 0x0C the base positions (s16
 *     x, y, z per animated vertex, big-endian);
 *   - (k + 2) * stride: the s8 x, y, z steps from frame k to frame k + 1,
 *     followed by frame k + 1's header.
 * A header's bytes 0-5 are the model offset (x, y, z) and 0xA-0xB the head
 * tilt, as big-endian s16s.
 *
 * The positions of the current whole frame are kept in the instance's third
 * vertex buffer (vertices[2], as s16 triples); the frame between them goes
 * to the other two in turn (animationTaskNum), the fraction (animFrame's low
 * 4 bits, in sixteenths) of the next step added through D_8011D644.
 */
#include "objects.h"
#include "structs.h"

extern s32 D_8011D644; // the fraction of the next step, s16 triples

#define BE16(p) ((s16) (((s8) (p)[0] << 8) | (p)[1]))

s32 obj_animate(Object *obj) {
    s32 modelIndex = obj->modelIndex;
    ModelInstance *inst;
    ObjectModel *model;
    ObjectModel_44 *anim;
    s16 *indices, *indicesEnd;
    s16 *frame, *step;
    Vertex *v;
    s32 animID, frameCount, frameIdx, curFrame, maxFrame, frac, stride, n;
    u8 *data, *header;
    s16 base[4];

    if (modelIndex < 0) {
        modelIndex = 0;
    }
    if (modelIndex >= obj->header->numberOfModelIds) {
        modelIndex = obj->header->numberOfModelIds;
    }
    inst = obj->modelInstances[modelIndex];
    model = inst->objModel;
    if (model->animations == NULL) {
        return 0;
    }
    frameCount = obj->animFrame;
    animID = obj->animationID;
    if (frameCount == inst->animationFrameCount && animID == inst->animationID) {
        return 0;
    }
    if (animID < 0) {
        animID = 0;
    }
    if (animID >= model->numberOfAnimations) {
        animID = model->numberOfAnimations - 1;
    }
    maxFrame = 0;
    if (model->numberOfAnimations > 0) {
        maxFrame = model->animations[animID].animLength - 2;
    }
    frameIdx = (s32) ((u32) frameCount >> 4);
    if (maxFrame < frameIdx) {
        frameCount = 0;
        frameIdx = 0;
        inst->animationID = -1;
    }
    frame = (s16 *) inst->vertices[2];
    curFrame = (animID == inst->animationID) ? inst->animationFrame : -1;
    inst->animationID = animID;
    inst->animationFrameCount = frameCount;
    inst->animationFrame = frameIdx;
    frac = frameCount & 0xF;

    anim = &model->animations[animID];
    data = (u8 *) anim->anim;
    n = model->numberOfAnimatedVertices;
    indices = (s16 *) model->animatedVertexIndices;

    if (frameIdx == 0 || curFrame == -1) {
        // Frame 0's positions: the model's vertices plus the base offsets
        u8 *src = data + 0xC;

        v = model->vertices;
        indicesEnd = indices + model->numberOfVertices;
        do {
            if (*indices != -1) {
                s16 *d = frame + *indices * 3;
                u8 *s = src + *indices * 6;
                d[0] = v->x + BE16(s);
                d[1] = v->y + BE16(s + 2);
                d[2] = v->z + BE16(s + 4);
            }
            indices++;
            v++;
        } while (indices < indicesEnd);
        indices = (s16 *) model->animatedVertexIndices;
        curFrame = 0;
    }
    stride = n * 3 + 12;

    // To the whole frame, a step at a time (each loop at least once, as on
    // the N64)
    for (; curFrame < frameIdx; curFrame++) {
        s8 *d = (s8 *) (data + stride * (curFrame + 2));
        s16 *p = frame, *end = frame + n * 3;

        do {
            p[0] += d[0];
            p[1] += d[1];
            p[2] += d[2];
            p += 3;
            d += 3;
        } while (p < end);
    }
    for (; curFrame > frameIdx; curFrame--) {
        s8 *d = (s8 *) (data + stride * (curFrame + 1));
        s16 *p = frame, *end = frame + n * 3;

        do {
            p[0] -= d[0];
            p[1] -= d[1];
            p[2] -= d[2];
            p += 3;
            d += 3;
        } while (p < end);
    }

    // The fraction of the next step
    step = (s16 *) D_8011D644;
    {
        s8 *d = (s8 *) (data + stride * (frameIdx + 2));
        s16 *p = step, *end = step + n * 3;

        do {
            p[0] = (s16) ((d[0] * frac) >> 4);
            p[1] = (s16) ((d[1] * frac) >> 4);
            p[2] = (s16) ((d[2] * frac) >> 4);
            p += 3;
            d += 3;
        } while (p < end);
    }

    // The model offset and head tilt, between this frame's header and the
    // next one's
    header = data;
    if (frameIdx != 0) {
        header = data + stride * (frameIdx + 1) - 0xC;
    }
    base[0] = BE16(header);
    base[1] = BE16(header + 2);
    base[2] = BE16(header + 4);
    base[3] = BE16(header + 0xA);
    header += stride;
    if (frameIdx == 0) {
        header += stride - 0xC;
    }
    inst->offsetX = base[0] + (((BE16(header) - base[0]) * frac) >> 4);
    inst->offsetY = base[1] + (((BE16(header + 2) - base[1]) * frac) >> 4);
    inst->offsetZ = base[2] + (((BE16(header + 4) - base[2]) * frac) >> 4);
    inst->headTilt = base[3] + (((BE16(header + 0xA) - base[3]) * frac) >> 4);

    // The frame drawn: whole frame plus fraction, into the other buffer
    inst->animationTaskNum ^= 1;
    v = inst->vertices[inst->animationTaskNum];
    indicesEnd = indices + model->numberOfVertices;
    do {
        s16 i0 = *indices++;

        if (i0 != -1) {
            v->x = frame[i0 * 3] + step[i0 * 3];
            v->y = frame[i0 * 3 + 1] + step[i0 * 3 + 1];
            v->z = frame[i0 * 3 + 2] + step[i0 * 3 + 2];
        }
        v++;
    } while (indices < indicesEnd);
    return 1;
}
