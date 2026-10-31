#include "MeshtasticPlugin.h"
#include "MeshtasticDecoder.h"
#include <module.h>
#include <imgui.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <config.h>
#include <utils/flog.h>
#include <utils/event.h>
#include <nlohmann/json.hpp>

using nlohmann::json;

MeshtasticPlugin::MeshtasticPlugin(std::string name) {
    this->name = name;
    decoder = std::make_unique<MeshtasticDecoder>();

    // Register GUI menu entry
    gui::menu.registerEntry(name, menuHandler, this);
}

MeshtasticPlugin::~MeshtasticPlugin() {
    gui::menu.removeEntry(name);
}

void MeshtasticPlugin::postInit() {
    // Bind to IQ stream after all modules loaded
    iqFrontEnd = sigpath::iqFrontEnd;
    if (iqFrontEnd) {
        iqStream = iqFrontEnd->bindIQStream(name);
        flog::info("[Meshtastic] Bound to IQ stream");
    } else {
        flog::warn("[Meshtastic] IQFrontEnd not available");
    }

    // Load configuration
    loadConfig();
}

void MeshtasticPlugin::enable() {
    enabled = true;
    flog::info("[Meshtastic] Plugin enabled");
}

void MeshtasticPlugin::disable() {
    enabled = false;
    flog::info("[Meshtastic] Plugin disabled");
}

bool MeshtasticPlugin::isEnabled() {
    return enabled;
}

void MeshtasticPlugin::menuHandler(void* ctx) {
    MeshtasticPlugin* _this = static_cast<MeshtasticPlugin*>(ctx);
    _this->drawMenu();
}

