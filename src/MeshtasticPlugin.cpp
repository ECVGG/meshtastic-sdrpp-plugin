#include "MeshtasticPlugin.h"
#include "MeshtasticProtocol.h"
#include <nlohmann/json.hpp>
#include <imgui.h>

// Use sdrpp namespace for forward-declared types
using namespace sdrpp;
using nlohmann::json;

// Global config manager instance
static ConfigManager g_config;

MeshtasticPlugin::MeshtasticPlugin(std::string name) {
    this->name = name;
    decoder = std::make_unique<MeshtasticDecoder>();

    // Register GUI menu entry
    if (gui::Menu::instance()) {
        gui::Menu::instance()->registerEntry(name, menuHandler, this);
    }
}

MeshtasticPlugin::~MeshtasticPlugin() {
    if (gui::Menu::instance()) {
        gui::Menu::instance()->removeEntry(name);
    }
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
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Meshtastic Decoder");
    ImGui::Separator();

    // Statistics
    ImGui::Text("Packets: RX=%llu Decoded=%llu Decrypted=%llu Failed=%llu",
                decoder->getPacketsReceived(),
                decoder->getPacketsDecoded(),
                decoder->getPacketsDecrypted(),
                decoder->getPacketsFailed());

    // LoRa Configuration
    if (ImGui::CollapsingHeader("LoRa Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& config = decoder->getLoRaConfig();

        ImGui::Text("Center Frequency");
        ImGui::SameLine(150);
        if (ImGui::InputDouble("##freq", &config.centerFrequency, 100000, 1000000, "%.3f MHz")) {
            decoder->setLoRaConfig(config);
        }

        ImGui::Text("Spreading Factor");
        ImGui::SameLine(150);
        if (ImGui::Combo("##sf", &config.spreadingFactor, "SF7\0SF8\0SF9\0SF10\0SF11\0SF12\0")) {
            decoder->setLoRaConfig(config);
        }

        ImGui::Text("Bandwidth");
        ImGui::SameLine(150);
        int bwIdx = 0;
        if (config.bandwidth == 125000) bwIdx = 0;
        else if (config.bandwidth == 250000) bwIdx = 1;
        else if (config.bandwidth == 500000) bwIdx = 2;
        if (ImGui::Combo("##bw", &bwIdx, "125 kHz\0250 kHz\0500 kHz\0")) {
            config.bandwidth = (bwIdx == 0) ? 125000 : (bwIdx == 1) ? 250000 : 500000;
            decoder->setLoRaConfig(config);
        }

        ImGui::Text("Coding Rate");
        ImGui::SameLine(150);
        if (ImGui::Combo("##cr", &config.codingRate, "4/5\04/6\04/7\04/8\0")) {
            config.codingRate = config.codingRate + 5;  // 0->5, 1->6, etc.
            decoder->setLoRaConfig(config);
        }

        ImGui::Text("Sync Word");
        ImGui::SameLine(150);
        if (ImGui::Combo("##sync", &config.syncWord, "Public (0x12)\0Private (0x2B)\0")) {
            config.syncWord = (config.syncWord == 0) ? 0x12 : 0x2B;
            decoder->setLoRaConfig(config);
        }

        ImGui::Text("Preamble Length");
        ImGui::SameLine(150);
        if (ImGui::InputInt("##preamble", &config.preambleLength, 1, 10)) {
            config.preambleLength = std::clamp(config.preambleLength, 6, 65535);
            decoder->setLoRaConfig(config);
        }

        ImGui::Text("Sample Rate");
        ImGui::SameLine(150);
        if (ImGui::InputDouble("##sr", &config.sampleRate, 100000, 1000000, "%.0f")) {
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
                ImGui::Text("Name");
                ImGui::SameLine(150);
                char nameBuf[64];
                strncpy(nameBuf, ch.name.c_str(), sizeof(nameBuf) - 1);
                nameBuf[sizeof(nameBuf) - 1] = 0;
                if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                    // Update name - need to cast away const
                }

                ImGui::Text("PSK (Base64)");
                ImGui::SameLine(150);
                char pskBuf[256];
                strncpy(pskBuf, ch.pskBase64.c_str(), sizeof(pskBuf) - 1);
                pskBuf[sizeof(pskBuf) - 1] = 0;
                if (ImGui::InputText("##psk", pskBuf, sizeof(pskBuf))) {
                    decoder->updateChannelKey(static_cast<int>(i), pskBuf);
                }

                ImGui::Text("AES-256");
                ImGui::SameLine(150);
                bool useAES256 = ch.useAES256;
                if (ImGui::Checkbox("##aes256", &useAES256)) {
                    // Would need update method
                }

                ImGui::Text("Enabled");
                ImGui::SameLine(150);
                bool enabled = ch.enabled;
                if (ImGui::Checkbox("##enabled", &enabled)) {
                    // Would need update method
                }

                ImGui::TreePop();
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Add Channel")) {
            MeshtasticDecoder::ChannelInfo newCh;
            newCh.name = "Channel " + std::to_string(channels.size());
            newCh.enabled = true;
            newCh.index = static_cast<int>(channels.size());
            decoder->addChannel(newCh);
        }

        ImGui::SameLine();

        if (ImGui::Button("Remove Selected") && selectedChannel >= 0 && selectedChannel < static_cast<int>(channels.size())) {
            decoder->removeChannel(selectedChannel);
        }
    }

    // Packet Display
    if (ImGui::CollapsingHeader("Received Packets", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Filter options
        ImGui::Text("Filter");
        ImGui::SameLine(150);
        static char filterBuf[128] = "";
        ImGui::InputText("##filter", filterBuf, sizeof(filterBuf));

        ImGui::Text("Show Decrypted Only");
        ImGui::SameLine(150);
        static bool showDecryptedOnly = false;
        ImGui::Checkbox("##decrypted", &showDecryptedOnly);

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
        if (ImGui::Button("Save Config")) {
            saveConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Config")) {
            loadConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear History")) {
            // Would need clear method
        }
    }
}

void MeshtasticPlugin::loadConfig() {
    json def = json({});
    def["lora"] = json({});
    def["channels"] = json::array();

    config = &g_config;
    config->setPath(core::args["root"].s() + "/meshtastic_config.json");
    config->load(def);
    config->enableAutoSave();

    if (config->conf.contains("lora") || config->conf.contains("channels")) {
        decoder->loadConfig(config->conf);
    }
}

void MeshtasticPlugin::saveConfig() {
    config->acquire();
    config->conf = decoder->saveConfig();
    config->release(true);
    config->save();
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

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["lora"] = json({});
    def["channels"] = json::array();
    g_config.setPath(core::args["root"].s() + "/meshtastic_config.json");
    g_config.load(def);
    g_config.enableAutoSave();
    flog::info("[Meshtastic] Module initialized");
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new MeshtasticPlugin(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete static_cast<MeshtasticPlugin*>(instance);
}

MOD_EXPORT void _END_() {
    g_config.disableAutoSave();
    g_config.save();
    flog::info("[Meshtastic] Module shutdown");
}