param([Parameter(Mandatory = $true)][string]$RuntimeDirectory)

$ErrorActionPreference = 'Stop'
$runtime = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$settings = Get-Content -LiteralPath (Join-Path $runtime 'runtime.json') -Raw | ConvertFrom-Json
$profile = (Resolve-Path -LiteralPath $settings.profileDirectory).Path
$avatar = (Resolve-Path -LiteralPath $settings.avatar).Path
$executable = Join-Path $runtime 'McOsuAvatar.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Avatar executable is missing' }
if (-not (Test-Path -LiteralPath $avatar -PathType Leaf)) { throw 'Avatar is missing' }
if (-not (Test-Path -LiteralPath (Join-Path $profile 'cfg') -PathType Container)) { throw 'Shared profile is missing' }
$previousAvatar = $env:MCOSU_AVATAR
$previousSpectator = $env:MCOSU_AVATAR_SPECTATOR
$previousTrackers = @{}
try {
    $env:MCOSU_AVATAR = $avatar
    $env:MCOSU_AVATAR_SPECTATOR = $(if ($settings.spectator) { '1' } else { '0' })
    if ($settings.bodyTrackers) {
        foreach ($binding in $settings.bodyTrackers.PSObject.Properties) {
            $name = 'MCOSU_osu_vr_avatar_' + $binding.Name + '_tracker'
            $previousTrackers[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
            [Environment]::SetEnvironmentVariable($name, $binding.Value, 'Process')
        }
    }
    $process = Start-Process -FilePath $executable -WorkingDirectory $profile -PassThru `
        -RedirectStandardOutput (Join-Path $runtime 'tracking.log') `
        -RedirectStandardError (Join-Path $runtime 'tracking-error.log')
    Write-Output $process.Id
}
finally {
    $env:MCOSU_AVATAR = $previousAvatar
    $env:MCOSU_AVATAR_SPECTATOR = $previousSpectator
    foreach ($name in $previousTrackers.Keys) {
        [Environment]::SetEnvironmentVariable($name, $previousTrackers[$name], 'Process')
    }
}
