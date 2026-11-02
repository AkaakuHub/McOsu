#ifndef MCOSU_TRACKING_TYPES_H
#define MCOSU_TRACKING_TYPES_H

#include "Matrices.h"
#include <string>

namespace avatar
{
struct TrackedPose { bool valid = false; Matrix4 matrix; bool palmSurface = false; std::string deviceId; };
struct TrackingFrame
{
    TrackedPose head, leftHand, rightHand, waist, leftFoot, rightFoot;
    bool headsetInUse = true;
    float deltaTime = 0;
    bool hasTrackingSpaceDelta = false;
    Matrix4 trackingSpaceDelta;
};
}

#endif
