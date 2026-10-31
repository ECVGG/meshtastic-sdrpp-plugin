// Protobuf round-trip tests for Meshtastic messages

#include <gtest/gtest.h>
#include "mesh.pb.h"
#include "telemetry.pb.h"
#include "nodeinfo.pb.h"
#include "position.pb.h"
#include "admin.pb.h"
#include "routing.pb.h"
#include "neighbor_info.pb.h"
#include "remote_hardware.pb.h"
#include "detection_sensor.pb.h"
#include "serial_interface.pb.h"
#include "channel.pb.h"
#include "radio.pb.h"
#include "store_forward.pb.h"
#include "local_only.pb.h"
#include "device_ui.pb.h"
#include "pax.pb.h"
#include "serialization.pb.h"
#include "remote_logging.pb.h"
#include "mqtt.pb.h"

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
    packet.set_portnum(1);  // TEXT_MESSAGE_APP
    packet.set_want_ack(true);
    packet.set_priority(1);
    packet.set_encrypted(false);

    Data data;
    data.set_text("Hello Meshtastic!");
    data.set_portnum(1);
    packet.set_payload(data.SerializeAsString());

    testRoundTrip(packet);
}

TEST(ProtobufTests, DataRoundTrip) {
    Data data;
    data.set_text("Test message");
    data.set_portnum(1);
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

TEST(ProtobufTests, NodeInfoRoundTrip) {
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
    admin.set_payload("payload data");

    testRoundTrip(admin);
}

TEST(ProtobufTests, RoutingRoundTrip) {
    Routing routing;
    auto* route = routing.add_routes();
    route->set_to(0x87654321);
    route->set_via(0x11223344);
    route->set_snr(5);
    route->set_hop_count(2);

    testRoundTrip(routing);
}

TEST(ProtobufTests, NeighborInfoRoundTrip) {
    NeighborInfo neighborInfo;
    auto* neighbor = neighborInfo.add_neighbors();
    neighbor->set_node_id(0x11223344);
    neighbor->set_snr(3);
    neighbor->set_last_heard(1234567890);
    neighbor->set_rx_count(50);
    neighbor->set_tx_count(30);

    testRoundTrip(neighborInfo);
}

TEST(ProtobufTests, RemoteHardwareRoundTrip) {
    RemoteHardware hw;
    hw.set_command(RemoteHardware::GET_GPS);
    hw.set_payload("gps request");
    hw.set_target(0x12345678);
    hw.set_request_id(1);

    testRoundTrip(hw);
}

TEST(ProtobufTests, DetectionSensorRoundTrip) {
    DetectionSensor sensor;
    sensor.set_type(DetectionSensor::MOTION);
    sensor.set_node_id(0x12345678);
    sensor.set_value(1.0);
    sensor.set_triggered(true);
    sensor.set_timestamp(1234567890);

    testRoundTrip(sensor);
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

TEST(ProtobufTests, RadioConfigRoundTrip) {
    RadioConfig radio;
    auto* modConfig = radio.mutable_module_config();
    modConfig->set_lora_region(1);  // US
    modConfig->set_lora_modem_preset(0);  // LongFast
    modConfig->set_lora_tx_power(22);
    modConfig->set_lora_ignore_mqtt(false);
    modConfig->set_lora_squelch_enabled(true);
    modConfig->set_lora_squelch_level(-120);

    testRoundTrip(radio);
}

TEST(ProtobufTests, SerialInterfaceRoundTrip) {
    SerialInterface::Request req;
    req.set_type(SerialInterface::Request::GET_CONFIG);

    testRoundTrip(req);

    SerialInterface::Response resp;
    resp.set_status(SerialInterface::Response::OK);
    resp.set_payload("config data");

    testRoundTrip(resp);
}

TEST(ProtobufTests, StoreForwardRoundTrip) {
    StoreForward sf;
    auto* msg = sf.add_messages();
    msg->set_payload("stored message");
    msg->set_from(0x12345678);
    msg->set_to(0x87654321);
    msg->set_id(42);
    msg->set_timestamp(1234567890);
    msg->set_channel(0);
    msg->set_hop_limit(3);
    msg->set_priority(1);

    testRoundTrip(sf);
}

TEST(ProtobufTests, LocalOnlyRoundTrip) {
    LocalOnly local;
    local.set_payload("local data");
    local.set_portnum(1);

    testRoundTrip(local);
}

TEST(ProtobufTests, DeviceUIRoundTrip) {
    DeviceUI ui;
    ui.set_action(DeviceUI::SHOW_TEXT);
    ui.set_text("Hello Display");
    ui.set_duration_ms(5000);

    testRoundTrip(ui);
}

TEST(ProtobufTests, PaxcountRoundTrip) {
    Paxcount pax;
    pax.set_count(42);
    pax.set_channel(0);

    testRoundTrip(pax);
}

TEST(ProtobufTests, SerializationRoundTrip) {
    Serialization ser;
    ser.set_data("serialized data");
    ser.set_format(1);  // protobuf

    testRoundTrip(ser);
}

TEST(ProtobufTests, RemoteLoggingRoundTrip) {
    RemoteLogging log;
    log.set_level(RemoteLogging::INFO);
    log.set_message("Test log message");
    log.set_timestamp(1234567890);
    log.set_module("test_module");

    testRoundTrip(log);
}

TEST(ProtobufTests, MqttRoundTrip) {
    Mqtt mqtt;
    mqtt.set_topic("msh/US/JSON");
    mqtt.set_payload("mqtt payload");
    mqtt.set_retain(false);
    mqtt.set_qos(1);

    testRoundTrip(mqtt);
}

// Test MeshPacket with all port types
TEST(ProtobufTests, MeshPacketAllPorts) {
    const std::vector<std::pair<uint32_t, std::string>> ports = {
        {1, "TEXT_MESSAGE_APP"},
        {2, "POSITION_APP"},
        {3, "NODEINFO_APP"},
        {4, "ROUTING_APP"},
        {5, "ADMIN_APP"},
        {6, "TELEMETRY_APP"},
        {7, "WAYPOINT_APP"},
        {8, "AUDIO_APP"},
        {9, "DETECTION_SENSOR_APP"},
        {10, "REPLY_APP"},
        {11, "IP_TUNNEL_APP"},
        {12, "PAXCOUNTER_APP"},
        {13, "SERIAL_APP"},
        {14, "STORE_FORWARD_APP"},
        {15, "ZPS_APP"},
        {64, "PRIVATE_APP"}
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
            case 1: {
                Data data;
                data.set_text("Test");
                data.set_portnum(port);
                payload = data.SerializeAsString();
                break;
            }
            case 2: {
                Position pos;
                pos.set_latitude(0);
                pos.set_longitude(0);
                payload = pos.SerializeAsString();
                break;
            }
            case 3: {
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

// Test encryption flag handling
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

// Test packet ID handling
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

// Test channel index
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