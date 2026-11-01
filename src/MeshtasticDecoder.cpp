#include "MeshtasticDecoder.h"
#include "MeshtasticDecryptor.h"
#include "MeshtasticProtocol.h"
#include "mesh.pb.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

using nlohmann::json;

MeshtasticDecoder::MeshtasticDecoder() {
    // Initialize LoRa demodulator with default config
    LoRaModemConfig defaultConfig;
    loraDemod = std::make_unique<LoRaDemodulator>();
    loraDemod->init(defaultConfig);
}

MeshtasticDecoder::~MeshtasticDecoder() {
}

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
        // Re-index
        for (size_t i = 0; i < channels.size(); i++) {
            channels[i].index = static_cast<int>(i);
        }
    }
}

void MeshtasticDecoder::updateChannelKey(int index, const std::string& pskBase64, bool useAES256) {
    std::lock_guard<std::mutex> lock(channelsMutex);
    if (index >= 0 && index < static_cast<int>(channels.size())) {
        auto derivedKey = MeshtasticDecryptor::deriveKeyFromPSK(
            pskBase64,
            useAES256 ? MeshtasticDecryptor::KeyType::AES256 : MeshtasticDecryptor::KeyType::AES128
        );
        if (derivedKey.size() >= 16) {
            std::copy(derivedKey.begin(), derivedKey.begin() + 16, channels[index].aesKey.begin());
            channels[index].pskBase64 = pskBase64;
            channels[index].useAES256 = useAES256;
        }
    }
}

std::vector<DecodedPacket> MeshtasticDecoder::processIQ(const Complex* iq, size_t count, double sampleRate) {
    std::vector<DecodedPacket> results;

    if (!loraDemod || !loraDemod->isInitialized()) {
        return results;
    }

    // Process IQ through LoRa demodulator
    auto loraPackets = loraDemod->process(iq, count, sampleRate);

    for (auto& loraPacket : loraPackets) {
        packetsReceived++;

        DecodedPacket decoded;
        decoded.timestamp = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        decoded.rssi = loraPacket.rssi;
        decoded.snr = loraPacket.snr;

        // Parse the Meshtastic packet
        if (processRawPacketInternal(loraPacket.payload, decoded)) {
            packetsDecoded++;
            results.push_back(decoded);
        } else {
            packetsFailed++;
        }
    }

    return results;
}

bool MeshtasticDecoder::processRawPacketInternal(const std::vector<uint8_t>& rawPacket, DecodedPacket& decoded) {
    // Parse mesh header
    MeshHeader header;
    if (!decodeMeshHeader(rawPacket.data(), rawPacket.size(), header)) {
        decoded.error = "Invalid mesh header";
        decoded.valid = false;
        return false;
    }

    decoded.header = header;

    // Validate CRC
    if (!validateCRC(rawPacket.data(), rawPacket.size())) {
        decoded.error = "CRC check failed";
        decoded.valid = false;
        return false;
    }

    // Parse protobuf payload
    meshtastic::MeshPacket packet;
    MeshtasticProtocolParser parser;

    if (!parser.parsePacket(header.payload.data(), header.payload.size(), packet)) {
        decoded.error = "Protobuf parsing failed";
        decoded.valid = false;
        return false;
    }

    decoded.protoPacket = std::make_unique<meshtastic::MeshPacket>(packet);

    // Try to decrypt
    ChannelInfo usedChannel;
    if (tryDecryptWithAllChannels(*decoded.protoPacket, usedChannel)) {
        decoded.decrypted = true;
        packetsDecrypted++;
    }

    // Extract payload - use a DecodedMeshtasticPacket for extraction
    DecodedMeshtasticPacket decodedProto;
    decodedProto.header = header;
    decodedProto.protoPacket = decoded.protoPacket;
    if (parser.extractPayload(packet, decodedProto)) {
        decoded.decodedText = decodedProto.textMessage;
        // Copy other fields as needed
    }

    // Update node database
    updateNodeDatabase(packet);

    // Handle fragments
    reassembleFragments(decoded);

    // Generate display text
    displayPacket(decoded);

    decoded.valid = true;
    return true;
}

