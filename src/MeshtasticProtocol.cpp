#include "MeshtasticProtocol.h"
#include "mesh.pb.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

using namespace meshtastic;

MeshtasticProtocolParser::MeshtasticProtocolParser() {
}

MeshtasticProtocolParser::~MeshtasticProtocolParser() {
}

bool MeshtasticProtocolParser::parsePacket(const uint8_t* data, size_t len, meshtastic::MeshPacket& packet) {
    if (!data || len == 0) return false;

    try {
        // Parse the protobuf message
        return packet.ParseFromArray(data, static_cast<int>(len));
    } catch (const std::exception&) {
        return false;
    }
}

bool MeshtasticProtocolParser::extractPayload(const meshtastic::MeshPacket& packet, DecodedMeshtasticPacket& decoded) {
    decoded.protoPacket = std::make_shared<MeshPacket>(packet);

    // Extract port number
    PortNum port = static_cast<PortNum>(packet.portnum());

    // Decode based on port number
    switch (port) {
        case PortNum::TEXT_MESSAGE_APP:
            return parseDataPayload(packet.data(), decoded);
        case PortNum::POSITION_APP:
            return parsePositionPayload(packet.position(), decoded);
        case PortNum::NODEINFO_APP:
            return parseNodeInfoPayload(packet.nodeinfo(), decoded);
        case PortNum::TELEMETRY_APP:
            return parseTelemetryPayload(packet.telemetry(), decoded);
        case PortNum::ROUTING_APP:
            return parseRoutingPayload(packet.routing(), decoded);
        case PortNum::ADMIN_APP:
            return parseAdminPayload(packet.admin(), decoded);
        case PortNum::NEIGHBORINFO_APP:
            return parseNeighborInfoPayload(packet.neighborinfo(), decoded);
        case PortNum::WAYPOINT_APP:
            return parseWaypointPayload(packet.waypoint(), decoded);
        case PortNum::AUDIO_APP:
            return parseAudioPayload(packet.audio(), decoded);
        case PortNum::DETECTION_SENSOR_APP:
            return parseDetectionSensorPayload(packet.detectionsensor(), decoded);
        case PortNum::REPLY_APP:
            return parseReplyPayload(packet.reply(), decoded);
        case PortNum::PAXCOUNTER_APP:
            return parsePaxcountPayload(packet.paxcount(), decoded);
        default:
            decoded.error = "Unknown port number: " + std::to_string(packet.portnum());
            return false;
    }
}

bool MeshtasticProtocolParser::validatePacket(const meshtastic::MeshPacket& packet) {
    // Basic validation
    if (packet.from() == 0 && packet.to() != BROADCAST_ADDR) {
        return false;
    }
    if (packet.id() == 0) {
        return false;
    }
    if (packet.portnum() > 255) {
        return false;
    }
    return true;
}

std::unique_ptr<meshtastic::MeshPacket> MeshtasticProtocolParser::createPacket(
    uint32_t from,
    uint32_t to,
    uint32_t id,
    uint8_t channel,
    uint8_t hopLimit,
    PortNum portNum,
    const uint8_t* payload,
    size_t payloadLen,
    bool wantAck,
    bool encrypted) {

    auto packet = std::make_unique<MeshPacket>();
    packet->set_from(from);
    packet->set_to(to);
    packet->set_id(id);
    packet->set_channel(channel);
    packet->set_hop_limit(hopLimit);
    packet->set_hop_start(hopLimit);
    packet->set_portnum(static_cast<uint32_t>(portNum));
    packet->set_want_ack(wantAck);
    packet->set_priority(1);  // Normal priority
    packet->set_encrypted(encrypted);

    // Set payload based on port
    // For text messages, we'd need to create a Data message
    if (portNum == PortNum::TEXT_MESSAGE_APP && payload && payloadLen > 0) {
        Data data;
        data.set_portnum(static_cast<uint32_t>(portNum));
        data.set_payload(payload, payloadLen);
        packet->set_payload(data.SerializeAsString());
    } else if (payload && payloadLen > 0) {
        packet->set_payload(std::string(reinterpret_cast<const char*>(payload), payloadLen));
    }

    return packet;
}

