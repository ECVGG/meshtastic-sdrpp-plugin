#pragma once

#include "MeshtasticProtocol.h"
#include "LoRaDemodulator.h"
#include <vector>
#include <string>
#include <array>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <google/protobuf/message.h>

// Forward declaration for nlohmann::json (include in cpp)
namespace nlohmann {
    class json;
}

namespace MeshtasticDecoder {

// Channel information
struct ChannelInfo {
    std::string name;
    std::string pskBase64;
    std::array<uint8_t, 32> aesKey = {0};
    bool enabled = true;
    int index = 0;
    bool useAES256 = false;
};

// Node information for display
struct NodeInfo {
    uint32_t id = 0;
    std::string longName;
    std::string shortName;
    std::string hwModel;
    uint8_t role = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    uint8_t batteryLevel = 0;
    double lastSeen = 0;
    int lastRssi = 0;
    float lastSnr = 0.0f;
};

// Fragment buffer for reassembly
struct FragmentBuffer {
    uint32_t fragmentId = 0;
    uint32_t totalFragments = 0;
    uint32_t fromNode = 0;
    double firstFragmentTime = 0;
    std::unordered_map<uint32_t, std::vector<uint8_t>> fragments;
};

// Decoded packet with all metadata (alias to avoid duplication)
using DecodedPacket = MeshtasticProtocol::DecodedMeshtasticPacket;

class MeshtasticDecoder {
public:
    MeshtasticDecoder();
    ~MeshtasticDecoder();

    // LoRa configuration
    void setLoRaConfig(const LoRaModemConfig& config);
    LoRaModemConfig getLoRaConfig() const { return loraConfig; }

    // Channel management
    void addChannel(const ChannelInfo& channel);
    void removeChannel(int index);
    void updateChannelKey(int index, const std::string& pskBase64, bool useAES256);
    std::vector<ChannelInfo> getChannels() const;

    // Process IQ samples
    std::vector<DecodedPacket> processIQ(const Complex* iq, size_t count, double sampleRate);

    // Process raw packet bytes
    DecodedPacket processRawPacket(const std::vector<uint8_t>& rawPacket, int rssi, float snr);

    // Statistics
    uint64_t getPacketsReceived() const { return packetsReceived; }
    uint64_t getPacketsDecoded() const { return packetsDecoded; }
    uint64_t getPacketsDecrypted() const { return packetsDecrypted; }
    uint64_t getPacketsFailed() const { return packetsFailed; }

    // Node database
    const NodeInfo* getNode(uint32_t id) const;
    std::vector<NodeInfo> getAllNodes() const;

    // Configuration persistence
    void loadConfig(const nlohmann::json& config);
    nlohmann::json saveConfig() const;

    // Packet history
    std::deque<DecodedPacket> getPacketHistory() const;
    void setMaxPacketHistory(size_t max) { maxPacketHistory = max; }

private:
    // LoRa demodulator
    std::unique_ptr<LoRaDemodulator> loraDemod;
    LoRaModemConfig loraConfig;

    // Channels
    std::vector<ChannelInfo> channels;
    mutable std::mutex channelsMutex;

    // Statistics
    uint64_t packetsReceived = 0;
    uint64_t packetsDecoded = 0;
    uint64_t packetsDecrypted = 0;
    uint64_t packetsFailed = 0;

    // Node database
    std::unordered_map<uint32_t, NodeInfo> nodeDatabase;
    mutable std::mutex nodeDatabaseMutex;

    // Fragment reassembly
    std::unordered_map<uint32_t, FragmentBuffer> fragmentBuffers;
    mutable std::mutex fragmentBuffersMutex;

    // Packet history
    std::deque<DecodedPacket> packetHistory;
    mutable std::mutex packetHistoryMutex;
    size_t maxPacketHistory = 1000;

    // Internal methods
    bool processRawPacketInternal(const std::vector<uint8_t>& rawPacket, DecodedPacket& decoded);
    bool decodeMeshHeader(const uint8_t* data, size_t len, MeshtasticProtocol::MeshHeader& header);
    bool validateCRC(const uint8_t* data, size_t len);
    bool tryDecryptWithAllChannels(google::protobuf::Message& packet, ChannelInfo& usedChannel);

    std::array<uint8_t, 32> deriveKeyFromPSK(const std::string& pskBase64, bool useAES256);
    std::array<uint8_t, 12> buildNonce(const google::protobuf::Message& packet);

    void handleDecodedPacket(DecodedPacket& packet);
    void updateNodeDatabase(const google::protobuf::Message& packet);
    void reassembleFragments(DecodedPacket& packet);
    void displayPacket(DecodedPacket& packet);

    std::string nodeIdToString(uint32_t id) const;
    std::string portNumToString(uint8_t port) const;
    std::string formatTime(uint32_t ms) const;
};

} // namespace MeshtasticDecoder