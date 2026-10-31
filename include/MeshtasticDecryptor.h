#pragma once

#include <array>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace meshtastic {
class MeshPacket;
}

// AES decryption for Meshtastic packets
// Meshtastic uses AES-128-CTR or AES-256-CTR with a specific nonce format
// Nonce format: 8 bytes packet_id (little-endian) + 8 bytes from_node (little-endian)

class MeshtasticDecryptor {
public:
    enum class KeyType {
        AES128,  // 16-byte key
        AES256   // 32-byte key
    };

    MeshtasticDecryptor();
    ~MeshtasticDecryptor();

    // Set the decryption key
    bool setKey(const std::array<uint8_t, 16>& key);      // AES-128
    bool setKey(const std::array<uint8_t, 32>& key);      // AES-256
    bool setKeyFromPSK(const std::string& pskBase64, KeyType type = KeyType::AES128);

    // Decrypt a Meshtastic packet payload
    // Returns true on success, false on failure
    bool decrypt(meshtastic::MeshPacket& packet);

    // Decrypt raw payload data
    // nonce: 16-byte nonce (8 bytes packet_id LE + 8 bytes from_node LE)
    bool decryptPayload(const uint8_t* ciphertext, size_t len,
                        uint8_t* plaintext,
                        const std::array<uint8_t, 16>& nonce);

    // Build nonce from packet fields
    // Meshtastic nonce = packet_id (8 bytes LE) + from_node (8 bytes LE)
    static std::array<uint8_t, 16> buildNonce(uint64_t packetId, uint64_t fromNode);

    // Derive AES key from PSK using PBKDF2-HMAC-SHA256
    // Meshtastic uses: key = PBKDF2-HMAC-SHA256(psk, salt="meshtastic", iterations=100000, keyLen=16/32)
    static std::vector<uint8_t> deriveKeyFromPSK(const std::string& pskBase64,
                                                  KeyType type = KeyType::AES128,
                                                  int iterations = 100000);

    // Check if key is set
    bool hasKey() const { return keySet; }
    KeyType getKeyType() const { return keyType; }

private:
    void* aesContext = nullptr;  // Opaque pointer to Crypto++ or OpenSSL context
    std::vector<uint8_t> key;    // 16 or 32 bytes
    KeyType keyType = KeyType::AES128;
    bool keySet = false;

    // Initialize AES-CTR context
    bool initAESCTR();

    // Cleanup
    void cleanup();
};

// Utility functions
namespace MeshtasticCrypto {
    // Base64 decode
    std::vector<uint8_t> base64Decode(const std::string& input);

    // Base64 encode
    std::string base64Encode(const uint8_t* data, size_t len);

    // Convert Meshtastic PSK (base64) to raw bytes
    // PSK can be 16 bytes (AES-128) or 32 bytes (AES-256) base64 encoded
    std::vector<uint8_t> parsePSK(const std::string& pskBase64);

    // Verify decryption by checking for valid protobuf structure
    bool verifyDecryption(const uint8_t* data, size_t len);
}