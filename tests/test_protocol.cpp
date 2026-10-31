// Protocol parser tests for Meshtastic

#include <gtest/gtest.h>
#include "MeshtasticProtocol.h"
#include "MeshtasticDecryptor.h"
#include "mesh.pb.h"

using namespace meshtastic;

TEST(ProtocolTests, PortNumToString) {
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(0), "UNSET");
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(1), "TEXT_MESSAGE");
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(2), "POSITION");
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(3), "NODEINFO");
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(4), "ROUTING");
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(5), "ADMIN");
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(15), "TELEMETRY");
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(64), "PRIVATE_APP");
    ASSERT_EQ(MeshtasticProtocolParser::portNumToString(99), "UNKNOWN(99)");
}

TEST(ProtocolTests, NodeIdToString) {
    ASSERT_EQ(MeshtasticProtocolParser::nodeIdToString(0x12345678), "!12345678");
    ASSERT_EQ(MeshtasticProtocolParser::nodeIdToString(0xFFFFFFFF), "!FFFFFFFF");
    ASSERT_EQ(MeshtasticProtocolParser::nodeIdToString(0), "!00000000");
    ASSERT_EQ(MeshtasticProtocolParser::nodeIdToString(1), "!00000001");
}

TEST(ProtocolTests, StringToNodeId) {
    ASSERT_EQ(MeshtasticProtocolParser::stringToNodeId("!12345678"), 0x12345678);
    ASSERT_EQ(MeshtasticProtocolParser::stringToNodeId("12345678"), 0x12345678);
    ASSERT_EQ(MeshtasticProtocolParser::stringToNodeId("!FFFFFFFF"), 0xFFFFFFFF);
    ASSERT_EQ(MeshtasticProtocolParser::stringToNodeId("invalid"), 0);
    ASSERT_EQ(MeshtasticProtocolParser::stringToNodeId(""), 0);
}

TEST(ProtocolTests, CRC16Calculation) {
    // Test known CRC16-CCITT values
    uint8_t data1[] = {0x94, 0xFF, 0xFF, 0xFF, 0xFF, 0x78, 0x56, 0x34, 0x12, 0x01, 0x04, 't', 'e', 's', 't'};
    uint16_t crc1 = MeshtasticProtocolParser::calculateCRC(data1, sizeof(data1));
    ASSERT_NE(crc1, 0);

    // Empty data
    uint16_t crcEmpty = MeshtasticProtocolParser::calculateCRC(nullptr, 0);
    ASSERT_EQ(crcEmpty, 0xFFFF);

    // Single byte
    uint8_t data2[] = {0x00};
    uint16_t crc2 = MeshtasticProtocolParser::calculateCRC(data2, 1);
    ASSERT_NE(crc2, 0);
}

TEST(ProtocolTests, CRC16Verification) {
    uint8_t data[] = {0x94, 0xFF, 0xFF, 0xFF, 0xFF, 0x78, 0x56, 0x34, 0x12, 0x01, 0x04, 't', 'e', 's', 't'};
    uint16_t crc = MeshtasticProtocolParser::calculateCRC(data, sizeof(data));

    // Create buffer with CRC appended
    std::vector<uint8_t> withCrc(data, data + sizeof(data));
    withCrc.push_back(crc & 0xFF);
    withCrc.push_back((crc >> 8) & 0xFF);

    ASSERT_TRUE(MeshtasticProtocolParser::verifyCRC(withCrc.data(), withCrc.size(), crc));

    // Wrong CRC
    withCrc[withCrc.size() - 1] ^= 0xFF;
    ASSERT_FALSE(MeshtasticProtocolParser::verifyCRC(withCrc.data(), withCrc.size(), crc));
}

