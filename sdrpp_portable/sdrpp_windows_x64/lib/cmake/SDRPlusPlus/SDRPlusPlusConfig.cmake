# Minimal SDRPlusPlus config for plugin building
set(SDRPlusPlus_VERSION "nightly")
set(SDRPlusPlus_INCLUDE_DIRS "C:/Users/victo/Documents/CLAUDE/sdrpp_source/include")
set(SDRPlusPlus_LIBRARY_DIRS "C:/Users/victo/Documents/CLAUDE/meshtastic-sdrpp-plugin/sdrpp_portable/sdrpp_windows_x64")
set(SDRPlusPlus_PLUGIN_SUFFIX ".dll")
set(SDRPlusPlus_PLUGIN_INSTALL_DIR "C:/Users/victo/Documents/CLAUDE/meshtastic-sdrpp-plugin/sdrpp_portable/sdrpp_windows_x64/plugins")

# Find required components
find_package(Protobuf REQUIRED)
find_package(Cryptopp REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt QUIET)
find_package(spdlog QUIET)
find_package(nlohmann_json QUIET)
