#pragma once

#include <module.h>
#include <imgui.h>
#include <json.hpp>
#include <string>
#include <memory>
#include <mutex>
#include <deque>
#include <chrono>
#include <filesystem>

#include <LoRaDemodulator.h>
#include <MeshtasticDecoder.h>
#include <MeshtasticDecryptor.h>
#include <MeshtasticProtocol.h>

// Forward declare SDR++ types to avoid heavy includes in header
namespace sigpath {
    class VFOManager;
    struct VFO;
}
namespace dsp {
    namespace sink {
        template<typename T> class Handler;
    }
    struct complex_t;
}

class MeshtasticDecoderModule : public ModuleManager::Instance {
public:
    explicit MeshtasticDecoderModule(std::string name);
    ~MeshtasticDecoderModule() override;

    void postInit() override;
    void enable() override;
    void disable() override;
    bool isEnabled() override;

private:
    // Configuration
    void applyChannelPreset(int channelIdx);
    void saveConfig();
    void updateLoRaConfig();
    static void menuHandler(void* ctx);
    static void iqHandlerCallback(dsp::complex_t* data, int count, void* ctx);
    void processIQSamples(const dsp::complex_t* data, size_t len);

    std::string name;
    bool enabled = false;
    std::unique_ptr<class PluginConfig> config;

    // Channel/PSK settings
    int selectedChannelIndex = 0;
    int channelIndex = 0;
    std::string channelName = "Primary";
    std::string pskInput = "";
    bool useAES256 = false;
    std::string appKeyInput = "";
    std::string networkKeyInput = "";

    // LoRa Configuration
    struct LoRaConfig {
        int spreadingFactor = 7;
        int bandwidth = 250000;
        int codingRate = 5;
        int preambleLength = 8;
        uint8_t syncWord = 0x34;
        bool crcEnabled = true;
        bool implicitHeader = false;
        bool iqInverted = false;
        double centerFrequency = 915000000.0;
        int sampleRate = 1000000;
    } loraConfig;

    // Statistics
    uint64_t packetsReceived = 0;
    uint64_t packetsDecoded = 0;
    uint64_t packetsDecrypted = 0;

    // Components
    LoRaDemodulator loraDemod;
    MeshtasticDecoder decoder;
    MeshtasticDecryptor decryptor;

    // Signal path
    sigpath::VFO* vfo = nullptr;
    dsp::sink::Handler<dsp::complex_t> iqHandler;

    // Message storage
    struct DecodedMessage {
        std::string timestamp;
        uint32_t fromNode = 0;
        uint32_t toNode = 0;
        std::string portName;
        std::string text;
        std::string position;
        std::string telemetry;
        int rssi = 0;
        float snr = 0.0f;
    };
    std::deque<DecodedMessage> messages;
    std::mutex messagesMutex;
    static constexpr size_t MAX_MESSAGES = 100;
};