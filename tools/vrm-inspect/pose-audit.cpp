#include "VrmDocument.h"
#include "VrmPose.h"
#include "VrmGeometry.h"
#include "VrmSkinning.h"
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
		avatar::TrackingFrame frame;
		frame.head.valid = frame.leftHand.valid = frame.rightHand.valid = true;
		frame.head.matrix.translate(0, 1.65f, 0);
		pose.update(frame);
		float maxEyeError = 0, maxHandError = 0;
		for (int step = 0; step < 90; ++step)
		{
			const float phase = step * 0.02f;
			Matrix4 yaw, tilt, location;
			yaw.rotateY(phase * 20);
			tilt.rotateX(std::sin(phase) * 15);
			location.translate(std::sin(phase) * 0.1f, 1.65f - std::sin(phase) * 0.15f, 0.1f);
			frame.head.matrix = location * yaw * tilt;
			for (auto *hand : {&frame.leftHand, &frame.rightHand})
			{
				Matrix4 relative;
				relative.translate(hand == &frame.leftHand ? -0.25f : 0.25f, -0.35f, -0.25f);
				hand->matrix = location * yaw * relative;
			}
			pose.update(frame);
			const auto eyes = (trackedBone(model, pose, "leftEye") + trackedBone(model, pose, "rightEye")) * 0.5f;
			const auto &head = frame.head.matrix;
			maxEyeError = std::max(maxEyeError, (eyes - Vector3(head[12], head[13], head[14])).length());
			for (const std::string side : {"left", "right"})
			{
				const auto &hand = side == "left" ? frame.leftHand.matrix : frame.rightHand.matrix;
				maxHandError = std::max(maxHandError,
					(trackedBone(model, pose, side + "Hand") - Vector3(hand[12], hand[13], hand[14])).length());
			}
			for (const auto &primitive : avatar::readGeometry(model.scene())) avatar::skinPositions(model.scene(), primitive);
		}
		std::cout << "frames=90 maxEyeError=" << maxEyeError << " maxHandError=" << maxHandError << '\n';
		if (maxEyeError > 0.0001f || maxHandError > 0.001f) throw std::runtime_error("Tracking targets were missed");
	}
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
