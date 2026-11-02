#ifndef MCOSU_VRM_VISIBILITY_H
#define MCOSU_VRM_VISIBILITY_H

#include "VrmGeometry.h"
#include <vector>

namespace avatar
{
class VrmDocument;
struct ViewIndices { std::vector<unsigned int> firstPerson; std::vector<unsigned int> thirdPerson; };
std::vector<ViewIndices> readViewIndices(const VrmDocument &document, const std::vector<Primitive> &geometry);
}

#endif
