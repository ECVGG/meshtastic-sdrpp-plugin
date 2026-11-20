# Building Meshtastic Decoder Plugin for SDR++

## Prerequisites

- **Windows 10/11** (x64)
- **Visual Studio 2022** with C++ workload
- **CMake** 3.13+
- **vcpkg** (for dependency management)
- **SDR++ source code** (for headers and CMake modules)

## Dependencies (via vcpkg)

```bash
# Install vcpkg if not already installed
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat

# Install required packages
./vcpkg install protobuf:x64-windows cryptopp:x64-windows fmt:x64-windows abseil:x64-windows nlohmann-json:x64-windows
```

## Build Steps

### 1. Get SDR++ Source
```bash
git clone --depth 1 https://github.com/AlexandreRouma/SDRPlusPlus.git sdrpp_src
```

### 2. Create SDR++ CMake Config
Create a minimal `SDRPlusPlusConfig.cmake` and `sdrpp_module.cmake` in a directory (e.g., `sdrpp_cmake/SDRPlusPlus/`):

```cmake
# SDRPlusPlusConfig.cmake
set(SDRPlusPlus_VERSION "nightly")
set(SDRPlusPlus_INCLUDE_DIRS
  "C:/sdrpp_src/core/src"
  "C:/sdrpp_src/src"
  "C:/sdrpp_src/core/src/gui"
  "C:/sdrpp_src/core/src/dsp"
  "C:/sdrpp_src/src/gui"
  "C:/sdrpp_src/third-party/imgui"
  "C:/sdrpp_src/core/third-party/imgui"
  "C:/sdrpp_src/core/src/gui/widgets"
  "C:/sdrpp_src/src/gui/widgets"
)
set(SDRPlusPlus_LIBRARY_DIRS "")
set(SDRPlusPlus_LIBRARIES "")
set(SDRPlusPlus_PLUGIN_SUFFIX ".dll")
set(SDRPlusPlus_PLUGIN_INSTALL_DIR "C:/sdrpp_src/plugins")
```

```cmake
# sdrpp_module.cmake
cmake_minimum_required(VERSION 3.13)

function(sdrpp_add_module MODULE_NAME)
  add_library(${MODULE_NAME} MODULE ${ARGN})
  set_target_properties(${MODULE_NAME} PROPERTIES
    PREFIX ""
    SUFFIX "${SDRPlusPlus_PLUGIN_SUFFIX}"
  )
  target_include_directories(${MODULE_NAME} PRIVATE ${SDRPlusPlus_INCLUDE_DIRS})
  target_link_libraries(${MODULE_NAME} PRIVATE ${SDRPlusPlus_LIBRARIES})
  if (SDRPlusPlus_PLUGIN_INSTALL_DIR)
    install(TARGETS ${MODULE_NAME} DESTINATION ${SDRPlusPlus_PLUGIN_INSTALL_DIR})
  endif()
endfunction()

function(sdrpp_get_module_cmake_dir OUT_VAR)
  set(${OUT_VAR} ${CMAKE_CURRENT_LIST_DIR} PARENT_SCOPE)
endfunction()
```

### 3. Configure with CMake
```bash
# From this repository root
mkdir build
cd build

# Configure with vcpkg toolchain and SDR++ config
# Replace paths as needed
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows \
  -DVCPKG_MANIFEST_MODE=OFF \
  -DSDRPP_CMAKE_DIR=C:/sdrpp_cmake/SDRPlusPlus \
  -DSDRPP_MODULE_CMAKE=C:/sdrpp_cmake/SDRPlusPlus/sdrpp_module.cmake \
  -DSDRPlusPlus_DIR=C:/sdrpp_cmake/SDRPlusPlus \
  -G "Visual Studio 17 2022" -A x64
```

### 4. Build
```bash
cmake --build . --config Release --parallel
```

### 5. Output
The built DLL will be at:
```
build/Release/meshtastic_decoder.dll
```

### 6. Install to SDR++ Portable
Copy the DLL to your SDR++ portable installation `modules` folder:
```bash
copy build\Release\meshtastic_decoder.dll <sdrpp-portable>\modules\
```

Also copy `libprotobuf.dll` from vcpkg if not already present in SDR++:
```bash
copy C:\vcpkg\installed\x64-windows\bin\libprotobuf.dll <sdrpp-portable>\
```

## Required Files for Distribution

For a portable SDR++ installation, you need:
- `meshtastic_decoder.dll` - The plugin (in `modules/` folder)
- `libprotobuf.dll` - Protobuf runtime (in SDR++ root folder)

The plugin has no other external dependencies.

## Troubleshooting

### "Could not find protobuf"
Make sure vcpkg is properly integrated and the toolchain file is passed to CMake.

### "Cryptopp not found"
```bash
./vcpkg install cryptopp:x64-windows
```

### "abseil_dll.lib not found"
```bash
./vcpkg install abseil:x64-windows
```

### Plugin doesn't load in SDR++
- Check that `libprotobuf.dll` is in the SDR++ root folder
- Check SDR++ logs for error messages
- Ensure the DLL is in the `modules` folder
- Verify SDR++ version compatibility (v1.3+)

## Development

### Adding New Protobuf Messages
1. Add `.proto` file to `proto/meshtastic/protobufs/meshtastic/`
2. Add to `MESHTASTIC_PROTO_FILES` and `MESHTASTIC_PROTO_BASENAMES` in CMakeLists.txt
3. Re-run CMake to generate new headers
4. Include generated header: `#include <meshtastic/new_message.pb.h>`

### Testing Without Hardware
Use the **File Source** module in SDR++ with recorded IQ samples of Meshtastic packets.

## CI/CD

GitHub Actions workflow (`.github/workflows/ci.yml`) builds on every push and tag.

The CI:
1. Checks out this repo with submodules (pinned to Meshtastic firmware commit 968692d)
2. Sets up vcpkg with binary caching
3. Fetches SDR++ source (cached)
4. Generates minimal SDRPlusPlusConfig.cmake
5. Configures and builds the plugin
6. Uploads DLL as artifact
7. On tags, creates a release package with both DLL and libprotobuf.dll