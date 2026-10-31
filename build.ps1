<#
.SYNOPSIS
    Meshtastic SDR++ Plugin Build Script for Windows

.DESCRIPTION
    Builds the Meshtastic decoder plugin for SDR++ on Windows using MSVC and vcpkg.

.PARAMETER BuildType
    Build type: Release, RelWithDebInfo, Debug (default: Release)

.PARAMETER Install
    Install after building

.PARAMETER Clean
    Clean build directory before building

.PARAMETER Generator
    CMake generator (default: "Visual Studio 17 2022")

.PARAMETER Arch
    Architecture: x64, x86, ARM64 (default: x64)

.EXAMPLE
    .\build.ps1 -BuildType Release -Install

.EXAMPLE
    $env:SDRPP_CORE_ROOT="C:\SDRPlusPlus\build"; .\build.ps1 -BuildType RelWithDebInfo
#>

param(
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string]$BuildType = "Release",

    [switch]$Install,

    [switch]$Clean,

    [string]$Generator = "Visual Studio 17 2022",

    [ValidateSet("x64", "x86", "ARM64")]
    [string]$Arch = "x64",

    [switch]$Help
)

if ($Help) {
    Get-Help $MyInvocation.MyCommand.Definition -Full
    exit 0
}

# Colors
$ErrorColor = "Red"
$WarningColor = "Yellow"
$SuccessColor = "Green"
$InfoColor = "Cyan"

function Write-Info { param($msg) Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" -ForegroundColor $InfoColor }
function Write-Success { param($msg) Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" -ForegroundColor $SuccessColor }
function Write-Warning { param($msg) Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" -ForegroundColor $WarningColor }
function Write-ErrorMsg { param($msg) Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" -ForegroundColor $ErrorColor }

Write-Info "Building Meshtastic SDR++ Plugin"
Write-Info "Build Type: $BuildType"
Write-Info "Architecture: $Arch"
Write-Info "Generator: $Generator"

# Check SDRPP_CORE_ROOT
if (-not $env:SDRPP_CORE_ROOT) {
    Write-ErrorMsg "SDRPP_CORE_ROOT environment variable not set"
    Write-Info "Set it to your SDR++ build or install directory"
    Write-Info 'Example: $env:SDRPP_CORE_ROOT="C:\SDRPlusPlus\build"'
    exit 1
}

if (-not (Test-Path $env:SDRPP_CORE_ROOT)) {
    Write-ErrorMsg "SDRPP_CORE_ROOT directory does not exist: $env:SDRPP_CORE_ROOT"
    exit 1
}

Write-Info "SDR++ root: $env:SDRPP_CORE_ROOT"

# Check vcpkg
$VcpkgRoot = $env:VCPKG_ROOT
if (-not $VcpkgRoot) {
    $VcpkgRoot = "C:\vcpkg"
}
if (-not (Test-Path "$VcpkgRoot\vcpkg.exe")) {
    Write-Warning "vcpkg not found at $VcpkgRoot\vcpkg.exe"
    Write-Info "Install vcpkg or set VCPKG_ROOT environment variable"
}

$BuildDir = "build_${Arch}_${BuildType}"

# Clean if requested
if ($Clean) {
    Write-Info "Cleaning build directory..."
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Set-Location $BuildDir

# Configure with CMake
Write-Info "Configuring with CMake..."
$CmakeArgs = @(
    "-G", $Generator,
    "-A", $Arch,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DSDRPP_CORE_ROOT=$env:SDRPP_CORE_ROOT",
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake",
    ".."
)

& cmake @CmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-ErrorMsg "CMake configuration failed"
    exit 1
}

# Build
Write-Info "Building..."
& cmake --build . --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) {
    Write-ErrorMsg "Build failed"
    exit 1
}

Write-Success "Build completed successfully!"

# Find built plugin
$PluginName = "meshtastic"
$PluginFile = "${PluginName}.dll"

if (Test-Path $PluginFile) {
    Write-Success "Plugin built: $PluginFile"
    Get-Item $PluginFile | Select-Object Name, Length, LastWriteTime
} elseif (Test-Path "$BuildType\$PluginFile") {
    $PluginFile = "$BuildType\$PluginFile"
    Write-Success "Plugin built: $PluginFile"
    Get-Item $PluginFile | Select-Object Name, Length, LastWriteTime
} else {
    Write-Warning "Plugin file not found in expected location"
    Get-ChildItem -Recurse -Filter "${PluginName}.*" | Select-Object -First 5
}

# Install if requested
if ($Install) {
    Write-Info "Installing..."
    & cmake --build . --config $BuildType --target INSTALL
    if ($LASTEXITCODE -eq 0) {
        Write-Success "Installation completed"
    } else {
        Write-ErrorMsg "Installation failed"
        exit 1
    }
}

Write-Info "Done!"