std::vector<uint8_t> MeshtasticProtocolParser::serializePacket(const meshtastic::MeshPacket& packet) {
    std::string serialized = packet.SerializeAsString();
    return std::vector<uint8_t>(serialized.begin(), serialized.end());
}

uint16_t MeshtasticProtocolParser::calculateCRC(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool MeshtasticProtocolParser::verifyCRC(const uint8_t* data, size_t len, uint16_t expectedCRC) {
    if (!data || len < 2) return false;
    // CRC is the last 2 bytes
    if (len < 2) return false;
    uint16_t calculated = calculateCRC(data, len - 2);
    return calculated == expectedCRC;
}

std::string MeshtasticProtocolParser::portNumToString(uint8_t port) {
    switch (static_cast<PortNum>(port)) {
        case PortNum::UNSET: return "UNSET";
        case PortNum::TEXT_MESSAGE_APP: return "TEXT_MESSAGE";
        case PortNum::POSITION_APP: return "POSITION";
        case PortNum::NODEINFO_APP: return "NODEINFO";
        case PortNum::ROUTING_APP: return "ROUTING";
        case PortNum::ADMIN_APP: return "ADMIN";
        case PortNum::TELEMETRY_APP: return "TELEMETRY";
        case PortNum::WAYPOINT_APP: return "WAYPOINT";
        case PortNum::AUDIO_APP: return "AUDIO";
        case PortNum::DETECTION_SENSOR_APP: return "DETECTION_SENSOR";
        case PortNum::REPLY_APP: return "REPLY";
        case PortNum::IP_TUNNEL_APP: return "IP_TUNNEL";
        case PortNum::PAXCOUNTER_APP: return "PAXCOUNTER";
        case PortNum::SERIAL_APP: return "SERIAL";
        case PortNum::STORE_FORWARD_APP: return "STORE_FORWARD";
        case PortNum::ZPS_APP: return "ZPS";
        case PortNum::PRIVATE_APP: return "PRIVATE_APP";
        case PortNum::ATAK_FORWARD: return "ATAK_FORWARD";
        default: return "UNKNOWN(" + std::to_string(port) + ")";
    }
}

std::string MeshtasticProtocolParser::nodeIdToString(uint32_t id) {
    std::stringstream ss;
    ss << "!" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << id;
    return ss.str();
}

uint32_t MeshtasticProtocolParser::stringToNodeId(const std::string& str) {
    if (str.empty()) return 0;
    size_t start = (str[0] == '!') ? 1 : 0;
    try {
        return std::stoul(str.substr(start), nullptr, 16);
    } catch (const std::exception&) {
        return 0;
    }
}

// MeshHeader serialization
std::vector<uint8_t> MeshHeader::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(11 + payload.size());

    result.push_back(flag);

    // Destination (little-endian)
    for (int i = 0; i < 4; i++) {
        result.push_back((destination >> (i * 8)) & 0xFF);
    }

    // Source (little-endian)
    for (int i = 0; i < 4; i++) {
        result.push_back((source >> (i * 8)) & 0xFF);
    }

    result.push_back(port);
    result.push_back(static_cast<uint8_t>(payload.size()));

    result.insert(result.end(), payload.begin(), payload.end());

    return result;
}

bool MeshHeader::deserialize(const uint8_t* data, size_t len, MeshHeader& header) {
    if (!data || len < 11) return false;

    size_t offset = 0;
    header.flag = data[offset++];

    header.destination = 0;
    for (int i = 0; i < 4; i++) {
        header.destination |= (uint32_t)data[offset++] << (i * 8);
    }

    header.source = 0;
    for (int i = 0; i < 4; i++) {
        header.source |= (uint32_t)data[offset++] << (i * 8);
    }

    header.port = data[offset++];
    header.length = data[offset++];

    if (len < offset + header.length + 2) return false;

    header.payload.assign(data + offset, data + offset + header.length);
    offset += header.length;

    header.crc = (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);

    // Verify CRC
    auto serialized = header.serialize();
    uint16_t calcCRC = MeshtasticProtocolParser::calculateCRC(serialized.data(), serialized.size());
    return calcCRC == header.crc;
}

