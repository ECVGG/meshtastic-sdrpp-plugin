#pragma once

#include <meshtastic/mesh.pb.h>
#include <meshtastic/telemetry.pb.h>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <MeshtasticDecryptor.h>

class MeshtasticDecoder {
public:
    MeshtasticDecoder() = default;
    ~MeshtasticDecoder() = default;

    // Parse a raw LoRa packet payload into a MeshPacket
    // Returns nullopt if parsing fails
    std::optional<meshtastic::MeshPacket> parsePacket(const std::vector<uint8_t>& payload);

    // Parse application payload based on port number
    // Returns true if successful
    bool parseAppPayload(const meshtastic::MeshPacket& meshPacket,
                         google::protobuf::Message& appPacket);

    // Set decryptor for encrypted packets
    void setDecryptor(class MeshtasticDecryptor* d) { decryptor = d; }

    // Get last parse error
    std::string getLastError() const { return lastError; }

    // Statistics
    uint64_t packetsParsed = 0;
    uint64_t packetsFailed = 0;

private:
    class MeshtasticDecryptor* decryptor = nullptr;
    std::string lastError;

    // Helper methods
    bool parseDataPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Data& data);
    bool parsePositionPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Position& position);
    bool parseUserPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::User& user);
    bool parseTelemetryPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Telemetry& telemetry);
    bool parseChannelPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Channel& channel);
    // ... other payload types
};