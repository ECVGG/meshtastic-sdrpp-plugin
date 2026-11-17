#pragma once

#include <module.h>
#include <imgui.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <config.h>
#include <dsp/chain.h>
#include <dsp/demod/lora_demod.h>
#include <utils/flog.h>
#include <core.h>
#include <utils/optionlist.h>
#include <utils/event.h>

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <array>
#include <deque>

// Generated protobuf
#include "mesh.pb.h"

using nlohmann::json;

class MeshtasticDecoder : public ModuleManager::Instance {
public:
    MeshtasticDecoder(std::string name);
    ~MeshtasticDecoder();

    // ModuleManager::Instance virtual methods
    void postInit() override;
    void enable() override;
    void disable() override;
    bool isEnabled() override;

private:
    // GUI handler
    static void menuHandler(void* ctx);

    // Signal processing
    void processIQ(const dsp::complex_t* iq, size_t count);
    void onPacketReceived(const std::vector<uint8_t>& packet);
    bool decodePacket(const std::vector<uint8_t>& rawPacket, meshtastic::MeshPacket& packet);
    bool decryptPacket(meshtastic::MeshPacket& packet, int channelIndex);
    void parsePayload(const meshtastic::MeshPacket& packet);
    void displayPacket(const meshtastic::MeshPacket& packet);

    // LoRa demodulation
    bool initLoRaDemod();
    void configureLoRa(int sf, int bw, int cr);

    // Channel/Key management
    void addChannel(const std::string& name, const std::string& pskBase64, int index);
    void removeChannel(int index);
    void updateChannelKey(int index, const std::string& pskBase64);
    std::vector<uint8_t> deriveKey(const std::string& pskBase64);
    std::vector<uint8_t> buildNonce(const meshtastic::MeshPacket& packet);

    // Configuration
    void loadConfig();
    void saveConfig();

    // Utility
    std::string nodeIdToString(uint32_t id);
    std::string portNumToString(meshtastic::PortNum port);
    std::string formatTime(uint32_t ms);

    // Member variables
    std::string name;
    bool enabled = true;

    // GUI state
    int selectedChannel = 0;
    int loraSf = 7;           // Spreading factor (7-12)
    int loraBw = 125000;      // Bandwidth (125k, 250k, 500k)
    int loraCr = 5;           // Coding rate (5-8)
    double centerFreq = 915000000.0; // Center frequency Hz

    // LoRa demodulator
    dsp::LoRaDemod* loraDemod = nullptr;
    bool loraDemodInitialized = false;

    // IQ stream binding
    dsp::stream<dsp::complex_t>* iqStream = nullptr;
    sigpath::IQFrontEnd* iqFrontEnd = nullptr;

    // Channel encryption keys
    struct ChannelInfo {
        std::string name;
        std::string pskBase64;
        std::vector<uint8_t> key;  // 32-byte AES key
        bool enabled = true;
        int index = 0;
    };
    std::vector<ChannelInfo> channels;
    std::mutex channelsMutex;

    // Packet display
    struct DisplayPacket {
        meshtastic::MeshPacket packet;
        std::string decodedText;
        std::string fromName;
        std::string toName;
        double timestamp;
        int rssi;
        float snr;
        bool decrypted;
    };
    std::deque<DisplayPacket> packetHistory;
    std::mutex packetHistoryMutex;
    size_t maxPacketHistory = 1000;

    // Node database
    struct NodeInfo {
        uint32_t id;
        std::string longName;
        std::string shortName;
        std::string hwModel;
        std::string role;
        double lastSeen;
        int lastRssi;
        float lastSnr;
    };
    std::map<uint32_t, NodeInfo> nodeDatabase;
    std::mutex nodeDatabaseMutex;

    // Configuration
    ConfigManager config;
    bool configLoaded = false;

    // Stats
    std::atomic<uint64_t> packetsReceived{0};
    std::atomic<uint64_t> packetsDecoded{0};
    std::atomic<uint64_t> packetsDecrypted{0};
    std::atomic<uint64_t> packetsFailed{0};

    // Fragment reassembly
    struct FragmentBuffer {
        uint32_t fragmentId;
        uint32_t totalFragments;
        std::vector<std::vector<uint8_t>> fragments;
        double firstFragmentTime;
    };
    std::map<uint32_t, FragmentBuffer> fragmentBuffers;
    std::mutex fragmentBuffersMutex;
};