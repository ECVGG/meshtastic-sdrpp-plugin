// Meshtastic Decoder Module for SDR++ Portable
// Self-contained plugin with own config management

// Include Windows.h first with minimal defines, then undef macros that
// conflict with protobuf 7.x generated identifiers (e.g. DWORD, ERROR).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VC_EXTRA_LEAN
#include <Windows.h>
#undef DWORD
#undef BYTE
#undef WORD
#undef BOOL
#undef ERROR
#undef OPTIONAL
#undef NEAR
#undef FAR

#include "MeshtasticDecoderModule.h"
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <imgui.h>
#include <json.hpp>
#include <meshtastic/mesh.pb.h>
#include <memory>
#include <string>
#include <fstream>
#include <mutex>
#include <filesystem>
#include <fmt/format.h>
#include <chrono>
#include <deque>
#include <vector>
#include <array>

#include <signal_path/signal_path.h>
#include <dsp/sink/handler_sink.h>
#include <MeshtasticDecoder.h>
#include <MeshtasticDecryptor.h>
#include <LoRaDemodulator.h>
#include <MeshtasticProtocol.h>

// Helper to read main config.json and check if this instance is enabled
static bool isInstanceEnabledInConfig(const std::string& instanceName) {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    std::string configPath = (exePath.parent_path() / "config.json").string();

    std::ifstream file(configPath);
    if (!file.is_open()) return false;

    try {
        nlohmann::json conf;
        file >> conf;
        if (conf.contains("moduleInstances") && conf["moduleInstances"].contains(instanceName)) {
            return conf["moduleInstances"][instanceName].value("enabled", false);
        }
    } catch (...) {
        return false;
    }
    return false;
}

// Simple config manager for the plugin
class PluginConfig {
public:
    PluginConfig(const std::string& pluginName) : pluginName(pluginName), lastSaveTime(std::chrono::steady_clock::now()) {
        configPath = getConfigPath();
        load();
    }

    ~PluginConfig() {
        save();
    }

    nlohmann::json& get() { return conf; }
    nlohmann::json& operator[](const std::string& key) { return conf[key]; }
    const nlohmann::json& operator[](const std::string& key) const { return conf[key]; }
    bool contains(const std::string& key) const { return conf.contains(key); }

    void load() {
        std::ifstream file(configPath);
        if (file.is_open()) {
            try {
                file >> conf;
            } catch (...) {
                conf = nlohmann::json({});
            }
        } else {
            conf = nlohmann::json({});
        }

        if (!conf.contains(pluginName)) {
            conf[pluginName] = nlohmann::json({});
        }
    }

    void save() {
        std::ofstream file(configPath);
        if (file.is_open()) {
            file << conf.dump(4);
        }
        lastSaveTime = std::chrono::steady_clock::now();
    }

    // Auto-save if enough time has passed (default 30 seconds)
    void autoSave() {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastSaveTime).count() >= 30) {
            save();
        }
    }

private:
    std::string getConfigPath() {
        // Get the executable directory
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        std::filesystem::path exePath(buffer);
        return (exePath.parent_path() / "meshtastic_decoder_config.json").string();
    }

    std::string pluginName;
    std::string configPath;
    nlohmann::json conf;
    std::chrono::steady_clock::time_point lastSaveTime;
};

