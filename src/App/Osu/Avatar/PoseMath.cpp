#include "PoseMath.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace avatar
{
namespace
{
Vector3 perpendicular(Vector3 direction)
{
	return direction.cross(std::abs(direction.y) < 0.9f ? Vector3(0, 1, 0) : Vector3(1, 0, 0)).normalize();
}
}

LimbSolution solveLimb(Vector3 start, Vector3 target, Vector3 bendDirection, float upperLength, float lowerLength)
{
	if (!std::isfinite(upperLength) || !std::isfinite(lowerLength) || upperLength <= 0 || lowerLength <= 0)
		throw std::runtime_error("Limb lengths must be positive and finite");
	auto direction = target - start;
	const auto distance = direction.length();
	direction = distance > 0.000001f ? direction / distance : Vector3(0, -1, 0);
	const float reach = std::max(std::abs(upperLength - lowerLength) + 0.000001f,
		std::min(distance, upperLength + lowerLength - 0.000001f));
	auto bend = bendDirection - direction * bendDirection.dot(direction);
	bend = bend.length() > 0.000001f ? bend.normalize() : perpendicular(direction);
	const float along = (upperLength * upperLength - lowerLength * lowerLength + reach * reach) / (2 * reach);
	const float height = std::sqrt(std::max(0.0f, upperLength * upperLength - along * along));
	return {start + direction * along + bend * height, start + direction * reach};
}

Matrix4 rotationBetween(Vector3 from, Vector3 to)
{
	if (from.length() < 0.000001f || to.length() < 0.000001f)
		throw std::runtime_error("Cannot rotate a zero-length bone");
	from.normalize();
	to.normalize();
	const float cosine = std::max(-1.0f, std::min(1.0f, from.dot(to)));
	if (cosine > 0.999999f) return Matrix4();
	const auto axis = cosine < -0.999999f ? perpendicular(from) : from.cross(to).normalize();
	Matrix4 rotation;
	rotation.rotate(std::acos(cosine) * 180.0f / 3.14159265358979323846f, axis);
	return rotation;
}
}
