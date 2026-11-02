# OpenVR hand pose input

`OpenVRHandTracking` reads the current process's existing OpenVR context. It does not initialize or shut down OpenVR, register an action manifest, update action sets, or replace legacy button bindings. The modern C function tables are isolated in its translation unit.

Call `tryGetPalmPose(side, deviceToTracking, palmToTracking)` only after obtaining a valid raw controller pose for that side in the same frame. On success the result uses `-Z` toward the fingers and `+Y` toward the back of the hand. Its origin is the physical palm surface center, **not the wrist**. The caller must use an avatar palm anchor to convert this origin to a wrist target; an arbitrary fixed translation is not a measured wrist offset.

The source is the render model's `openxr_handmodel` component. Valve maps that component to OpenXR's `palm_ext/pose` (now `grip_surface`). Its original `+X` points out of the left palm and into the right palm, while `-Z` follows the extended index finger. The adapter therefore uses `-X` as the left hand's back-of-hand normal and `+X` as the right hand's back-of-hand normal, then rebuilds an orthonormal basis.

Sources:

- [Valve render model pose mappings](https://github.com/ValveSoftware/openvr/wiki/Render-Model-Reference)
- [OpenXR standard pose identifiers](https://github.com/KhronosGroup/OpenXR-Docs/blob/main/specification/sources/chapters/semantic_paths.adoc#semantic-paths-grip_surface-pose)
- [Valve skeletal input](https://github.com/ValveSoftware/openvr/wiki/SteamVR-Skeletal-Input)

This adapter returns explicit disconnected, unavailable component, interface, and invalid transform states. It does not substitute raw controller axes or claim to read wrist/finger joints. True skeletal input requires skeletal actions in an action manifest and matching bindings. Registering a new manifest changes the application's input configuration, so this path leaves the existing McOsu legacy manifest intact.

`tools/vr-tracking-inspect` is a separate read-only utility. It uses `VRApplication_Utility`, lists connected devices and available render model component transforms, and shuts down only its own connection. On 2026-11-02 the captured runtime had only the Quest 3 HMD connected (`vrlink`, `generic_hmd` render model, `rift` controller type). Both hand devices were disconnected, so live palm component availability and offsets remain unverified.
