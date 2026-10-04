$ErrorActionPreference = "Stop"

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe was not found on the Windows build image"
}

$visualStudio = & $vswhere `
    -latest `
    -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $visualStudio) {
    throw "A Visual Studio installation with the C++ toolchain was not found"
}

$cmakeDirectory = Join-Path $visualStudio `
    "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if (-not (Test-Path (Join-Path $cmakeDirectory "cmake.exe"))) {
    throw "Visual Studio's bundled cmake.exe was not found at $cmakeDirectory"
}

$env:Path = "$cmakeDirectory;$env:Path"
