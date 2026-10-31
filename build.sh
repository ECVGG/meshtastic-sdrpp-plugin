#!/bin/bash

# Meshtastic SDR++ Plugin Build Script
# Usage: ./build.sh [--type Release|RelWithDebInfo|Debug] [--install] [--clean] [--help]

set -e

# Default values
BUILD_TYPE="Release"
DO_INSTALL=false
DO_CLEAN=false
BUILD_DIR="build"
GENERATOR="Ninja"
CMAKE_EXTRA_ARGS=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    cat << EOF
Meshtastic SDR++ Plugin Build Script

Usage: $0 [OPTIONS]

Options:
    --type TYPE         Build type: Release, RelWithDebInfo, Debug (default: Release)
    --install           Install after building
    --clean             Clean build directory before building
    --generator GEN     CMake generator: Ninja, Unix Makefiles (default: Ninja)
    --help              Show this help message

Environment Variables:
    SDRPP_CORE_ROOT     Path to SDR++ build or install directory (required)

Examples:
    $0 --type Release --install
    SDRPP_CORE_ROOT=/home/user/sdrpp/build $0 --type RelWithDebInfo
    $0 --clean --type Debug

EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --install)
            DO_INSTALL=true
            shift
            ;;
        --clean)
            DO_CLEAN=true
            shift
            ;;
        --generator)
            GENERATOR="$2"
            shift 2
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validate build type
if [[ ! "$BUILD_TYPE" =~ ^(Release|RelWithDebInfo|Debug)$ ]]; then
    print_error "Invalid build type: $BUILD_TYPE"
    exit 1
fi

# Check SDRPP_CORE_ROOT
if [[ -z "$SDRPP_CORE_ROOT" ]]; then
    print_error "SDRPP_CORE_ROOT environment variable not set"
    print_info "Set it to your SDR++ build or install directory"
    print_info "Example: export SDRPP_CORE_ROOT=/path/to/sdrpp/build"
    exit 1
fi

if [[ ! -d "$SDRPP_CORE_ROOT" ]]; then
    print_error "SDRPP_CORE_ROOT directory does not exist: $SDRPP_CORE_ROOT"
    exit 1
fi

print_info "Building Meshtastic SDR++ Plugin"
print_info "Build type: $BUILD_TYPE"
print_info "Generator: $GENERATOR"
print_info "SDR++ root: $SDRPP_CORE_ROOT"
print_info "Build dir: $BUILD_DIR"

# Clean if requested
if [[ "$DO_CLEAN" == true ]]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
print_info "Configuring with CMake..."
cmake -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DSDRPP_CORE_ROOT="$SDRPP_CORE_ROOT" \
    $CMAKE_EXTRA_ARGS \
    ..

if [[ $? -ne 0 ]]; then
    print_error "CMake configuration failed"
    exit 1
fi

# Build
print_info "Building..."
cmake --build . --config "$BUILD_TYPE" --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

if [[ $? -ne 0 ]]; then
    print_error "Build failed"
    exit 1
fi

print_success "Build completed successfully!"

# Find built plugin
PLUGIN_NAME="meshtastic"
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLUGIN_FILE="${PLUGIN_NAME}.dylib"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLUGIN_FILE="${PLUGIN_NAME}.so"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    PLUGIN_FILE="${PLUGIN_NAME}.dll"
fi

if [[ -f "$PLUGIN_FILE" ]]; then
    print_success "Plugin built: $PLUGIN_FILE"
    ls -lh "$PLUGIN_FILE"
else
    # Try with build type subdirectory (for multi-config generators)
    if [[ -f "$BUILD_TYPE/$PLUGIN_FILE" ]]; then
        PLUGIN_FILE="$BUILD_TYPE/$PLUGIN_FILE"
        print_success "Plugin built: $PLUGIN_FILE"
        ls -lh "$PLUGIN_FILE"
    else
        print_warning "Plugin file not found in expected location"
        find . -name "${PLUGIN_NAME}.*" -type f | head -5
    fi
fi

# Install if requested
if [[ "$DO_INSTALL" == true ]]; then
    print_info "Installing..."
    cmake --build . --config "$BUILD_TYPE" --target install
    if [[ $? -eq 0 ]]; then
        print_success "Installation completed"
    else
        print_error "Installation failed"
        exit 1
    fi
fi

print_info "Done!"