void MeshtasticPlugin::drawMenu() {
    // Main window title
    SmGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Meshtastic Decoder");
    SmGui::Separator();

    // Statistics
    SmGui::Text("Packets: RX=%llu Decoded=%llu Decrypted=%llu Failed=%llu",
                decoder->getPacketsReceived(),
                decoder->getPacketsDecoded(),
                decoder->getPacketsDecrypted(),
                decoder->getPacketsFailed());

    // LoRa Configuration
    if (ImGui::CollapsingHeader("LoRa Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& config = decoder->getLoRaConfig();

        SmGui::LeftLabel("Center Frequency");
        SmGui::FillWidth();
        if (SmGui::InputDouble("##freq", &config.centerFrequency, 100000, 1000000, "%.3f MHz")) {
            decoder->setLoRaConfig(config);
        }

        SmGui::LeftLabel("Spreading Factor");
        SmGui::FillWidth();
        if (SmGui::Combo("##sf", &config.spreadingFactor, "SF7\0SF8\0SF9\0SF10\0SF11\0SF12\0")) {
            decoder->setLoRaConfig(config);
        }

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        int bwIdx = 0;
        if (config.bandwidth == 125000) bwIdx = 0;
        else if (config.bandwidth == 250000) bwIdx = 1;
        else if (config.bandwidth == 500000) bwIdx = 2;
        if (SmGui::Combo("##bw", &bwIdx, "125 kHz\0250 kHz\0500 kHz\0")) {
            config.bandwidth = (bwIdx == 0) ? 125000 : (bwIdx == 1) ? 250000 : 500000;
            decoder->setLoRaConfig(config);
        }

        SmGui::LeftLabel("Coding Rate");
        SmGui::FillWidth();
        if (SmGui::Combo("##cr", &config.codingRate, "4/5\04/6\04/7\04/8\0")) {
            config.codingRate = config.codingRate + 5;  // 0->5, 1->6, etc.
            decoder->setLoRaConfig(config);
        }

        SmGui::LeftLabel("Sync Word");
        SmGui::FillWidth();
        if (SmGui::Combo("##sync", &config.syncWord, "Public (0x12)\0Private (0x2B)\0")) {
            config.syncWord = (config.syncWord == 0) ? 0x12 : 0x2B;
            decoder->setLoRaConfig(config);
        }

        SmGui::LeftLabel("Preamble Length");
        SmGui::FillWidth();
        if (SmGui::InputInt("##preamble", &config.preambleLength, 1, 10)) {
            config.preambleLength = std::clamp(config.preambleLength, 6, 65535);
            decoder->setLoRaConfig(config);
        }

        SmGui::LeftLabel("Sample Rate");
        SmGui::FillWidth();
        if (SmGui::InputDouble("##sr", &config.sampleRate, 100000, 1000000, "%.0f")) {
            decoder->setLoRaConfig(config);
        }
    }

    // Channel Management
    if (ImGui::CollapsingHeader("Channels & Encryption", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto channels = decoder->getChannels();

        for (size_t i = 0; i < channels.size(); i++) {
            const auto& ch = channels[i];
            std::string header = ch.name.empty() ? "Channel " + std::to_string(i) : ch.name;
            if (!ch.enabled) header += " (disabled)";

            if (ImGui::TreeNode(header.c_str())) {
                SmGui::LeftLabel("Name");
                SmGui::FillWidth();
                char nameBuf[64];
                strncpy(nameBuf, ch.name.c_str(), sizeof(nameBuf) - 1);
                nameBuf[sizeof(nameBuf) - 1] = 0;
                if (SmGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                    // Update name - need to cast away const
                }

                SmGui::LeftLabel("PSK (Base64)");
                SmGui::FillWidth();
                char pskBuf[256];
                strncpy(pskBuf, ch.pskBase64.c_str(), sizeof(pskBuf) - 1);
                pskBuf[sizeof(pskBuf) - 1] = 0;
                if (SmGui::InputText("##psk", pskBuf, sizeof(pskBuf))) {
                    decoder->updateChannelKey(static_cast<int>(i), pskBuf);
                }

                SmGui::LeftLabel("AES-256");
                SmGui::FillWidth();
                bool useAES256 = ch.useAES256;
                if (SmGui::Checkbox("##aes256", &useAES256)) {
                    // Would need update method
                }

                SmGui::LeftLabel("Enabled");
                SmGui::FillWidth();
                bool enabled = ch.enabled;
                if (SmGui::Checkbox("##enabled", &enabled)) {
                    // Would need update method
                }

                ImGui::TreePop();
            }
        }

        SmGui::Separator();

        if (SmGui::Button("Add Channel")) {
            MeshtasticDecoder::ChannelInfo newCh;
            newCh.name = "Channel " + std::to_string(channels.size());
            newCh.enabled = true;
            newCh.index = static_cast<int>(channels.size());
            decoder->addChannel(newCh);
        }

        SmGui::SameLine();

        if (SmGui::Button("Remove Selected") && selectedChannel >= 0 && selectedChannel < static_cast<int>(channels.size())) {
            decoder->removeChannel(selectedChannel);
        }
    }

    // Packet Display
    if (ImGui::CollapsingHeader("Received Packets", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Filter options
        SmGui::LeftLabel("Filter");
        SmGui::FillWidth();
        static char filterBuf[128] = "";
        SmGui::InputText("##filter", filterBuf, sizeof(filterBuf));

        SmGui::LeftLabel("Show Decrypted Only");
        SmGui::FillWidth();
        static bool showDecryptedOnly = false;
        SmGui::Checkbox("##decrypted", &showDecryptedOnly);

        ImGui::Separator();

        // Packet list
        const auto& history = decoder->getPacketHistory();
        if (ImGui::BeginTable("packets", 6,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {

            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("To", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Port", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("SNR/RSSI", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            // Iterate in reverse (newest first)
            for (auto it = history.rbegin(); it != history.rend(); ++it) {
                const auto& pkt = *it;

                // Apply filter
                if (filterBuf[0] && pkt.decodedText.find(filterBuf) == std::string::npos &&
                    pkt.fromName.find(filterBuf) == std::string::npos &&
                    pkt.toName.find(filterBuf) == std::string::npos) {
                    continue;
                }

                if (showDecryptedOnly && !pkt.decrypted) continue;

                ImGui::TableNextRow();

                // Time
                ImGui::TableSetColumnIndex(0);
                auto time_t = static_cast<time_t>(pkt.timestamp);
                char timeStr[32];
                strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&time_t));
                ImGui::Text("%s", timeStr);

                // From
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", pkt.fromName.c_str());

                // To
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", pkt.toName.c_str());

                // Port
                ImGui::TableSetColumnIndex(3);
                if (pkt.decrypted) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", pkt.protoPacket ? MeshtasticProtocolParser::portNumToString(pkt.protoPacket->portnum()).c_str() : "?");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s (ENC)", pkt.protoPacket ? MeshtasticProtocolParser::portNumToString(pkt.protoPacket->portnum()).c_str() : "?");
                }

                // SNR/RSSI
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.1f dB / %d dBm", pkt.snr, pkt.rssi);

                // Data
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%s", pkt.decodedText.c_str());
            }

            ImGui::EndTable();
        }
    }

    // Node Database
    if (ImGui::CollapsingHeader("Node Database")) {
        const auto& nodes = decoder->getNodes();
        if (ImGui::BeginTable("nodes", 7,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY)) {

            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Long Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Short", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Last Seen", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("SNR/RSSI", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto& [id, node] : nodes) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("!%08X", id);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", node.longName.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", node.shortName.c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", node.role.c_str());

                ImGui::TableSetColumnIndex(4);
                auto time_t = static_cast<time_t>(node.lastSeen);
                char timeStr[32];
                strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&time_t));
                ImGui::Text("%s", timeStr);

                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%.1f / %d", node.lastSnr, node.lastRssi);

                ImGui::TableSetColumnIndex(6);
                if (node.latitude != 0 || node.longitude != 0) {
                    ImGui::Text("%.6f, %.6f", node.latitude, node.longitude);
                }
            }

            ImGui::EndTable();
        }
    }

    // Configuration persistence
    if (ImGui::CollapsingHeader("Configuration")) {
        if (SmGui::Button("Save Config")) {
            saveConfig();
        }
        SmGui::SameLine();
        if (SmGui::Button("Load Config")) {
            loadConfig();
        }
        SmGui::SameLine();
        if (SmGui::Button("Clear History")) {
            // Would need clear method
        }
    }
}

