#pragma once

#include <ModuleManager/Instance.h>
#include <config.h>
#include <imgui.h>
#include <gui/widgets/folder_select.h>
#include <json.hpp>
#include <string>
#include <memory>
#include <string>
#include <mutex>
#include <fstream>
#include <cstdint>

#include <LoRaDemodulator.h>
#include <MeshtasticDecoder.h>
#include <MeshtasticDecryptor.h>

class MeshtasticDecoderModule : public ModuleManager::Instance {
public:
    MeshtasticDecoderModule(std::string name);
    ~MeshtasticDecoderModule() override;

    void postInit() override;
    void enable() override;
    void disable() override;
    bool isEnabled() override;

    void drawGUI();

private:
    std::string name;
    bool enabled = false;

    // Configuration
    int channelIndex = 0;
    std::string channelName = "Primary";
    std::string pskInput = "";
    bool useAES256 = false;
    std::string appKeyInput = "";
    std::string networkKeyInput = "";

    // LoRa Configuration
    struct LoRaConfig {
        int spreadingFactor = 7;
        int bandwidth = 125000;
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
    bool enabled = false;

    // Components
    class LoRaDemodulator loraDemod;
    class MeshtasticDecoder decoder;
    class MeshtasticDecryptor decryptor;

    // GUI
    class FolderSelect folderSelect;
    std::string pskInput = "";
    std::string channelName = "Primary";
    std::string appKeyInput = "";
    std::string networkKeyInput = "";
    bool useAES256 = false;
    int channelIndex = 0;
};