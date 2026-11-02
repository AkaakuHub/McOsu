#include "VrmTransforms.h"
#include "PoseMath.h"
#include "cgltf.h"
#include <algorithm>

namespace avatar
{
Matrix4 world(const cgltf_node &node)
{
	float matrix[16];
	cgltf_node_transform_world(&node, matrix);
	return Matrix4(matrix);
}

Vector3 position(const Matrix4 &matrix) { return Vector3(matrix[12], matrix[13], matrix[14]); }

Vector3 point(const Matrix4 &matrix, Vector3 value)
{
	const auto result = matrix * Vector4(value.x, value.y, value.z, 1);
	return Vector3(result.x, result.y, result.z);
}

void setWorld(cgltf_node &node, const Matrix4 &matrix)
{
	const auto local = node.parent ? world(*node.parent).invert() * matrix : matrix;
	std::copy(local.get(), local.get() + 16, node.matrix);
	node.has_matrix = true;
}

void rotateBone(cgltf_node &node, Vector3 from, Vector3 to)
{
	const auto current = world(node);
	auto rotated = rotationBetween(from, to) * current;
	rotated[12] = current[12];
	rotated[13] = current[13];
	rotated[14] = current[14];
	setWorld(node, rotated);
}

Matrix4 orientation(Matrix4 matrix)
{
	matrix[12] = matrix[13] = matrix[14] = 0;
	return matrix;
}

Matrix4 handFrame(Vector3 forward, Vector3 up)
{
	forward.normalize();
	const auto right = up.cross(forward).normalize();
	up = forward.cross(right).normalize();
	return Matrix4(right.x, right.y, right.z, 0, up.x, up.y, up.z, 0,
		forward.x, forward.y, forward.z, 0, 0, 0, 0, 1);
}
}
