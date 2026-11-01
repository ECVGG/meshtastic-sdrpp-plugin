#include "MeshtasticDecoder.h"
#include "MeshtasticProtocol.h"
#include "MeshtasticDecryptor.h"
#include "mesh.pb.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace MeshtasticDecoder {

MeshtasticDecoder::MeshtasticDecoder() {
    loraDemod = std::make_unique<LoRaDemodulator>();

    // Default LoRa config for Meshtastic (US 915 MHz)
    loraConfig.spreadingFactor = 7;
    loraConfig.bandwidth = 125000;
    loraConfig.codingRate = 5;
    loraConfig.preambleLength = 8;
    loraConfig.syncWord = 0x34;  // Meshtastic uses 0x34
    loraConfig.crcEnabled = true;
    loraConfig.implicitHeader = false;
    loraConfig.iqInverted = false;
    loraConfig.centerFrequency = 915000000.0;
    loraConfig.sampleRate = 1000000.0;

    loraDemod->init(loraConfig);
}

MeshtasticDecoder::~MeshtasticDecoder() = default;

void MeshtasticDecoder::setLoRaConfig(const LoRaModemConfig& config) {
    loraConfig = config;
    if (loraDemod) {
        loraDemod->configure(config);
    }
}

void MeshtasticDecoder::addChannel(const ChannelInfo& channel) {
    std::lock_guard<std::mutex> lock(channelsMutex);
    ChannelInfo newChannel = channel;
    newChannel.index = static_cast<int>(channels.size());
    channels.push_back(newChannel);
}

void MeshtasticDecoder::removeChannel(int index) {
    std::lock_guard<std::mutex> lock(channelsMutex);
    if (index >= 0 && index < static_cast<int>(channels.size())) {
        channels.erase(channels.begin() + index);
        // Reindex
        for (size_t i = 0; i < channels.size(); ++i) {
            channels[i].index = static_cast<int>(i);
        }
    }
}

void MeshtasticDecoder::updateChannelKey(int index, const std::string& pskBase64, bool useAES256) {
    std::lock_guard<std::mutex> lock(channelsMutex);
    if (index >= 0 && index < static_cast<int>(channels.size())) {
        channels[index].pskBase64 = pskBase64;
        channels[index].useAES256 = useAES256;
        // Derive and store AES key
        auto key = deriveKeyFromPSK(pskBase64, useAES256);
        if (key.size() == 32) {
            std::copy(key.begin(), key.end(), channels[index].aesKey.begin());
        }
    }
}

std::vector<MeshtasticDecoder::ChannelInfo> MeshtasticDecoder::getChannels() const {
    std::lock_guard<std::mutex> lock(channelsMutex);
    return channels;
}

std::vector<MeshtasticDecoder::DecodedPacket> MeshtasticDecoder::processIQ(const Complex* iq, size_t count, double sampleRate) {
    std::vector<DecodedPacket> decodedPackets;

    if (!loraDemod || !loraDemod->isInitialized()) {
        return decodedPackets;
    }

    // Process IQ samples through LoRa demodulator
    auto loraPackets = loraDemod->process(iq, count, sampleRate);

    for (const auto& loraPacket : loraPackets) {
        if (loraPacket.crcValid && !loraPacket.payload.empty()) {
            DecodedPacket decoded = processRawPacket(loraPacket.payload, loraPacket.rssi, loraPacket.snr);
            if (decoded.valid) {
                decodedPackets.push_back(decoded);
            }
        }
    }

    return decodedPackets;
}

MeshtasticDecoder::DecodedPacket MeshtasticDecoder::processRawPacket(const std::vector<uint8_t>& rawPacket, int rssi, float snr) {
    DecodedPacket decoded;
    decoded.timestamp = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    decoded.rssi = rssi;
    decoded.snr = snr;

    packetsReceived++;

    if (processRawPacketInternal(rawPacket, decoded)) {
        packetsDecoded++;
        handleDecodedPacket(decoded);
    } else {
        packetsFailed++;
    }

    return decoded;
}

bool MeshtasticDecoder::processRawPacketInternal(const std::vector<uint8_t>& rawPacket, DecodedPacket& decoded) {
    // Decode mesh header
    if (!decodeMeshHeader(rawPacket.data(), rawPacket.size(), decoded.header)) {
        decoded.error = "Invalid mesh header";
        return false;
    }

    // Validate CRC
    if (!validateCRC(rawPacket.data(), rawPacket.size())) {
        decoded.error = "CRC check failed";
        return false;
    }

    // Extract payload (skip 13 byte header, exclude 2 byte CRC)
    size_t payloadStart = 13;
    size_t payloadLen = rawPacket.size() - payloadStart - 2;

    if (payloadLen == 0) {
        decoded.error = "Empty payload";
        return false;
    }

    // Parse protobuf
    auto meshPacket = std::make_unique<meshtastic::MeshPacket>();
    if (!MeshtasticProtocol::MeshtasticProtocolParser::parsePacket(
            rawPacket.data() + payloadStart, payloadLen, *meshPacket)) {
        decoded.error = "Protobuf parse failed";
        return false;
    }

    decoded.protoPacket = std::move(meshPacket);

    // Try to decrypt if encrypted
    if (decoded.protoPacket &&
        dynamic_cast<meshtastic::MeshPacket*>(decoded.protoPacket.get())->encrypted()) {
        ChannelInfo usedChannel;
        if (tryDecryptWithAllChannels(*decoded.protoPacket, usedChannel)) {
            decoded.decrypted = true;
            packetsDecrypted++;
        }
    }

    // Extract human-readable payload
    if (decoded.protoPacket) {
        MeshtasticProtocol::MeshtasticProtocolParser::extractPayload(*decoded.protoPacket, decoded);
    }

    decoded.valid = true;
    return true;
}

