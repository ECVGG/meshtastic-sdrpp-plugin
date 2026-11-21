#pragma once

#include <meshtastic/mesh.pb.h>
#include <meshtastic/telemetry.pb.h>
#include <string>
#include <optional>
#include <vector>

class MeshtasticProtocol {
public:
    MeshtasticProtocol() = default;
    ~MeshtasticProtocol() = default;

    // Parse a raw LoRa packet payload into a MeshPacket
    static std::optional<meshtastic::MeshPacket> parsePacket(const std::vector<uint8_t>& payload);

    // Parse application payload based on port number
    static bool parseAppPayload(const meshtastic::MeshPacket& meshPacket,
                                google::protobuf::Message& appPacket);

    // Specific payload parsers (core types that exist in protobuf)
    static bool parseDataPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Data& data);
    static bool parsePositionPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Position& position);
    static bool parseUserPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::User& user);
    static bool parseTelemetryPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Telemetry& telemetry);
    static bool parseChannelPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Channel& channel);

    // Parse MeshPacket header (before protobuf)
    // Returns true if valid Meshtastic packet
    static bool parseMeshHeader(const std::vector<uint8_t>& payload,
                                uint8_t& flags,
                                uint32_t& packetId,
                                uint32_t& fromNode,
                                uint32_t& toNode,
                                uint8_t& hopLimit,
                                uint8_t& hopStart,
                                uint32_t& fromNodeId,
                                uint32_t& toNodeId,
                                uint8_t& portNum,
                                uint8_t& flagsOut);

    // Get port number name
    static std::string getPortName(int portNum);

    // Validate CRC16-CCITT
    static bool validateCRC(const std::vector<uint8_t>& data);

private:
    // Mesh header structure (before protobuf):
    // Flag (1 byte) - 0x94 for Meshtastic
    // Packet ID (4 bytes LE)
    // From Node (4 bytes LE)
    // To Node (4 bytes LE)
    // Hop Limit (1 byte)
    // Hop Start (1 byte)
    // ... then protobuf payload
    // CRC16-CCITT (2 bytes at end)
};