void MeshtasticPlugin::loadConfig() {
    json def = json({});
    def["lora"] = json({});
    def["channels"] = json::array();

    config.setPath(core::args["root"].s() + "/meshtastic_config.json");
    config.load(def);
    config.enableAutoSave();

    if (config.conf.contains("lora") || config.conf.contains("channels")) {
        decoder->loadConfig(config.conf);
    }
}

void MeshtasticPlugin::saveConfig() {
    config.acquire();
    config.conf = decoder->saveConfig();
    config.release(true);
    config.save();
    flog::info("[Meshtastic] Configuration saved");
}

// IQ stream processing callback
void MeshtasticPlugin::processIQStream() {
    if (!enabled || !iqStream) return;

    // Get available samples
    size_t available = iqStream->getReadAvailable();
    if (available == 0) return;

    // Read samples
    std::vector<dsp::complex_t> buffer(available);
    size_t read = iqStream->read(buffer.data(), available);
    if (read == 0) return;

    // Process through decoder
    auto packets = decoder->processIQ(buffer.data(), read, iqFrontEnd->getSampleRate());

    // Packets are stored in decoder's history, GUI will display them
}

// ============================================================================
// SDR++ Module Exports
// ============================================================================

SDRPP_MOD_INFO{
    /* Name:            */ "meshtastic",
    /* Description:     */ "Meshtastic LoRa protocol decoder and decryptor",
    /* Author:          */ "Victor",
    /* Version:         */ 1, 0, 0,
    /* Max instances    */ 1
};

ConfigManager config;

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["lora"] = json({});
    def["channels"] = json::array();
    config.setPath(core::args["root"].s() + "/meshtastic_config.json");
    config.load(def);
    config.enableAutoSave();
    flog::info("[Meshtastic] Module initialized");
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new MeshtasticPlugin(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete static_cast<MeshtasticPlugin*>(instance);
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
    flog::info("[Meshtastic] Module shutdown");
}