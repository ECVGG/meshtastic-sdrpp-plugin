# meshtastic_decoder.cmake - CMake find module for Meshtastic Decoder plugin
# This module allows other projects to find and link against the Meshtastic Decoder plugin.
#
# Usage:
#   find_package(meshtastic_decoder REQUIRED)
#   target_link_libraries(my_target PRIVATE meshtastic_decoder::meshtastic_decoder)

# -----------------------------------------------------------------------------
# Version handling
# -----------------------------------------------------------------------------

set(meshtastic_decoder_VERSION_MAJOR 1)
set(meshtastic_decoder_VERSION_MINOR 0)
set(meshtastic_decoder_VERSION_PATCH 0)
set(meshtastic_decoder_VERSION "${meshtastic_decoder_VERSION_MAJOR}.${meshtastic_decoder_VERSION_MINOR}.${meshtastic_decoder_VERSION_PATCH}")

# -----------------------------------------------------------------------------
# Find plugin library
# -----------------------------------------------------------------------------

# Search paths
set(_meshtastic_decoder_SEARCH_PATHS
    ${CMAKE_INSTALL_PREFIX}/lib/sdrpp/plugins
    ${CMAKE_INSTALL_PREFIX}/lib
    /usr/local/lib/sdrpp/plugins
    /usr/local/lib
    /usr/lib/sdrpp/plugins
    /usr/lib
    ~/sdrpp/build/lib/sdrpp/plugins
    ~/SDRPlusPlus/build/lib/sdrpp/plugins
    ${SDRPlusPlus_PLUGIN_INSTALL_DIR}
)

# Platform-specific library name
if (WIN32)
    set(_meshtastic_decoder_LIB_NAMES meshtastic_decoder meshtastic)
    set(_meshtastic_decoder_LIB_SUFFIXES .dll .lib)
elseif (APPLE)
    set(_meshtastic_decoder_LIB_NAMES meshtastic_decoder meshtastic)
    set(_meshtastic_decoder_LIB_SUFFIXES .dylib)
else()
    set(_meshtastic_decoder_LIB_NAMES meshtastic_decoder meshtastic)
    set(_meshtastic_decoder_LIB_SUFFIXES .so)
endif()

find_library(meshtastic_decoder_LIBRARY
    NAMES ${_meshtastic_decoder_LIB_NAMES}
    PATHS ${_meshtastic_decoder_SEARCH_PATHS}
    PATH_SUFFIXES ""
    NO_DEFAULT_PATH
)

# -----------------------------------------------------------------------------
# Find headers
# -----------------------------------------------------------------------------

find_path(meshtastic_decoder_INCLUDE_DIR
    NAMES MeshtasticPlugin.h MeshtasticDecoder.h MeshtasticDecryptor.h MeshtasticProtocol.h LoRaDemodulator.h
    PATHS
        ${CMAKE_INSTALL_PREFIX}/include/sdrpp/plugins/meshtastic
        ${CMAKE_INSTALL_PREFIX}/include/meshtastic_decoder
        /usr/local/include/sdrpp/plugins/meshtastic
        /usr/local/include/meshtastic_decoder
        /usr/include/sdrpp/plugins/meshtastic
        /usr/include/meshtastic_decoder
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PATH_SUFFIXES ""
    NO_DEFAULT_PATH
)

# -----------------------------------------------------------------------------
# Handle QUIETLY and REQUIRED
# -----------------------------------------------------------------------------

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(meshtastic_decoder
    REQUIRED_VARS meshtastic_decoder_LIBRARY meshtastic_decoder_INCLUDE_DIR
    VERSION_VAR meshtastic_decoder_VERSION
)

# -----------------------------------------------------------------------------
# Create imported target
# -----------------------------------------------------------------------------

if (meshtastic_decoder_FOUND AND NOT TARGET meshtastic_decoder::meshtastic_decoder)
    add_library(meshtastic_decoder::meshtastic_decoder SHARED IMPORTED)

    set_target_properties(meshtastic_decoder::meshtastic_decoder PROPERTIES
        IMPORTED_LOCATION "${meshtastic_decoder_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${meshtastic_decoder_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS "MESHTASTIC_DECODER_STATIC_DEFINE"
    )

    # Add dependencies
    find_package(SDRPlusPlus REQUIRED)
    find_package(Protobuf REQUIRED)
    find_package(Cryptopp REQUIRED)

    target_link_libraries(meshtastic_decoder::meshtastic_decoder INTERFACE
        SDRPlusPlus::SDRPlusPlus
        ${Protobuf_LIBRARIES}
        ${CRYPTOPP_LIBRARIES}
    )
endif()

# -----------------------------------------------------------------------------
# Export variables
# -----------------------------------------------------------------------------

set(meshtastic_decoder_LIBRARIES ${meshtastic_decoder_LIBRARY})
set(meshtastic_decoder_INCLUDE_DIRS ${meshtastic_decoder_INCLUDE_DIR})
set(meshtastic_decoder_VERSION ${meshtastic_decoder_VERSION})

# Mark as advanced
mark_as_advanced(
    meshtastic_decoder_LIBRARY
    meshtastic_decoder_INCLUDE_DIR
)