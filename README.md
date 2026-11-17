# Meshtastic Decoder Plugin for SDR++

A SDR++ decoder module for decoding and decrypting Meshtastic LoRa packets in real-time.

## Features

- **Real-time LoRa demodulation** (SF7-SF12, BW 125kHz, CR 4/5)
- **Meshtastic protocol parsing** - Mesh header + Protobuf payloads
- **AES-CTR decryption** - AppKey (128/256-bit) and NetworkKey support
- **PBKDF2-SHA256 key derivation** from PSK (1000 iterations, salt "Meshtastic")
- **Full GUI integration** - Channel config, LoRa parameters, encryption keys, statistics, decoded messages
- **Auto-save configuration** - Every 30 seconds + on shutdown

## Installation (Portable)

### Option 1: Pre-built DLL (Easiest)
1. Download `meshtastic_decoder.dll` from the [Releases](https://github.com/yourusername/meshtastic-sdrpp-plugin/releases)
2. Copy it to your SDR++ `modules` folder:
   ```
   SDR++/
   └── modules/
       └── meshtastic_decoder.dll
   ```
3. Copy `libprotobuf.dll` (from vcpkg) to the SDR++ root folder if not present
4. Restart SDR++
5. The plugin will appear as "Meshtastic Decoder" in the module list

### Option 2: Build from Source
See [BUILDING.md](BUILDING.md) for detailed build instructions.

## Configuration

The plugin creates two instances by default (defined in `config.json`):
- **Mesh** - Disabled by default
- **Meshtastic Decoder** - Enabled by default

### Settings (GUI Menu)
Right-click the module → "Meshtastic Decoder" to access:

| Section | Settings |
|---------|----------|
| **Channel Configuration** | Channel index (0-7), name, PSK, AES-256 toggle |
| **LoRa Parameters** | SF (7-12), Bandwidth, Coding Rate (4/5-4/8), Preamble, Sync Word (0x34), CRC, Implicit Header, IQ Inverted, Center Frequency, Sample Rate |
| **Encryption** | AES-256 toggle, App Key (hex), Network Key (hex) |
| **Statistics** | Packets received/decoded/decrypted |
| **Decoded Messages** | Timestamp, From/To Node IDs, Port, RSSI, SNR, Decoded content |

### Encryption Keys
- **PSK Method**: Enter a Pre-Shared Key → auto-derives AppKey/NetworkKey via PBKDF2
- **Direct Keys**: Paste 32-byte AppKey (AES-256) or 16-byte NetworkKey (AES-128) as hex strings

### Meshtastic Default Channel
For default Meshtastic channels:
- **PSK**: `1PG7OiApB1nwvP+rz05pAQLK` (Base64)
- **Sync Word**: `0x34`
- **SF**: 7, **BW**: 125kHz, **CR**: 4/5

## Supported Message Types

| Port | Name | Decoded Content |
|------|------|-----------------|
| 1 | TEXT_MESSAGE_APP | Text messages |
| 3 | POSITION_APP | Latitude, Longitude, Altitude |
| 4 | NODEINFO_APP | User long/short name |
| 65 | TELEMETRY_APP | Battery level, voltage |
| * | Others | Raw payload (bytes) |

## Requirements

- SDR++ v1.3+
- Windows x64 (pre-built DLL)
- For building: vcpkg with protobuf, cryptopp, fmt, spdlog, abseil, gtest

## LoRa Parameters (Meshtastic Default)

| Parameter | Value |
|-----------|-------|
| Spreading Factor | 7 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Sync Word | 0x34 |
| Preamble | 8 symbols |
| CRC | Enabled |
| IQ Inverted | No |

## Architecture

```
IQ Samples → VFO → LoRa Demodulator → Mesh Header Parser → AES-CTR Decryptor → Protobuf Parser → GUI
                ↓
         Preamble Detection
         CSS Demodulation (SF7-SF12)
         Deinterleaving (matrix transpose)
         Hamming (7,4) Decoding (CR 4/5)
         CRC16-CCITT Validation
```

## License

MIT License - see [LICENSE](LICENSE) for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

## Acknowledgments

- [Meshtastic](https://meshtastic.org/) - Open source mesh networking
- [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus) - SDR software
- [protobuf](https://github.com/protocolbuffers/protobuf) - Serialization
- [Crypto++](https://www.cryptopp.com/) - Cryptography