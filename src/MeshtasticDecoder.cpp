#include "MeshtasticDecoder.h"
#include <meshtastic/mesh.pb.h>
#include <meshtastic/telemetry.pb.h>
#include <string>
#include <cstring>
#include <iostream>

std::optional<meshtastic::MeshPacket> MeshtasticDecoder::parsePacket(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        lastError = "Empty payload";
        packetsFailed++;
        return std::nullopt;
    }

    meshtastic::MeshPacket packet;
    if (!packet.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        lastError = "Failed to parse MeshPacket";
        packetsFailed++;
        return std::nullopt;
    }

    packetsParsed++;
    return packet;
}

bool MeshtasticDecoder::parseAppPayload(const meshtastic::MeshPacket& meshPacket,
                                        google::protobuf::Message& appPacket) {
    // MeshPacket uses oneof payload_variant with 'decoded' (Data) or 'encrypted' (bytes)
    // We need to check which variant is present
    if (!meshPacket.has_decoded() && meshPacket.encrypted().empty()) {
        lastError = "Empty application payload";
        return false;
    }

    try {
        if (meshPacket.has_decoded()) {
            // Decoded payload is a Data protobuf message
            const meshtastic::Data& data = meshPacket.decoded();
            return appPacket.ParseFromString(data.payload());
        } else {
            // Encrypted payload is raw bytes
            return appPacket.ParseFromString(meshPacket.encrypted());
        }
    } catch (const std::exception& e) {
        lastError = std::string("Failed to parse app payload: ") + e.what();
        return false;
    }
}

bool MeshtasticDecoder::parseDataPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Data& data) {
    return parseAppPayload(meshPacket, data);
}

bool MeshtasticDecoder::parsePositionPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Position& position) {
    return parseAppPayload(meshPacket, position);
}

bool MeshtasticDecoder::parseUserPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::User& user) {
    return parseAppPayload(meshPacket, user);
}

bool MeshtasticDecoder::parseTelemetryPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Telemetry& telemetry) {
    return parseAppPayload(meshPacket, telemetry);
}

bool MeshtasticDecoder::parseChannelPayload(const meshtastic::MeshPacket& meshPacket, meshtastic::Channel& channel) {
    return parseAppPayload(meshPacket, channel);
}
