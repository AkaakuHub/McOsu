#include "OpenVRCapi.h"
#include "json.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

using nlohmann::json;

S_API intptr_t VR_InitInternal(EVRInitError *error, EVRApplicationType application);
S_API void VR_ShutdownInternal();

std::string property(VR_IVRSystem_FnTable &system, uint32_t device, ETrackedDeviceProperty property)
{
    std::array<char, 2048> text = {};
    ETrackedPropertyError error;
    system.GetStringTrackedDeviceProperty(device, property, text.data(), text.size(), &error);
    return error == ETrackedPropertyError_TrackedProp_Success ? text.data() : "";
}

json matrix(const HmdMatrix34_t &value)
{
    json rows = json::array();
    for (int row = 0; row < 3; ++row) rows.push_back({value.m[row][0], value.m[row][1], value.m[row][2], value.m[row][3]});
    return rows;
}

int main()
{
    EVRInitError error = EVRInitError_VRInitError_None;
    VR_InitInternal(&error, EVRApplicationType_VRApplication_Utility);
    if (error != EVRInitError_VRInitError_None) { std::cerr << "Utility initialization error " << error << '\n'; return 1; }
    try
    {
        auto *system = reinterpret_cast<VR_IVRSystem_FnTable *>(VR_GetGenericInterface("FnTable:IVRSystem_026", &error));
        if (!system || error != EVRInitError_VRInitError_None) throw std::runtime_error("System interface is unavailable");
        auto *models = reinterpret_cast<VR_IVRRenderModels_FnTable *>(VR_GetGenericInterface("FnTable:IVRRenderModels_006", &error));
        if (!models || error != EVRInitError_VRInitError_None) throw std::runtime_error("Render models interface is unavailable");
        auto *input = reinterpret_cast<VR_IVRInput_FnTable *>(VR_GetGenericInterface("FnTable:IVRInput_011", &error));
        if (!input || error != EVRInitError_VRInitError_None) throw std::runtime_error("Input interface is unavailable");
        json output = json::array();
        for (uint32_t device = 0; device < 64; ++device)
        {
            if (!system->IsTrackedDeviceConnected(device)) continue;
            auto model = property(*system, device, ETrackedDeviceProperty_Prop_RenderModelName_String);
            auto role = system->GetControllerRoleForTrackedDeviceIndex(device);
            json item = {{"device", device}, {"class", system->GetTrackedDeviceClass(device)}, {"role", role},
                {"activity", system->GetTrackedDeviceActivityLevel(device)},
                {"model", property(*system, device, ETrackedDeviceProperty_Prop_ModelNumber_String)},
                {"renderModel", model}, {"controllerType", property(*system, device, ETrackedDeviceProperty_Prop_ControllerType_String)},
                {"inputProfile", property(*system, device, ETrackedDeviceProperty_Prop_InputProfilePath_String)}};
            if (device == 0)
            {
                ETrackedPropertyError propertyError;
                item["hasProximity"] = system->GetBoolTrackedDeviceProperty(device,
                    ETrackedDeviceProperty_Prop_ContainsProximitySensor_Bool, &propertyError);
                VRControllerState_t state = {};
                item["proximityReadable"] = system->GetControllerState(device, &state, sizeof(state));
                item["proximityPressed"] = (state.ulButtonPressed & (1ULL << 31)) != 0;
            }
            if (role == ETrackedControllerRole_TrackedControllerRole_LeftHand || role == ETrackedControllerRole_TrackedControllerRole_RightHand)
            {
                VRInputValueHandle_t path = 0;
                std::string handPath = role == ETrackedControllerRole_TrackedControllerRole_LeftHand ? "/user/hand/left" : "/user/hand/right";
                auto result = input->GetInputSourceHandle(&handPath[0], &path);
                item["sourceError"] = result;
                item["components"] = json::array();
                const auto componentCount = models->GetComponentCount(&model[0]);
                for (uint32_t component = 0; component < componentCount; ++component)
                {
                    std::array<char, 256> name = {};
                    models->GetComponentName(&model[0], component, name.data(), name.size());
                    item["componentNames"].push_back(name.data());
                }
                for (const auto *component : {"handgrip", "openxr_grip", "openxr_handmodel", "grip", "tip", "base"})
                {
                    std::string name(component);
                    RenderModel_ControllerMode_State_t mode = {};
                    RenderModel_ComponentState_t state = {};
                    const bool available = result == EVRInputError_VRInputError_None && models->GetComponentStateForDevicePath(
                        &model[0], &name[0], path, &mode, &state);
                    json data = {{"name", component}, {"available", available}};
                    if (available) data["local"] = matrix(state.mTrackingToComponentLocal);
                    item["components"].push_back(data);
                }
            }
            output.push_back(item);
        }
        std::cout << output.dump(2) << '\n';
    }
    catch (const std::exception &failure) { VR_ShutdownInternal(); std::cerr << failure.what() << '\n'; return 1; }
    VR_ShutdownInternal();
}
