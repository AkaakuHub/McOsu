param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$ProfileDirectory,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [Parameter(Mandatory = $true)][string]$Avatar,
    [string]$Toolchain = 'C:/msys64/mingw32'
)

$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Output directory already exists' }
$executable = Join-Path $BuildDirectory 'McOsuAvatar.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Build executable is missing' }
if (-not (Test-Path -LiteralPath $Avatar -PathType Leaf)) { throw 'Avatar is missing' }
foreach ($directory in @('cfg', 'fonts', 'materials', 'models', 'shaders')) {
    if (-not (Test-Path -LiteralPath (Join-Path $ProfileDirectory $directory) -PathType Container)) {
        throw "Missing game assets: $directory"
    }
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Copy-Item -LiteralPath $executable -Destination $OutputDirectory
$licenses = Join-Path $OutputDirectory 'licenses'
New-Item -ItemType Directory -Path $licenses | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../../third_party/ozz-animation/LICENSE.md') -Destination (Join-Path $licenses 'ozz-animation.txt')
foreach ($name in @('bass.dll', 'bass_fx.dll', 'openvr_api.dll', 'glew32.dll')) {
    Copy-Item -LiteralPath (Join-Path $ProfileDirectory $name) -Destination $OutputDirectory
}
foreach ($name in @('libatomic-1.dll', 'libgcc_s_dw2-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
    Copy-Item -LiteralPath (Join-Path "$Toolchain/bin" $name) -Destination $OutputDirectory
}
& (Join-Path $PSScriptRoot 'share-profile.ps1') -RuntimeDirectory $OutputDirectory -ProfileDirectory $ProfileDirectory -Avatar $Avatar
