#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

// Forward declarations for protobuf messages
namespace meshtastic {
class MeshPacket;
class Data;
class Position;
class NodeInfo;
class Telemetry;
class Routing;
class Admin;
class NeighborInfo;
class Waypoint;
class Audio;
class DetectionSensor;
class Reply;
class IP_Tunnel;
class Paxcount;
class StoreForward;
class LocalOnly;
class DeviceUI;
class Channel;
class RadioConfig;
class SerialInterface;
}

// Port numbers (from Meshtastic specification) - using local enum to avoid conflict with protobuf-generated meshtastic::PortNum
enum class MSPortNum : uint8_t {
    UNSET = 0,
    TEXT_MESSAGE_APP = 1,
    POSITION_APP = 2,
    NODEINFO_APP = 3,
    ROUTING_APP = 4,
    ADMIN_APP = 5,
    TEXT_MESSAGE_APP_OLD = 6,
    WAYPOINT_APP = 7,
    AUDIO_APP = 8,
    DETECTION_SENSOR_APP = 9,
    REPLY_APP = 10,
    IP_TUNNEL_APP = 11,
    PAXCOUNTER_APP = 12,
    SERIAL_APP = 13,
    STORE_FORWARD_APP = 14,
    TELEMETRY_APP = 15,
    ZPS_APP = 16,
    PRIVATE_APP = 64,
    ATAK_FORWARD = 65,
    SERIAL = 66
};

// Packet priority levels
enum class PacketPriority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// Hop limit defaults
constexpr uint8_t DEFAULT_HOP_LIMIT = 3;
constexpr uint8_t MAX_HOP_LIMIT = 7;

// Maximum payload sizes
constexpr size_t MAX_PAYLOAD_SIZE = 230;
constexpr size_t MAX_PACKET_SIZE = 256;

// Meshtastic protocol constants
namespace MeshtasticProtocol {
    // Start flag for on-air packets
    constexpr uint8_t START_FLAG = 0x94;

    // Broadcast address
    constexpr uint32_t BROADCAST_ADDR = 0xFFFFFFFF;

    // CRC polynomial (CRC16-CCITT)
    constexpr uint16_t CRC_POLY = 0x1021;
    constexpr uint16_t CRC_INIT = 0xFFFF;

    // Default sync words
    constexpr uint8_t PUBLIC_SYNC_WORD = 0x12;
    constexpr uint8_t PRIVATE_SYNC_WORD = 0x2B;

    // PBKDF2 parameters for key derivation
    constexpr const char* PBKDF2_SALT = "meshtastic";
    constexpr int PBKDF2_ITERATIONS = 100000;

    // Fragment reassembly timeout (seconds)
    constexpr double FRAGMENT_TIMEOUT = 30.0;
}

// Mesh header structure (unencrypted, on-air format)
struct MeshHeader {
    uint8_t flag = MeshtasticProtocol::START_FLAG;
    uint32_t destination = 0;      // Little-endian
    uint32_t source = 0;           // Little-endian
    uint8_t port = 0;
    uint8_t length = 0;
    std::vector<uint8_t> payload;
    uint16_t crc = 0;

    // Serialize to bytes (without CRC)
    std::vector<uint8_t> serialize() const;

    // Deserialize from bytes
    static bool deserialize(const uint8_t* data, size_t len, MeshHeader& header);
};

// Decoded packet with all metadata
struct DecodedMeshtasticPacket {
    MeshHeader header;
    std::shared_ptr<meshtastic::MeshPacket> protoPacket;