// Helper for ImGui InputText with std::string
static bool InputText(const char* label, std::string* str) {
    char buf[256];
    strncpy(buf, str->c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    if (ImGui::InputText(label, buf, sizeof(buf))) {
        *str = buf;
        return true;
    }
    return false;
}

// Known Meshtastic channels with their public PSKs and default LoRa parameters
struct KnownChannel {
    const char* name;
    const char* psk;          // Base64 encoded PSK
    int spreadingFactor;
    int bandwidth;
    int codingRate;           // 5 = 4/5, 6 = 4/6, etc.
    uint8_t syncWord;
    const char* description;
};

static const KnownChannel KNOWN_CHANNELS[] = {
    { "LongFast (Default)", "1PG7OiApB1nwvP+rz05pAQLK", 7, 250000, 5, 0x34, "Public long range fast channel - most popular" },
    { "LongSlow", "AQ==", 11, 125000, 5, 0x34, "Public long range slow channel" },
    { "Medium", "Ag==", 9, 250000, 5, 0x34, "Public medium range channel" },
    { "ShortFast", "BA==", 7, 500000, 5, 0x34, "Public short range fast channel" },
    { "ShortSlow", "BQ==", 8, 250000, 5, 0x34, "Public short range slow channel" },
    { "Bots (DE)", "AQ==", 7, 250000, 5, 0x34, "Germany Bots channel" },
    { "LinkinSat", "Bw==", 7, 250000, 5, 0x34, "LinkinSat community channel" },
    { "Custom", "", 7, 250000, 5, 0x34, "Custom channel - enter your own PSK and parameters" }
};

static const int KNOWN_CHANNELS_COUNT = sizeof(KNOWN_CHANNELS) / sizeof(KNOWN_CHANNELS[0]);

// Common bandwidth values for dropdown
static const int BANDWIDTH_OPTIONS[] = { 125000, 250000, 500000, 7500, 10400, 15600, 20800, 31250, 41700, 62500 };
static const int BANDWIDTH_OPTIONS_COUNT = sizeof(BANDWIDTH_OPTIONS) / sizeof(BANDWIDTH_OPTIONS[0]);
static const char* BANDWIDTH_LABELS[] = { "125 kHz", "250 kHz", "500 kHz", "7.5 kHz", "10.4 kHz", "15.6 kHz", "20.8 kHz", "31.25 kHz", "41.7 kHz", "62.5 kHz" };

MeshtasticDecoderModule::MeshtasticDecoderModule(std::string name)
    : name(std::move(name)), config(std::make_unique<PluginConfig>(this->name)) {

    // Load config
    auto& pluginConf = config->get()[this->name];

    selectedChannelIndex = pluginConf.value("selectedChannelIndex", 0);
    channelIndex = pluginConf.value("channelIndex", 0);
    channelName = pluginConf.value("channelName", "Primary");
    pskInput = pluginConf.value("pskInput", "");
    useAES256 = pluginConf.value("useAES256", false);
    appKeyInput = pluginConf.value("appKeyInput", "");
    networkKeyInput = pluginConf.value("networkKeyInput", "");

    loraConfig.spreadingFactor = pluginConf.value("spreadingFactor", 7);
    loraConfig.bandwidth = pluginConf.value("bandwidth", 250000);
    loraConfig.codingRate = pluginConf.value("codingRate", 5);
    loraConfig.preambleLength = pluginConf.value("preambleLength", 8);
    loraConfig.syncWord = pluginConf.value("syncWord", 0x34);
    loraConfig.crcEnabled = pluginConf.value("crcEnabled", true);
    loraConfig.implicitHeader = pluginConf.value("implicitHeader", false);
    loraConfig.iqInverted = pluginConf.value("iqInverted", false);
    loraConfig.centerFrequency = pluginConf.value("centerFrequency", 915000000.0);
    loraConfig.sampleRate = pluginConf.value("sampleRate", 1000000);

    // Initialize LoRa demodulator
    LoRaModemConfig loraModemConfig;
    loraModemConfig.spreadingFactor = loraConfig.spreadingFactor;
    loraModemConfig.bandwidth = loraConfig.bandwidth;
    loraModemConfig.codingRate = loraConfig.codingRate;
    loraModemConfig.preambleLength = loraConfig.preambleLength;
    loraModemConfig.syncWord = loraConfig.syncWord;
    loraModemConfig.crcEnabled = loraConfig.crcEnabled;
    loraModemConfig.implicitHeader = loraConfig.implicitHeader;
    loraModemConfig.iqInverted = loraConfig.iqInverted;
    loraModemConfig.centerFrequency = loraConfig.centerFrequency;
    loraModemConfig.sampleRate = static_cast<double>(loraConfig.sampleRate);
    loraDemod.init(loraModemConfig);

    // Initialize decryptor
    decryptor.init();

    // Register GUI menu entry
    gui::menu.registerEntry(this->name, menuHandler, this, this);

    // Auto-enable if configured in main config.json
    if (isInstanceEnabledInConfig(this->name)) {
        enable();
    }
}

MeshtasticDecoderModule::~MeshtasticDecoderModule() {
    disable();
    gui::menu.removeEntry(name);
    saveConfig();
}

void MeshtasticDecoderModule::postInit() {}

void MeshtasticDecoderModule::enable() {
    // Create VFO and connect to signal path
    try {
        // Create VFO with the configured center frequency and sample rate
        // Parameters: name, reference, offset, bandwidth, sampleRate, minBandwidth, maxBandwidth, bandwidthLocked
        vfo = sigpath::vfoManager.createVFO(
            name,
            ImGui::WaterfallVFO::REF_CENTER,
            0.0,
            loraConfig.bandwidth,
            loraConfig.sampleRate,
            loraConfig.bandwidth,
            loraConfig.bandwidth,
            true
        );

        if (vfo) {
            // Initialize handler sink to process IQ samples
            iqHandler.init(vfo->output, iqHandlerCallback, this);

            // Start LoRa demodulator
            loraDemod.start();

            enabled = true;
            fmt::print("MeshtasticDecoder: Enabled (VFO created, signal path connected)\n");
        } else {
            fmt::print("MeshtasticDecoder: Failed to create VFO\n");
        }
    } catch (const std::exception& e) {
        fmt::print("MeshtasticDecoder: Error enabling: {}\n", e.what());
    }
}

void MeshtasticDecoderModule::disable() {
    // Stop LoRa demodulator
    loraDemod.stop();

    // Destroy VFO
    if (vfo) {
        sigpath::vfoManager.deleteVFO(vfo);
        vfo = nullptr;
    }

    enabled = false;
    fmt::print("MeshtasticDecoder: Disabled\n");
}

bool MeshtasticDecoderModule::isEnabled() {
    return enabled;
}

void MeshtasticDecoderModule::applyChannelPreset(int channelIdx) {
    if (channelIdx < 0 || channelIdx >= KNOWN_CHANNELS_COUNT) return;

    const auto& ch = KNOWN_CHANNELS[channelIdx];

    // Don't apply if Custom is selected
    if (channelIdx == KNOWN_CHANNELS_COUNT - 1) return;

    // Apply channel preset
    pskInput = ch.psk;
    loraConfig.spreadingFactor = ch.spreadingFactor;
    loraConfig.bandwidth = ch.bandwidth;
    loraConfig.codingRate = ch.codingRate;
    loraConfig.syncWord = ch.syncWord;

    // Derive keys from PSK
    if (!pskInput.empty()) {
        decryptor.setPSK(pskInput, useAES256);
    }

    // Update LoRa demodulator
    updateLoRaConfig();
    saveConfig();
}

void MeshtasticDecoderModule::menuHandler(void* ctx) {
    MeshtasticDecoderModule* _this = static_cast<MeshtasticDecoderModule*>(ctx);

    // Auto-save config periodically
    _this->config->autoSave();

    if (!_this->enabled) { ImGui::BeginDisabled(); }

    // Channel Preset Selector
    if (ImGui::CollapsingHeader("Channel Preset", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Channel dropdown
        static const char* channelLabels[KNOWN_CHANNELS_COUNT];
        for (int i = 0; i < KNOWN_CHANNELS_COUNT; ++i) {
            channelLabels[i] = KNOWN_CHANNELS[i].name;
        }

        if (ImGui::Combo("Preset Channel", &_this->selectedChannelIndex, channelLabels, KNOWN_CHANNELS_COUNT)) {
            _this->applyChannelPreset(_this->selectedChannelIndex);
        }

        // Show description of selected channel
        if (_this->selectedChannelIndex >= 0 && _this->selectedChannelIndex < KNOWN_CHANNELS_COUNT - 1) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", KNOWN_CHANNELS[_this->selectedChannelIndex].description);
        }

        ImGui::Separator();

        // Channel configuration (editable even with presets)
        ImGui::Text("Channel Index: %d", _this->channelIndex);
        if (ImGui::InputInt("Channel Index", &_this->channelIndex, 1, 10)) {
            _this->channelIndex = std::clamp(_this->channelIndex, 0, 7);
            _this->saveConfig();
        }

        if (InputText("Channel Name", &_this->channelName)) {
            _this->saveConfig();
        }
        if (InputText("PSK (Base64)", &_this->pskInput)) {
            _this->saveConfig();
            _this->decryptor.setPSK(_this->pskInput, _this->useAES256);
        }

        if (ImGui::Checkbox("AES-256", &_this->useAES256)) {
            _this->saveConfig();
            if (!_this->pskInput.empty()) {
                _this->decryptor.setPSK(_this->pskInput, _this->useAES256);
            }
        }

        // Show derived key info
        if (!_this->pskInput.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Keys derived from PSK via PBKDF2-SHA256 (1000 iters, salt='Meshtastic')");
        }
    }

    ImGui::Separator();

    // LoRa Parameters
    if (ImGui::CollapsingHeader("LoRa Parameters")) {
        if (ImGui::InputInt("SF", &_this->loraConfig.spreadingFactor)) {
            _this->loraConfig.spreadingFactor = std::clamp(_this->loraConfig.spreadingFactor, 7, 12);
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        ImGui::Text("Spreading Factor: %d", _this->loraConfig.spreadingFactor);

        // Bandwidth dropdown
        int bwIdx = -1;
        for (int i = 0; i < BANDWIDTH_OPTIONS_COUNT; ++i) {
            if (BANDWIDTH_OPTIONS[i] == _this->loraConfig.bandwidth) {
                bwIdx = i;
                break;
            }
        }
        if (bwIdx == -1) bwIdx = 1; // Default to 250 kHz

        if (ImGui::Combo("Bandwidth", &bwIdx, BANDWIDTH_LABELS, BANDWIDTH_OPTIONS_COUNT)) {
            _this->loraConfig.bandwidth = BANDWIDTH_OPTIONS[bwIdx];
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        ImGui::Text("Bandwidth: %d Hz", _this->loraConfig.bandwidth);

        if (ImGui::InputInt("CR 4/", &_this->loraConfig.codingRate)) {
            _this->loraConfig.codingRate = std::clamp(_this->loraConfig.codingRate, 5, 8);
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        ImGui::Text("Coding Rate: 4/%d", _this->loraConfig.codingRate);

        if (ImGui::InputInt("Preamble", &_this->loraConfig.preambleLength)) {
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        ImGui::Text("Preamble Length: %d", _this->loraConfig.preambleLength);

        if (ImGui::InputScalar("Sync Word", ImGuiDataType_U8, &_this->loraConfig.syncWord)) {
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        ImGui::Text("Sync Word: 0x%02X", _this->loraConfig.syncWord);

        if (ImGui::Checkbox("CRC", &_this->loraConfig.crcEnabled)) {
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        if (ImGui::Checkbox("Implicit Header", &_this->loraConfig.implicitHeader)) {
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        if (ImGui::Checkbox("IQ Inverted", &_this->loraConfig.iqInverted)) {
            _this->saveConfig();
            _this->updateLoRaConfig();
        }

        double freqMHz = _this->loraConfig.centerFrequency / 1e6;
        if (ImGui::InputDouble("Center Freq (MHz)", &freqMHz, 0.1, 1.0, "%.1f")) {
            _this->loraConfig.centerFrequency = freqMHz * 1e6;
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        ImGui::Text("Center Frequency: %.1f MHz", _this->loraConfig.centerFrequency / 1e6);

        if (ImGui::InputInt("Sample Rate", &_this->loraConfig.sampleRate)) {
            _this->saveConfig();
            _this->updateLoRaConfig();
        }
        ImGui::Text("Sample Rate: %d", _this->loraConfig.sampleRate);
    }

    ImGui::Separator();

    // Encryption
    if (ImGui::CollapsingHeader("Encryption")) {
        if (ImGui::Checkbox("AES-256", &_this->useAES256)) {
            _this->saveConfig();
        }
        if (InputText("App Key (hex)", &_this->appKeyInput)) {
            _this->saveConfig();
        }
        if (InputText("Network Key (hex)", &_this->networkKeyInput)) {
            _this->saveConfig();
        }
    }

    ImGui::Separator();

    // Statistics
    if (ImGui::CollapsingHeader("Statistics")) {
        ImGui::Text("Packets Received: %llu", _this->packetsReceived);
        ImGui::Text("Packets Decoded: %llu", _this->packetsDecoded);
        ImGui::Text("Packets Decrypted: %llu", _this->packetsDecrypted);
    }

    ImGui::Separator();

    // Decoded Messages
    if (ImGui::CollapsingHeader("Decoded Messages")) {
        std::lock_guard<std::mutex> lock(_this->messagesMutex);

        if (_this->messages.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No messages received yet...");
        } else {
            // Show messages in reverse order (newest first)
            for (auto it = _this->messages.rbegin(); it != _this->messages.rend(); ++it) {
                const auto& msg = *it;

                ImGui::PushID(&msg);

                // Header with timestamp and node IDs
                ImGui::Text("[%s] Node 0x%08X -> 0x%08X | %s | RSSI: %d | SNR: %.1f dB",
                    msg.timestamp.c_str(),
                    msg.fromNode,
                    msg.toNode,
                    msg.portName.c_str(),
                    msg.rssi,
                    msg.snr);

                // Message content
                if (!msg.text.empty()) {
                    ImGui::Indent();
                    ImGui::TextWrapped("%s", msg.text.c_str());
                    ImGui::Unindent();
                }
                if (!msg.position.empty()) {
                    ImGui::Indent();
                    ImGui::Text("%s", msg.position.c_str());
                    ImGui::Unindent();
                }
                if (!msg.telemetry.empty()) {
                    ImGui::Indent();
                    ImGui::Text("%s", msg.telemetry.c_str());
                    ImGui::Unindent();
                }

                ImGui::Separator();
                ImGui::PopID();
            }
        }

        if (ImGui::Button("Clear Messages")) {
            _this->messages.clear();
        }
    }

    if (!_this->enabled) { ImGui::EndDisabled(); }
}

void MeshtasticDecoderModule::saveConfig() {
    auto& pluginConf = config->get()[name];
    pluginConf["selectedChannelIndex"] = selectedChannelIndex;
    pluginConf["channelIndex"] = channelIndex;
    pluginConf["channelName"] = channelName;
    pluginConf["pskInput"] = pskInput;
    pluginConf["useAES256"] = useAES256;
    pluginConf["appKeyInput"] = appKeyInput;
    pluginConf["networkKeyInput"] = networkKeyInput;

    pluginConf["spreadingFactor"] = loraConfig.spreadingFactor;
    pluginConf["bandwidth"] = loraConfig.bandwidth;
    pluginConf["codingRate"] = loraConfig.codingRate;
    pluginConf["preambleLength"] = loraConfig.preambleLength;
    pluginConf["syncWord"] = loraConfig.syncWord;
    pluginConf["crcEnabled"] = loraConfig.crcEnabled;
    pluginConf["implicitHeader"] = loraConfig.implicitHeader;
    pluginConf["iqInverted"] = loraConfig.iqInverted;
    pluginConf["centerFrequency"] = loraConfig.centerFrequency;
    pluginConf["sampleRate"] = loraConfig.sampleRate;

    config->save();
}

void MeshtasticDecoderModule::updateLoRaConfig() {
    LoRaModemConfig loraModemConfig;
    loraModemConfig.spreadingFactor = loraConfig.spreadingFactor;
    loraModemConfig.bandwidth = loraConfig.bandwidth;
    loraModemConfig.codingRate = loraConfig.codingRate;
    loraModemConfig.preambleLength = loraConfig.preambleLength;
    loraModemConfig.syncWord = loraConfig.syncWord;
    loraModemConfig.crcEnabled = loraConfig.crcEnabled;
    loraModemConfig.implicitHeader = loraConfig.implicitHeader;
    loraModemConfig.iqInverted = loraConfig.iqInverted;
    loraModemConfig.centerFrequency = loraConfig.centerFrequency;
    loraModemConfig.sampleRate = static_cast<double>(loraConfig.sampleRate);
    loraDemod.configure(loraModemConfig);
}

// Process IQ samples from signal path
void MeshtasticDecoderModule::processIQSamples(const dsp::complex_t* data, size_t len) {
    if (!enabled) return;

    // Auto-save config periodically
    config->autoSave();

    // Process through LoRa demodulator
    auto packets = loraDemod.process(data, len, static_cast<double>(loraConfig.sampleRate));
    packetsReceived += packets.size();

    for (const auto& loraPacket : packets) {
        packetsDecoded++;

        // Parse Meshtastic header to get packetId and fromNode
        uint8_t flags;
        uint32_t packetId = 0;
        uint32_t fromNode = 0;
        uint32_t toNode = 0;
        uint8_t hopLimit = 0;
        uint8_t hopStart = 0;
        uint32_t fromNodeId = 0;
        uint32_t toNodeId = 0;
        uint8_t portNum = 0;
        uint8_t flagsOut = 0;

        if (!MeshtasticProtocol::parseMeshHeader(loraPacket.payload, flags, packetId, fromNode, toNode,
                                                 hopLimit, hopStart, fromNodeId, toNodeId, portNum, flagsOut)) {
            continue; // Not a valid Meshtastic packet
        }

        // Try to decrypt
        std::vector<uint8_t> decrypted = decryptor.decryptPacket(loraPacket.payload, packetId, fromNodeId);
        if (decrypted.empty()) {
            continue; // Decryption failed
        }
        packetsDecrypted++;

        // Parse the decrypted meshtastic packet
        auto meshPacketOpt = decoder.parsePacket(decrypted);
        if (!meshPacketOpt.has_value()) {
            continue;
        }

        const auto& meshPacket = meshPacketOpt.value();

        // Store message for display
        DecodedMessage msg;

        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        char timeBuf[32];
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);
        msg.timestamp = timeBuf;

        msg.fromNode = meshPacket.from();
        msg.toNode = meshPacket.to();
        msg.portName = MeshtasticProtocol::getPortName(meshPacket.portnum());
        msg.rssi = loraPacket.rssi;
        msg.snr = loraPacket.snr;

        // Decode payload based on port
        switch (meshPacket.portnum()) {
            case meshtastic::PortNum::TEXT_MESSAGE_APP: {
                meshtastic::Data dataMsg;
                if (decoder.parseAppPayload(meshPacket, dataMsg)) {
                    // Data.payload contains raw UTF-8 text for TEXT_MESSAGE_APP
                    msg.text = dataMsg.payload();
                }
                break;
            }
            case meshtastic::PortNum::POSITION_APP: {
                meshtastic::Position pos;
                if (decoder.parseAppPayload(meshPacket, pos)) {
                    // latitude_i and longitude_i are integers scaled by 1e7
                    msg.position = fmt::format("Lat: {:.6f}, Lon: {:.6f}, Alt: {}m",
                        pos.latitude_i() * 1e-7,
                        pos.longitude_i() * 1e-7,
                        pos.altitude());
                }
                break;
            }
            // Telemetry uses TELEMETRY_APP = 67 in Meshtastic protobufs
            case 67: {
                meshtastic::Telemetry telemetry;
                if (decoder.parseAppPayload(meshPacket, telemetry)) {
                    if (telemetry.has_device_metrics()) {
                        const auto& dm = telemetry.device_metrics();
                        msg.telemetry = fmt::format("Battery: {}%, Voltage: {:.2f}V",
                            dm.battery_level(),
                            dm.voltage() * 0.001);
                    }
                }
                break;
            }
            case meshtastic::PortNum::NODEINFO_APP: {
                meshtastic::User user;
                if (decoder.parseAppPayload(meshPacket, user)) {
                    msg.text = fmt::format("User: {} ({})", user.long_name(), user.short_name());
                }
                break;
            }
            default:
                // MeshPacket uses oneof payload_variant - check which is present
                size_t payloadSize = 0;
                if (meshPacket.has_decoded()) {
                    payloadSize = meshPacket.decoded().payload().size();
                } else if (!meshPacket.encrypted().empty()) {
                    payloadSize = meshPacket.encrypted().size();
                }
                msg.text = fmt::format("Raw payload ({} bytes)", payloadSize);
                break;
        }

        // Add to message queue
        {
            std::lock_guard<std::mutex> lock(messagesMutex);
            messages.push_back(msg);
            if (messages.size() > MAX_MESSAGES) {
                messages.pop_front();
            }
        }
    }
}

// Static IQ handler callback
void MeshtasticDecoderModule::iqHandlerCallback(dsp::complex_t* data, int count, void* ctx) {
    MeshtasticDecoderModule* _this = static_cast<MeshtasticDecoderModule*>(ctx);
    _this->processIQSamples(data, static_cast<size_t>(count));
}

MOD_EXPORT const ModuleManager::ModuleInfo_t _INFO_{
    /* Name:            */ "meshtastic_decoder",
    /* Description:     */ "Meshtastic LoRa packet decoder and decryptor",
    /* Author:          */ "Victor",
    /* Version:         */ 1, 0, 0,
    /* Max instances    */ 4
};

MOD_EXPORT void _INIT_() {
    // Create default recording directory
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    std::string root = exePath.parent_path().string();

    if (!std::filesystem::exists(root + "/recordings")) {
        if (!std::filesystem::create_directory(root + "/recordings")) {
            fmt::print("Could not create recordings directory\n");
        }
    }
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new MeshtasticDecoderModule(std::move(name));
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete static_cast<MeshtasticDecoderModule*>(instance);
}

MOD_EXPORT void _END_() {
    // Config is saved automatically in destructor
}