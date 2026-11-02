#include "OpenVRBodyTracking.h"
#include "OpenVRCapi.h"
#include <cmath>

namespace avatar
{
namespace
{
Matrix4 convert(const HmdMatrix34_t &m)
{
    return Matrix4(m.m[0][0], m.m[1][0], m.m[2][0], 0, m.m[0][1], m.m[1][1], m.m[2][1], 0,
        m.m[0][2], m.m[1][2], m.m[2][2], 0, m.m[0][3], m.m[1][3], m.m[2][3], 1);
}
}

bool bodyTrackerPoseUsable(bool connected, bool poseValid, bool runningNormally, const Matrix4 &matrix)
{
    if (!connected || !poseValid || !runningNormally) return false;
    for (int index = 0; index < 16; ++index)
        if (!std::isfinite(matrix[index])) return false;
    return true;
}

BodyTrackerRole bodyTrackerRole(const std::string &type)
{
    if (type == "vive_tracker_waist") return BodyTrackerRole::Waist;
    if (type == "vive_tracker_left_foot") return BodyTrackerRole::LeftFoot;
    if (type == "vive_tracker_right_foot") return BodyTrackerRole::RightFoot;
    return BodyTrackerRole::Unassigned;
}

BodyTrackingFrame assignBodyTrackers(const std::vector<BodyTrackerDevice> &devices, const std::array<std::string, 3> &bindings)
{
    BodyTrackingFrame result;
    std::array<TrackedPose *, 3> poses = {{&result.waist, &result.leftFoot, &result.rightFoot}};
    for (unsigned int role = 0; role < 3; ++role)
    {
        bool duplicate = false;
        for (unsigned int other = 0; other < 3; ++other)
            if (other != role && !bindings[role].empty() && bindings[role] == bindings[other]) duplicate = true;
        unsigned int count = 0;
        const BodyTrackerDevice *selected = nullptr;
        for (const auto &device : devices)
        {
            bool match = bindings[role].empty() ? static_cast<unsigned int>(bodyTrackerRole(device.controllerType)) == role : device.serial == bindings[role];
            if (match)
            {
                for (unsigned int other = 0; other < 3; ++other)
                    if (other != role && !bindings[other].empty() && bindings[other] == device.serial) match = false;
            }
            if (match) { ++count; selected = &device; }
        }
        if (duplicate || count > 1) result.status[role] = BodyTrackingStatus::Ambiguous;
        else if (!selected) result.status[role] = bindings[role].empty() ? BodyTrackingStatus::Unassigned : BodyTrackingStatus::Missing;
        else if (!selected->connected) result.status[role] = BodyTrackingStatus::Missing;
        else if (!selected->pose.valid) result.status[role] = BodyTrackingStatus::InvalidPose;
        else { *poses[role] = selected->pose; result.status[role] = BodyTrackingStatus::Available; }
    }
    return result;
}

struct OpenVRBodyTracking::Impl
{
    VR_IVRSystem_FnTable *system = nullptr;
    std::array<std::string, 3> bindings;
    std::vector<BodyTrackerDevice> devices;
    Matrix4 rawToStanding;
    bool hasRawToStanding = false;
};

OpenVRBodyTracking::OpenVRBodyTracking() : m_impl(new Impl()) {}
OpenVRBodyTracking::~OpenVRBodyTracking() = default;
void OpenVRBodyTracking::setBindings(const std::string &waist, const std::string &left, const std::string &right)
{
    m_impl->bindings = {{waist, left, right}};
}
const std::vector<BodyTrackerDevice> &OpenVRBodyTracking::devices() const { return m_impl->devices; }

BodyTrackingFrame OpenVRBodyTracking::sample(float predictionSeconds)
{
    m_impl->devices.clear();
    const auto previousSystem = m_impl->system;
    EVRInitError error = EVRInitError_VRInitError_None;
    m_impl->system = reinterpret_cast<VR_IVRSystem_FnTable *>(VR_GetGenericInterface("FnTable:IVRSystem_026", &error));
    if (error != EVRInitError_VRInitError_None || !m_impl->system)
    {
        m_impl->system = nullptr;
        m_impl->hasRawToStanding = false;
        BodyTrackingFrame unavailable;
        unavailable.status.fill(BodyTrackingStatus::InterfaceUnavailable);
        return unavailable;
    }
    if (m_impl->system != previousSystem) m_impl->hasRawToStanding = false;
    Matrix4 rawToStanding = convert(m_impl->system->GetRawZeroPoseToStandingAbsoluteTrackingPose());
    const bool validOrigin = bodyTrackerPoseUsable(true, true, true, rawToStanding)
        && std::fabs(rawToStanding.getDeterminant()) > 0.000001f;
    Matrix4 delta;
    bool hasDelta = false;
    if (validOrigin && m_impl->hasRawToStanding)
    {
        delta = rawToStanding * m_impl->rawToStanding.invert();
        for (int index = 0; index < 16; ++index)
            if (std::fabs(delta[index] - (index % 5 == 0 ? 1.0f : 0.0f)) > 0.000001f) hasDelta = true;
        hasDelta = hasDelta && bodyTrackerPoseUsable(true, true, true, delta);
    }
    m_impl->hasRawToStanding = validOrigin;
    if (validOrigin) m_impl->rawToStanding = rawToStanding;
    std::array<TrackedDevicePose_t, 64> poses = {};
    m_impl->system->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding,
        std::isfinite(predictionSeconds) ? predictionSeconds : 0, poses.data(), poses.size());
    for (unsigned int index = 0; index < poses.size(); ++index)
    {
        if (m_impl->system->GetTrackedDeviceClass(index) != ETrackedDeviceClass_TrackedDeviceClass_GenericTracker) continue;
        BodyTrackerDevice device;
        device.index = index;
        device.connected = poses[index].bDeviceIsConnected;
        auto property = [&](ETrackedDeviceProperty key) {
            std::array<char, 1024> value = {};
            ETrackedPropertyError propertyError;
            m_impl->system->GetStringTrackedDeviceProperty(index, key, value.data(), value.size(), &propertyError);
            return propertyError == ETrackedPropertyError_TrackedProp_Success ? std::string(value.data()) : std::string();
        };
        device.serial = property(ETrackedDeviceProperty_Prop_SerialNumber_String);
        device.pose.deviceId = device.serial;
        device.controllerType = property(ETrackedDeviceProperty_Prop_ControllerType_String);
        const auto &pose = poses[index];
        const auto &m = pose.mDeviceToAbsoluteTracking;
        const Matrix4 matrix = convert(m);
        device.pose.valid = bodyTrackerPoseUsable(device.connected, pose.bPoseIsValid,
            pose.eTrackingResult == ETrackingResult_TrackingResult_Running_OK, matrix);
        if (device.pose.valid) device.pose.matrix = matrix;
        m_impl->devices.push_back(device);
    }
    auto result = assignBodyTrackers(m_impl->devices, m_impl->bindings);
    result.hasTrackingSpaceDelta = hasDelta;
    if (hasDelta) result.trackingSpaceDelta = delta;
    return result;
}
}
