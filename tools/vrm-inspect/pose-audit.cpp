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
		frame.head.valid = frame.leftHand.valid = frame.rightHand.valid = true;
		frame.head.matrix.translate(0, 1.65f, 0);
		pose.update(frame);
		const float avatarScale = Vector3(pose.modelToTracking()[0], pose.modelToTracking()[1], pose.modelToTracking()[2]).length();
		const auto calibrationOrigin = pose.modelToTracking() * Vector4(0, 0, 0, 1);
		if (std::abs(calibrationOrigin.y) > 0.00001f) throw std::runtime_error("Avatar floor differs from tracking floor");
		float maxEyeError = 0, maxHandError = 0, maxHandDirectionError = 0, maxForearmStep = 0;
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
			const auto eyes = (trackedBone(model, pose, "leftEye") + trackedBone(model, pose, "rightEye")) * 0.5f;
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
		if (maxEyeError > 0.0001f || maxHandError > 0.001f || maxHandDirectionError > 0.001f || maxForearmStep > 20)
			throw std::runtime_error("Tracking targets were missed");
		const auto beforeStandby = trackedBone(model, pose, "head");
		frame.headsetInUse = false;
		frame.head.matrix.identity().translate(0.4f, 0.6f, 0.2f);
		pose.update(frame);
		if ((trackedBone(model, pose, "head") - beforeStandby).length() > 0.00001f)
			throw std::runtime_error("Standby headset moved the avatar");
		frame.headsetInUse = true;
		frame.head.matrix.identity().translate(0.2f, 1.8f, 0.3f);
		pose.update(frame);
		const auto wornEyes = (trackedBone(model, pose, "leftEye") + trackedBone(model, pose, "rightEye")) * 0.5f;
		if ((wornEyes - Vector3(0.2f, 1.8f, 0.3f)).length() > 0.00001f)
			throw std::runtime_error("Worn headset did not recalibrate the eye position");
		std::cout << "Standby pose freeze and wear calibration passed\n";
	}
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
