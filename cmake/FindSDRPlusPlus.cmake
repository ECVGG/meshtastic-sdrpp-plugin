# FindSDRPlusPlus.cmake - Find module for SDR++ core
# This module locates SDR++ installation or build directory and provides
# imported targets and variables for building plugins.

# Copyright (c) 2024 Victor
# MIT License

# =============================================================================
# Usage:
#   find_package(SDRPlusPlus REQUIRED)
#   target_link_libraries(my_plugin PRIVATE SDRPlusPlus::SDRPlusPlus)
# =============================================================================

# -----------------------------------------------------------------------------
# Configuration variables (set before find_package or via cache)
# -----------------------------------------------------------------------------

# Path to SDR++ installation or build directory
set(SDRPlusPlus_ROOT "" CACHE PATH "Path to SDR++ installation or build directory")

# Path to SDR++ CMake config directory (auto-detected from SDRPlusPlus_ROOT)
set(SDRPlusPlus_CMAKE_DIR "" CACHE PATH "Path to SDR++ CMake config directory")

# -----------------------------------------------------------------------------
# Find SDR++ CMake config
# -----------------------------------------------------------------------------

if (SDRPlusPlus_CMAKE_DIR)
    # User provided explicit CMake config directory
    set(CMAKE_FIND_ROOT_PATH "${SDRPlusPlus_CMAKE_DIR}" ${CMAKE_FIND_ROOT_PATH})
else if (SDRPlusPlus_ROOT)
    # User provided SDR++ root, try common CMake config locations
    set(SDRPlusPlus_CMAKE_DIR "${SDRPlusPlus_ROOT}/lib/cmake/SDRPlusPlus" CACHE PATH "SDR++ CMake config dir" FORCE)
    if (NOT EXISTS "${SDRPlusPlus_CMAKE_DIR}/SDRPlusPlusConfig.cmake")
        set(SDRPlusPlus_CMAKE_DIR "${SDRPlusPlus_ROOT}/cmake" CACHE PATH "SDR++ CMake config dir" FORCE)
    endif()
    if (NOT EXISTS "${SDRPlusPlus_CMAKE_DIR}/SDRPlusPlusConfig.cmake")
        set(SDRPlusPlus_CMAKE_DIR "${SDRPlusPlus_ROOT}" CACHE PATH "SDR++ CMake config dir" FORCE)
    endif()
    list(APPEND CMAKE_FIND_ROOT_PATH "${SDRPlusPlus_ROOT}")
else()
    # Try to find via standard paths
    find_path(SDRPlusPlus_CMAKE_DIR
        NAMES SDRPlusPlusConfig.cmake
        PATHS
            ${CMAKE_INSTALL_PREFIX}/lib/cmake/SDRPlusPlus
            ${CMAKE_INSTALL_PREFIX}/cmake
            /usr/local/lib/cmake/SDRPlusPlus
            /usr/lib/cmake/SDRPlusPlus
            /opt/sdrpp/lib/cmake/SDRPlusPlus
            ~/sdrpp/build
            ~/SDRPlusPlus/build
        PATH_SUFFIXES lib/cmake/SDRPlusPlus cmake
        NO_DEFAULT_PATH
    )
endif()

# -----------------------------------------------------------------------------
# Include SDR++ config
# -----------------------------------------------------------------------------

if (SDRPlusPlus_CMAKE_DIR AND EXISTS "${SDRPlusPlus_CMAKE_DIR}/SDRPlusPlusConfig.cmake")
    message(STATUS "Found SDR++ CMake config at: ${SDRPlusPlus_CMAKE_DIR}")
    include("${SDRPlusPlus_CMAKE_DIR}/SDRPlusPlusConfig.cmake")
else()
    # Try find_package as fallback
    find_package(SDRPlusPlus QUIET)
    if (NOT SDRPlusPlus_FOUND)
        message(FATAL_ERROR "SDR++ not found. Set SDRPlusPlus_ROOT or SDRPlusPlus_CMAKE_DIR to your SDR++ build/install directory.")
    endif()
endif()

# -----------------------------------------------------------------------------
# Verify required components
# -----------------------------------------------------------------------------

if (NOT TARGET SDRPlusPlus::SDRPlusPlus)
    message(FATAL_ERROR "SDRPlusPlus::SDRPlusPlus target not found. SDR++ config may be incomplete.")
endif()

# -----------------------------------------------------------------------------
# Provide convenience variables
# -----------------------------------------------------------------------------

