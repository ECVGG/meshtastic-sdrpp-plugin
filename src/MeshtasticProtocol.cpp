#include "MeshtasticProtocol.h"
#include <mesh.pb.h>
#include <telemetry.pb.h>
#include <string>
#include <algorithm>

std::optional<meshtastic::MeshPacket> MeshtasticProtocol::parsePacket(const std::vector<uint8_t>& payload) {
    // Meshtastic LoRa packet format:
    // Byte 0: Flag (0x94)
    // Bytes 1-4: Packet ID (4 bytes LE)
    // Bytes 5-8: From Node (4 bytes LE)
    // Bytes 9-12: To Node (4 bytes LE)
    // Byte 13: Hop Limit
    // Byte 14: Hop Start
    // Bytes 15...: Protobuf payload
    // Last 2 bytes: CRC16-CCITT

    if (payload.size() < 17) { // Flag (1) + Mesh Header (14) + min protobuf (1) + CRC (2) = 18 minimum
        return std::nullopt;
    }

    // Check flag byte
    if (payload[0] != 0x94) {
        return std::nullopt;
    }

    // Verify CRC16-CCITT before parsing
    size_t crcPos = payload.size() - 2;
    uint16_t expectedCRC = payload[crcPos] | (payload[crcPos + 1] << 8);

    uint16_t calcCRC = 0xFFFF;
    for (size_t i = 0; i < payload.size() - 2; ++i) {
        calcCRC ^= (uint16_t)payload[i] << 8;
        for (int j = 0; j < 8; ++j) {
            if (calcCRC & 0x8000)
                calcCRC = (calcCRC << 1) ^ 0x1021;
            else
                calcCRC <<= 1;
        }
    }

    if (calcCRC != expectedCRC) {
        return std::nullopt;
    }

    // Parse protobuf payload (starts at byte 15, ends before CRC)
    meshtastic::MeshPacket packet;
    size_t protobufStart = 15; // After flag + mesh header (1 + 4 + 4 + 4 + 1 + 1 = 15)
    size_t protobufSize = payload.size() - 2 - protobufStart; // Exclude CRC

    if (!packet.ParseFromArray(payload.data() + protobufStart, protobufSize)) {
        return std::nullopt;
    }

    return packet;
}

bool MeshtasticProtocol::parseAppPayload(const meshtastic::MeshPacket& meshPacket,
                                         google::protobuf::Message& appPacket) {
    if (meshPacket.payload().empty()) {
        return false;
    }

    try {
        return appPacket.ParseFromString(meshPacket.payload());
    } catch (const std::exception&) {
        return false;
    }
}

bool MeshtasticProtocol::parseDataPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Data& data) {
    return parseAppPayload(meshPacket, data);
}

bool MeshtasticProtocol::parsePositionPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Position& position) {
    return parseAppPayload(meshPacket, position);
}

bool MeshtasticProtocol::parseUserPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::User& user) {
    return parseAppPayload(meshPacket, user);
}

bool MeshtasticProtocol::parseTelemetryPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Telemetry& telemetry) {
    return parseAppPayload(meshPacket, telemetry);
}

bool MeshtasticProtocol::parseChannelPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Channel& channel) {
    return parseAppPayload(meshPacket, channel);
}

bool MeshtasticProtocol::parseMeshHeader(const std::vector<uint8_t>& payload,
                                         uint8_t& flags,
                                         uint32_t& packetId,
                                         uint32_t& fromNode,
                                         uint32_t& toNode,
                                         uint8_t& hopLimit,
                                         uint8_t& hopStart,
                                         uint32_t& fromNodeId,
                                         uint32_t& toNodeId,
                                         uint8_t& portNum,
                                         uint8_t& flagsOut) {
    if (payload.size() < 13) { // Minimum header size
        return false;
    }

    // Check flag byte (0x94 for Meshtastic)
    if (payload[0] != 0x94) {
        return false;
    }

    // Extract fields (little-endian)
    flags = payload[0];
    packetId = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);
    fromNode = payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24);
    toNode = payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24);
    hopLimit = payload[13];
    hopStart = payload[14];

    // CRC16-CCITT at end (2 bytes)
    if (payload.size() < 17) { // header(15) + protobuf(min 1) + CRC(2)
        return false;
    }

    // Verify CRC16-CCITT (0x1021)
    size_t crcPos = payload.size() - 2;
    uint16_t expectedCRC = payload[crcPos] | (payload[crcPos + 1] << 8);

    uint16_t calcCRC = 0xFFFF;
    for (size_t i = 0; i < payload.size() - 2; ++i) {
        calcCRC ^= (uint16_t)payload[i] << 8;
        for (int j = 0; j < 8; ++j) {
            if (calcCRC & 0x8000)
                calcCRC = (calcCRC << 1) ^ 0x1021;
            else
                calcCRC <<= 1;
        }
    }

    if (calcCRC != expectedCRC) {
        return false;
    }

    flags = payload[0];
    return true;
}

std::string MeshtasticProtocol::getPortName(uint8_t portNum) {
    switch (portNum) {
        case 0: return "UNKNOWN_APP";
        case 1: return "TEXT_MESSAGE_APP";
        case 2: return "REMOTE_HARDWARE_APP";
        case 3: return "POSITION_APP";
        case 4: return "NODEINFO_APP";
        case 5: return "ROUTING_APP";
        case 6: return "ADMIN_APP";
        case 7: return "TEXT_MESSAGE_APP_OLD";
        case 8: return "WAYPOINT_APP";
        case 9: return "AUDIO_APP";
        case 10: return "DETECTION_SENSOR_APP";
        case 11: return "REPLY_APP";
        case 12: return "IP_TUNNEL_APP";
        case 13: return "PAXCOUNTER_APP";
        case 14: return "SERIAL_APP";
        case 15: return "STORE_FORWARD_APP";
        case 16: return "TRACEROUTE_APP";
        case 17: return "NEIGHBORINFO_APP";
        case 64: return "PRIVATE_APP";
        default: return "UNKNOWN(" + std::to_string(portNum) + ")";
    }
}

bool MeshtasticProtocol::validateCRC(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;

    uint16_t calcCRC = 0xFFFF;
    for (size_t i = 0; i < data.size() - 2; ++i) {
        calcCRC ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j) {
            if (calcCRC & 0x8000)
                calcCRC = (calcCRC << 1) ^ 0x1021;
            else
                calcCRC <<= 1;
        }
    }

    uint16_t expectedCRC = data[data.size() - 2] | (data[data.size() - 1] << 8);
    return calcCRC == expectedCRC;
}