    // Decoded payload (variant based on portnum)
    std::string textMessage;
    std::shared_ptr<meshtastic::Position> position;
    std::shared_ptr<meshtastic::NodeInfo> nodeInfo;
    std::shared_ptr<meshtastic::Telemetry> telemetry;
    std::shared_ptr<meshtastic::Routing> routing;
    std::shared_ptr<meshtastic::Admin> admin;
    std::shared_ptr<meshtastic::NeighborInfo> neighborInfo;
    std::shared_ptr<meshtastic::Waypoint> waypoint;
    std::shared_ptr<meshtastic::Audio> audio;
    std::shared_ptr<meshtastic::DetectionSensor> detectionSensor;
    std::shared_ptr<meshtastic::Reply> reply;
    std::shared_ptr<meshtastic::Paxcount> paxcount;

    // Metadata
    std::string fromName;
    std::string toName;
    double timestamp = 0;
    int rssi = 0;
    float snr = 0;
    bool decrypted = false;
    bool valid = false;
    std::string error;

    // Helper methods
    MSPortNum getPortNum() const;
    std::string getPortName() const;
    std::string getSummary() const;
};

// Protocol parser - handles protobuf parsing and packet validation
class MeshtasticProtocolParser {
public:
    MeshtasticProtocolParser();
    ~MeshtasticProtocolParser();

    // Parse raw payload into MeshPacket protobuf
    bool parsePacket(const uint8_t* data, size_t len, meshtastic::MeshPacket& packet);

    // Extract specific payload based on portnum
    bool extractPayload(const meshtastic::MeshPacket& packet, DecodedMeshtasticPacket& decoded);

    // Validate packet structure
    bool validatePacket(const meshtastic::MeshPacket& packet);

    // Create a MeshPacket for transmission
    std::unique_ptr<meshtastic::MeshPacket> createPacket(
        uint32_t from,
        uint32_t to,
        uint32_t id,
        uint8_t channel,
        uint8_t hopLimit,
        MSPortNum portNum,
        const uint8_t* payload,
        size_t payloadLen,
        bool wantAck = false,
        bool encrypted = false
    );

    // Serialize MeshPacket to bytes
    std::vector<uint8_t> serializePacket(const meshtastic::MeshPacket& packet);

    // Calculate CRC16-CCITT
    static uint16_t calculateCRC(const uint8_t* data, size_t len);

    // Verify CRC
    static bool verifyCRC(const uint8_t* data, size_t len, uint16_t expectedCRC);

    // Port number to string
    static std::string portNumToString(uint8_t port);

    // Node ID to string (format: !hex)
    static std::string nodeIdToString(uint32_t id);

    // String to node ID (parse !hex format)
    static uint32_t stringToNodeId(const std::string& str);

private:
    // Internal protobuf parsing helpers
    bool parseDataPayload(const meshtastic::Data& data, DecodedMeshtasticPacket& decoded);
    bool parsePositionPayload(const meshtastic::Position& pos, DecodedMeshtasticPacket& decoded);
    bool parseNodeInfoPayload(const meshtastic::NodeInfo& nodeInfo, DecodedMeshtasticPacket& decoded);
    bool parseTelemetryPayload(const meshtastic::Telemetry& telemetry, DecodedMeshtasticPacket& decoded);
    bool parseRoutingPayload(const meshtastic::Routing& routing, DecodedMeshtasticPacket& decoded);
    bool parseAdminPayload(const meshtastic::Admin& admin, DecodedMeshtasticPacket& decoded);
    bool parseNeighborInfoPayload(const meshtastic::NeighborInfo& neighborInfo, DecodedMeshtasticPacket& decoded);
    bool parseWaypointPayload(const meshtastic::Waypoint& waypoint, DecodedMeshtasticPacket& decoded);
    bool parseAudioPayload(const meshtastic::Audio& audio, DecodedMeshtasticPacket& decoded);
    bool parseDetectionSensorPayload(const meshtastic::DetectionSensor& sensor, DecodedMeshtasticPacket& decoded);
    bool parseReplyPayload(const meshtastic::Reply& reply, DecodedMeshtasticPacket& decoded);
    bool parsePaxcountPayload(const meshtastic::Paxcount& paxcount, DecodedMeshtasticPacket& decoded);
}