#ifndef MCOSU_VRM_TRANSFORMS_H
#define MCOSU_VRM_TRANSFORMS_H

#include "Matrices.h"
struct cgltf_node;

namespace avatar
{
Matrix4 world(const cgltf_node &node);
Vector3 position(const Matrix4 &matrix);
Vector3 point(const Matrix4 &matrix, Vector3 value);
void setWorld(cgltf_node &node, const Matrix4 &matrix);
void rotateBone(cgltf_node &node, Vector3 from, Vector3 to);
Matrix4 orientation(Matrix4 matrix);
Matrix4 handFrame(Vector3 forward, Vector3 up);
}

#endif
