#include "VrmDocument.h"
#include "VrmPose.h"
#include "VrmGeometry.h"
#include "VrmSkinning.h"
#include "VrmTransforms.h"
#include "VrmHandAnchors.h"
#include "cgltf.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void auditGarment(const char *path);

Vector3 trackedBone(avatar::VrmDocument &model, const avatar::VrmPose &pose, const std::string &name)
{
	float matrix[16];
	cgltf_node_transform_world(&model.scene().nodes[model.humanBones().at(name)], matrix);
	const auto result = pose.modelToTracking() * Vector4(matrix[12], matrix[13], matrix[14], 1);
	return Vector3(result.x, result.y, result.z);
}

int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	try
	{
		avatar::VrmDocument model(argv[1]);
		avatar::VrmPose pose(model);
		const auto anchors = avatar::measurePalmAnchors(model);
		Vector3 fingerDirections[2];
		int sideIndex = 0;
		for (const std::string side : {"left", "right"})
		{
			const auto &hand = model.scene().nodes[model.humanBones().at(side + "Hand")];
			const auto &finger = model.scene().nodes[model.humanBones().at(side + "MiddleProximal")];
			fingerDirections[sideIndex++] = avatar::orientation(avatar::world(hand)).invert()
				* (avatar::position(avatar::world(finger)) - avatar::position(avatar::world(hand))).normalize();
		}
		avatar::TrackingFrame frame;
		frame.deltaTime = 1.0f / 60;
		frame.head.valid = frame.leftHand.valid = frame.rightHand.valid = true;
		frame.head.matrix.translate(0, 1.65f, 0);
		pose.update(frame);
		const float avatarScale = Vector3(pose.modelToTracking()[0], pose.modelToTracking()[1], pose.modelToTracking()[2]).length();
		const auto calibrationOrigin = pose.modelToTracking() * Vector4(0, 0, 0, 1);
		if (std::abs(calibrationOrigin.y) > 0.00001f) throw std::runtime_error("Avatar floor differs from tracking floor");
		float maxEyeError = 0, maxHandError = 0, maxReachableHandError = 0, maxHandDirectionError = 0, maxForearmStep = 0;
		Matrix4 previousForearms[2];
		for (int step = 0; step < 360; ++step)
		{
			const float phase = step * 0.02f;
			Matrix4 yaw, tilt, location;
			yaw.rotateY(phase * 20);
			tilt.rotateX(std::sin(phase) * 15);
			location.translate(std::sin(phase) * 0.1f, 1.65f - std::sin(phase) * 0.15f, 0.1f);
			frame.head.matrix = location * yaw * tilt;
			for (auto *hand : {&frame.leftHand, &frame.rightHand})
			{
				Matrix4 relative, wrist;
				relative.translate(hand == &frame.leftHand ? -0.25f : 0.25f, -0.35f, -0.25f);
				wrist.rotateZ(phase * 180);
				hand->matrix = location * yaw * relative * wrist;
				hand->palmSurface = step % 2 == 1;
				if (hand->palmSurface)
				{
					Matrix4 turn;
					turn.rotateY(180);
					const auto offset = hand->matrix * turn * (anchors[hand == &frame.leftHand ? 0 : 1] * avatarScale);
					hand->matrix[12] += offset.x;
					hand->matrix[13] += offset.y;
					hand->matrix[14] += offset.z;
				}
			}
			pose.update(frame);
			const auto eyes = pose.viewpoint();
			const auto &head = frame.head.matrix;
			maxEyeError = std::max(maxEyeError, (eyes - Vector3(head[12], head[13], head[14])).length());
			sideIndex = 0;
			for (const std::string side : {"left", "right"})
			{
				const auto &tracked = side == "left" ? frame.leftHand : frame.rightHand;
				const auto &hand = tracked.matrix;
				Matrix4 turn;
				turn.rotateY(180);
				const auto offset = tracked.palmSurface ? hand * turn * (anchors[side == "left" ? 0 : 1] * avatarScale) : Vector3();
				maxHandError = std::max(maxHandError,
					(trackedBone(model, pose, side + "Hand") - Vector3(hand[12], hand[13], hand[14]) + offset).length());
				const auto shoulder = trackedBone(model, pose, side + "UpperArm");
				const auto elbow = trackedBone(model, pose, side + "LowerArm");
				const auto wrist = trackedBone(model, pose, side + "Hand");
				const auto target = Vector3(hand[12], hand[13], hand[14]) - offset;
				const float reach = (elbow - shoulder).length() + (wrist - elbow).length();
				const float excess = std::max(0.0f, (target - shoulder).length() - reach);
				maxReachableHandError = std::max(maxReachableHandError, (wrist - target).length() - excess);
				auto current = pose.modelToTracking()
					* avatar::orientation(avatar::world(model.scene().nodes[model.humanBones().at(side + "Hand")]))
					* fingerDirections[sideIndex++];
				current.normalize();
				maxHandDirectionError = std::max(maxHandDirectionError,
					(current - Vector3(-hand[8], -hand[9], -hand[10])).length());
				const auto forearm = avatar::orientation(avatar::world(model.scene().nodes[model.humanBones().at(side + "LowerArm")]));
				if (step > 0)
				{
					const auto delta = Matrix4(previousForearms[sideIndex - 1]).invert() * forearm;
					const float cosine = std::max(-1.0f, std::min(1.0f, (delta[0] + delta[5] + delta[10] - 1) * 0.5f));
					maxForearmStep = std::max(maxForearmStep, std::acos(cosine) * 180.0f / 3.141592654f);
				}
				previousForearms[sideIndex - 1] = forearm;
				if (step == 0)
				{
					const auto shoulder = trackedBone(model, pose, side + "UpperArm");
					const auto elbow = trackedBone(model, pose, side + "LowerArm");
					std::cout << side << " shoulderY=" << shoulder.y << " elbowY=" << elbow.y << '\n';
					if (elbow.y > shoulder.y) throw std::runtime_error("Elbow bent upward with hands held forward");
				}
			}
			for (const auto &primitive : avatar::readGeometry(model.scene())) avatar::skinPositions(model.scene(), primitive);
		}
		std::cout << "frames=360 maxEyeError=" << maxEyeError << " maxHandError=" << maxHandError
			<< " maxHandDirectionError=" << maxHandDirectionError << " maxForearmStepDegrees=" << maxForearmStep << '\n';
		if (maxEyeError > 0.0001f || maxReachableHandError > 0.001f || maxHandDirectionError > 0.001f || maxForearmStep > 20)
			throw std::runtime_error("Tracking targets were missed");
		frame.head.matrix.identity().translate(0, 1.65f, 0);
		for (int idle = 0; idle < 60; ++idle) pose.update(frame);
		const auto beforeStandby = trackedBone(model, pose, "head");
		for (int removal = 0; removal < 15; ++removal)
		{
			frame.head.matrix.identity().rotateX(100);
			frame.head.matrix[13] = 0.5f;
			pose.update(frame);
		}
		frame.headsetInUse = false;
		frame.head.matrix.identity().translate(0.4f, 0.6f, 0.2f);
		pose.update(frame);
		if ((trackedBone(model, pose, "head") - beforeStandby).length() > 0.00001f)
			throw std::runtime_error("Standby did not restore the stable pose before headset removal");
		frame.headsetInUse = true;
		frame.head.matrix.identity().translate(0.2f, 1.8f, 0.3f);
		pose.update(frame);
		const auto wornEyes = pose.viewpoint();
		if ((wornEyes - Vector3(0.2f, 1.8f, 0.3f)).length() > 0.00001f)
			throw std::runtime_error("Worn headset did not recalibrate the eye position");
		std::cout << "Standby pose freeze and wear calibration passed\n";
		{
			avatar::VrmDocument trackedModel(argv[1]);
			avatar::VrmPose trackedPose(trackedModel);
			avatar::TrackingFrame trackedFrame;
			trackedFrame.deltaTime = 1.0f / 90;
			trackedFrame.head.valid = trackedFrame.leftHand.valid = trackedFrame.rightHand.valid = true;
			trackedFrame.head.matrix.translate(0, 1.65f, 0);
			trackedFrame.leftHand.matrix.translate(-0.25f, 1.3f, -0.25f);
			trackedFrame.rightHand.matrix.translate(0.25f, 1.3f, -0.25f);
			trackedPose.update(trackedFrame);
			trackedFrame.waist.valid = trackedFrame.leftFoot.valid = trackedFrame.rightFoot.valid = true;
			trackedFrame.waist.deviceId = "waist";
			trackedFrame.leftFoot.deviceId = "left";
			trackedFrame.rightFoot.deviceId = "right";
			trackedPose.update(trackedFrame);
			const auto initialHip = trackedBone(trackedModel, trackedPose, "hips");
			const auto initialFoot = trackedBone(trackedModel, trackedPose, "leftFoot");
			trackedFrame.leftFoot.matrix.translate(0, 0.12f, 0.04f);
			trackedFrame.waist.matrix.translate(0.025f, -0.05f, 0);
			trackedFrame.head.matrix[13] -= 0.1f;
			trackedPose.update(trackedFrame);
			for (int settle = 0; settle < 90; ++settle) trackedPose.update(trackedFrame);
			const auto trackedEyes = trackedPose.viewpoint();
			if ((trackedEyes - avatar::position(trackedFrame.head.matrix)).length() > 0.001f)
				throw std::runtime_error("Torso did not reach a reachable measured head pose");
			if ((trackedBone(trackedModel, trackedPose, "hips") - initialHip - Vector3(0.025f, -0.05f, 0)).length() > 0.0001f)
				throw std::runtime_error("Measured waist pose was changed by head IK");
			if ((trackedBone(trackedModel, trackedPose, "leftFoot") - initialFoot - Vector3(0, 0.12f, 0.04f)).length() > 0.001f)
				throw std::runtime_error("Measured raised foot was not followed");
			trackedFrame.leftFoot.valid = false;
			trackedPose.update(trackedFrame);
			if (!std::isfinite(trackedBone(trackedModel, trackedPose, "leftFoot").y))
				throw std::runtime_error("Partial tracker loss produced invalid pose");
			std::cout << "Waist, raised foot and partial tracker loss passed\n";
		}
		{
			avatar::VrmDocument plantedModel(argv[1]);
			avatar::VrmPose plantedPose(plantedModel);
			avatar::TrackingFrame plantedFrame;
			plantedFrame.deltaTime = 1.0f / 90;
			plantedFrame.head.valid = plantedFrame.leftHand.valid = plantedFrame.rightHand.valid = true;
			plantedFrame.head.matrix.translate(0, 1.65f, 0);
			plantedFrame.leftHand.matrix.translate(-0.25f, 1.3f, -0.25f);
			plantedFrame.rightHand.matrix.translate(0.25f, 1.3f, -0.25f);
			plantedPose.update(plantedFrame);
			const auto plantedFoot = trackedBone(plantedModel, plantedPose, "leftFoot");
			float slip = 0;
			for (int step = 0; step < 180; ++step)
			{
				const float phase = step * 3.14159265f / 180;
				plantedFrame.head.matrix[12] = std::sin(phase * 6) * 0.02f;
				plantedFrame.head.matrix[13] = 1.65f - std::sin(phase) * 0.8f;
				plantedPose.update(plantedFrame);
				slip = std::max(slip, (trackedBone(plantedModel, plantedPose, "leftFoot") - plantedFoot).length());
				if (trackedBone(plantedModel, plantedPose, "hips").y < 0)
					throw std::runtime_error("Crouching put hips under the floor");
			}
			std::cout << "Crouch and lean maxFootSlip=" << slip << '\n';
			if (slip > 0.005f) throw std::runtime_error("Crouch or head lean slid the planted foot");
			const auto beforeRebase = trackedBone(plantedModel, plantedPose, "leftFoot");
			plantedFrame.headsetInUse = false;
			plantedFrame.hasTrackingSpaceDelta = true;
			plantedFrame.trackingSpaceDelta.translate(1, 0, 0);
			plantedPose.update(plantedFrame);
			plantedFrame.headsetInUse = true;
			plantedFrame.hasTrackingSpaceDelta = false;
			plantedFrame.head.matrix[12] += 1;
			plantedFrame.leftHand.matrix[12] += 1;
			plantedFrame.rightHand.matrix[12] += 1;
			plantedPose.update(plantedFrame);
			if ((trackedBone(plantedModel, plantedPose, "leftFoot") - beforeRebase - Vector3(1, 0, 0)).length() > 0.005f)
				throw std::runtime_error("Standby lost a tracking-space change");
		}
		for (const std::string sample : {"front", "side", "up", "chest", "cross"})
		{
			avatar::VrmDocument sampleModel(argv[1]);
			avatar::VrmPose samplePose(sampleModel);
			Vector3 previousElbows[2], firstElbows[2];
			Matrix4 previousForearmFrames[2];
			float mirrorError = 0, wristError = 0, elbowStep = 0, forearmDegrees = 0;
			for (int sampleStep = 0; sampleStep <= 120; ++sampleStep)
			{
				const float roll = -120.0f + sampleStep * 2;
				avatar::TrackingFrame sampleFrame;
				sampleFrame.deltaTime = 1.0f / 60;
				sampleFrame.head.valid = sampleFrame.leftHand.valid = sampleFrame.rightHand.valid = true;
				sampleFrame.head.matrix.translate(0, 1.65f, 0);
				for (const std::string side : {"left", "right"})
				{
					const float sign = side == "left" ? -1.0f : 1.0f;
					Vector3 relative(sign * 0.25f, -0.35f, -0.25f), forward(0, 0, -1), up(0, 1, 0);
					if (sample == "side") { relative = Vector3(sign * 0.55f, -0.25f, 0); forward = Vector3(sign, 0, 0); }
					if (sample == "up") { relative = Vector3(sign * 0.2f, 0.1f, -0.12f); forward = Vector3(0, 1, 0); up = Vector3(0, 0, -1); }
					if (sample == "chest") relative = Vector3(sign * 0.1f, -0.3f, -0.2f);
					if (sample == "cross") relative = Vector3(-sign * 0.1f, -0.3f, -0.3f);
					Matrix4 handRoll, turn;
					handRoll.rotateZ(roll * -sign);
					turn.rotateY(180);
					auto &hand = side == "left" ? sampleFrame.leftHand : sampleFrame.rightHand;
					hand.matrix = avatar::handFrame(forward, up) * handRoll * turn;
					hand.matrix[12] = relative.x;
					hand.matrix[13] = relative.y + 1.65f;
					hand.matrix[14] = relative.z;
				}
				samplePose.update(sampleFrame);
				const auto left = trackedBone(sampleModel, samplePose, "leftLowerArm");
				const auto right = trackedBone(sampleModel, samplePose, "rightLowerArm");
				mirrorError = std::max(mirrorError, (Vector3(-left.x, left.y, left.z) - right).length());
				if (left.x > 0 || right.x < 0) throw std::runtime_error("Elbow crossed the torso center in reference poses");
				int sideNumber = 0;
				for (const std::string side : {"left", "right"})
				{
					const auto elbow = side == "left" ? left : right;
					const auto &hand = side == "left" ? sampleFrame.leftHand.matrix : sampleFrame.rightHand.matrix;
					wristError = std::max(wristError, (trackedBone(sampleModel, samplePose, side + "Hand")
						- Vector3(hand[12], hand[13], hand[14])).length());
					if (sampleStep > 0) elbowStep = std::max(elbowStep, (elbow - previousElbows[sideNumber]).length());
					else firstElbows[sideNumber] = elbow;
					const auto lower = avatar::orientation(avatar::world(sampleModel.scene().nodes[sampleModel.humanBones().at(side + "LowerArm")]));
					if (sampleStep > 0)
					{
						const auto delta = Matrix4(previousForearmFrames[sideNumber]).invert() * lower;
						const float cosine = std::max(-1.0f, std::min(1.0f, (delta[0] + delta[5] + delta[10] - 1) * 0.5f));
						forearmDegrees = std::max(forearmDegrees, std::acos(cosine) * 180.0f / 3.141592654f);
					}
					previousForearmFrames[sideNumber] = lower;
					previousElbows[sideNumber++] = elbow;
				}
			}
			const float wristRollResponse = (previousElbows[0] - firstElbows[0]).length();
			std::cout << sample << " mirrorError=" << mirrorError << " wristError=" << wristError
				<< " elbowStep=" << elbowStep << " forearmStepDegrees=" << forearmDegrees << " wristRollResponse=" << wristRollResponse << '\n';
			if (mirrorError > 0.0001f || wristError > 0.001f || elbowStep > 0.01f || forearmDegrees > 5)
				throw std::runtime_error("Arm reference sweep failed");
		}
	}
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
	try { auditGarment(argv[1]); }
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}

