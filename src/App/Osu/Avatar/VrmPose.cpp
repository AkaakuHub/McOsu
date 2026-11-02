#include "VrmPose.h"
#include "VrmDocument.h"
#include "PoseMath.h"
#include "cgltf.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace avatar
{
namespace
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
}

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

void VrmPose::recalibrate() { m_calibrated = false; }

void VrmPose::moveLimb(const std::string &side, bool arm, Vector3 target)
{
	auto &upper = bone(side + (arm ? "UpperArm" : "UpperLeg"));
	auto &lower = bone(side + (arm ? "LowerArm" : "LowerLeg"));
	auto &end = bone(side + (arm ? "Hand" : "Foot"));
	const auto start = position(world(upper));
	const auto middle = position(world(lower));
	const auto tip = position(world(end));
	const auto bend = arm ? Vector3(side == "left" ? 0.25f : -0.25f, -1, -0.5f) : Vector3(0, 0, 1);
	const auto solution = solveLimb(start, target, bend, (middle - start).length(), (tip - middle).length());
	rotateBone(upper, middle - start, solution.middle - start);
	const auto movedMiddle = position(world(lower));
	rotateBone(lower, position(world(end)) - movedMiddle, solution.end - movedMiddle);
}

void VrmPose::update(const TrackingFrame &tracking)
{
	if (!tracking.head.valid) return;
	const auto headPosition = position(tracking.head.matrix);
	if (!m_calibrated)
	{
		m_floorHeight = headPosition.y - m_restEyes.y;
		m_calibrated = true;
	}
	auto &scene = m_document.scene();
	for (std::size_t index = 0; index < scene.nodes_count; ++index)
	{
		std::copy(m_restLocal[index].get(), m_restLocal[index].get() + 16, scene.nodes[index].matrix);
		scene.nodes[index].has_matrix = true;
	}
	const float yaw = std::atan2(-tracking.head.matrix[8], -tracking.head.matrix[10]) * 180.0f / 3.14159265358979323846f;
	Matrix4 rotation, translation, turn;
	rotation.rotateY(yaw);
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
		if (hand.valid) moveLimb(side, true, point(trackingToModel, position(hand.matrix)));
		const auto foot = m_document.humanBones().at(side + "Foot");
		moveLimb(side, false, position(m_restWorld[foot]));
	}
}
}
