param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$AssetDirectory,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [Parameter(Mandatory = $true)][string]$Avatar,
    [string]$Toolchain = 'C:/msys64/mingw32'
)

$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Output directory already exists' }
$executable = Join-Path $BuildDirectory 'McOsuAvatar.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Build executable is missing' }
if (-not (Test-Path -LiteralPath $Avatar -PathType Leaf)) { throw 'Avatar is missing' }
foreach ($directory in @('fonts', 'materials', 'models', 'shaders')) {
    if (-not (Test-Path -LiteralPath (Join-Path $AssetDirectory $directory) -PathType Container)) {
        throw "Missing game assets: $directory"
    }
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Copy-Item -LiteralPath $executable -Destination $OutputDirectory
foreach ($directory in @('fonts', 'materials', 'models', 'shaders')) {
    Copy-Item -LiteralPath (Join-Path $AssetDirectory $directory) -Destination $OutputDirectory -Recurse
}
foreach ($name in @('bass.dll', 'bass_fx.dll', 'openvr_api.dll', 'glew32.dll')) {
    Copy-Item -LiteralPath (Join-Path $AssetDirectory $name) -Destination $OutputDirectory
}
foreach ($name in @('libatomic-1.dll', 'libgcc_s_dw2-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
    Copy-Item -LiteralPath (Join-Path "$Toolchain/bin" $name) -Destination $OutputDirectory
}
$configuration = Join-Path $OutputDirectory 'cfg'
New-Item -ItemType Directory -Path $configuration | Out-Null
$avatarPath = (Resolve-Path -LiteralPath $Avatar).Path.Replace('\', '/')
$lines = @(
    "osu_vr_avatar $avatarPath"
    'vr_draw_hmd_to_window 1'
    'vr_spectator_mode 1'
    'vr_spectator_camera_position 0 0.8 2'
    'vr_spectator_camera_rotation 180 0 0'
)
[System.IO.File]::WriteAllLines((Join-Path $configuration 'osuvr.cfg'), $lines, [System.Text.UTF8Encoding]::new($false))
