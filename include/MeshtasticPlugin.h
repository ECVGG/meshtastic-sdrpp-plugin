#pragma once

#include "MeshtasticDecoder.h"
#include <string>
#include <memory>

// Forward declare SDR++ types to avoid including SDR++ headers
namespace sdrpp {
    class ModuleManager;
    class Module;
    class GUI;
    class Source;
}

// SDR++ plugin API macros
#ifdef _WIN32
    #define SDRPP_EXPORT __declspec(dllexport)
#else
    #define SDRPP_EXPORT __attribute__((visibility("default")))
#endif

// Module info macro (simplified for portable release)
#define SDRPP_MOD_INFO(name, version, description) \
    extern "C" SDRPP_EXPORT const char* SDRPP_MODULE_NAME = name; \
    extern "C" SDRPP_EXPORT const char* SDRPP_MODULE_VERSION = version; \
    extern "C" SDRPP_EXPORT const char* SDRPP_MODULE_DESCRIPTION = description;

// Plugin class using forward declarations
class MeshtasticPlugin {
public:
    MeshtasticPlugin(sdrpp::ModuleManager* manager, const std::string& configPath);
    virtual ~MeshtasticPlugin();

    // Module interface
    virtual void postInit() {}
    virtual void enable() { enabled = true; }
    virtual void disable() { enabled = false; }
    virtual bool isEnabled() const { return enabled; }
    virtual const char* getName() const { return "Meshtastic"; }
    virtual const char* getVersion() const { return "1.0.0"; }
    virtual const char* getDescription() const { return "Meshtastic protocol decoder/decryptor"; }

    // Process IQ data from SDR++
    void processIQ(const std::complex<float>* iq, size_t count, double sampleRate);

    // GUI
    void drawGUI();
    void drawSettings();

    // Configuration
    void loadConfig();
    void saveConfig();

    // Statistics
    uint64_t getPacketsReceived() const;
    uint64_t getPacketsDecoded() const;
    uint64_t getPacketsDecrypted() const;

private:
    sdrpp::ModuleManager* moduleManager = nullptr;
    std::string configPath;
    bool enabled = false;

    // Decoder
    std::unique_ptr<MeshtasticDecoder::MeshtasticDecoder> decoder;

    // GUI state
    bool showWindow = true;
    bool showSettingsWindow = false;
    int selectedChannel = -1;
    char pskInput[256] = "";
    char channelNameInput[128] = "";
    bool useAES256 = false;
    int spreadingFactor = 7;
    int bandwidth = 125000;
    int codingRate = 5;
    int preambleLength = 8;
    uint8_t syncWord = 0x34;
    bool crcEnabled = true;
    bool implicitHeader = false;
    bool iqInverted = false;
    double centerFrequency = 915000000.0;
    double sampleRate = 1000000.0;

    // Statistics for display
    uint64_t lastPacketsReceived = 0;
    uint64_t lastPacketsDecoded = 0;
    uint64_t lastPacketsDecrypted = 0;
};

// C API exports for SDR++ plugin system
extern "C" {
    SDRPP_EXPORT void* SDRPP_MODULE_CREATE_INSTANCE(sdrpp::ModuleManager* manager, const char* configPath);
    SDRPP_EXPORT void SDRPP_MODULE_DELETE_INSTANCE(void* instance);
    SDRPP_EXPORT void SDRPP_MODULE_INIT();
    SDRPP_EXPORT void SDRPP_MODULE_END();
}

SDRPP_MOD_INFO("Meshtastic", "1.0.0", "Meshtastic protocol decoder/decryptor plugin for SDR++")