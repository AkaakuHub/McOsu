param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [string]$Toolchain = 'C:/msys64/mingw32',
    [int]$Jobs = 6
)

$ErrorActionPreference = 'Stop'
$previousPath = $env:PATH
try {
    $compilerDirectory = Join-Path $Toolchain 'bin'
    $compiler = Join-Path $compilerDirectory 'g++.exe'
    $ninja = Join-Path $compilerDirectory 'ninja.exe'
    foreach ($tool in @($compiler, $ninja)) {
        if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Missing build tool: $tool" }
    }
    $env:PATH = "$compilerDirectory;$previousPath"
    & rtk proxy cmake -S $PSScriptRoot -B $BuildDirectory -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$ninja" `
        "-DCMAKE_C_COMPILER=$(Join-Path $compilerDirectory 'gcc.exe')" `
        "-DCMAKE_CXX_COMPILER=$compiler"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    & rtk proxy cmake --build $BuildDirectory --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'McOsu build failed' }
}
finally {
    $env:PATH = $previousPath
}
