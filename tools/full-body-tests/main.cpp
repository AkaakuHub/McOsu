#include "VrmDocument.h"
#include "VrmPose.h"
#include "VrmTransforms.h"
#include "cgltf.h"
#include <iostream>
#include <stdexcept>
#include <cmath>

using namespace avatar;

Vector3 joint(VrmDocument &model, const VrmPose &pose, const std::string &name)
{
    return point(pose.modelToTracking(), position(world(model.scene().nodes[model.humanBones().at(name)])));
}

void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    try
    {
        VrmDocument model(argv[1]);
        VrmPose pose(model);
        TrackingFrame frame;
        frame.deltaTime = 1.0f / 90;
        frame.head.valid = frame.leftHand.valid = frame.rightHand.valid = true;
        frame.waist.valid = frame.leftFoot.valid = frame.rightFoot.valid = true;
        frame.leftElbow.valid = frame.rightElbow.valid = true;
        const auto rest = [&](const char *name) { return position(world(model.scene().nodes[model.humanBones().at(name)])); };
        const auto eyes = (rest("leftEye") + rest("rightEye")) * 0.5f;
        const auto restHeadPosition = rest("head");
        const float scale = 1.6f / eyes.y;
        const Vector3 offset(0, -0.75f, 0);
        const auto input = [&](const char *name, TrackedPose &target) {
            const auto p = rest(name) * scale + offset;
            target.matrix.translate(p.x, p.y, p.z);
            target.deviceId = name;
        };
        frame.head.matrix.translate(0, 0.85f, 0);
        input("hips", frame.waist);
        input("leftFoot", frame.leftFoot);
        input("rightFoot", frame.rightFoot);
        frame.leftHand.matrix.translate(-0.28f, 0.5f, -0.28f);
        frame.rightHand.matrix.translate(0.28f, 0.5f, -0.28f);
        frame.leftElbow.matrix.translate(-0.4f, 0.38f, -0.05f);
        frame.rightElbow.matrix.translate(0.4f, 0.38f, -0.05f);
        const float upperLength = (rest("leftLowerArm") - rest("leftUpperArm")).length() * scale;
        const float lowerLength = (rest("leftHand") - rest("leftLowerArm")).length() * scale;
        pose.update(frame);
        check((joint(model, pose, "head") - point(pose.modelToTracking(), restHeadPosition)).length() < 0.001f,
            "Upright calibration bent the spine to compensate for the model eye offset");
        const auto calibratedFoot = joint(model, pose, "leftFoot");
        check(std::abs(pose.modelToTracking()[13] + 0.75f) < 0.0001f, "Measured foot height did not correct floor");
        check((joint(model, pose, "leftHand") - position(frame.leftHand.matrix)).length() < 0.001f, "Tracked elbow broke wrist target");
        check(std::abs((joint(model, pose, "leftLowerArm") - joint(model, pose, "leftUpperArm")).length() - upperLength) < 0.001f, "Tracker stretched upper arm");
        check(std::abs((joint(model, pose, "leftHand") - joint(model, pose, "leftLowerArm")).length() - lowerLength) < 0.001f, "Tracker stretched forearm");
        const auto shoulder = joint(model, pose, "leftUpperArm");
        const auto axis = (joint(model, pose, "leftHand") - shoulder).normalize();
        auto measured = position(frame.leftElbow.matrix) - shoulder;
        measured = (measured - axis * measured.dot(axis)).normalize();
        auto solved = joint(model, pose, "leftLowerArm") - shoulder;
        solved = (solved - axis * solved.dot(axis)).normalize();
        check(measured.dot(solved) > 0.999f, "Elbow bend plane ignored measured tracker");
        frame.leftElbow.valid = false;
        pose.update(frame);
        check(std::isfinite(joint(model, pose, "leftLowerArm").y), "Elbow loss produced invalid pose");
        const auto beforeWaist = joint(model, pose, "hips");
        frame.waist.matrix[12] += 0.05f;
        pose.update(frame);
        const float firstWaistStep = joint(model, pose, "hips").x - beforeWaist.x;
        check(firstWaistStep > 0 && firstWaistStep < 0.05f, "Waist noise filter did not interpolate");
        for (int step = 0; step < 90; ++step) pose.update(frame);
        check(std::abs(joint(model, pose, "hips").x - beforeWaist.x - 0.05f) < 0.0001f, "Waist filter drifted away from measured pose");
        frame.leftFoot.matrix[13] += 0.15f;
        frame.leftFoot.matrix[14] -= 0.08f;
        pose.update(frame);
        const auto ankle = joint(model, pose, "leftFoot");
        check((ankle - calibratedFoot - Vector3(0, 0.15f, -0.08f)).length() < 0.001f, "Measured knee lift lost foot position");
        const auto hip = joint(model, pose, "leftUpperLeg");
        const auto knee = joint(model, pose, "leftLowerLeg");
        const auto legAxis = (ankle - hip).normalize();
        const auto kneeOffset = knee - hip - legAxis * (knee - hip).dot(legAxis);
        check(kneeOffset.z < 0, "Knee bent behind the tracked body");
        {
            VrmDocument acquiredModel(argv[1]);
            VrmPose acquiredPose(acquiredModel);
            TrackingFrame acquired;
            acquired.deltaTime = 1.0f / 90;
            acquired.head.valid = acquired.leftHand.valid = acquired.rightHand.valid = true;
            acquired.head.matrix.translate(0.03f, 1.6f, 0);
            acquired.leftHand.matrix.translate(-0.25f, 1.3f, -0.25f);
            acquired.rightHand.matrix.translate(0.25f, 1.3f, -0.25f);
            for (int step = 0; step < 90; ++step) acquiredPose.update(acquired);
            const auto previousPelvis = joint(acquiredModel, acquiredPose, "hips");
            acquired.waist.valid = true;
            acquired.waist.deviceId = "new-waist";
            acquired.waist.matrix.translate(0.1f, 1, 0);
            acquiredPose.update(acquired);
            check((joint(acquiredModel, acquiredPose, "hips") - previousPelvis).length() < 0.0001f,
                "New waist tracker snapped the current body to the rest pose");
        }
        std::cout << "Nonzero floor, measured elbow plane, wrist target and elbow loss passed\n";
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
