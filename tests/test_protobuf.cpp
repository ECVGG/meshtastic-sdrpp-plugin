// Protobuf round-trip tests for Meshtastic messages

#include <gtest/gtest.h>
#include "mesh.pb.h"
#include "telemetry.pb.h"

using namespace meshtastic;

// Helper to test serialization/deserialization round-trip
template<typename T>
void testRoundTrip(const T& original) {
    std::string serialized = original.SerializeAsString();
    ASSERT_FALSE(serialized.empty()) << "Serialization produced empty string";

    T deserialized;
    ASSERT_TRUE(deserialized.ParseFromString(serialized)) << "Deserialization failed";

    // Compare serialized forms
    std::string reserialized = deserialized.SerializeAsString();
    ASSERT_EQ(serialized, reserialized) << "Round-trip mismatch";
}

TEST(ProtobufTests, MeshPacketRoundTrip) {
    MeshPacket packet;
    packet.set_from(0x12345678);
    packet.set_to(0xFFFFFFFF);
    packet.set_id(42);
    packet.set_channel(0);
    packet.set_hop_limit(3);
    packet.set_hop_start(3);
    packet.set_portnum(PortNum::TEXT_MESSAGE_APP);
    packet.set_want_ack(true);
    packet.set_priority(1);
    packet.set_encrypted(false);

    Data data;
    data.set_text("Hello Meshtastic!");
    data.set_portnum(PortNum::TEXT_MESSAGE_APP);
    packet.set_payload(data.SerializeAsString());

    testRoundTrip(packet);
}

TEST(ProtobufTests, DataRoundTrip) {
    Data data;
    data.set_text("Test message");
    data.set_portnum(PortNum::TEXT_MESSAGE_APP);
    data.set_payload("binary data");
    data.set_reply_id(123);
    data.set_emoji("👍");
    data.set_channel(0);

    testRoundTrip(data);
}

TEST(ProtobufTests, PositionRoundTrip) {
    Position pos;
    pos.set_latitude(37.7749);
    pos.set_longitude(-122.4194);
    pos.set_altitude(100);
    pos.set_time(1234567890);
    pos.set_location_source(1);
    pos.set_gps_accuracy(10);
    pos.set_battery_level(85);
    pos.set_voltage(3700);
    pos.set_channel(0);

    testRoundTrip(pos);
}

TEST(ProtobufTests, UserRoundTrip) {
    User user;
    user.set_id("!12345678");
    user.set_long_name("Test Node");
    user.set_short_name("TN");
    user.set_macaddr("AA:BB:CC:DD:EE:FF");
    user.set_hw_model("TBEAM");
    user.set_public_key("base64pubkey");
    user.set_role("CLIENT");
    user.set_is_licensed(false);

    testRoundTrip(user);
}

TEST(ProtobufTests, TelemetryDeviceMetricsRoundTrip) {
    Telemetry telemetry;
    auto* metrics = telemetry.mutable_device_metrics();
    metrics->set_battery_level(90.0);
    metrics->set_voltage(3800.0);
    metrics->set_channel_utilization(0.15);
    metrics->set_air_util_tx(0.05);
    metrics->set_uptime_seconds(3600);

    testRoundTrip(telemetry);
}

TEST(ProtobufTests, TelemetryEnvironmentMetricsRoundTrip) {
    Telemetry telemetry;
    auto* metrics = telemetry.mutable_environment_metrics();
    metrics->set_temperature(22.5);
    metrics->set_relative_humidity(55.0);
    metrics->set_barometric_pressure(1013.25);
    metrics->set_gas_resistance(10000.0);
    metrics->set_iaq(50.0);

    testRoundTrip(telemetry);
}

TEST(ProtobufTests, TelemetryPowerMetricsRoundTrip) {
    Telemetry telemetry;
    auto* metrics = telemetry.mutable_power_metrics();
    metrics->set_ch0_voltage(3.3);
    metrics->set_ch1_voltage(5.0);
    metrics->set_ch2_voltage(12.0);
    metrics->set_ch3_voltage(0.0);
    metrics->set_ch0_current(0.1);
    metrics->set_ch1_current(0.05);
    metrics->set_ch2_current(0.0);
    metrics->set_ch3_current(0.0);

    testRoundTrip(telemetry);
}

TEST(ProtobufTests, TelemetryLocalStatsRoundTrip) {
    Telemetry telemetry;
    auto* stats = telemetry.mutable_local_stats();
    stats->set_num_packets_rx(100);
    stats->set_num_packets_tx(50);
    stats->set_num_packets_rx_dropped(2);
    stats->set_num_packets_tx_dropped(0);
    stats->set_num_online_nodes(10);
    stats->set_num_total_nodes(15);
    stats->set_num_nbr_nodes(5);
    stats->set_num_nbr_nodes_no_heard(1);

    testRoundTrip(telemetry);
}