DecodedPacket MeshtasticDecoder::processRawPacket(const std::vector<uint8_t>& rawPacket, int rssi, float snr) {
    DecodedPacket decoded;
    decoded.rssi = rssi;
    decoded.snr = snr;
    decoded.timestamp = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    processRawPacketInternal(rawPacket, decoded);
    return decoded;
}

bool MeshtasticDecoder::decodeMeshHeader(const uint8_t* data, size_t len, MeshHeader& header) {
    return MeshHeader::deserialize(data, len, header);
}

bool MeshtasticDecoder::validateCRC(const uint8_t* data, size_t len) {
    if (len < 2) return false;
    uint16_t expectedCRC = (uint16_t)data[len - 2] | ((uint16_t)data[len - 1] << 8);
    uint16_t calcCRC = MeshtasticProtocolParser::calculateCRC(data, len - 2);
    return calcCRC == expectedCRC;
}

bool MeshtasticDecoder::tryDecryptWithAllChannels(meshtastic::MeshPacket& packet, ChannelInfo& usedChannel) {
    std::lock_guard<std::mutex> lock(channelsMutex);

    if (!packet.encrypted() || channels.empty()) {
        return false;
    }

    // Try each enabled channel
    for (auto& channel : channels) {
        if (!channel.enabled) continue;

        // Create decryptor with channel key
        MeshtasticDecryptor decryptor;
        if (channel.useAES256) {
            if (!decryptor.setKey(channel.aesKey)) continue;
        } else {
            std::array<uint8_t, 16> key128;
            std::copy(channel.aesKey.begin(), channel.aesKey.begin() + 16, key128.begin());
            if (!decryptor.setKey(key128)) continue;
        }

        // Try to decrypt
        meshtastic::MeshPacket testPacket = packet;
        if (decryptor.decrypt(testPacket)) {
            // Verify decryption by checking protobuf structure
            if (MeshtasticCrypto::verifyDecryption(
                    reinterpret_cast<const uint8_t*>(testPacket.payload().data()),
                    testPacket.payload().size())) {
                packet = std::move(testPacket);
                usedChannel = channel;
                return true;
            }
        }
    }

    return false;
}

std::array<uint8_t, 32> MeshtasticDecoder::deriveKeyFromPSK(const std::string& pskBase64, bool useAES256) {
    auto derived = MeshtasticDecryptor::deriveKeyFromPSK(
        pskBase64,
        useAES256 ? MeshtasticDecryptor::KeyType::AES256 : MeshtasticDecryptor::KeyType::AES128
    );
    std::array<uint8_t, 32> key = {};
    size_t copyLen = std::min(derived.size(), key.size());
    std::copy(derived.begin(), derived.begin() + copyLen, key.begin());
    return key;
}

std::array<uint8_t, 16> MeshtasticDecoder::buildNonce(const meshtastic::MeshPacket& packet) {
    return MeshtasticDecryptor::buildNonce(packet.id(), packet.from());
}

void MeshtasticDecoder::handleDecodedPacket(DecodedPacket& packet) {
    // Add to history
    {
        std::lock_guard<std::mutex> lock(packetHistoryMutex);
        packetHistory.push_back(packet);
        if (packetHistory.size() > maxPacketHistory) {
            packetHistory.pop_front();
        }
    }
}