TEST(ProtocolTests, MeshHeaderSerialize) {
    MeshHeader header;
    header.flag = 0x94;
    header.destination = 0xFFFFFFFF;
    header.source = 0x12345678;
    header.port = 1;
    header.payload = {0x01, 0x02, 0x03, 0x04};

    auto serialized = header.serialize();

    // Check structure
    ASSERT_EQ(serialized[0], 0x94);  // flag
    // Destination (LE)
    ASSERT_EQ(serialized[1], 0xFF);
    ASSERT_EQ(serialized[2], 0xFF);
    ASSERT_EQ(serialized[3], 0xFF);
    ASSERT_EQ(serialized[4], 0xFF);
    // Source (LE)
    ASSERT_EQ(serialized[5], 0x78);
    ASSERT_EQ(serialized[6], 0x56);
    ASSERT_EQ(serialized[7], 0x34);
    ASSERT_EQ(serialized[8], 0x12);
    // Port
    ASSERT_EQ(serialized[9], 0x01);
    // Length
    ASSERT_EQ(serialized[10], 0x04);
    // Payload
    ASSERT_EQ(serialized[11], 0x01);
    ASSERT_EQ(serialized[12], 0x02);
    ASSERT_EQ(serialized[13], 0x03);
    ASSERT_EQ(serialized[14], 0x04);
}

TEST(ProtocolTests, MeshHeaderDeserialize) {
    std::vector<uint8_t> data = {
        0x94,                           // flag
        0xFF, 0xFF, 0xFF, 0xFF,         // dest (LE)
        0x78, 0x56, 0x34, 0x12,         // source (LE)
        0x01,                           // port
        0x04,                           // length
        0x01, 0x02, 0x03, 0x04,         // payload
    };

    // Add CRC
    uint16_t crc = MeshtasticProtocolParser::calculateCRC(data.data(), data.size());
    data.push_back(crc & 0xFF);
    data.push_back((crc >> 8) & 0xFF);

    MeshHeader header;
    ASSERT_TRUE(MeshHeader::deserialize(data.data(), data.size(), header));

    ASSERT_EQ(header.flag, 0x94);
    ASSERT_EQ(header.destination, 0xFFFFFFFF);
    ASSERT_EQ(header.source, 0x12345678);
    ASSERT_EQ(header.port, 1);
    ASSERT_EQ(header.length, 4);
    ASSERT_EQ(header.payload.size(), 4);
    ASSERT_EQ(header.payload[0], 0x01);
    ASSERT_EQ(header.crc, crc);
}

TEST(ProtocolTests, MeshHeaderDeserializeInvalidCRC) {
    std::vector<uint8_t> data = {
        0x94, 0xFF, 0xFF, 0xFF, 0xFF, 0x78, 0x56, 0x34, 0x12, 0x01, 0x04, 0x01, 0x02, 0x03, 0x04,
        0x00, 0x00  // Wrong CRC
    };

    MeshHeader header;
    ASSERT_FALSE(MeshHeader::deserialize(data.data(), data.size(), header));
}

TEST(ProtocolTests, MeshHeaderDeserializeTooShort) {
    std::vector<uint8_t> data = {0x94, 0xFF, 0xFF};  // Too short
    MeshHeader header;
    ASSERT_FALSE(MeshHeader::deserialize(data.data(), data.size(), header));
}

TEST(ProtocolTests, CreatePacket) {
    MeshtasticProtocolParser parser;

    auto packet = parser.createPacket(
        0x12345678,      // from
        0xFFFFFFFF,      // to
        42,              // id
        0,               // channel
        3,               // hop limit
        PortNum::TEXT_MESSAGE_APP,
        reinterpret_cast<const uint8_t*>("test"), 4,
        true,            // want ack
        false            // encrypted
    );

    ASSERT_NE(packet, nullptr);
    ASSERT_EQ(packet->from(), 0x12345678);
    ASSERT_EQ(packet->to(), 0xFFFFFFFF);
    ASSERT_EQ(packet->id(), 42);
    ASSERT_EQ(packet->channel(), 0);
    ASSERT_EQ(packet->hop_limit(), 3);
    ASSERT_EQ(packet->portnum(), 1);
    ASSERT_TRUE(packet->want_ack());
    ASSERT_FALSE(packet->encrypted());
}