TEST(ProtobufTests, AdminMessageRoundTrip) {
    AdminMessage admin;
    admin.set_text("Admin command");
    admin.set_target("!12345678");
    admin.set_command(1);

    testRoundTrip(admin);
}

TEST(ProtobufTests, ChannelRoundTrip) {
    Channel channel;
    channel.set_index(0);
    channel.set_name("Primary");
    channel.set_psk("base64psk");
    channel.set_enabled(true);
    channel.set_uplink_enabled(true);
    channel.set_downlink_enabled(true);
    channel.set_role(1);
    channel.set_flags(0);
    channel.set_slot(0);
    channel.set_module("LoRa");

    testRoundTrip(channel);
}

TEST(ProtobufTests, PaxcountRoundTrip) {
    Paxcount pax;
    pax.set_count(42);
    pax.set_channel(0);

    testRoundTrip(pax);
}

TEST(ProtobufTests, MeshPacketAllPorts) {
    const std::vector<std::pair<PortNum, std::string>> ports = {
        {PortNum::TEXT_MESSAGE_APP, "TEXT_MESSAGE_APP"},
        {PortNum::POSITION_APP, "POSITION_APP"},
        {PortNum::NODEINFO_APP, "NODEINFO_APP"},
        {PortNum::ROUTING_APP, "ROUTING_APP"},
        {PortNum::ADMIN_APP, "ADMIN_APP"},
        {PortNum::STORE_FORWARD_APP, "STORE_FORWARD_APP"},
        {PortNum::PAXCOUNTER_APP, "PAXCOUNTER_APP"},
        {PortNum::PRIVATE_APP, "PRIVATE_APP"}
    };

    for (const auto& [port, name] : ports) {
        MeshPacket packet;
        packet.set_from(0x12345678);
        packet.set_to(0xFFFFFFFF);
        packet.set_id(42);
        packet.set_channel(0);
        packet.set_hop_limit(3);
        packet.set_portnum(port);
        packet.set_priority(1);
        packet.set_encrypted(false);

        // Add appropriate payload based on port
        std::string payload;
        switch (port) {
            case PortNum::TEXT_MESSAGE_APP: {
                Data data;
                data.set_text("Test");
                data.set_portnum(port);
                payload = data.SerializeAsString();
                break;
            }
            case PortNum::POSITION_APP: {
                Position pos;
                pos.set_latitude(0);
                pos.set_longitude(0);
                payload = pos.SerializeAsString();
                break;
            }
            case PortNum::NODEINFO_APP: {
                User user;
                user.set_id("!12345678");
                user.set_long_name("Test");
                payload = user.SerializeAsString();
                break;
            }
            default:
                payload = "test";
                break;
        }
        packet.set_payload(payload);

        testRoundTrip(packet);
    }
}

TEST(ProtobufTests, MeshPacketEncryption) {
    MeshPacket packet;
    packet.set_from(0x12345678);
    packet.set_to(0xFFFFFFFF);
    packet.set_id(42);
    packet.set_channel(0);
    packet.set_hop_limit(3);
    packet.set_portnum(1);
    packet.set_encrypted(true);

    Data data;
    data.set_text("Encrypted message");
    packet.set_payload(data.SerializeAsString());

    testRoundTrip(packet);

    // Verify encrypted flag is preserved
    MeshPacket parsed;
    ASSERT_TRUE(parsed.ParseFromString(packet.SerializeAsString()));
    ASSERT_TRUE(parsed.encrypted());
}

TEST(ProtobufTests, MeshPacketId) {
    MeshPacket packet;
    packet.set_from(0x12345678);
    packet.set_to(0xFFFFFFFF);
    packet.set_id(UINT32_MAX);
    packet.set_channel(0);
    packet.set_hop_limit(3);
    packet.set_portnum(1);
    packet.set_encrypted(false);

    testRoundTrip(packet);

    MeshPacket parsed;
    ASSERT_TRUE(parsed.ParseFromString(packet.SerializeAsString()));
    ASSERT_EQ(parsed.id(), UINT32_MAX);
}

TEST(ProtobufTests, MeshPacketChannel) {
    MeshPacket packet;
    packet.set_from(0x12345678);
    packet.set_to(0xFFFFFFFF);
    packet.set_id(42);
    packet.set_channel(7);  // Max channel index
    packet.set_hop_limit(3);
    packet.set_portnum(1);
    packet.set_encrypted(false);

    testRoundTrip(packet);

    MeshPacket parsed;
    ASSERT_TRUE(parsed.ParseFromString(packet.SerializeAsString()));
    ASSERT_EQ(parsed.channel(), 7);
}