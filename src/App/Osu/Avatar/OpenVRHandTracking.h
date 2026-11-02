#ifndef MCOSU_OPENVR_HAND_TRACKING_H
#define MCOSU_OPENVR_HAND_TRACKING_H

#include "Matrices.h"
#include <memory>

namespace avatar
{
enum class HandSide { Left, Right };
enum class HandTrackingStatus { Available, InterfaceUnavailable, DeviceDisconnected, ComponentUnavailable, InvalidTransform };

class OpenVRHandTracking
{
public:
    OpenVRHandTracking();
    ~OpenVRHandTracking();
    bool tryGetPalmPose(HandSide side, const Matrix4 &deviceToTracking, Matrix4 &palmToTracking);
    HandTrackingStatus status(HandSide side) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}

#endif
