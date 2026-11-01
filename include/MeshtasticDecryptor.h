#pragma once

#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

// Forward declare protobuf message
namespace google {
    namespace protobuf {
        class Message;
    }
}

namespace MeshtasticDecryptor {

enum class KeyType {
    AES128,
    AES256
};

class MeshtasticDecryptor {
public:
    MeshtasticDecryptor();
    ~MeshtasticDecryptor();

    // Set encryption key
    bool setKey(const std::array<uint8_t, 16>& key128);
    bool setKey(const std::array<uint8_t, 32>& key256);

    // Decrypt a MeshPacket (modifies the packet in place)
    bool decrypt(google::protobuf::Message& packet);

    // Static key derivation from PSK
    // Meshtastic uses PBKDF2-HMAC-SHA256 with salt "Meshtastic" and 1000 iterations
    static std::vector<uint8_t> deriveKeyFromPSK(const std::string& pskBase64, KeyType keyType);

    // Build Meshtastic nonce: packet_id (4 bytes LE) + from_node (4 bytes LE) + 4 zero bytes
    static std::array<uint8_t, 12> buildNonce(uint32_t packetId, uint32_t fromNode);

    // Get key type
    KeyType getKeyType() const { return keyType; }
    bool hasKey() const { return hasKey_; }

private:
    KeyType keyType = KeyType::AES128;
    std::vector<uint8_t> key;
    bool hasKey_ = false;

    // AES-CTR decryption implementation
    bool decryptCTR(const uint8_t* input, size_t len, uint8_t* output, const std::array<uint8_t, 12>& nonce);
};

} // namespace MeshtasticDecryptor