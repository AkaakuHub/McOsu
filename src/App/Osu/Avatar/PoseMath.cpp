#include "PoseMath.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "ozz/animation/runtime/ik_two_bone_job.h"
#include "ozz/base/maths/simd_quaternion.h"

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
	using namespace ozz::math;
	const auto load = [](Vector3 value) { return simd_float4::Load(value.x, value.y, value.z, 0); };
	const auto vector = [](SimdFloat4 value) {
		float elements[4];
		StorePtrU(value, elements);
		return Vector3(elements[0], elements[1], elements[2]);
	};
	Float4x4 root = Float4x4::identity();
	Float4x4 middle = Float4x4::Translation(simd_float4::Load(0, -upperLength, 0, 0));
	Float4x4 end = Float4x4::Translation(simd_float4::Load(0, -upperLength - lowerLength, 0, 0));
	SimdQuaternion rootCorrection, middleCorrection;
	ozz::animation::IKTwoBoneJob job;
	job.start_joint = &root;
	job.mid_joint = &middle;
	job.end_joint = &end;
	job.target = load(direction * reach);
	job.pole_vector = load(bend);
	job.start_joint_correction = &rootCorrection;
	job.mid_joint_correction = &middleCorrection;
	if (!job.Run()) throw std::runtime_error("ozz two-bone IK rejected the limb");
	const auto upper = TransformVector(rootCorrection, simd_float4::Load(0, -upperLength, 0, 0));
	const auto lower = TransformVector(rootCorrection,
		TransformVector(middleCorrection, simd_float4::Load(0, -lowerLength, 0, 0)));
	return {start + vector(upper), start + vector(upper + lower)};
}

Matrix4 rotationBetween(Vector3 from, Vector3 to)
{
	if (from.length() == 0 || to.length() == 0)
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
