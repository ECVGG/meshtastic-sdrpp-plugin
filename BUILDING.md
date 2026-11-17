# Building Meshtastic Decoder Plugin for SDR++

## Prerequisites

- **Windows 10/11** (x64)
- **Visual Studio 2022** with C++ workload
- **CMake** 3.13+
- **vcpkg** (for dependency management)
- **SDR++ source code** (for in-tree build)

## Dependencies (via vcpkg)

```bash
# Install vcpkg if not already installed
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
./vcpkg integrate install

# Install required packages
./vcpkg install protobuf:x64-windows cryptopp:x64-windows fmt:x64-windows spdlog:x64-windows abseil:x64-windows gtest:x64-windows nlohmann-json:x64-windows
```

## Build Steps

### 1. Get SDR++ Source
```bash
git clone --recursive https://github.com/AlexandreRouma/SDRPlusPlus.git sdrpp_src
cd sdrpp_src
```

### 2. Place Plugin in Decoder Modules
```bash
# Copy this plugin to SDR++ decoder_modules
cp -r /path/to/meshtastic-sdrpp-plugin sdrpp_src/decoder_modules/meshtastic_decoder
```

### 3. Configure with CMake
```bash
cd sdrpp_src
mkdir build
cd build

# Configure with vcpkg toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DSDRPP_MODULE_CMAKE=../cmake/sdrpp_module.cmake
```

### 4. Build
```bash
cmake --build . --target meshtastic_decoder --config Release
```

### 5. Output
The built DLL will be at:
```
sdrpp_src/build/decoder_modules/meshtastic_decoder/Release/meshtastic_decoder.dll
```

### 6. Install to SDR++
Copy the DLL to your SDR++ portable installation:
```bash
copy meshtastic_decoder.dll C:\path\to\sdrpp_portable\modules\
```

Also copy `libprotobuf.dll` from vcpkg if not already present:
```bash
copy C:\vcpkg\installed\x64-windows\bin\libprotobuf.dll C:\path\to\sdrpp_portable\
```

## Alternative: Out-of-Tree Build

For building without the full SDR++ source tree, you need the SDR++ CMake module and headers. This is more complex and not recommended.

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

### Build fails with "MeshtasticPlugin.cpp duplicate"
The CMakeLists.txt excludes `MeshtasticPlugin.cpp` (old duplicate file). Make sure it's not being compiled.

### Plugin doesn't load in SDR++
- Check that `libprotobuf.dll` is in the SDR++ root folder
- Check SDR++ logs for error messages
- Ensure the DLL is in the `modules` folder
- Verify SDR++ version compatibility (v1.3+)

## Development

### Adding New Protobuf Messages
1. Add `.proto` file to `proto/`
2. Add to `MESHTASTIC_PROTO_FILES` in CMakeLists.txt
3. Re-run CMake to generate new headers
4. Include generated header: `#include <new_message.pb.h>`

### Testing Without Hardware
Use the **File Source** module in SDR++ with recorded IQ samples of Meshtastic packets.

## CI/CD

GitHub Actions workflow example:
```yaml
name: Build
on: [push, pull_request]
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v10
      - name: Configure
        run: cmake -B build -DCMAKE_TOOLCHAIN_FILE=${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake
      - name: Build
        run: cmake --build build --target meshtastic_decoder --config Release
      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: meshtastic_decoder.dll
          path: build/decoder_modules/meshtastic_decoder/Release/meshtastic_decoder.dll
```