void MeshtasticDecoder::updateNodeDatabase(const meshtastic::MeshPacket& packet) {
    std::lock_guard<std::mutex> lock(nodeDatabaseMutex);

    uint32_t nodeId = packet.from();
    auto& node = nodeDatabase[nodeId];
    node.id = nodeId;
    node.lastSeen = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    node.lastRssi = 0;  // Would be set from signal quality
    node.lastSnr = 0;

    // Update node info if this is a NODEINFO packet
    if (packet.portnum() == static_cast<uint32_t>(MSPortNum::NODEINFO_APP) && packet.has_nodeinfo()) {
        node.longName = packet.nodeinfo().long_name();
        node.shortName = packet.nodeinfo().short_name();
        node.hwModel = packet.nodeinfo().hw_model();
        node.role = packet.nodeinfo().role();
    }

    // Update position if available
    if (packet.portnum() == static_cast<uint32_t>(MSPortNum::POSITION_APP) && packet.has_position()) {
        node.latitude = packet.position().latitude();
        node.longitude = packet.position().longitude();
        node.altitude = packet.position().altitude();
        node.batteryLevel = packet.position().battery_level();
    }
}

void MeshtasticDecoder::reassembleFragments(DecodedPacket& packet) {
    if (!packet.protoPacket || !packet.protoPacket->is_fragment()) {
        return;
    }

    std::lock_guard<std::mutex> lock(fragmentBuffersMutex);
    uint32_t fragId = packet.protoPacket->fragment_id();

    auto& buffer = fragmentBuffers[fragId];
    buffer.fragmentId = fragId;
    buffer.totalFragments = packet.protoPacket->fragment_total();
    buffer.fromNode = packet.protoPacket->from();
    buffer.firstFragmentTime = packet.timestamp;

    // Store this fragment
    buffer.fragments[packet.protoPacket->fragment_index()] = packet.header.payload;

    // Check if we have all fragments
    if (buffer.fragments.size() >= buffer.totalFragments) {
        // Reassemble
        std::vector<uint8_t> reassembled;
        for (uint32_t i = 0; i < buffer.totalFragments; i++) {
            auto it = buffer.fragments.find(i);
            if (it != buffer.fragments.end()) {
                reassembled.insert(reassembled.end(), it->second.begin(), it->second.end());
            }
        }

        // Parse reassembled packet
        meshtastic::MeshPacket reassembledPacket;
        MeshtasticProtocolParser parser;
        if (parser.parsePacket(reassembled.data(), reassembled.size(), reassembledPacket)) {
            packet.protoPacket = std::make_unique<meshtastic::MeshPacket>(reassembledPacket);
            DecodedMeshtasticPacket decodedProto;
            decodedProto.header = packet.header;
            decodedProto.protoPacket = packet.protoPacket;
            parser.extractPayload(reassembledPacket, decodedProto);
            packet.decodedText = decodedProto.textMessage;
        }

        // Clean up
        fragmentBuffers.erase(fragId);
    }
}

void MeshtasticDecoder::displayPacket(DecodedPacket& packet) {
    if (packet.protoPacket) {
        // Generate from/to names
        packet.fromName = nodeIdToString(packet.header.source);
        if (packet.header.destination == MeshtasticProtocol::BROADCAST_ADDR) {
            packet.toName = "BROADCAST";
        } else {
            packet.toName = nodeIdToString(packet.header.destination);
        }

        // Check node database for names
        {
            std::lock_guard<std::mutex> lock(nodeDatabaseMutex);
            auto it = nodeDatabase.find(packet.header.source);
            if (it != nodeDatabase.end() && !it->second.longName.empty()) {
                packet.fromName = it->second.longName + " (" + packet.fromName + ")";
            }
            if (packet.header.destination != MeshtasticProtocol::BROADCAST_ADDR) {
                it = nodeDatabase.find(packet.header.destination);
                if (it != nodeDatabase.end() && !it->second.longName.empty()) {
                    packet.toName = it->second.longName + " (" + packet.toName + ")";
                }
            }
        }

        // Generate decoded text
        if (packet.protoPacket->portnum() == static_cast<uint32_t>(MSPortNum::TEXT_MESSAGE_APP) && packet.protoPacket->has_data()) {
            packet.decodedText = packet.protoPacket->data().text();
        } else {
            packet.decodedText = portNumToString(static_cast<uint8_t>(packet.protoPacket->portnum()));
        }
    }
}

