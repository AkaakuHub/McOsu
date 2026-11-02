#ifndef MCOSU_VRM_SKINNING_H
#define MCOSU_VRM_SKINNING_H

#include "VrmGeometry.h"
#include "Vectors.h"

namespace avatar
{
std::vector<Vector3> skinPositions(const cgltf_data &scene, const Primitive &primitive);
}

#endif