get_target_property(SDRPlusPlus_INCLUDE_DIRS SDRPlusPlus::SDRPlusPlus INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(SDRPlusPlus_LIBRARY_DIRS SDRPlusPlus::SDRPlusPlus INTERFACE_LINK_DIRECTORIES)
get_target_property(SDRPlusPlus_DEFINITIONS SDRPlusPlus::SDRPlusPlus INTERFACE_COMPILE_DEFINITIONS)
get_target_property(SDRPlusPlus_COMPILE_OPTIONS SDRPlusPlus::SDRPlusPlus INTERFACE_COMPILE_OPTIONS)

# Plugin installation directory
if (NOT SDRPlusPlus_PLUGIN_INSTALL_DIR)
    if (SDRPlusPlus_ROOT)
        set(SDRPlusPlus_PLUGIN_INSTALL_DIR "${SDRPlusPlus_ROOT}/lib/sdrpp/plugins" CACHE PATH "SDR++ plugin install directory")
    else()
        set(SDRPlusPlus_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/sdrpp/plugins" CACHE PATH "SDR++ plugin install directory")
    endif()
endif()

# Plugin suffix (platform-specific)
if (NOT SDRPlusPlus_PLUGIN_SUFFIX)
    if (WIN32)
        set(SDRPlusPlus_PLUGIN_SUFFIX ".dll")
    elseif (APPLE)
        set(SDRPlusPlus_PLUGIN_SUFFIX ".dylib")
    else()
        set(SDRPlusPlus_PLUGIN_SUFFIX ".so")
    endif()
endif()

# Version
if (NOT SDRPlusPlus_VERSION)
    set(SDRPlusPlus_VERSION "unknown")
endif()

message(STATUS "SDR++ Version: ${SDRPlusPlus_VERSION}")
message(STATUS "SDR++ Include dirs: ${SDRPlusPlus_INCLUDE_DIRS}")
message(STATUS "SDR++ Library dirs: ${SDRPlusPlus_LIBRARY_DIRS}")
message(STATUS "SDR++ Plugin install dir: ${SDRPlusPlus_PLUGIN_INSTALL_DIR}")
message(STATUS "SDR++ Plugin suffix: ${SDRPlusPlus_PLUGIN_SUFFIX}")

# -----------------------------------------------------------------------------
# Provide macro for creating SDR++ plugins
# -----------------------------------------------------------------------------

macro(sdrpp_create_plugin PLUGIN_NAME)
    # Create module library with proper naming
    add_library(${PLUGIN_NAME} MODULE ${ARGN})

    set_target_properties(${PLUGIN_NAME} PROPERTIES
        PREFIX ""
        SUFFIX "${SDRPlusPlus_PLUGIN_SUFFIX}"
        POSITION_INDEPENDENT_CODE ON
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    # Link against SDR++
    target_link_libraries(${PLUGIN_NAME} PRIVATE SDRPlusPlus::SDRPlusPlus)

    # Install to plugin directory
    install(TARGETS ${PLUGIN_NAME}
        LIBRARY DESTINATION "${SDRPlusPlus_PLUGIN_INSTALL_DIR}"
        RUNTIME DESTINATION "${SDRPlusPlus_PLUGIN_INSTALL_DIR}"
        ARCHIVE DESTINATION "${SDRPlusPlus_PLUGIN_INSTALL_DIR}"
    )
endmacro()

# -----------------------------------------------------------------------------
# Export variables for parent scope
# -----------------------------------------------------------------------------

set(SDRPlusPlus_FOUND TRUE PARENT_SCOPE)
set(SDRPlusPlus_VERSION ${SDRPlusPlus_VERSION} PARENT_SCOPE)
set(SDRPlusPlus_INCLUDE_DIRS ${SDRPlusPlus_INCLUDE_DIRS} PARENT_SCOPE)
set(SDRPlusPlus_LIBRARY_DIRS ${SDRPlusPlus_LIBRARY_DIRS} PARENT_SCOPE)
set(SDRPlusPlus_CMAKE_DIR ${SDRPlusPlus_CMAKE_DIR} PARENT_SCOPE)
set(SDRPlusPlus_PLUGIN_INSTALL_DIR ${SDRPlusPlus_PLUGIN_INSTALL_DIR} PARENT_SCOPE)
set(SDRPlusPlus_PLUGIN_SUFFIX ${SDRPlusPlus_PLUGIN_SUFFIX} PARENT_SCOPE)