// Payload parsing implementations
bool MeshtasticProtocolParser::parseDataPayload(const meshtastic::Data& data, DecodedMeshtasticPacket& decoded) {
    decoded.textMessage = data.text();
    return true;
}

bool MeshtasticProtocolParser::parsePositionPayload(const meshtastic::Position& pos, DecodedMeshtasticPacket& decoded) {
    decoded.position = std::make_shared<Position>(pos);
    return true;
}

bool MeshtasticProtocolParser::parseNodeInfoPayload(const meshtastic::NodeInfo& nodeInfo, DecodedMeshtasticPacket& decoded) {
    decoded.nodeInfo = std::make_shared<NodeInfo>(nodeInfo);
    return true;
}

bool MeshtasticProtocolParser::parseTelemetryPayload(const meshtastic::Telemetry& telemetry, DecodedMeshtasticPacket& decoded) {
    decoded.telemetry = std::make_shared<Telemetry>(telemetry);
    return true;
}

bool MeshtasticProtocolParser::parseRoutingPayload(const meshtastic::Routing& routing, DecodedMeshtasticPacket& decoded) {
    decoded.routing = std::make_shared<Routing>(routing);
    return true;
}

bool MeshtasticProtocolParser::parseAdminPayload(const meshtastic::Admin& admin, DecodedMeshtasticPacket& decoded) {
    decoded.admin = std::make_shared<Admin>(admin);
    return true;
}

bool MeshtasticProtocolParser::parseNeighborInfoPayload(const meshtastic::NeighborInfo& neighborInfo, DecodedMeshtasticPacket& decoded) {
    decoded.neighborInfo = std::make_shared<NeighborInfo>(neighborInfo);
    return true;
}

bool MeshtasticProtocolParser::parseWaypointPayload(const meshtastic::Waypoint& waypoint, DecodedMeshtasticPacket& decoded) {
    decoded.waypoint = std::make_shared<Waypoint>(waypoint);
    return true;
}

bool MeshtasticProtocolParser::parseAudioPayload(const meshtastic::Audio& audio, DecodedMeshtasticPacket& decoded) {
    decoded.audio = std::make_shared<Audio>(audio);
    return true;
}

bool MeshtasticProtocolParser::parseDetectionSensorPayload(const meshtastic::DetectionSensor& sensor, DecodedMeshtasticPacket& decoded) {
    decoded.detectionSensor = std::make_shared<DetectionSensor>(sensor);
    return true;
}

bool MeshtasticProtocolParser::parseReplyPayload(const meshtastic::Reply& reply, DecodedMeshtasticPacket& decoded) {
    decoded.reply = std::make_shared<Reply>(reply);
    return true;
}

bool MeshtasticProtocolParser::parsePaxcountPayload(const meshtastic::Paxcount& paxcount, DecodedMeshtasticPacket& decoded) {
    decoded.paxcount = std::make_shared<Paxcount>(paxcount);
    return true;
}

// DecodedMeshtasticPacket methods
PortNum DecodedMeshtasticPacket::getPortNum() const {
    if (!protoPacket) return PortNum::UNSET;
    return static_cast<PortNum>(protoPacket->portnum());
}

std::string DecodedMeshtasticPacket::getPortName() const {
    return MeshtasticProtocolParser::portNumToString(getPortNum());
}

std::string DecodedMeshtasticPacket::getSummary() const {
    std::stringstream ss;
    ss << "[" << getPortName() << "] ";

    if (!fromName.empty()) {
        ss << fromName;
    } else {
        ss << MeshtasticProtocolParser::nodeIdToString(header.source);
    }

    ss << " -> ";

    if (header.destination == BROADCAST_ADDR) {
        ss << "BROADCAST";
    } else if (!toName.empty()) {
        ss << toName;
    } else {
        ss << MeshtasticProtocolParser::nodeIdToString(header.destination);
    }

    if (!textMessage.empty()) {
        ss << ": " << textMessage;
    }

    if (decrypted) {
        ss << " [DECRYPTED]";
    }

    return ss.str();
}