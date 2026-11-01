#include "MeshtasticProtocol.h"
#include "mesh.pb.h"
#include <cstring>

namespace MeshtasticProtocol {

bool MeshtasticProtocolParser::parsePacket(const uint8_t* data, size_t len, meshtastic::MeshPacket& packet) {
    if (!data || len == 0) return false;

    try {
        return packet.ParseFromArray(data, static_cast<int>(len));
    } catch (...) {
        return false;
    }
}

bool MeshtasticProtocolParser::extractPayload(const meshtastic::MeshPacket& packet, DecodedMeshtasticPacket& decoded) {
    // Check port number and extract appropriate payload
    uint32_t port = packet.portnum();

    switch (static_cast<MSPortNum>(port)) {
        case MSPortNum::TEXT_MESSAGE_APP: {
            if (packet.has_data()) {
                const auto& data = packet.data();
                if (data.has_text()) {
                    decoded.textMessage = data.text();
                }
            }
            break;
        }
        case MSPortNum::POSITION_APP: {
            if (packet.has_position()) {
                const auto& pos = packet.position();
                decoded.textMessage = "Position: lat=" + std::to_string(pos.latitude()) +
                                     " lon=" + std::to_string(pos.longitude()) +
                                     " alt=" + std::to_string(pos.altitude());
            }
            break;
        }
        case MSPortNum::NODEINFO_APP: {
            if (packet.has_nodeinfo()) {
                const auto& info = packet.nodeinfo();
                decoded.textMessage = "NodeInfo: " + info.long_name() + " (" + info.short_name() + ")";
            }
            break;
        }
        case MSPortNum::TELEMETRY_APP: {
            if (packet.has_telemetry()) {
                decoded.textMessage = "Telemetry data";
            }
            break;
        }
        case MSPortNum::ROUTING_APP: {
            if (packet.has_routing()) {
                decoded.textMessage = "Routing data";
            }
            break;
        }
        case MSPortNum::ADMIN_APP: {
            if (packet.has_admin()) {
                decoded.textMessage = "Admin message";
            }
            break;
        }
        case MSPortNum::NEIGHBORINFO_APP: {
            if (packet.has_neighborinfo()) {
                decoded.textMessage = "Neighbor info";
            }
            break;
        }
        default: {
            decoded.textMessage = "Port " + std::to_string(port);
            break;
        }
    }

    return true;
}

std::string MeshtasticProtocolParser::portNumToString(uint8_t port) {
    switch (static_cast<MSPortNum>(port)) {
        case MSPortNum::UNKNOWN_APP: return "UNKNOWN";
        case MSPortNum::TEXT_MESSAGE_APP: return "TEXT_MESSAGE";
        case MSPortNum::REMOTE_HARDWARE_APP: return "REMOTE_HARDWARE";
        case MSPortNum::POSITION_APP: return "POSITION";
        case MSPortNum::NODEINFO_APP: return "NODEINFO";
        case MSPortNum::ROUTING_APP: return "ROUTING";
        case MSPortNum::ADMIN_APP: return "ADMIN";
        case MSPortNum::TEXT_MESSAGE_COMPRESSED_APP: return "TEXT_COMPRESSED";
        case MSPortNum::WAYPOINT_APP: return "WAYPOINT";
        case MSPortNum::AUDIO_APP: return "AUDIO";
        case MSPortNum::DETECTION_SENSOR_APP: return "DETECTION_SENSOR";
        case MSPortNum::REPLY_APP: return "REPLY";
        case MSPortNum::IP_TUNNEL_APP: return "IP_TUNNEL";
        case MSPortNum::PAXCOUNTER_APP: return "PAXCOUNTER";
        case MSPortNum::SERIAL_APP: return "SERIAL";
        case MSPortNum::STORE_FORWARD_APP: return "STORE_FORWARD";
        case MSPortNum::NEIGHBORINFO_APP: return "NEIGHBORINFO";
        case MSPortNum::MqttProxy: return "MQTT_PROXY";
        case MSPortNum::ZPS_APP: return "ZPS";
        case MSPortNum::TRACEROUTE_APP: return "TRACEROUTE";
        case MSPortNum::LOCAL_ONLY_APP: return "LOCAL_ONLY";
        case MSPortNum::RANGE_TEST_APP: return "RANGE_TEST";
        case MSPortNum::TELEMETRY_APP: return "TELEMETRY";
        case MSPortNum::PRIVATE_APP: return "PRIVATE";
        default: return "PORT_" + std::to_string(port);
    }
}

uint16_t MeshtasticProtocolParser::calculateCRC(const uint8_t* data, size_t len) {
    return MeshHeader::calculateCRC(data, len);
}

} // namespace MeshtasticProtocol