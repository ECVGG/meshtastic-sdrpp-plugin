#include "MeshtasticProtocol.h"
#include "mesh.pb.h"
#include "data.pb.h"
#include "telemetry.pb.h"
#include "remote_hardware.pb.h"
#include "admin.pb.h"
#include "neighbor_info.pb.h"
#include "routing.pb.h"
#include "local_only.pb.h"
#include "pax.pb.h"
#include "store_forward.pb.h"
#include "detection_sensor.pb.h"
#include <cstring>

namespace MeshtasticProtocol {

bool MeshtasticProtocolParser::parsePacket(const uint8_t* data, size_t len, google::protobuf::Message& packet) {
    if (!data || len == 0) return false;

    try {
        return packet.ParseFromArray(data, static_cast<int>(len));
    } catch (...) {
        return false;
    }
}

bool MeshtasticProtocolParser::parseAppPayload(const meshtastic::MeshPacket& meshPacket, google::protobuf::Message& appPacket) {
    const std::string& payload = meshPacket.payload();
    if (payload.empty()) return false;

    try {
        return appPacket.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    } catch (...) {
        return false;
    }
}

bool MeshtasticProtocolParser::extractPayload(const google::protobuf::Message& packet, DecodedMeshtasticPacket& decoded) {
    // Cast to MeshPacket to access fields
    const auto* meshPacket = dynamic_cast<const meshtastic::MeshPacket*>(&packet);
    if (!meshPacket) return false;

    uint32_t port = meshPacket->portnum();

    // Parse the application payload based on portnum
    switch (static_cast<MSPortNum>(port)) {
        case MSPortNum::TEXT_MESSAGE_APP: {
            meshtastic::Data data;
            if (parseAppPayload(*meshPacket, data)) {
                if (data.has_text()) {
                    decoded.textMessage = data.text();
                }
                if (data.has_payload() && !data.payload().empty()) {
                    // Also include binary payload info
                    if (!decoded.textMessage.empty()) decoded.textMessage += " ";
                    decoded.textMessage += "[binary:" + std::to_string(data.payload().size()) + " bytes]";
                }
            }
            break;
        }
        case MSPortNum::POSITION_APP: {
            meshtastic::Position pos;
            if (parseAppPayload(*meshPacket, pos)) {
                decoded.textMessage = "Position: lat=" + std::to_string(pos.latitude()) +
                                     " lon=" + std::to_string(pos.longitude()) +
                                     " alt=" + std::to_string(pos.altitude());
                if (pos.has_battery_level()) {
                    decoded.textMessage += " bat=" + std::to_string(pos.battery_level()) + "%";
                }
            }
            break;
        }
        case MSPortNum::NODEINFO_APP: {
            meshtastic::User user;
            if (parseAppPayload(*meshPacket, user)) {
                decoded.textMessage = "NodeInfo: " + user.long_name() + " (" + user.short_name() + ")";
                decoded.fromName = user.long_name();
                if (!user.hw_model().empty()) {
                    decoded.textMessage += " hw=" + user.hw_model();
                }
                if (!user.role().empty()) {
                    decoded.textMessage += " role=" + user.role();
                }
            }
            break;
        }
        case MSPortNum::TELEMETRY_APP: {
            meshtastic::Telemetry telemetry;
            if (parseAppPayload(*meshPacket, telemetry)) {
                decoded.textMessage = "Telemetry";
                if (telemetry.has_device_metrics()) {
                    const auto& dm = telemetry.device_metrics();
                    decoded.textMessage += ": bat=" + std::to_string(dm.battery_level()) +
                                         "V voltage=" + std::to_string(dm.voltage()) +
                                         " util=" + std::to_string(dm.channel_utilization());
                } else if (telemetry.has_environment_metrics()) {
                    const auto& em = telemetry.environment_metrics();
                    decoded.textMessage += ": temp=" + std::to_string(em.temperature()) +
                                         "C hum=" + std::to_string(em.relative_humidity()) +
                                         "% pres=" + std::to_string(em.barometric_pressure());
                } else {
                    decoded.textMessage += " data";
                }
            }
            break;
        }
        case MSPortNum::ROUTING_APP: {
            meshtastic::RouteDiscovery routing;
            if (parseAppPayload(*meshPacket, routing)) {
                decoded.textMessage = "RouteDiscovery to=!" + std::to_string(routing.route_to());
            } else {
                decoded.textMessage = "Routing data";
            }
            break;
        }
        case MSPortNum::ADMIN_APP: {
            meshtastic::AdminMessage admin;
            if (parseAppPayload(*meshPacket, admin)) {
                decoded.textMessage = "Admin: " + admin.text();
                if (admin.has_config()) {
                    decoded.textMessage += " [config]";
                }
            }
            break;
        }
        case MSPortNum::NEIGHBORINFO_APP: {
            meshtastic::NeighborInfo neighborInfo;
            if (parseAppPayload(*meshPacket, neighborInfo)) {
                decoded.textMessage = "NeighborInfo: " + std::to_string(neighborInfo.neighbors_size()) + " neighbors";
            }
            break;
        }
        case MSPortNum::TRACEROUTE_APP: {
            meshtastic::Traceroute traceroute;
            if (parseAppPayload(*meshPacket, traceroute)) {
                decoded.textMessage = "Traceroute: " + std::to_string(traceroute.route_size()) + " hops";
            }
            break;
        }
        case MSPortNum::PAXCOUNTER_APP: {
            meshtastic::Paxcount pax;
            if (parseAppPayload(*meshPacket, pax)) {
                decoded.textMessage = "Paxcount: " + std::to_string(pax.count()) + " devices";
            }
            break;
        }
        case MSPortNum::DETECTION_SENSOR_APP: {
            meshtastic::DetectionSensor sensor;
            if (parseAppPayload(*meshPacket, sensor)) {
                decoded.textMessage = "DetectionSensor";
            }
            break;
        }
        case MSPortNum::LOCAL_ONLY_APP: {
            decoded.textMessage = "LocalOnly";
            break;
        }
        case MSPortNum::RANGE_TEST_APP: {
            decoded.textMessage = "RangeTest";
            break;
        }
        default: {
            decoded.textMessage = "Port " + std::to_string(port);
            break;
        }
    }

    return true;
}

std::string MeshtasticProtocolParser::portNumToString(uint32_t port) {
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
        case MSPortNum::MQTT_PROXY: return "MQTT_PROXY";
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