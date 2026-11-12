#ifndef _COLLISION_H_
#define _COLLISION_H_

#include "types.h"
#include "structs.h"

#define MAX_COLLISION_CANDIDATES 500

// gCollisionCandidates holds both segments and facets: a segment's address with
// its top bit clear (N64 RAM addresses have it set). On the S30 addresses have
// it clear, so a segment gets it set instead.
#ifdef TARGET_S30
#define CANDIDATE_SEGMENT(seg) ((s32) ((uintptr_t) (seg) | 0x80000000u))
#define CANDIDATE_IS_SEGMENT(c) ((c) < 0)
#define CANDIDATE_TO_SEGMENT(c) ((LevelModelSegment *) (uintptr_t) ((u32) (c) & 0x7FFFFFFFu))
#else
#define CANDIDATE_SEGMENT(seg) ((s32) K0_TO_PHYS(seg))
#define CANDIDATE_IS_SEGMENT(c) ((c) >= 0)
#define CANDIDATE_TO_SEGMENT(c) ((LevelModelSegment *) (PHYS_TO_K0(c)))
#endif

enum CollisionModes { COLLISION_MODE_DEFAULT, COLLISION_MODE_1, COLLISION_MODE_NO_WALLS };

void generate_collision_candidates(s32 numPoints, Vec3f *origins, Vec3f *targets, s32 vehicleID);
s32 compute_grid_overlap_mask(LevelModelSegmentBoundingBox *bbox, s32 x1, s32 z1, s32 x2, s32 z2);
s32 resolve_collisions(Vec3f *origin, Vec3f *target, f32 *radius, s8 *surface, s32 numEntries, s32 *numCollisions);

#endif
