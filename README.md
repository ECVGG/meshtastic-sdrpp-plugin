# Meshtastic SDR++ Plugin

A plugin for [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus) that decodes and decrypts [Meshtastic](https://meshtastic.org/) LoRa mesh network packets.

## Features

- **LoRa Demodulation**: Decodes LoRa packets (SF7-SF12, BW 125/250/500 kHz)
- **Protocol Parsing**: Full Meshtastic protobuf packet parsing
- **Decryption**: AES-128-CTR / AES-256-CTR decryption with channel keys
- **Real-time Display**: ImGui-based packet viewer with node database
- **Channel Management**: Multiple channel support with PSK configuration
- **Configuration Persistence**: Auto-saves settings to JSON

## Supported Meshtastic Features

- Text messages
- Position/GPS data
- Node information (names, hardware, roles)
- Telemetry (battery, voltage, environment sensors)
- Routing information
- Neighbor info
- Admin messages
- Fragment reassembly
- Paxcounter data

## Requirements

- SDR++ (built from source or installed)
- CMake 3.16+
- C++17 compiler
- Dependencies:
  - protobuf (libprotobuf-dev / protobuf)
  - Crypto++ (libcrypto++-dev / cryptopp)
  - liquid-dsp (optional, for enhanced demodulation)
  - nlohmann-json (nlohmann-json3-dev / nlohmann-json)

## Building

### Linux/macOS

```bash
# Set SDR++ root directory (build or install directory)
export SDRPP_CORE_ROOT=/path/to/sdrpp/build

# Run build script
./build.sh --type Release --install
```

### Windows (PowerShell)

```powershell
# Set SDR++ root directory
$env:SDRPP_CORE_ROOT="C:\path\to\sdrpp\build"

# Run build script
.\build.ps1 -BuildType Release -Install
```

### Manual CMake

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DSDRPP_CORE_ROOT=/path/to/sdrpp ..
cmake --build . --config Release
```

## Installation

The plugin installs to SDR++'s plugin directory:
- Linux: `~/.local/lib/sdrpp/plugins/` or `/usr/local/lib/sdrpp/plugins/`
- macOS: `/usr/local/lib/sdrpp/plugins/` or `~/Library/Application Support/SDR++/plugins/`
- Windows: `%APPDATA%\SDR++\plugins\` or `<sdrpp_install>\lib\sdrpp\plugins\`

After installation, restart SDR++ and enable the plugin in the Module Manager.

## Usage

1. Open SDR++
2. Select a source module (RTL-SDR, HackRF, etc.)
3. Tune to a Meshtastic frequency (e.g., 915 MHz US, 868 MHz EU, 433 MHz)
4. Set sample rate to at least 1 MSPS (2 MSPS recommended)
5. Open the Meshtastic module from the Modules menu
6. Configure LoRa parameters (SF, BW, CR) to match the Meshtastic network
7. Add channel PSKs for decryption
8. View decoded packets in real-time

## LoRa Modem Presets

| Preset | SF | BW | CR | Use Case |
|--------|-----|-----|-----|----------|
| LongFast | 7 | 125 kHz | 4/5 | Default, ~8-10 km |
| LongSlow | 11 | 125 kHz | 4/5 | Max range, ~15-20 km |
| VeryLongSlow | 12 | 125 kHz | 4/5 | Extreme range, ~20+ km |
| MediumFast | 7 | 250 kHz | 4/5 | Faster, ~5-7 km |
| Fast | 7 | 500 kHz | 4/5 | High throughput, ~2-4 km |

## Configuration

Settings are saved to `meshtastic_config.json` in SDR++'s config directory:

```json
{
  "lora": {
    "spreading_factor": 7,
    "bandwidth": 125000,
    "coding_rate": 5,
    "preamble_length": 8,
    "sync_word": 18,
    "crc_enabled": true,
    "implicit_header": false,
    "iq_inverted": false,
    "center_frequency": 915000000.0,
    "sample_rate": 2000000.0
  },
  "channels": [
    {
      "name": "Primary",
      "psk": "base64_encoded_psk",
      "enabled": true,
      "index": 0,
      "use_aes256": false
    }
  ]
}
```

## Meshtastic Channel Encryption

Meshtastic uses AES-CTR encryption with:
- **Key derivation**: PBKDF2-HMAC-SHA256(PSK, salt="meshtastic", iterations=100000)
- **Key size**: 16 bytes (AES-128) or 32 bytes (AES-256)
- **Nonce format**: 8 bytes packet_id (LE) + 8 bytes from_node (LE)

To decrypt packets, add the channel's PSK (base64 encoded) in the plugin's channel settings.

## License

MIT License - See LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

## Acknowledgments

- [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus) - The SDR application
- [Meshtastic](https://meshtastic.org/) - The LoRa mesh protocol
- [liquid-dsp](https://github.com/jgaeddert/liquid-dsp) - DSP library
- [Crypto++](https://www.cryptopp.com/) - Cryptography library
- [protobuf](https://github.com/protocolbuffers/protobuf) - Protocol buffers# CI trigger
