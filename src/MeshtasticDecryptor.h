#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include <string_view>

class MeshtasticDecryptor {
public:
    MeshtasticDecryptor() = default;
    ~MeshtasticDecryptor() = default;

    // Initialize with default or provided keys
    bool init();

    // Set encryption keys
    void setAppKey(const std::array<uint8_t, 32>& key);      // AES-256
    void setAppKey(const std::array<uint8_t, 16>& key);      // AES-128
    void setNetworkKey(const std::array<uint8_t, 16>& key);  // Always AES-128

    // Set keys from PSK (Pre-Shared Key) using PBKDF2
    bool setKeyFromPSK(const std::string& psk, bool isAppKey = true);

    // Convenience: set both app and network key from same PSK (Meshtastic standard)
    bool setPSK(const std::string& psk, bool useAES256 = false) {
        return setKeyFromPSK(psk, true) && setKeyFromPSK(psk, false);
    }

    // Decrypt a Meshtastic packet payload
    // Returns empty vector on failure
    std::vector<uint8_t> decryptPacket(const std::vector<uint8_t>& encryptedPayload,
                                       uint32_t packetId,
                                       uint32_t fromNodeId);

    // Check if keys are set
    bool hasAppKey() const { return hasAppKey_; }
    bool hasNetworkKey() const { return hasNetworkKey_; }

    // Get last error
    std::string getLastError() const { return lastError; }

private:
    // AES-CTR decryption with 128-bit key
    std::vector<uint8_t> aesCtrDecrypt(const std::vector<uint8_t>& ciphertext,
                                       const std::array<uint8_t, 16>& key,
                                       const std::array<uint8_t, 16>& nonce);

    // AES-CTR decryption with 256-bit key
    std::vector<uint8_t> aesCtrDecrypt256(const std::vector<uint8_t>& ciphertext,
                                          const std::array<uint8_t, 32>& key,
                                          const std::array<uint8_t, 16>& nonce);

    // Build nonce from packet_id (4B LE) + from_node (4B LE) + 4 zero bytes
    std::array<uint8_t, 16> buildNonce(uint32_t packetId, uint32_t fromNodeId);

    // PBKDF2-HMAC-SHA256 key derivation
    // 1000 iterations, salt "Meshtastic"
    bool deriveKeyFromPSK(const std::string& psk,
                          std::array<uint8_t, 16>& key128,
                          std::array<uint8_t, 32>& key256,
                          bool isAppKey);

    // AES-CTR implementation using CryptoPP
    template<size_t KeySize>
    std::vector<uint8_t> aesCtrDecryptImpl(const std::vector<uint8_t>& ciphertext,
                                           const std::array<uint8_t, KeySize>& key,
                                           const std::array<uint8_t, 16>& nonce);

    // State
    std::vector<uint8_t> appKey128;
    std::vector<uint8_t> appKey256;
    std::vector<uint8_t> networkKey;
    bool hasAppKey_ = false;
    bool hasNetworkKey_ = false;
    std::string lastError;

    static constexpr const char* PBKDF2_SALT = "Meshtastic";
    static constexpr int PBKDF2_ITERATIONS = 1000;
};