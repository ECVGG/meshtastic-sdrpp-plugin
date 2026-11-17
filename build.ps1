<#
.SYNOPSIS
    Build Meshtastic SDR++ Plugin for Windows
.DESCRIPTION
    Builds a portable .dll plugin using SDR++ portable nightly + vcpkg dependencies.
    Run from "Developer Command Prompt for VS 2022" (x64).
#>

param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$BuildType = 'Release',
    [switch]$Install,
    [switch]$Clean,
    [switch]$Package
)

# =============================================================================
# Configuración
# =============================================================================
$ProjectRoot = $PSScriptRoot
$SDRPP_DIR = "C:\sdrpp_portable\sdrpp_windows_x64"
$VCPKG_ROOT = "C:\vcpkg"

# =============================================================================
# Validaciones
# =============================================================================
Write-Host "=== Meshtastic SDR++ Plugin Builder ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectRoot" -ForegroundColor Gray
Write-Host "Build Type: $BuildType" -ForegroundColor Gray
Write-Host "SDR++ Dir: $SDRPP_DIR" -ForegroundColor Gray

if (-not (Test-Path $SDRPP_DIR)) {
    Write-Error "SDR++ portable not found at: $SDRPP_DIR"
    Write-Host "Download from: https://github.com/AlexandreRouma/SDRPlusPlus/releases/download/nightly/sdrpp_windows_x64.zip" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path "$VCPKG_ROOT\vcpkg.exe")) {
    Write-Error "vcpkg not found at: $VCPKG_ROOT"
    exit 1
}

# Verify we're in VS Developer Command Prompt
if (-not $env:VCToolsInstallDir) {
    Write-Warning "Not in Visual Studio Developer Command Prompt!"
    Write-Host "Please run from: 'Developer Command Prompt for VS 2022' (x64)" -ForegroundColor Yellow
    Write-Host "Or run: `"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat`" x64" -ForegroundColor Gray
    exit 1
}

# =============================================================================
# Set SDRPP_CORE_ROOT environment variable (REQUIRED for CMake)
# =============================================================================
$env:SDRPP_CORE_ROOT = $SDRPP_DIR
Write-Host "SDRPP_CORE_ROOT = $env:SDRPP_CORE_ROOT" -ForegroundColor Green

# =============================================================================
# Clean
# =============================================================================
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue "$ProjectRoot\build"
    Write-Host "Done." -ForegroundColor Green
    exit 0
}

# =============================================================================
# Install vcpkg dependencies
# =============================================================================
Write-Host "`n=== Installing vcpkg dependencies ===" -ForegroundColor Cyan
& "$VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows --x-manifest-root="$ProjectRoot" --feature-flags=manifests,binarycaching
if ($LASTEXITCODE -ne 0) { Write-Error "vcpkg install failed"; exit 1 }

# =============================================================================
# Configure with CMake
# =============================================================================
Write-Host "`n=== Configuring with CMake ===" -ForegroundColor Cyan
$BuildDir = "$ProjectRoot\build"
$ToolchainFile = "$VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"

$cmakeArgs = @(
    "-S", $ProjectRoot
    "-B", $BuildDir
    "-G", "Visual Studio 17 2022"
    "-A", "x64"
    "-DCMAKE_BUILD_TYPE=$BuildType"
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"
    "-DSDRPP_CORE_ROOT=$SDRPP_DIR"
    # IMPORTANTE: Comillas alrededor de 3.5 para CMake 4.x
    "-DCMAKE_POLICY_VERSION_MINIMUM=`"3.5`""
)

Write-Host "Running: cmake $($cmakeArgs -join ' ')" -ForegroundColor Gray
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed"; exit 1 }

# =============================================================================
# Build
# =============================================================================
Write-Host "`n=== Building ===" -ForegroundColor Cyan
& cmake --build $BuildDir --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

# =============================================================================
# Verify DLL
# =============================================================================
$DllPath = "$BuildDir\$BuildType\meshtastic.dll"
if (Test-Path $DllPath) {
    Write-Host "`n✓ BUILD SUCCESSFUL!" -ForegroundColor Green
    Write-Host "Plugin: $DllPath" -ForegroundColor Green
    $size = (Get-Item $DllPath).Length / 1MB
    Write-Host "Size: $([math]::Round($size, 2)) MB" -ForegroundColor Gray
} else {
    Write-Error "DLL not found at expected path: $DllPath"
    exit 1
}

# =============================================================================
# Install to SDR++ plugins folder
# =============================================================================
if ($Install) {
    $PluginsDir = "$SDRPP_DIR\plugins"
    if (-not (Test-Path $PluginsDir)) { New-Item -ItemType Directory -Path $PluginsDir | Out-Null }
    Copy-Item -Force $DllPath -Destination "$PluginsDir\meshtastic.dll"
    Write-Host "`n✓ Installed to: $PluginsDir\meshtastic.dll" -ForegroundColor Green
}

# =============================================================================
# Package (CPack)
# =============================================================================
if ($Package) {
    Write-Host "`n=== Creating package ===" -ForegroundColor Cyan
    & cmake --build $BuildDir --config $BuildType --target package
    if ($LASTEXITCODE -eq 0) {
        $zip = Get-ChildItem "$BuildDir\*.zip" | Select-Object -First 1
        if ($zip) { Write-Host "Package: $($zip.FullName)" -ForegroundColor Green }
    }
}

Write-Host "`nDone!" -ForegroundColor Cyan