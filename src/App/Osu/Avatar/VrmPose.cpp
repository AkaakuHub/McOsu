#include "VrmPose.h"
#include "VrmDocument.h"
#include "PoseMath.h"
#include "VrmTransforms.h"
#include "cgltf.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace avatar
{
VrmPose::VrmPose(VrmDocument &document) : m_document(document)
{
	const auto &scene = document.scene();
	for (std::size_t index = 0; index < scene.nodes_count; ++index)
	{
		float local[16];
		cgltf_node_transform_local(&scene.nodes[index], local);
		m_restLocal.emplace_back(local);
		m_restWorld.push_back(world(scene.nodes[index]));
	}
	for (const auto *name : {"hips", "head", "leftEye", "rightEye", "leftUpperArm", "leftLowerArm", "leftHand",
		"rightUpperArm", "rightLowerArm", "rightHand", "leftUpperLeg", "leftLowerLeg", "leftFoot",
		"rightUpperLeg", "rightLowerLeg", "rightFoot"}) bone(name);
	m_restEyes = (position(world(bone("leftEye"))) + position(world(bone("rightEye")))) * 0.5f;
	if (m_restEyes.y <= 0) throw std::runtime_error("Avatar eyes must be above the floor");
}

cgltf_node &VrmPose::bone(const std::string &name)
{
	const auto found = m_document.humanBones().find(name);
	if (found == m_document.humanBones().end()) throw std::runtime_error("Tracking requires humanoid bone: " + name);
	return m_document.scene().nodes[found->second];
}

void VrmPose::recalibrate()
{
	m_calibrated = false;
	m_hasArmTwist.fill(false);
}

void VrmPose::moveLimb(const std::string &side, bool arm, Vector3 target, Vector3 bend)
{
	auto &upper = bone(side + (arm ? "UpperArm" : "UpperLeg"));
	auto &lower = bone(side + (arm ? "LowerArm" : "LowerLeg"));
	auto &end = bone(side + (arm ? "Hand" : "Foot"));
	const auto start = position(world(upper));
	const auto middle = position(world(lower));
	const auto tip = position(world(end));
	const auto solution = solveLimb(start, target, bend, (middle - start).length(), (tip - middle).length());
	rotateBone(upper, middle - start, solution.middle - start);
	const auto movedMiddle = position(world(lower));
	rotateBone(lower, position(world(end)) - movedMiddle, solution.end - movedMiddle);
}

void VrmPose::update(const TrackingFrame &tracking)
{
	if (!tracking.head.valid) return;
	const auto headPosition = position(tracking.head.matrix);
	const auto now = std::chrono::steady_clock::now();
	const float deltaTime = m_calibrated ? std::min(0.05f, std::chrono::duration<float>(now - m_lastUpdate).count()) : 0;
	m_lastUpdate = now;
	const float headYaw = std::atan2(-tracking.head.matrix[8], -tracking.head.matrix[10]);
	if (!m_calibrated)
	{
		m_floorHeight = headPosition.y - m_restEyes.y;
		m_bodyYaw = headYaw;
		m_calibrated = true;
	}
	auto forward = Vector3(-tracking.head.matrix[8], 0, -tracking.head.matrix[10]);
	if (forward.length() > 0.1f)
	{
		forward.normalize();
		if (tracking.leftHand.valid && tracking.rightHand.valid)
		{
			auto across = position(tracking.rightHand.matrix) - position(tracking.leftHand.matrix);
			across.y = 0;
			if (across.length() > 0.2f)
			{
				const auto handForward = across.normalize().cross(Vector3(0, 1, 0));
				if (handForward.dot(forward) > 0.5f) forward = (forward * 0.65f + handForward * 0.35f).normalize();
			}
		}
		const float desired = std::atan2(forward.x, forward.z);
		const float difference = std::atan2(std::sin(desired - m_bodyYaw), std::cos(desired - m_bodyYaw));
		auto movement = headPosition - m_previousHead;
		movement.y = 0;
		const bool moving = deltaTime > 0 && movement.length() / deltaTime > 0.1f;
		if (moving || std::abs(difference) > 35.0f * 3.14159265f / 180.0f)
		{
			const float amount = std::max(deltaTime * 120.0f * 3.14159265f / 180.0f,
				std::abs(difference) - 65.0f * 3.14159265f / 180.0f);
			m_bodyYaw += std::copysign(std::min(std::abs(difference), amount), difference);
		}
	}
	m_previousHead = headPosition;
	auto &scene = m_document.scene();
	for (std::size_t index = 0; index < scene.nodes_count; ++index)
	{
		std::copy(m_restLocal[index].get(), m_restLocal[index].get() + 16, scene.nodes[index].matrix);
		scene.nodes[index].has_matrix = true;
	}
	Matrix4 rotation, translation, turn;
	rotation.rotateY(m_bodyYaw * 180.0f / 3.14159265358979323846f);
	translation.translate(headPosition.x, m_floorHeight, headPosition.z);
	turn.rotateY(180);
	m_modelToTracking = translation * rotation;
	const auto trackingToModel = Matrix4(m_modelToTracking).invert();
	const auto headDelta = Matrix4(rotation).invert() * orientation(tracking.head.matrix) * turn;
	auto &head = bone("head");
	const auto restHead = world(head);
	const auto rotatedEyeOffset = headDelta * (m_restEyes - position(restHead));
	const auto desiredEyes = point(trackingToModel, headPosition);
	const auto shift = desiredEyes - position(restHead) - rotatedEyeOffset;
	auto &hips = bone("hips");
	auto hipsWorld = world(hips);
	hipsWorld[12] += shift.x;
	hipsWorld[13] += shift.y;
	hipsWorld[14] += shift.z;
	setWorld(hips, hipsWorld);
	auto headWorld = headDelta * restHead;
	const auto headOrigin = position(world(head));
	headWorld[12] = headOrigin.x;
	headWorld[13] = headOrigin.y;
	headWorld[14] = headOrigin.z;
	setWorld(head, headWorld);
	for (const std::string side : {"left", "right"})
	{
		const auto &hand = side == "left" ? tracking.leftHand : tracking.rightHand;
		if (hand.valid) moveArm(side, trackingToModel * hand.matrix);
		const auto foot = m_document.humanBones().at(side + "Foot");
		moveLimb(side, false, position(m_restWorld[foot]), Vector3(0, 0, 1));
	}
}
}
