#ifndef MCOSU_AVATAR_POSE_MATH_H
#define MCOSU_AVATAR_POSE_MATH_H

#include "Matrices.h"

namespace avatar
{
struct LimbSolution { Vector3 middle; Vector3 end; };
LimbSolution solveLimb(Vector3 start, Vector3 target, Vector3 bendDirection, float upperLength, float lowerLength);
Matrix4 rotationBetween(Vector3 from, Vector3 to);
}

#endif