std::string MeshtasticDecoder::nodeIdToString(uint32_t id) const {
    std::stringstream ss;
    ss << "!" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << id;
    return ss.str();
}

std::string MeshtasticDecoder::portNumToString(uint8_t port) const {
    return MeshtasticProtocolParser::portNumToString(port);
}

std::string MeshtasticDecoder::formatTime(uint32_t ms) const {
    auto secs = ms / 1000;
    auto hours = secs / 3600;
    auto mins = (secs % 3600) / 60;
    secs = secs % 60;
    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << hours << ":"
       << std::setw(2) << std::setfill('0') << mins << ":"
       << std::setw(2) << std::setfill('0') << secs;
    return ss.str();
}

// Configuration persistence
void MeshtasticDecoder::loadConfig(const nlohmann::json& config) {
    if (config.contains("lora")) {
        auto& lora = config["lora"];
        loraConfig.spreadingFactor = lora.value("spreading_factor", 7);
        loraConfig.bandwidth = lora.value("bandwidth", 125000);
        loraConfig.codingRate = lora.value("coding_rate", 5);
        loraConfig.preambleLength = lora.value("preamble_length", 8);
        loraConfig.syncWord = static_cast<uint8_t>(lora.value("sync_word", 0x12));
        loraConfig.crcEnabled = lora.value("crc_enabled", true);
        loraConfig.implicitHeader = lora.value("implicit_header", false);
        loraConfig.iqInverted = lora.value("iq_inverted", false);
        loraConfig.centerFrequency = lora.value("center_frequency", 915000000.0);
        loraConfig.sampleRate = lora.value("sample_rate", 1000000.0);
        setLoRaConfig(loraConfig);
    }

    if (config.contains("channels")) {
        std::lock_guard<std::mutex> lock(channelsMutex);
        channels.clear();
        for (const auto& ch : config["channels"]) {
            ChannelInfo channel;
            channel.name = ch.value("name", "");
            channel.pskBase64 = ch.value("psk", "");
            channel.enabled = ch.value("enabled", true);
            channel.index = ch.value("index", 0);
            channel.useAES256 = ch.value("use_aes256", false);

            auto derived = deriveKeyFromPSK(channel.pskBase64, channel.useAES256);
            std::copy(derived.begin(), derived.begin() + std::min(derived.size(), channel.aesKey.size()), channel.aesKey.begin());

            channels.push_back(channel);
        }
    }
}

nlohmann::json MeshtasticDecoder::saveConfig() const {
    json config;

    // LoRa config
    config["lora"]["spreading_factor"] = loraConfig.spreadingFactor;
    config["lora"]["bandwidth"] = loraConfig.bandwidth;
    config["lora"]["coding_rate"] = loraConfig.codingRate;
    config["lora"]["preamble_length"] = loraConfig.preambleLength;
    config["lora"]["sync_word"] = loraConfig.syncWord;
    config["lora"]["crc_enabled"] = loraConfig.crcEnabled;
    config["lora"]["implicit_header"] = loraConfig.implicitHeader;
    config["lora"]["iq_inverted"] = loraConfig.iqInverted;
    config["lora"]["center_frequency"] = loraConfig.centerFrequency;
    config["lora"]["sample_rate"] = loraConfig.sampleRate;

    // Channels
    {
        std::lock_guard<std::mutex> lock(channelsMutex);
        for (const auto& channel : channels) {
            json ch;
            ch["name"] = channel.name;
            ch["psk"] = channel.pskBase64;
            ch["enabled"] = channel.enabled;
            ch["index"] = channel.index;
            ch["use_aes256"] = channel.useAES256;
            config["channels"].push_back(ch);
        }
    }

    return config;
}

const MeshtasticDecoder::NodeInfo* MeshtasticDecoder::getNode(uint32_t id) const {
    std::lock_guard<std::mutex> lock(nodeDatabaseMutex);
    auto it = nodeDatabase.find(id);
    if (it != nodeDatabase.end()) {
        return &it->second;
    }
    return nullptr;
}