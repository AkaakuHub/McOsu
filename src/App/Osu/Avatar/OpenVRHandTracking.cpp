#include "OpenVRHandTracking.h"
#include "OpenVRCapi.h"
#include <array>
#include <cmath>
#include <string>

namespace avatar
{
namespace
{
Matrix4 convert(const HmdMatrix34_t &source)
{
    return Matrix4(source.m[0][0], source.m[1][0], source.m[2][0], 0,
        source.m[0][1], source.m[1][1], source.m[2][1], 0,
        source.m[0][2], source.m[1][2], source.m[2][2], 0,
        source.m[0][3], source.m[1][3], source.m[2][3], 1);
}

bool canonicalPalm(HandSide side, const Matrix4 &component, Matrix4 &output)
{
    for (int index = 0; index < 16; ++index)
        if (!std::isfinite(component[index])) return false;
    auto back = Vector3(component[8], component[9], component[10]);
    auto normal = Vector3(component[0], component[1], component[2]) * (side == HandSide::Left ? -1.0f : 1.0f);
    if (back.length() < 0.000001f || normal.length() < 0.000001f) return false;
    back.normalize();
    auto right = normal.cross(back);
    if (right.length() < 0.000001f) return false;
    right.normalize();
    normal = back.cross(right).normalize();
    output = Matrix4(right.x, right.y, right.z, 0, normal.x, normal.y, normal.z, 0,
        back.x, back.y, back.z, 0, component[12], component[13], component[14], 1);
    return true;
}
}

struct OpenVRHandTracking::Impl
{
    VR_IVRSystem_FnTable *system = nullptr;
    VR_IVRRenderModels_FnTable *models = nullptr;
    VR_IVRInput_FnTable *input = nullptr;
    std::array<HandTrackingStatus, 2> status = {{HandTrackingStatus::InterfaceUnavailable, HandTrackingStatus::InterfaceUnavailable}};
};

OpenVRHandTracking::OpenVRHandTracking() : m_impl(new Impl())
{
    EVRInitError error = EVRInitError_VRInitError_None;
    m_impl->system = reinterpret_cast<VR_IVRSystem_FnTable *>(VR_GetGenericInterface("FnTable:IVRSystem_026", &error));
    if (error != EVRInitError_VRInitError_None) m_impl->system = nullptr;
    m_impl->models = reinterpret_cast<VR_IVRRenderModels_FnTable *>(VR_GetGenericInterface("FnTable:IVRRenderModels_006", &error));
    if (error != EVRInitError_VRInitError_None) m_impl->models = nullptr;
    m_impl->input = reinterpret_cast<VR_IVRInput_FnTable *>(VR_GetGenericInterface("FnTable:IVRInput_011", &error));
    if (error != EVRInitError_VRInitError_None) m_impl->input = nullptr;
}

OpenVRHandTracking::~OpenVRHandTracking() = default;

HandTrackingStatus OpenVRHandTracking::status(HandSide side) const
{
    return m_impl->status[side == HandSide::Left ? 0 : 1];
}

bool OpenVRHandTracking::tryGetPalmPose(HandSide side, const Matrix4 &deviceToTracking, Matrix4 &palmToTracking)
{
    auto &state = m_impl->status[side == HandSide::Left ? 0 : 1];
    if (!m_impl->system || !m_impl->models || !m_impl->input)
    {
        state = HandTrackingStatus::InterfaceUnavailable;
        return false;
    }
    const auto device = m_impl->system->GetTrackedDeviceIndexForControllerRole(side == HandSide::Left
        ? ETrackedControllerRole_TrackedControllerRole_LeftHand : ETrackedControllerRole_TrackedControllerRole_RightHand);
    if (device >= 64 || !m_impl->system->IsTrackedDeviceConnected(device))
    {
        state = HandTrackingStatus::DeviceDisconnected;
        return false;
    }
    std::array<char, 2048> model = {};
    ETrackedPropertyError propertyError;
    m_impl->system->GetStringTrackedDeviceProperty(device, ETrackedDeviceProperty_Prop_RenderModelName_String,
        model.data(), model.size(), &propertyError);
    VRInputValueHandle_t source = 0;
    std::string sourcePath = side == HandSide::Left ? "/user/hand/left" : "/user/hand/right";
    const auto inputError = m_impl->input->GetInputSourceHandle(&sourcePath[0], &source);
    std::string componentName("openxr_handmodel");
    RenderModel_ControllerMode_State_t mode = {};
    RenderModel_ComponentState_t component = {};
    if (propertyError != ETrackedPropertyError_TrackedProp_Success || inputError != EVRInputError_VRInputError_None
        || !m_impl->models->GetComponentStateForDevicePath(model.data(), &componentName[0], source, &mode, &component))
    {
        state = HandTrackingStatus::ComponentUnavailable;
        return false;
    }
    Matrix4 canonical;
    if (!canonicalPalm(side, deviceToTracking * convert(component.mTrackingToComponentLocal), canonical))
    {
        state = HandTrackingStatus::InvalidTransform;
        return false;
    }
    palmToTracking = canonical;
    state = HandTrackingStatus::Available;
    return true;
}
}
