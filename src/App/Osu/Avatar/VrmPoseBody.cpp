#include "VrmPose.h"
#include "VrmDocument.h"
#include "VrmTransforms.h"
#include "cgltf.h"
#include <algorithm>

namespace avatar
{
Matrix4 VrmPose::trackedBonePose(const TrackedPose &tracked, int index, const std::string &name)
{
	auto &binding = m_trackerBindings[index];
	if (!binding.calibrated || binding.deviceId != tracked.deviceId)
	{
		binding.offset = Matrix4(tracked.matrix).invert() * m_modelToTracking
			* m_restWorld[m_document.humanBones().at(name)];
		binding.deviceId = tracked.deviceId;
		binding.calibrated = true;
	}
	return tracked.matrix * binding.offset;
}

void VrmPose::solveTorso(const Matrix4 &headPose, Vector3 eyeTarget, bool fixedPelvis)
{
	auto &head = bone("head");
	auto &hips = bone("hips");
	std::vector<cgltf_node *> chain;
	for (auto *node = &head; node && node != &hips; node = node->parent) chain.push_back(node);
	std::reverse(chain.begin(), chain.end());
	if (chain.size() < 2) return;
	std::vector<Vector3> points;
	std::vector<float> lengths;
	float reach = 0;
	for (auto *node : chain) points.push_back(position(world(*node)));
	for (std::size_t i = 1; i < points.size(); ++i)
	{
		const float length = (points[i] - points[i - 1]).length();
		lengths.push_back(length);
		reach += length;
	}
	const auto restHead = m_restWorld[m_document.humanBones().at("head")];
	const auto eyeOffset = point(Matrix4(restHead).invert(), m_restEyes);
	const auto target = eyeTarget - headPose * eyeOffset;
	auto root = points.front();
	if (!fixedPelvis && (target - root).length() > reach * 0.999f)
	{
		const auto shift = (target - root).normalize() * ((target - root).length() - reach * 0.999f);
		auto pelvis = world(hips);
		pelvis[12] += shift.x; pelvis[13] += shift.y; pelvis[14] += shift.z;
		setWorld(hips, pelvis);
		for (auto &point : points) point += shift;
		root += shift;
	}
	if ((target - root).length() >= reach)
	{
		const auto direction = (target - root).normalize();
		for (std::size_t i = 1; i < points.size(); ++i) points[i] = points[i - 1] + direction * lengths[i - 1];
	}
	else
	{
		for (int iteration = 0; iteration < 48 && (points.back() - target).length() > 0.000001f; ++iteration)
		{
			points.back() = target;
			for (std::size_t i = points.size() - 1; i > 0; --i)
			{
				const auto direction = points[i - 1] - points[i];
				if (direction.length() > 0) points[i - 1] = points[i] + Vector3(direction).normalize() * lengths[i - 1];
			}
			points.front() = root;
			for (std::size_t i = 1; i < points.size(); ++i)
			{
				const auto direction = points[i] - points[i - 1];
				if (direction.length() > 0) points[i] = points[i - 1] + Vector3(direction).normalize() * lengths[i - 1];
			}
		}
	}
	for (std::size_t i = 0; i + 1 < chain.size(); ++i)
	{
		const auto current = position(world(*chain[i + 1])) - position(world(*chain[i]));
		const auto solved = points[i + 1] - points[i];
		if (current.length() > 0 && solved.length() > 0) rotateBone(*chain[i], current, solved);
	}
	auto result = headPose;
	const auto origin = position(world(head));
	result[12] = origin.x; result[13] = origin.y; result[14] = origin.z;
	setWorld(head, result);
	if (!fixedPelvis)
	{
		const auto correction = eyeTarget - point(world(head), eyeOffset);
		auto pelvis = world(hips);
		pelvis[12] += correction.x; pelvis[13] += correction.y; pelvis[14] += correction.z;
		setWorld(hips, pelvis);
	}
}
}
