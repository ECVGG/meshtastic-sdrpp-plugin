#include "MeshtasticPlugin.h"
#include "MeshtasticDecoder.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

using json = nlohmann::json;

// Global state
static std::unique_ptr<MeshtasticPlugin> g_pluginInstance = nullptr;

// Module info - matches SDR++ ModuleManager::ModuleInfo_t
// Name: "meshtastic", Description: "...", Author: "Victor", Version: 1.0.0, Max instances: -1
SDRPP_MOD_INFO{
    /* Name:            */ "meshtastic",
    /* Description:     */ "Meshtastic protocol decoder/decryptor plugin for SDR++",
    /* Author:          */ "Victor",
    /* Version:         */ 1, 0, 0,
    /* Max instances    */ -1
};

MeshtasticPlugin::MeshtasticPlugin(const std::string& configPath)
    : configPath(configPath) {
    decoder = std::make_unique<MeshtasticDecoder::MeshtasticDecoder>();
    loadConfig();
}

void MeshtasticPlugin::postInit() {
    // Module initialization
}

void MeshtasticPlugin::enable() {
    enabled = true;
}

void MeshtasticPlugin::disable() {
    enabled = false;
}

bool MeshtasticPlugin::isEnabled() {
    return enabled;
}

void MeshtasticPlugin::processIQ(const std::complex<float>* iq, size_t count, double sampleRate) {
    if (!enabled || !decoder) return;

    auto packets = decoder->processIQ(iq, count, sampleRate);
    // Packets are handled internally by decoder (history, node database, etc.)
}

