# Shared McOsu profile

The avatar executable runs with the installed McOsu directory as its working directory. Both executables use the same `cfg`, scores, collections, star cache, skins, songs and assets. Keep one instance running at a time so that settings and scores have one writer.

Build and stage the executable with `tools/build/windows.ps1` and `tools/build/stage-runtime.ps1`. Pass the installed McOsu directory as `-ProfileDirectory`. The staged directory contains the avatar executable, its runtime DLLs and `runtime.json`; it does not duplicate the profile or game assets.

To connect an existing staged executable to the installed profile:

```powershell
./tools/build/share-profile.ps1 -RuntimeDirectory C:/path/to/avatar-runtime -ProfileDirectory 'C:/Program Files (x86)/Steam/steamapps/common/McOsu' -Avatar C:/path/to/avatar.vrm
./tools/build/launch-runtime.ps1 -RuntimeDirectory C:/path/to/avatar-runtime
```

The launcher supplies `MCOSU_AVATAR` and `MCOSU_AVATAR_SPECTATOR` only to the child process. Avatar selection is separate from the shared user configuration. The installed `override.cfg` also retains the user's existing database version setting, so it is loaded before the song database instead of forcing a separate raw song scan.

Avatar spectator output fills the desktop window with one camera. A saved spectator camera takes priority; otherwise the first calibrated avatar initializes the framing. The camera then stays where the user places it. The headset keeps its two eye views and hides head-related triangles according to the VRM first-person rules. Avatar spectator output does not change the shared `vr_spectator_mode` or eye display preference.

Focus the desktop window and hold the right mouse button to turn the camera with the mouse. While holding it, use WASD to move, Space to ascend, Ctrl to descend, Shift to accelerate, and F to frame the current avatar again. Releasing the right mouse button or pressing Esc ends camera control and saves its position and rotation through the existing spectator camera configuration. Losing focus releases capture and clears movement keys. These controls do not move the HMD camera.

Standard output and errors go to `tracking.log` and `tracking-error.log` in the staged runtime directory. Launching replaces these diagnostic logs; it does not place logs in the installed profile.
