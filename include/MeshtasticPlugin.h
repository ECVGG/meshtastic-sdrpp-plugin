#pragma once

#include "MeshtasticDecoder.h"
#include <string>
#include <memory>
#include <complex>

// Include SDR++ module header for proper plugin API
#include <module.h>
#include <gui/gui.h>

// Try to include ImGui - it's bundled with SDR++
#if defined(__has_include)
#  if __has_include(<imgui.h>)
#    define HAS_IMGUI 1
#    include <imgui.h>
#  endif
#endif

#ifndef HAS_IMGUI
#  define HAS_IMGUI 0
// Minimal stub types for when ImGui is not available
namespace ImGui {
    struct ImVec2 { float x, y; ImVec2(float x_, float y_) : x(x_), y(y_) {} };
    enum ImGuiCond_ { ImGuiCond_FirstUseEver = 1 };
    enum ImGuiTreeNodeFlags_ { ImGuiTreeNodeFlags_DefaultOpen = 1 };
    enum ImGuiDataType_ { ImGuiDataType_U8 = 1 };
    inline void SetNextWindowSize(ImVec2, ImGuiCond_) {}
    inline bool Begin(const char*, bool*) { return false; }
    inline void End() {}
    inline void Text(const char*, ...) {}
    inline void Separator() {}
    inline void Spacing() {}
    inline bool CollapsingHeader(const char*, int) { return false; }
    inline bool Checkbox(const char*, bool*) { return false; }
    inline void SameLine() {}
    inline bool Button(const char*) { return false; }
    inline void PushID(int) {}
    inline void PopID() {}
    inline bool Combo(const char*, int*, const char*) { return false; }
    inline bool InputInt(const char*, int*) { return false; }
    inline bool InputScalar(const char*, ImGuiDataType_, void*) { return false; }
    inline bool InputText(const char*, char*, size_t) { return false; }
    inline bool InputDouble(const char*, double*, double, double, const char*) { return false; }
    inline void Columns(int, const char*) {}
    inline void NextColumn() {}
    inline void Strftime(char*, size_t, const char*, const std::tm*) {}
    inline std::tm* localtime(const time_t*) { return nullptr; }
}

// Use SDR++ module API macros
#define MOD_EXPORT extern "C" __declspec(dllexport)

// Plugin class - inherits from SDR++ ModuleManager::Instance
class MeshtasticPlugin : public ModuleManager::Instance {
public:
    MeshtasticPlugin(const std::string& configPath);
    virtual ~MeshtasticPlugin() = default;

    // Module interface (matches ModuleManager::Instance)
    void postInit() override;
    void enable() override;
    void disable() override;
    bool isEnabled() override;

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
};

// Module info (defined in MeshtasticPlugin.cpp)
// SDRPP_MOD_INFO expands to: MOD_EXPORT const ModuleManager::ModuleInfo_t _INFO_
SDRPP_MOD_INFO;

// C API exports (matching SDR++ expectations)
MOD_EXPORT void _INIT_();
MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name);
MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance);
MOD_EXPORT void _END_();