#ifndef MCOSU_VRM_HAND_ANCHORS_H
#define MCOSU_VRM_HAND_ANCHORS_H

#include "Vectors.h"
#include <array>

namespace avatar
{
class VrmDocument;
std::array<Vector3, 2> measurePalmAnchors(const VrmDocument &document);
}

#endif
