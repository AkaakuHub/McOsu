#include "VrmPose.h"
#include "VrmDocument.h"
#include "PoseMath.h"
#include "VrmTransforms.h"
#include "VrmHandAnchors.h"
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
	m_palmAnchors = measurePalmAnchors(document);
	FootLocomotion::Config config;
	config.restPelvis = position(world(bone("hips")));
	for (int i = 0; i < 2; ++i)
	{
		const std::string side = i == 0 ? "left" : "right";
		config.restFeet[i] = world(bone(side + "Foot"));
		config.legLength[i] = (position(world(bone(side + "UpperLeg"))) - position(world(bone(side + "LowerLeg")))).length()
			+ (position(world(bone(side + "LowerLeg"))) - position(config.restFeet[i])).length();
	}
	m_locomotion.configure(config);
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
	m_scale = 0;
	m_trackingFloor = 0;
	m_lastWaistTime = -1;
	m_hasArmTwist.fill(false);
	for (auto &binding : m_trackerBindings) binding.calibrated = false;
	m_locomotion.reset();
	m_hasFeet = false;
	m_stablePoses.clear();
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

void VrmPose::update(const TrackingFrame &input)
{
	if (input.hasTrackingSpaceDelta)
	{
		m_pendingSpaceDelta = m_hasPendingSpaceDelta ? input.trackingSpaceDelta * m_pendingSpaceDelta : input.trackingSpaceDelta;
		m_hasPendingSpaceDelta = true;
	}
	auto tracking = input;
	if (!tracking.headsetInUse)
	{
		if (m_calibrated) freezeStablePose();
		m_calibrated = false;
		m_hasArmTwist.fill(false);
		return;
	}
	if (!tracking.head.valid)
	{
		if (m_calibrated) freezeStablePose();
		m_calibrated = false;
		return;
	}
	if (m_scale == 0 && (!tracking.leftHand.valid || !tracking.rightHand.valid)) return;
	tracking.hasTrackingSpaceDelta = m_hasPendingSpaceDelta;
	tracking.trackingSpaceDelta = m_pendingSpaceDelta;
	m_hasPendingSpaceDelta = false;
	const auto headPosition = position(tracking.head.matrix);
	const float deltaTime = std::max(0.0f, std::min(0.05f, tracking.deltaTime));
	m_trackingTime += deltaTime;
	const float headSpeed = m_calibrated && deltaTime > 0 ? (headPosition - m_previousHead).length() / deltaTime : 0;
	if (tracking.hasTrackingSpaceDelta)
	{
		m_previousHead = point(tracking.trackingSpaceDelta, m_previousHead);
		m_bodyYaw += std::atan2(tracking.trackingSpaceDelta[8], tracking.trackingSpaceDelta[10]);
		for (auto &foot : m_feet) foot.pose = tracking.trackingSpaceDelta * foot.pose;
	}
	const float headYaw = std::atan2(-tracking.head.matrix[8], -tracking.head.matrix[10]);
	if (!m_calibrated)
	{
		if (m_scale == 0)
		{
			if (tracking.leftFoot.valid && tracking.rightFoot.valid)
			{
				const float measuredFeet = (tracking.leftFoot.matrix[13] + tracking.rightFoot.matrix[13]) * 0.5f;
				const float restFeet = (position(m_restWorld[m_document.humanBones().at("leftFoot")]).y
					+ position(m_restWorld[m_document.humanBones().at("rightFoot")]).y) * 0.5f;
				if (headPosition.y <= measuredFeet || m_restEyes.y <= restFeet) return;
				m_scale = (headPosition.y - measuredFeet) / (m_restEyes.y - restFeet);
				m_trackingFloor = measuredFeet - restFeet * m_scale;
			}
			else
			{
				if (headPosition.y <= 0) return;
				m_scale = headPosition.y / m_restEyes.y;
			}
		}
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
	Matrix4 rotation, translation, scale, turn;
	rotation.rotateY(m_bodyYaw * 180.0f / 3.14159265358979323846f);
	translation.translate(headPosition.x, m_trackingFloor, headPosition.z);
	scale.scale(m_scale);
	turn.rotateY(180);
	m_modelToTracking = translation * rotation * scale;
	Matrix4 waistPose;
	if (tracking.waist.valid)
	{
		waistPose = trackedBonePose(tracking.waist, 0, "hips");
		const auto heading = orientation(waistPose) * orientation(m_restWorld[m_document.humanBones().at("hips")]).invert();
		m_bodyYaw = std::atan2(heading[8], heading[10]);
		rotation.identity().rotateY(m_bodyYaw * 180.0f / 3.14159265358979323846f);
		const auto offset = rotation * (position(m_restWorld[m_document.humanBones().at("hips")]) * m_scale);
		translation.identity().translate(waistPose[12] - offset.x, m_trackingFloor, waistPose[14] - offset.z);
		m_modelToTracking = translation * rotation * scale;
	}
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
	if (!tracking.waist.valid)
	{
		const float pitch = std::atan2(-headDelta[9], std::sqrt(headDelta[8] * headDelta[8] + headDelta[10] * headDelta[10]));
		const float bend = std::max(-20.0f, std::min(55.0f, pitch * 180.0f / 3.14159265f * 0.65f));
		Matrix4 torsoTilt;
		torsoTilt.rotateX(bend);
		const auto pelvisPosition = desiredEyes - rotatedEyeOffset - torsoTilt * (position(restHead) - position(world(hips)));
		hipsWorld = torsoTilt * world(hips);
		hipsWorld[12] = pelvisPosition.x;
		hipsWorld[13] = pelvisPosition.y;
		hipsWorld[14] = pelvisPosition.z;
	}
	if (tracking.waist.valid) hipsWorld = trackingToModel * waistPose;
	else if (m_hasFeet)
	{
		const auto support = point(trackingToModel, (position(m_feet[0].pose) + position(m_feet[1].pose)) * 0.5f);
		auto lean = Vector3(support.x - hipsWorld[12], 0, support.z - hipsWorld[14]) * 0.35f;
		const float limit = m_restEyes.y * 0.08f;
		if (lean.length() > limit) lean = lean.normalize() * limit;
		hipsWorld[12] += lean.x;
		hipsWorld[14] += lean.z;
	}
	if (!tracking.waist.valid)
		hipsWorld[13] = std::max(hipsWorld[13], position(m_restWorld[m_document.humanBones().at("hips")]).y * 0.15f);
	setWorld(hips, hipsWorld);
	auto headWorld = headDelta * restHead;
	const auto headOrigin = position(world(head));
	headWorld[12] = headOrigin.x;
	headWorld[13] = headOrigin.y;
	headWorld[14] = headOrigin.z;
	setWorld(head, headWorld);
	solveTorso(headDelta * restHead, desiredEyes, tracking.waist.valid);
	FootLocomotion::Input locomotion;
	locomotion.dt = deltaTime;
	locomotion.modelToTracking = m_modelToTracking;
	locomotion.head = tracking.head.matrix;
	locomotion.hipsWorld = point(m_modelToTracking, position(world(hips)));
	locomotion.balancePoint = headPosition * 0.45f + locomotion.hipsWorld * 0.55f;
	for (int i = 0; i < 2; ++i)
	{
		const auto &hand = i == 0 ? tracking.leftHand : tracking.rightHand;
		locomotion.hands[i] = hand.valid ? position(hand.matrix) : headPosition;
		if (hand.valid) locomotion.balancePoint = locomotion.balancePoint * 0.96f + position(hand.matrix) * 0.04f;
		const auto &foot = i == 0 ? tracking.leftFoot : tracking.rightFoot;
		locomotion.trackedFeet[i].valid = foot.valid;
		if (foot.valid) locomotion.trackedFeet[i].pose = trackedBonePose(foot, i + 1, i == 0 ? "leftFoot" : "rightFoot");
	}
	locomotion.pelvisTracked = tracking.waist.valid;
	locomotion.hasTrackingSpaceDelta = tracking.hasTrackingSpaceDelta;
	locomotion.trackingSpaceDelta = tracking.trackingSpaceDelta;
	m_feet = m_locomotion.update(locomotion);
	m_hasFeet = true;
	for (const std::string side : {"left", "right"})
	{
		const auto &hand = side == "left" ? tracking.leftHand : tracking.rightHand;
		auto elbow = side == "left" ? tracking.leftElbow : tracking.rightElbow;
		if (elbow.valid) elbow.matrix = trackingToModel * elbow.matrix;
		if (hand.valid) moveArm(side, trackingToModel * hand.matrix, hand.palmSurface, elbow);
		const auto footTarget = trackingToModel * m_feet[side == "left" ? 0 : 1].pose;
		moveLimb(side, false, position(footTarget), Vector3(side == "left" ? -0.1f : 0.1f, 0, 1));
		auto &foot = bone(side + "Foot");
		auto footWorld = orientation(footTarget) * orientation(world(foot)).invert() * world(foot);
		const auto footPosition = position(world(foot));
		footWorld[12] = footPosition.x; footWorld[13] = footPosition.y; footWorld[14] = footPosition.z;
		setWorld(foot, footWorld);
	}
	rememberStablePose(tracking, headSpeed);
}
}
