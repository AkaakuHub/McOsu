#ifndef MCOSU_OPENVR_BODY_TRACKING_H
#define MCOSU_OPENVR_BODY_TRACKING_H

#include "TrackingTypes.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace avatar
{
enum class BodyTrackerRole { Waist, LeftFoot, RightFoot, LeftElbow, RightElbow, Unassigned };
enum class BodyTrackingStatus { Available, InterfaceUnavailable, Unassigned, Missing, Ambiguous, InvalidPose };
struct BodyTrackerDevice
{
    unsigned int index = 0;
    std::string serial, controllerType;
    bool connected = false;
    TrackedPose pose;
};
struct BodyTrackingFrame
{
    TrackedPose waist, leftFoot, rightFoot, leftElbow, rightElbow;
    bool hasTrackingSpaceDelta = false;
    Matrix4 trackingSpaceDelta;
    std::array<BodyTrackingStatus, 5> status = {{BodyTrackingStatus::Unassigned, BodyTrackingStatus::Unassigned, BodyTrackingStatus::Unassigned, BodyTrackingStatus::Unassigned, BodyTrackingStatus::Unassigned}};
};
BodyTrackerRole bodyTrackerRole(const std::string &controllerType);
bool bodyTrackerPoseUsable(bool connected, bool poseValid, bool runningNormally, const Matrix4 &matrix);
BodyTrackingFrame assignBodyTrackers(const std::vector<BodyTrackerDevice> &devices, const std::array<std::string, 5> &bindings);

class OpenVRBodyTracking
{
public:
    OpenVRBodyTracking();
    ~OpenVRBodyTracking();
    void setBindings(const std::array<std::string, 5> &bindings);
    BodyTrackingFrame sample();
    const std::vector<BodyTrackerDevice> &devices() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
