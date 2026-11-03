#include "VrmDocument.h"
#include "VrmPose.h"
#include "VrmGeometry.h"
#include "VrmSkinning.h"
#include "VrmTransforms.h"
#include "Spring/SpringRuntime.h"
#include "cgltf.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <chrono>

void auditGarment(const char *path)
{
    avatar::VrmDocument model(path);
    auto geometry = avatar::readGeometry(model.scene());
    geometry.erase(std::remove_if(geometry.begin(), geometry.end(), [&model](const avatar::Primitive &primitive) {
        const auto *skin = model.scene().nodes[primitive.node].skin;
        if (!skin) return true;
        for (const auto &vertex : primitive.vertices) for (int i = 0; i < 4; ++i)
            if (vertex.weights[i] > 0 && skin->joints[vertex.joints[i]]->name
                && std::strncmp(skin->joints[vertex.joints[i]]->name, "ClothSpring_", 12) == 0) return false;
        return true;
    }), geometry.end());
    if (geometry.empty()) return;
    auto definition = avatar::readSpringDefinition(model.scene());
    definition.colliders.erase(std::remove_if(definition.colliders.begin(), definition.colliders.end(), [&model](const avatar::SpringCollider &collider) {
        if (collider.radius >= 0.3f) return true;
        for (const auto *name : {"leftUpperLeg", "leftLowerLeg", "rightUpperLeg", "rightLowerLeg"})
            for (auto *node = &model.scene().nodes[collider.node]; node; node = node->parent)
                if (node == &model.scene().nodes[model.humanBones().at(name)]) return false;
        return true;
    }), definition.colliders.end());
    avatar::VrmPose pose(model);
    avatar::SpringRuntime springs(model);
    avatar::TrackingFrame frame;
    frame.deltaTime = 1.0f / 60;
    frame.head.valid = frame.leftHand.valid = frame.rightHand.valid = true;
    frame.head.matrix.translate(0, 1.65f, 0);
    pose.update(frame);
    const auto trackedRest = [&](const char *name) {
        const auto matrix = pose.modelToTracking() * avatar::world(model.scene().nodes[model.humanBones().at(name)]);
        auto rigid = avatar::orientation(matrix);
        rigid[12] = matrix[12]; rigid[13] = matrix[13]; rigid[14] = matrix[14];
        return rigid;
    };
    const auto waistRest = trackedRest("hips"), leftFootRest = trackedRest("leftFoot"), rightFootRest = trackedRest("rightFoot");
    const float scale = Vector3(pose.modelToTracking()[0], pose.modelToTracking()[1], pose.modelToTracking()[2]).length();
    float penetration = 0;
    double springMilliseconds = 0;
    int worstFrame = 0;
    const char *worstBone = "", *worstCollider = "";
    for (int step = 0; step < 480; ++step)
    {
        const float crouch = step < 60 || step >= 240 ? 0 : step < 120 ? (step - 60) / 60.0f : step < 180 ? 1 : (240 - step) / 60.0f;
        frame.head.matrix[13] = 1.65f - crouch * 0.4f * scale;
        frame.leftHand.matrix.identity().translate(-0.25f, frame.head.matrix[13] - 0.35f, -0.25f);
        frame.rightHand.matrix.identity().translate(0.25f, frame.head.matrix[13] - 0.35f, -0.25f);
        if (step >= 240)
        {
            if (step == 240) springs.reset();
            frame.waist.valid = frame.leftFoot.valid = frame.rightFoot.valid = true;
            frame.waist.matrix = waistRest;
            frame.leftFoot.matrix = leftFootRest;
            frame.rightFoot.matrix = rightFootRest;
            const float lift = step < 300 ? 0 : step < 360 ? (step - 300) / 60.0f : step < 420 ? 1 : (480 - step) / 60.0f;
            frame.leftFoot.matrix[13] += lift * 0.25f * scale;
            frame.leftFoot.matrix[14] -= lift * 0.28f * scale;
        }
        pose.update(frame);
        const auto springStart = std::chrono::steady_clock::now();
        springs.update(frame.deltaTime, pose.modelToTracking());
        springMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - springStart).count();
        for (const auto &primitive : geometry)
        {
            const auto points = avatar::skinPositions(model.scene(), primitive);
            for (std::size_t vertex = 0; vertex < points.size(); ++vertex)
            {
                const auto &point = points[vertex];
                for (const auto &collider : definition.colliders)
                {
                    const auto transform = avatar::world(model.scene().nodes[collider.node]);
                    const auto start = avatar::point(transform, collider.offset);
                    const auto end = avatar::point(transform, collider.tail);
                    const auto axis = end - start;
                    const float fraction = axis.dot(axis) > 0 ? std::max(0.0f, std::min(1.0f, (point - start).dot(axis) / axis.dot(axis))) : 0;
                    const auto nearest = start + axis * fraction;
                    const float radiusScale = std::max(Vector3(transform[0], transform[1], transform[2]).length(),
                        std::max(Vector3(transform[4], transform[5], transform[6]).length(), Vector3(transform[8], transform[9], transform[10]).length()));
                    const float depth = collider.radius * radiusScale - (point - nearest).length();
                    if (depth > penetration)
                    {
                        penetration = depth;
                        worstFrame = step;
                        const auto *skin = model.scene().nodes[primitive.node].skin;
                        worstBone = skin->joints[primitive.vertices[vertex].joints[0]]->name;
                        worstCollider = model.scene().nodes[collider.node].name;
                    }
                }
            }
        }
    }
    std::cout << "Garment crouch and tracked knee lift audit frames=480 maxPenetration=" << penetration
        << " meanSpringMs=" << springMilliseconds / 480 << " worstFrame=" << worstFrame
        << " worstBone=" << (worstBone ? worstBone : "") << " collider=" << (worstCollider ? worstCollider : "") << '\n';
    if (penetration > 0.005f) throw std::runtime_error("Garment penetrates leg colliders in the McOsu runtime");
}
