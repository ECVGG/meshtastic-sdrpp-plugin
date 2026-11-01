#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <memory>
#include <atomic>
#include <mutex>
#include <deque>
#include <map>

// Forward declarations
namespace meshtastic {
class MeshPacket;
class Data;
class Position;
class NodeInfo;
class Telemetry;
class Routing;
class Admin;
class NeighborInfo;
}

// Complex number type for IQ samples (replaces dsp::complex_t)
struct Complex {
    float re = 0;
    float im = 0;
};

// LoRa modulation parameters
struct LoRaModemConfig {
    int spreadingFactor = 7;      // SF7-SF12
    int bandwidth = 125000;       // 125k, 250k, 500k Hz
    int codingRate = 5;           // 5=4/5, 6=4/6, 7=4/7, 8=4/8
    int preambleLength = 8;
    uint8_t syncWord = 0x12;      // 0x12 for public, 0x2B for private
    bool crcEnabled = true;
    bool implicitHeader = false;
    bool iqInverted = false;
    double centerFrequency = 915000000.0;
    double sampleRate = 1000000.0; // IQ sample rate
};

// Meshtastic on-air packet header (unencrypted)
struct MeshHeader {
    uint8_t flag = 0x94;          // START_FLAG
    uint32_t destination = 0;     // Little-endian
    uint32_t source = 0;          // Little-endian
    uint8_t port = 0;
    uint8_t length = 0;
    std::vector<uint8_t> payload;
    uint16_t crc = 0;             // CRC16-CCITT
};

// Decoded packet with metadata
struct DecodedPacket {
    MeshHeader header;
    std::unique_ptr<meshtastic::MeshPacket> protoPacket;
    std::string decodedText;
    std::string fromName;
    std::string toName;
    double timestamp = 0;
    int rssi = 0;
    float snr = 0;
    bool decrypted = false;
    bool valid = false;
    std::string error;
};

// Channel encryption info
struct ChannelInfo {
    std::string name;
    std::string pskBase64;
    std::array<uint8_t, 32> aesKey;  // AES-128 key (16 bytes) or AES-256 key (32 bytes)
    bool enabled = true;
    int index = 0;
    bool useAES256 = false;
};

// Node database entry
struct NodeInfo {
    uint32_t id = 0;
    std::string longName;
    std::string shortName;
    std::string hwModel;
    std::string role;
    double lastSeen = 0;
    int lastRssi = 0;
    float lastSnr = 0;
    double latitude = 0;
    double longitude = 0;
    int altitude = 0;
    int batteryLevel = 0;
    int voltage = 0;
};

// LoRa demodulator interface
class LoRaDemodulator {
public:
    LoRaDemodulator();
    ~LoRaDemodulator();

    bool init(const LoRaModemConfig& config);
    void configure(const LoRaModemConfig& config);

    // Process IQ samples and return detected packets
    std::vector<std::vector<uint8_t>> process(const Complex* iq, size_t count);

    // Get/set config
    const LoRaModemConfig& getConfig() const { return config; }

    // Signal quality metrics
    float getLastSNR() const { return lastSNR; }
    int getLastRSSI() const { return lastRSSI; }

private:
    LoRaModemConfig config;
    void* demodState = nullptr;  // Opaque pointer to liquid-dsp or internal demodulator
    float lastSNR = 0;
    int lastRSSI = 0;

    // Internal state for packet detection
    struct {
        std::vector<Complex> buffer;
        std::vector<float> preambleCorr;
        size_t sampleCount = 0;
        bool inPacket = false;
        size_t packetStart = 0;
    } detector;
};

// Meshtastic protocol decoder
class MeshtasticDecoder {
public:
    MeshtasticDecoder();
    ~MeshtasticDecoder();

    // Configure LoRa parameters
    void setLoRaConfig(const LoRaModemConfig& config);
    const LoRaModemConfig& getLoRaConfig() const { return loraConfig; }

    // Add/remove channels with encryption keys
    void addChannel(const ChannelInfo& channel);
    void removeChannel(int index);
    void updateChannelKey(int index, const std::string& pskBase64, bool useAES256 = false);
    const std::vector<ChannelInfo>& getChannels() const { return channels; }

    // Process IQ samples - main entry point
    std::vector<DecodedPacket> processIQ(const Complex* iq, size_t count, double sampleRate);

    // Process a single raw packet (for testing or alternative input)
    DecodedPacket processRawPacket(const std::vector<uint8_t>& rawPacket, int rssi = 0, float snr = 0);

    // Get packet history
    const std::deque<DecodedPacket>& getPacketHistory() const { return packetHistory; }
    void setMaxPacketHistory(size_t max) { maxPacketHistory = max; }

    // Get node database
    const std::map<uint32_t, NodeInfo>& getNodes() const { return nodeDatabase; }
    const NodeInfo* getNode(uint32_t id) const;

    // Stats
    uint64_t getPacketsReceived() const { return packetsReceived.load(); }
    uint64_t getPacketsDecoded() const { return packetsDecoded.load(); }
    uint64_t getPacketsDecrypted() const { return packetsDecrypted.load(); }
    uint64_t getPacketsFailed() const { return packetsFailed.load(); }

    // Configuration persistence
    void loadConfig(const nlohmann::json& config);
    nlohmann::json saveConfig() const;

private:
    // LoRa demodulator
    std::unique_ptr<LoRaDemodulator> loraDemod;
    LoRaModemConfig loraConfig;

    // Channels with encryption keys
    std::vector<ChannelInfo> channels;
    mutable std::mutex channelsMutex;

    // Packet history
    std::deque<DecodedPacket> packetHistory;
    mutable std::mutex packetHistoryMutex;
    size_t maxPacketHistory = 1000;

    // Node database
    std::map<uint32_t, NodeInfo> nodeDatabase;
    mutable std::mutex nodeDatabaseMutex;

    // Fragment reassembly
    struct FragmentBuffer {
        uint32_t fragmentId = 0;
        uint32_t totalFragments = 0;
        std::map<uint32_t, std::vector<uint8_t>> fragments;  // index -> data
        double firstFragmentTime = 0;
        uint32_t fromNode = 0;
    };
    std::map<uint32_t, FragmentBuffer> fragmentBuffers;
    mutable std::mutex fragmentBuffersMutex;

    // Stats
    std::atomic<uint64_t> packetsReceived{0};
    std::atomic<uint64_t> packetsDecoded{0};
    std::atomic<uint64_t> packetsDecrypted{0};
    std::atomic<uint64_t> packetsFailed{0};

    // Internal methods
    bool decodeMeshHeader(const uint8_t* data, size_t len, MeshHeader& header);
    bool validateCRC(const uint8_t* data, size_t len);
    bool parseProtobuf(const uint8_t* data, size_t len, meshtastic::MeshPacket& packet);
    bool decryptPayload(meshtastic::MeshPacket& packet, const ChannelInfo& channel);
    bool tryDecryptWithAllChannels(meshtastic::MeshPacket& packet, ChannelInfo& usedChannel);
    std::array<uint8_t, 32> deriveKeyFromPSK(const std::string& pskBase64, bool useAES256);
    std::array<uint8_t, 16> buildNonce(const meshtastic::MeshPacket& packet);
    void handleDecodedPacket(DecodedPacket& packet);
    void updateNodeDatabase(const meshtastic::MeshPacket& packet);
    void reassembleFragments(DecodedPacket& packet);
    std::string nodeIdToString(uint32_t id) const;
    std::string portNumToString(uint8_t port) const;
};