bool MeshtasticDecoder::decodeMeshHeader(const uint8_t* data, size_t len, MeshtasticProtocol::MeshHeader& header) {
    return MeshtasticProtocol::MeshHeader::deserialize(data, len, header);
}

bool MeshtasticDecoder::validateCRC(const uint8_t* data, size_t len) {
    if (len < 15) return false; // header(13) + min payload(1) + crc(2) = 16

    MeshtasticProtocol::MeshHeader header;
    if (!MeshtasticProtocol::MeshHeader::deserialize(data, len, header)) {
        return false;
    }

    // Calculate CRC over header + payload (excluding CRC bytes)
    size_t crcLen = len - 2;
    uint16_t calcCRC = MeshtasticProtocol::MeshHeader::calculateCRC(data, crcLen);

    return calcCRC == header.crc;
}

bool MeshtasticDecoder::tryDecryptWithAllChannels(google::protobuf::Message& packet, ChannelInfo& usedChannel) {
    std::lock_guard<std::mutex> lock(channelsMutex);

    for (auto& channel : channels) {
        if (!channel.enabled || channel.pskBase64.empty()) continue;

        MeshtasticDecryptor::MeshtasticDecryptor decryptor;
        if (channel.useAES256) {
            decryptor.setKey(channel.aesKey);
        } else {
            // Use first 16 bytes for AES-128
            std::array<uint8_t, 16> key128;
            std::copy(channel.aesKey.begin(), channel.aesKey.begin() + 16, key128.begin());
            decryptor.setKey(key128);
        }

        if (decryptor.decrypt(packet)) {
            usedChannel = channel;
            return true;
        }
    }

    return false;
}

std::array<uint8_t, 32> MeshtasticDecoder::deriveKeyFromPSK(const std::string& pskBase64, bool useAES256) {
    auto keyVec = MeshtasticDecryptor::MeshtasticDecryptor::deriveKeyFromPSK(
        pskBase64,
        useAES256 ? MeshtasticDecryptor::KeyType::AES256 : MeshtasticDecryptor::KeyType::AES128
    );

    std::array<uint8_t, 32> key = {0};
    if (keyVec.size() >= 32) {
        std::copy(keyVec.begin(), keyVec.begin() + 32, key.begin());
    } else if (keyVec.size() == 16) {
        // AES-128 key, pad to 32 bytes (duplicate for compatibility)
        std::copy(keyVec.begin(), keyVec.end(), key.begin());
        std::copy(keyVec.begin(), keyVec.end(), key.begin() + 16);
    }
    return key;
}

std::array<uint8_t, 12> MeshtasticDecoder::buildNonce(const google::protobuf::Message& packet) {
    const auto* meshPacket = dynamic_cast<const meshtastic::MeshPacket*>(&packet);
    if (!meshPacket) {
        return {0};
    }
    return MeshtasticProtocol::MeshtasticCrypto::buildNonce(meshPacket->id(), meshPacket->from());
}

void MeshtasticDecoder::handleDecodedPacket(DecodedPacket& packet) {
    // Update node database
    if (packet.protoPacket) {
        updateNodeDatabase(*packet.protoPacket);
    }

    // Add to history
    {
        std::lock_guard<std::mutex> lock(packetHistoryMutex);
        packetHistory.push_back(packet);
        if (packetHistory.size() > maxPacketHistory) {
            packetHistory.pop_front();
        }
    }
}

void MeshtasticDecoder::updateNodeDatabase(const google::protobuf::Message& packet) {
    const auto* meshPacket = dynamic_cast<const meshtastic::MeshPacket*>(&packet);
    if (!meshPacket) return;

    std::lock_guard<std::mutex> lock(nodeDatabaseMutex);

    uint32_t nodeId = meshPacket->from();
    auto& node = nodeDatabase[nodeId];
    node.id = nodeId;
    node.lastSeen = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Extract node info if present
    if (meshPacket->has_nodeinfo()) {
        const auto& info = meshPacket->nodeinfo();
        node.longName = info.long_name();
        node.shortName = info.short_name();
        node.hwModel = info.hw_model();
        node.role = static_cast<uint8_t>(info.role());
    }

    // Extract position if present
    if (meshPacket->has_position()) {
        const auto& pos = meshPacket->position();
        node.latitude = pos.latitude();
        node.longitude = pos.longitude();
        node.altitude = pos.altitude();
        if (pos.has_battery_level()) {
            node.batteryLevel = static_cast<uint8_t>(pos.battery_level());
        }
    }
}

