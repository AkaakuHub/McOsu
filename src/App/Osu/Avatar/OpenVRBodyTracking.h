#ifndef MCOSU_OPENVR_BODY_TRACKING_H
#define MCOSU_OPENVR_BODY_TRACKING_H

#include "TrackingTypes.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace avatar
{
enum class BodyTrackerRole { Waist, LeftFoot, RightFoot, Unassigned };
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
    TrackedPose waist, leftFoot, rightFoot;
    bool hasTrackingSpaceDelta = false;
    Matrix4 trackingSpaceDelta;
    std::array<BodyTrackingStatus, 3> status = {{BodyTrackingStatus::Unassigned, BodyTrackingStatus::Unassigned, BodyTrackingStatus::Unassigned}};
};
BodyTrackerRole bodyTrackerRole(const std::string &controllerType);
bool bodyTrackerPoseUsable(bool connected, bool poseValid, bool runningNormally, const Matrix4 &matrix);
BodyTrackingFrame assignBodyTrackers(const std::vector<BodyTrackerDevice> &devices, const std::array<std::string, 3> &bindings);

class OpenVRBodyTracking
{
public:
    OpenVRBodyTracking();
    ~OpenVRBodyTracking();
    void setBindings(const std::string &waistSerial, const std::string &leftFootSerial, const std::string &rightFootSerial);
    BodyTrackingFrame sample(float predictionSeconds = 0);
    const std::vector<BodyTrackerDevice> &devices() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