TEST(ProtocolTests, SerializePacket) {
    MeshtasticProtocolParser parser;

    MeshPacket packet;
    packet.set_from(0x12345678);
    packet.set_to(0xFFFFFFFF);
    packet.set_id(42);
    packet.set_channel(0);
    packet.set_hop_limit(3);
    packet.set_portnum(1);
    packet.set_priority(1);
    packet.set_want_ack(true);
    packet.set_encrypted(false);

    auto serialized = parser.serializePacket(packet);
    ASSERT_FALSE(serialized.empty());

    // Verify we can parse it back
    MeshPacket parsed;
    ASSERT_TRUE(parsed.ParseFromArray(serialized.data(), serialized.size()));
    ASSERT_EQ(parsed.from(), 0x12345678);
    ASSERT_EQ(parsed.to(), 0xFFFFFFFF);
    ASSERT_EQ(parsed.id(), 42);
}

TEST(ProtocolTests, ParsePacket) {
    MeshtasticProtocolParser parser;

    MeshPacket original;
    original.set_from(0x12345678);
    original.set_to(0xFFFFFFFF);
    original.set_id(42);
    original.set_channel(0);
    original.set_hop_limit(3);
    original.set_portnum(1);
    original.set_priority(1);
    original.set_want_ack(true);
    original.set_encrypted(false);

    Data data;
    data.set_text("Test message");
    data.set_portnum(1);
    original.set_payload(data.SerializeAsString());

    std::string serialized = original.SerializeAsString();

    MeshPacket parsed;
    ASSERT_TRUE(parser.parsePacket(
        reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size(), parsed));

    ASSERT_EQ(parsed.from(), original.from());
    ASSERT_EQ(parsed.to(), original.to());
    ASSERT_EQ(parsed.id(), original.id());
    ASSERT_EQ(parsed.portnum(), original.portnum());
}

TEST(ProtocolTests, ValidatePacket) {
    MeshtasticProtocolParser parser;

    // Valid packet
    MeshPacket valid;
    valid.set_from(0x12345678);
    valid.set_to(0xFFFFFFFF);
    valid.set_id(42);
    valid.set_portnum(1);
    ASSERT_TRUE(parser.validatePacket(valid));

    // Invalid - zero from with non-broadcast to
    MeshPacket invalid1;
    invalid1.set_from(0);
    invalid1.set_to(0x12345678);
    invalid1.set_id(42);
    ASSERT_FALSE(parser.validatePacket(invalid1));

    // Invalid - zero id
    MeshPacket invalid2;
    invalid2.set_from(0x12345678);
    invalid2.set_to(0xFFFFFFFF);
    invalid2.set_id(0);
    ASSERT_FALSE(parser.validatePacket(invalid2));

    // Invalid - port > 255
    MeshPacket invalid3;
    invalid3.set_from(0x12345678);
    invalid3.set_to(0xFFFFFFFF);
    invalid3.set_id(42);
    invalid3.set_portnum(300);
    ASSERT_FALSE(parser.validatePacket(invalid3));
}

TEST(ProtocolTests, DecodedPacketSummary) {
    DecodedMeshtasticPacket decoded;
    decoded.header.flag = 0x94;
    decoded.header.source = 0x12345678;
    decoded.header.destination = 0xFFFFFFFF;
    decoded.header.port = 1;
    decoded.fromName = "!12345678";
    decoded.toName = "BROADCAST";
    decoded.textMessage = "Hello world";

    std::string summary = decoded.getSummary();
    ASSERT_NE(summary.find("TEXT_MESSAGE"), std::string::npos);
    ASSERT_NE(summary.find("!12345678"), std::string::npos);
    ASSERT_NE(summary.find("BROADCAST"), std::string::npos);
    ASSERT_NE(summary.find("Hello world"), std::string::npos);
}

TEST(ProtocolTests, DecodedPacketPortName) {
    DecodedMeshtasticPacket decoded;
    decoded.header.port = 1;
    ASSERT_EQ(decoded.getPortNum(), PortNum::TEXT_MESSAGE_APP);
    ASSERT_EQ(decoded.getPortName(), "TEXT_MESSAGE");

    decoded.header.port = 2;
    ASSERT_EQ(decoded.getPortNum(), PortNum::POSITION_APP);
    ASSERT_EQ(decoded.getPortName(), "POSITION");
}