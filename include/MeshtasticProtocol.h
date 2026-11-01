#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <array>

namespace MeshtasticProtocol {

// Meshtastic protocol constants
constexpr uint32_t BROADCAST_ADDR = 0xFFFFFFFF;
constexpr uint8_t MESHTASTIC_FLAG = 0x94;
constexpr size_t MESH_HEADER_SIZE = 12; // flag(1) + dest(4) + src(4) + port(1) + len(1) + crc(2) = 13? Wait...

// Port numbers from Meshtastic specification
enum class MSPortNum : uint8_t {
    UNKNOWN_APP = 0,
    TEXT_MESSAGE_APP = 1,
    REMOTE_HARDWARE_APP = 2,
    POSITION_APP = 3,
    NODEINFO_APP = 4,
    ROUTING_APP = 5,
    ADMIN_APP = 6,
    TEXT_MESSAGE_COMPRESSED_APP = 7,
    WAYPOINT_APP = 8,
    AUDIO_APP = 9,
    DETECTION_SENSOR_APP = 10,
    REPLY_APP = 11,
    IP_TUNNEL_APP = 12,
    PAXCOUNTER_APP = 13,
    SERIAL_APP = 14,
    STORE_FORWARD_APP = 15,
    NEIGHBORINFO_APP = 16,
    MqttProxy = 17,
    ZPS_APP = 18,
    TRACEROUTE_APP = 19,
    LOCAL_ONLY_APP = 64,
    RANGE_TEST_APP = 65,
    TELEMETRY_APP = 66,
    PRIVATE_APP = 255
};

// Mesh packet header (on-air format)
struct MeshHeader {
    uint8_t flag;           // 0x94 for Meshtastic
    uint32_t destination;   // little-endian
    uint32_t source;        // little-endian
    uint8_t port;           // MSPortNum
    uint8_t length;         // payload length (excludes CRC)
    uint16_t crc;           // CRC16-CCITT (little-endian)

    // Payload is variable length after header

    static bool deserialize(const uint8_t* data, size_t len, MeshHeader& header) {
        if (len < 13) return false; // Minimum header + 1 byte payload + 2 byte CRC

        header.flag = data[0];
        if (header.flag != MESHTASTIC_FLAG) return false;

        header.destination =
            (uint32_t)data[1] |
            ((uint32_t)data[2] << 8) |
            ((uint32_t)data[3] << 16) |
            ((uint32_t)data[4] << 24);

        header.source =
            (uint32_t)data[5] |
            ((uint32_t)data[6] << 8) |
            ((uint32_t)data[7] << 16) |
            ((uint32_t)data[8] << 24);

        header.port = data[9];
        header.length = data[10];
        header.crc =
            (uint16_t)data[11] |
            ((uint16_t)data[12] << 8);

        // Validate length matches
        if (len != 13 + header.length) return false;

        return true;
    }

    static uint16_t calculateCRC(const uint8_t* data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                if (crc & 1) crc = (crc >> 1) ^ 0x8408;
                else crc >>= 1;
            }
        }
        return crc;
    }
};

// Decoded packet with all metadata
struct DecodedMeshtasticPacket {
    MeshHeader header;
    std::unique_ptr<google::protobuf::Message> protoPacket;
    std::string textMessage;
    std::string fromName;
    std::string toName;
    std::string decodedText;
    bool decrypted = false;
    bool valid = false;
    std::string error;
    double timestamp = 0;
    int rssi = 0;
    float snr = 0.0f;
};

// Key derivation salt (Meshtastic uses "Meshtastic" as salt)
constexpr const char* PBKDF2_SALT = "Meshtastic";

// Crypto helper functions
class MeshtasticCrypto {
public:
    // Verify decrypted payload looks like valid protobuf
    static bool verifyDecryption(const uint8_t* data, size_t len) {
        if (len == 0) return false;
        // Basic heuristic: check if it could be a valid protobuf
        // First byte should be a valid field tag (field_num << 3 | wire_type)
        uint8_t firstByte = data[0];
        uint8_t wireType = firstByte & 0x07;
        uint32_t fieldNum = firstByte >> 3;
        // Wire types: 0=varint, 1=64-bit, 2=length-delimited, 5=32-bit
        return wireType <= 5 && fieldNum > 0;
    }

    // Build Meshtastic nonce: packet_id (4 bytes LE) + from_node (4 bytes LE) + 4 zero bytes
    static std::array<uint8_t, 12> buildNonce(uint32_t packetId, uint32_t fromNode) {
        std::array<uint8_t, 12> nonce = {0};
        nonce[0] = packetId & 0xFF;
        nonce[1] = (packetId >> 8) & 0xFF;
        nonce[2] = (packetId >> 16) & 0xFF;
        nonce[3] = (packetId >> 24) & 0xFF;
        nonce[4] = fromNode & 0xFF;
        nonce[5] = (fromNode >> 8) & 0xFF;
        nonce[6] = (fromNode >> 16) & 0xFF;
        nonce[7] = (fromNode >> 24) & 0xFF;
        // bytes 8-11 are zeros (counter)
        return nonce;
    }
};

} // namespace MeshtasticProtocol