void MeshtasticDecoder::reassembleFragments(DecodedPacket& packet) {
    // Placeholder for fragment reassembly
}

void MeshtasticDecoder::displayPacket(DecodedPacket& packet) {
    // Placeholder for packet display
}

const MeshtasticDecoder::NodeInfo* MeshtasticDecoder::getNode(uint32_t id) const {
    std::lock_guard<std::mutex> lock(nodeDatabaseMutex);
    auto it = nodeDatabase.find(id);
    if (it != nodeDatabase.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<MeshtasticDecoder::NodeInfo> MeshtasticDecoder::getAllNodes() const {
    std::lock_guard<std::mutex> lock(nodeDatabaseMutex);
    std::vector<NodeInfo> nodes;
    nodes.reserve(nodeDatabase.size());
    for (const auto& pair : nodeDatabase) {
        nodes.push_back(pair.second);
    }
    return nodes;
}

void MeshtasticDecoder::loadConfig(const nlohmann::json& config) {
    if (config.contains("channels")) {
        for (const auto& ch : config["channels"]) {
            ChannelInfo channel;
            channel.name = ch.value("name", "");
            channel.pskBase64 = ch.value("psk", "");
            channel.enabled = ch.value("enabled", true);
            channel.useAES256 = ch.value("useAES256", false);
            channel.index = ch.value("index", 0);
            addChannel(channel);
            if (!channel.pskBase64.empty()) {
                updateChannelKey(channel.index, channel.pskBase64, channel.useAES256);
            }
        }
    }

    if (config.contains("lora")) {
        const auto& lora = config["lora"];
        loraConfig.spreadingFactor = lora.value("spreadingFactor", 7);
        loraConfig.bandwidth = lora.value("bandwidth", 125000);
        loraConfig.codingRate = lora.value("codingRate", 5);
        loraConfig.preambleLength = lora.value("preambleLength", 8);
        loraConfig.syncWord = static_cast<uint8_t>(lora.value("syncWord", 0x34));
        loraConfig.crcEnabled = lora.value("crcEnabled", true);
        loraConfig.implicitHeader = lora.value("implicitHeader", false);
        loraConfig.iqInverted = lora.value("iqInverted", false);
        loraConfig.centerFrequency = lora.value("centerFrequency", 915000000.0);
        loraConfig.sampleRate = lora.value("sampleRate", 1000000.0);
        setLoRaConfig(loraConfig);
    }
}

nlohmann::json MeshtasticDecoder::saveConfig() const {
    nlohmann::json config;

    // Save channels
    std::lock_guard<std::mutex> lock(channelsMutex);
    nlohmann::json channelsJson = nlohmann::json::array();
    for (const auto& ch : channels) {
        nlohmann::json chJson;
        chJson["name"] = ch.name;
        chJson["psk"] = ch.pskBase64;
        chJson["enabled"] = ch.enabled;
        chJson["useAES256"] = ch.useAES256;
        chJson["index"] = ch.index;
        channelsJson.push_back(chJson);
    }
    config["channels"] = channelsJson;

    // Save LoRa config
    nlohmann::json loraJson;
    loraJson["spreadingFactor"] = loraConfig.spreadingFactor;
    loraJson["bandwidth"] = loraConfig.bandwidth;
    loraJson["codingRate"] = loraConfig.codingRate;
    loraJson["preambleLength"] = loraConfig.preambleLength;
    loraJson["syncWord"] = loraConfig.syncWord;
    loraJson["crcEnabled"] = loraConfig.crcEnabled;
    loraJson["implicitHeader"] = loraConfig.implicitHeader;
    loraJson["iqInverted"] = loraConfig.iqInverted;
    loraJson["centerFrequency"] = loraConfig.centerFrequency;
    loraJson["sampleRate"] = loraConfig.sampleRate;
    config["lora"] = loraJson;

    return config;
}

std::deque<MeshtasticDecoder::DecodedPacket> MeshtasticDecoder::getPacketHistory() const {
    std::lock_guard<std::mutex> lock(packetHistoryMutex);
    return packetHistory;
}

std::string MeshtasticDecoder::nodeIdToString(uint32_t id) const {
    char buf[16];
    snprintf(buf, sizeof(buf), "!%08X", id);
    return std::string(buf);
}

std::string MeshtasticDecoder::portNumToString(uint8_t port) const {
    return MeshtasticProtocol::MeshtasticProtocolParser::portNumToString(port);
}

std::string MeshtasticDecoder::formatTime(uint32_t ms) const {
    uint32_t hours = ms / 3600000;
    uint32_t minutes = (ms % 3600000) / 60000;
    uint32_t seconds = (ms % 60000) / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, minutes, seconds);
    return std::string(buf);
}

} // namespace MeshtasticDecoder