void MeshtasticPlugin::drawGUI() {
#if HAS_IMGUI
    if (!showWindow) return;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Meshtastic Decoder", &showWindow)) {
        ImGui::End();
        return;
    }

    // Statistics
    ImGui::Text("Statistics:");
    ImGui::Separator();
    ImGui::Text("Packets Received: %llu", decoder->getPacketsReceived());
    ImGui::Text("Packets Decoded: %llu", decoder->getPacketsDecoded());
    ImGui::Text("Packets Decrypted: %llu", decoder->getPacketsDecrypted());
    ImGui::Text("Success Rate: %.1f%%",
        decoder->getPacketsReceived() > 0 ?
        (100.0 * decoder->getPacketsDecoded() / decoder->getPacketsReceived()) : 0.0);

    ImGui::Spacing();
    ImGui::Separator();

    // Channel management
    if (ImGui::CollapsingHeader("Channels", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto channels = decoder->getChannels();
        for (size_t i = 0; i < channels.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Checkbox("##enabled", const_cast<bool*>(&channels[i].enabled));
            ImGui::SameLine();
            ImGui::Text("%s", channels[i].name.c_str());
            ImGui::SameLine();
            ImGui::Text("(AES%s)", channels[i].useAES256 ? "256" : "128");
            if (ImGui::Button("Remove")) {
                decoder->removeChannel(static_cast<int>(i));
            }
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::InputText("Channel Name", channelNameInput, sizeof(channelNameInput));
        ImGui::InputText("PSK (Base64)", pskInput, sizeof(pskInput));
        ImGui::Checkbox("AES-256", &useAES256);
        if (ImGui::Button("Add Channel")) {
            MeshtasticDecoder::ChannelInfo ch;
            ch.name = channelNameInput;
            ch.pskBase64 = pskInput;
            ch.enabled = true;
            ch.useAES256 = useAES256;
            decoder->addChannel(ch);
            channelNameInput[0] = '\0';
            pskInput[0] = '\0';
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // LoRa Settings
    if (ImGui::CollapsingHeader("LoRa Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        changed |= ImGui::Combo("Spreading Factor", &spreadingFactor, "7\08\09\010\011\012\0");
        changed |= ImGui::Combo("Bandwidth", &bandwidth, "125000\0250000\0500000\0");
        changed |= ImGui::Combo("Coding Rate", &codingRate, "5 (4/5)\06 (4/6)\07 (4/7)\08 (4/8)\0");
        changed |= ImGui::InputInt("Preamble Length", &preambleLength);
        changed |= ImGui::InputScalar("Sync Word", ImGuiDataType_U8, &syncWord);
        changed |= ImGui::Checkbox("CRC Enabled", &crcEnabled);
        changed |= ImGui::Checkbox("Implicit Header", &implicitHeader);
        changed |= ImGui::Checkbox("IQ Inverted", &iqInverted);
        changed |= ImGui::InputDouble("Center Frequency (Hz)", &centerFrequency, 1e6, 1e7, "%.0f");
        changed |= ImGui::InputDouble("Sample Rate (Hz)", &sampleRate, 1e5, 1e6, "%.0f");

        if (changed) {
            MeshtasticDecoder::LoRaModemConfig config;
            config.spreadingFactor = spreadingFactor;
            config.bandwidth = bandwidth;
            config.codingRate = codingRate;
            config.preambleLength = preambleLength;
            config.syncWord = syncWord;
            config.crcEnabled = crcEnabled;
            config.implicitHeader = implicitHeader;
            config.iqInverted = iqInverted;
            config.centerFrequency = centerFrequency;
            config.sampleRate = sampleRate;
            decoder->setLoRaConfig(config);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Node database
    if (ImGui::CollapsingHeader("Nodes")) {
        auto nodes = decoder->getAllNodes();
        ImGui::Columns(6, "nodes");
        ImGui::Text("ID"); ImGui::NextColumn();
        ImGui::Text("Long Name"); ImGui::NextColumn();
        ImGui::Text("Short Name"); ImGui::NextColumn();
        ImGui::Text("Lat"); ImGui::NextColumn();
        ImGui::Text("Lon"); ImGui::NextColumn();
        ImGui::Text("Last Seen"); ImGui::NextColumn();
        ImGui::Separator();

        for (const auto& node : nodes) {
            char idStr[16];
            snprintf(idStr, sizeof(idStr), "!%08X", node.id);
            ImGui::Text("%s", idStr); ImGui::NextColumn();
            ImGui::Text("%s", node.longName.c_str()); ImGui::NextColumn();
            ImGui::Text("%s", node.shortName.c_str()); ImGui::NextColumn();
            ImGui::Text("%.6f", node.latitude); ImGui::NextColumn();
            ImGui::Text("%.6f", node.longitude); ImGui::NextColumn();
            ImGui::Text("%.0fs ago", node.lastSeen > 0 ? (time(nullptr) - static_cast<time_t>(node.lastSeen)) : 0); ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Packet history
    if (ImGui::CollapsingHeader("Recent Packets")) {
        auto history = decoder->getPacketHistory();
        ImGui::Columns(7, "packets");
        ImGui::Text("Time"); ImGui::NextColumn();
        ImGui::Text("From"); ImGui::NextColumn();
        ImGui::Text("To"); ImGui::NextColumn();
        ImGui::Text("Port"); ImGui::NextColumn();
        ImGui::Text("RSSI"); ImGui::NextColumn();
        ImGui::Text("SNR"); ImGui::NextColumn();
        ImGui::Text("Message"); ImGui::NextColumn();
        ImGui::Separator();

        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            const auto& p = *it;
            time_t t = static_cast<time_t>(p.timestamp);
            char timeStr[32];
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&t));
            ImGui::Text("%s", timeStr); ImGui::NextColumn();
            ImGui::Text("%s", p.fromName.c_str()); ImGui::NextColumn();
            ImGui::Text("%s", p.toName.c_str()); ImGui::NextColumn();
            ImGui::Text("%s", MeshtasticDecoder::MeshtasticDecoder::portNumToString(static_cast<uint32_t>(p.header.port)).c_str()); ImGui::NextColumn();
            ImGui::Text("%d", p.rssi); ImGui::NextColumn();
            ImGui::Text("%.1f", p.snr); ImGui::NextColumn();
            ImGui::Text("%s", p.decodedText.c_str()); ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }

    ImGui::End();
#else
    // ImGui not available - GUI disabled
    (void)showWindow;
    (void)showSettingsWindow;
    (void)selectedChannel;
    (void)pskInput;
    (void)channelNameInput;
    (void)useAES256;
    (void)spreadingFactor;
    (void)bandwidth;
    (void)codingRate;
    (void)preambleLength;
    (void)syncWord;
    (void)crcEnabled;
    (void)implicitHeader;
    (void)iqInverted;
    (void)centerFrequency;
    (void)sampleRate;
#endif
}

void MeshtasticPlugin::drawSettings() {
#if HAS_IMGUI
    if (showSettingsWindow) {
        ImGui::Begin("Meshtastic Settings", &showSettingsWindow);
        ImGui::Text("Settings panel - to be implemented");
        ImGui::End();
    }
#else
    (void)showSettingsWindow;
#endif
}

void MeshtasticPlugin::loadConfig() {
    std::ifstream file(configPath);
    if (file.is_open()) {
        try {
            json config;
            file >> config;
            decoder->loadConfig(config);

            // Update GUI state
            auto loraConfig = decoder->getLoRaConfig();
            spreadingFactor = loraConfig.spreadingFactor;
            bandwidth = loraConfig.bandwidth;
            codingRate = loraConfig.codingRate;
            preambleLength = loraConfig.preambleLength;
            syncWord = loraConfig.syncWord;
            crcEnabled = loraConfig.crcEnabled;
            implicitHeader = loraConfig.implicitHeader;
            iqInverted = loraConfig.iqInverted;
            centerFrequency = loraConfig.centerFrequency;
            sampleRate = loraConfig.sampleRate;
        } catch (...) {
            // Ignore config errors, use defaults
        }
    }
}

void MeshtasticPlugin::saveConfig() {
    json config = decoder->saveConfig();
    std::ofstream file(configPath);
    if (file.is_open()) {
        file << config.dump(4);
    }
}

uint64_t MeshtasticPlugin::getPacketsReceived() const {
    return decoder ? decoder->getPacketsReceived() : 0;
}

uint64_t MeshtasticPlugin::getPacketsDecoded() const {
    return decoder ? decoder->getPacketsDecoded() : 0;
}

uint64_t MeshtasticPlugin::getPacketsDecrypted() const {
    return decoder ? decoder->getPacketsDecrypted() : 0;
}

// C API exports
extern "C" {

MOD_EXPORT void _INIT_() {
    // Module initialization - called once when module is loaded
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    // Create a new instance with the given name
    // The config path will be based on the instance name
    std::string configPath = name + "_config.json";
    g_pluginInstance = std::make_unique<MeshtasticPlugin>(configPath);
    return g_pluginInstance.get();
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    // Delete the instance
    if (g_pluginInstance && g_pluginInstance.get() == instance) {
        g_pluginInstance.reset();
    }
}

MOD_EXPORT void _END_() {
    // Module cleanup - called when module is unloaded
    g_pluginInstance.reset();
}

} // extern "C"