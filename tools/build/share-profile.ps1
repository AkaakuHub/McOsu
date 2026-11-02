param(
    [Parameter(Mandatory = $true)][string]$RuntimeDirectory,
    [Parameter(Mandatory = $true)][string]$ProfileDirectory,
    [Parameter(Mandatory = $true)][string]$Avatar
)

$ErrorActionPreference = 'Stop'
$runtime = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$profile = (Resolve-Path -LiteralPath $ProfileDirectory).Path
$avatarPath = (Resolve-Path -LiteralPath $Avatar).Path
if (-not (Test-Path -LiteralPath (Join-Path $runtime 'McOsuAvatar.exe') -PathType Leaf)) { throw 'Avatar executable is missing' }
if (-not (Test-Path -LiteralPath $avatarPath -PathType Leaf)) { throw 'Avatar is missing' }
foreach ($directory in @('cfg', 'fonts', 'materials', 'models', 'shaders')) {
    if (-not (Test-Path -LiteralPath (Join-Path $profile $directory) -PathType Container)) {
        throw "Missing profile directory: $directory"
    }
}
$settings = [ordered]@{
    profileDirectory = $profile
    avatar = $avatarPath
    spectator = $true
}
[System.IO.File]::WriteAllText((Join-Path $runtime 'runtime.json'), ($settings | ConvertTo-Json), [System.Text.UTF8Encoding]::new($false))
Write-Output "Shared profile: $profile"
