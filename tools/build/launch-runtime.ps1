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
$start = [System.Diagnostics.ProcessStartInfo]::new()
$start.FileName = $executable
$start.WorkingDirectory = $profile
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.EnvironmentVariables['MCOSU_AVATAR'] = $avatar
$start.EnvironmentVariables['MCOSU_AVATAR_SPECTATOR'] = $(if ($settings.spectator) { '1' } else { '0' })
$process = [System.Diagnostics.Process]::Start($start)
Write-Output $process.Id
