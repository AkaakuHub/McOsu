# OpenVR hand pose input

`OpenVRHandTracking` reads the current process's existing OpenVR context. It does not initialize or shut down OpenVR, register an action manifest, update action sets, or replace legacy button bindings. The modern C function tables are isolated in its translation unit.

Call `tryGetPalmPose(side, deviceToTracking, palmToTracking)` only after obtaining a valid raw controller pose for that side in the same frame. On success the result uses `-Z` toward the fingers and `+Y` toward the back of the hand. Its origin is the physical palm surface center, **not the wrist**. The caller must use an avatar palm anchor to convert this origin to a wrist target; an arbitrary fixed translation is not a measured wrist offset.

The source is the render model's `openxr_handmodel` component. Valve maps that component to OpenXR's `palm_ext/pose` (now `grip_surface`). Its original `+X` points out of the left palm and into the right palm, while `-Z` follows the extended index finger. The adapter therefore uses `-X` as the left hand's back-of-hand normal and `+X` as the right hand's back-of-hand normal, then rebuilds an orthonormal basis.

Sources:

- [Valve render model pose mappings](https://github.com/ValveSoftware/openvr/wiki/Render-Model-Reference)
- [OpenXR standard pose identifiers](https://github.com/KhronosGroup/OpenXR-Docs/blob/main/specification/sources/chapters/semantic_paths.adoc#semantic-paths-grip_surface-pose)
- [Valve skeletal input](https://github.com/ValveSoftware/openvr/wiki/SteamVR-Skeletal-Input)

This adapter returns explicit disconnected, unavailable component, interface, and invalid transform states. It does not substitute raw controller axes or claim to read wrist/finger joints. True skeletal input requires skeletal actions in an action manifest and matching bindings. Registering a new manifest changes the application's input configuration, so this path leaves the existing McOsu legacy manifest intact.

The renderer only drives a hand after its palm component is available. Missing components stop that hand's tracking and are reported in `tracking.log`; raw device axes are not silently treated as anatomical hand axes. `handStatus` values are 0=available, 1=interface unavailable, 2=device disconnected, 3=component unavailable, 4=invalid transform.

The pose is updated once per engine frame and reused by both eyes and the spectator. OpenVR's headset activity state suspends pose updates in standby. The first in-use head pose sets a uniform avatar scale from its eye height above the standing tracking floor; the avatar origin remains at floor Y=0. Taking the headset off suspends tracking without changing that scale. `osu_vr_avatar_calibrate` explicitly measures it again. Calibrate while upright; the system does not infer a standing height from a crouched first pose. The runtime audit covers standby pose preservation, return-to-use eye alignment and floor origin, but live wear-state transitions remain unverified.

`VrmHandAnchors` derives a palm center line from the wrist and proximal finger bones, then raycasts the rest skinned hand mesh toward the back of the hand. The first surface intersection defines the avatar palm surface anchor. The pose solver subtracts the rotated anchor from the tracked palm position before solving the wrist. The runtime audit alternates wrist and palm targets, rotates both wrists repeatedly, and checks wrist position, finger direction and forearm continuity. This validates the mesh-derived mapping, not live device accuracy.

`tools/vr-tracking-inspect` is a separate read-only utility. It uses `VRApplication_Utility`, lists connected devices and available render model component transforms, and shuts down only its own connection. On 2026-11-02 the captured runtime had only the Quest 3 HMD connected (`vrlink`, `generic_hmd` render model, `rift` controller type). Both hand devices were disconnected, so live palm component availability and offsets remain unverified.
