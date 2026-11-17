#include "MeshtasticDecryptor.h"
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <algorithm>
#include <iostream>

bool MeshtasticDecryptor::init() {
    hasAppKey_ = false;
    hasNetworkKey_ = false;
    lastError.clear();
    return true;
}

void MeshtasticDecryptor::setAppKey(const std::array<uint8_t, 32>& key) {
    appKey256.assign(key.begin(), key.end());
    hasAppKey_ = true;
}

void MeshtasticDecryptor::setAppKey(const std::array<uint8_t, 16>& key) {
    appKey128.assign(key.begin(), key.end());
    hasAppKey_ = true;
}

void MeshtasticDecryptor::setNetworkKey(const std::array<uint8_t, 16>& key) {
    networkKey.assign(key.begin(), key.end());
    hasNetworkKey_ = true;
}

bool MeshtasticDecryptor::setKeyFromPSK(const std::string& psk, bool isAppKey) {
    std::array<uint8_t, 16> key128;
    std::array<uint8_t, 32> key256;

    if (!deriveKeyFromPSK(psk, key128, key256, true)) {
        return false;
    }

    if (isAppKey) {
        appKey128.assign(key128.begin(), key128.end());
        appKey256.assign(key256.begin(), key256.end());
        hasAppKey_ = true;
    } else {
        networkKey.assign(key128.begin(), key128.end());
        hasNetworkKey_ = true;
    }

    return true;
}

std::vector<uint8_t> MeshtasticDecryptor::decryptPacket(const std::vector<uint8_t>& encryptedPayload,
                                                        uint32_t packetId,
                                                        uint32_t fromNodeId) {
    if (encryptedPayload.size() < 17) { // Minimum: mesh header (15) + 1 byte protobuf + CRC (2)
        lastError = "Payload too small";
        return {};
    }

    // Meshtastic packet format:
    // Bytes 0-14: Mesh header (15 bytes) - NOT encrypted
    //   Byte 0: Flag (0x94)
    //   Bytes 1-4: Packet ID (4 bytes LE)
    //   Bytes 5-8: From Node (4 bytes LE)
    //   Bytes 9-12: To Node (4 bytes LE)
    //   Byte 13: Hop Limit
    //   Byte 14: Hop Start
    // Bytes 15...N-2: Protobuf payload - ENCRYPTED
    // Bytes N-2, N-1: CRC16-CCITT - NOT encrypted (CRC is computed over plaintext)

    // Extract the encrypted protobuf portion
    size_t protobufStart = 15;
    size_t protobufEnd = encryptedPayload.size() - 2; // Exclude CRC
    size_t protobufSize = protobufEnd - protobufStart;

    if (protobufSize == 0) {
        lastError = "No protobuf payload to decrypt";
        return {};
    }

    std::vector<uint8_t> encryptedProtobuf(encryptedPayload.begin() + protobufStart,
                                           encryptedPayload.begin() + protobufEnd);

    // Build nonce from packet_id (4B LE) + from_node (4B LE) + 4 zero bytes
    std::array<uint8_t, 16> nonce = buildNonce(packetId, fromNodeId);

    std::vector<uint8_t> decryptedProtobuf;

    // Try with AppKey (AES-256 first, then AES-128)
    if (hasAppKey_ && !appKey256.empty()) {
        decryptedProtobuf = aesCtrDecrypt256(encryptedProtobuf,
                                             *reinterpret_cast<const std::array<uint8_t, 32>*>(appKey256.data()),
                                             nonce);
        if (!decryptedProtobuf.empty()) {
            // Reconstruct full packet with decrypted protobuf
            std::vector<uint8_t> result;
            result.insert(result.end(), encryptedPayload.begin(), encryptedPayload.begin() + protobufStart);
            result.insert(result.end(), decryptedProtobuf.begin(), decryptedProtobuf.end());
            result.insert(result.end(), encryptedPayload.begin() + protobufEnd, encryptedPayload.end());
            return result;
        }
    }

    if (hasAppKey_ && !appKey128.empty()) {
        decryptedProtobuf = aesCtrDecrypt(encryptedProtobuf,
                                          *reinterpret_cast<const std::array<uint8_t, 16>*>(appKey128.data()),
                                          nonce);
        if (!decryptedProtobuf.empty()) {
            std::vector<uint8_t> result;
            result.insert(result.end(), encryptedPayload.begin(), encryptedPayload.begin() + protobufStart);
            result.insert(result.end(), decryptedProtobuf.begin(), decryptedProtobuf.end());
            result.insert(result.end(), encryptedPayload.begin() + protobufEnd, encryptedPayload.end());
            return result;
        }
    }

    // Try with Network Key
    if (hasNetworkKey_) {
        decryptedProtobuf = aesCtrDecrypt(encryptedProtobuf,
                                          *reinterpret_cast<const std::array<uint8_t, 16>*>(networkKey.data()),
                                          nonce);
        if (!decryptedProtobuf.empty()) {
            std::vector<uint8_t> result;
            result.insert(result.end(), encryptedPayload.begin(), encryptedPayload.begin() + protobufStart);
            result.insert(result.end(), decryptedProtobuf.begin(), decryptedProtobuf.end());
            result.insert(result.end(), encryptedPayload.begin() + protobufEnd, encryptedPayload.end());
            return result;
        }
    }

    lastError = "Failed to decrypt with available keys";
    return {};
}

std::array<uint8_t, 16> MeshtasticDecryptor::buildNonce(uint32_t packetId, uint32_t fromNodeId) {
    std::array<uint8_t, 16> nonce = {};

    // packet_id (4 bytes little-endian)
    nonce[0] = packetId & 0xFF;
    nonce[1] = (packetId >> 8) & 0xFF;
    nonce[2] = (packetId >> 16) & 0xFF;
    nonce[3] = (packetId >> 24) & 0xFF;

    // from_node (4 bytes little-endian)
    nonce[4] = fromNodeId & 0xFF;
    nonce[5] = (fromNodeId >> 8) & 0xFF;
    nonce[6] = (fromNodeId >> 16) & 0xFF;
    nonce[7] = (fromNodeId >> 24) & 0xFF;

    // Last 4 bytes are zeros (already zero-initialized)

    return nonce;
}

bool MeshtasticDecryptor::deriveKeyFromPSK(const std::string& psk,
                                           std::array<uint8_t, 16>& key128,
                                           std::array<uint8_t, 32>& key256,
                                           bool isAppKey) {
    try {
        CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;

        // Derive 256-bit key
        std::vector<uint8_t> derived256(32);
        pbkdf2.DeriveKey(derived256.data(), derived256.size(),
                         0,
                         reinterpret_cast<const CryptoPP::byte*>(psk.data()), psk.size(),
                         reinterpret_cast<const CryptoPP::byte*>(PBKDF2_SALT), strlen(PBKDF2_SALT),
                         PBKDF2_ITERATIONS);

        // Derive 128-bit key (first 16 bytes of 256-bit)
        std::vector<uint8_t> derived128(16);
        CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2_128;
        pbkdf2_128.DeriveKey(derived128.data(), derived128.size(),
                             0,
                             reinterpret_cast<const CryptoPP::byte*>(psk.data()), psk.size(),
                             reinterpret_cast<const CryptoPP::byte*>(PBKDF2_SALT), strlen(PBKDF2_SALT),
                             PBKDF2_ITERATIONS);

        std::copy(derived256.begin(), derived256.end(), key256.begin());
        std::copy(derived128.begin(), derived128.end(), key128.begin());

        return true;
    } catch (const std::exception& e) {
        lastError = std::string("PBKDF2 failed: ") + e.what();
        return false;
    }
}

template<size_t KeySize>
std::vector<uint8_t> MeshtasticDecryptor::aesCtrDecryptImpl(const std::vector<uint8_t>& ciphertext,
                                                            const std::array<uint8_t, KeySize>& key,
                                                            const std::array<uint8_t, 16>& nonce) {
    try {
        // Use CryptoPP's CTR mode
        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption decryptor;
        decryptor.SetKeyWithIV(key.data(), KeySize, nonce.data());

        std::vector<uint8_t> plaintext(ciphertext.size());
        decryptor.ProcessData(plaintext.data(), ciphertext.data(), ciphertext.size());

        return plaintext;
    } catch (const std::exception& e) {
        lastError = std::string("AES-CTR decryption failed: ") + e.what();
        return {};
    }
}

std::vector<uint8_t> MeshtasticDecryptor::aesCtrDecrypt(const std::vector<uint8_t>& ciphertext,
                                                        const std::array<uint8_t, 16>& key,
                                                        const std::array<uint8_t, 16>& nonce) {
    return aesCtrDecryptImpl<16>(ciphertext, key, nonce);
}

std::vector<uint8_t> MeshtasticDecryptor::aesCtrDecrypt256(const std::vector<uint8_t>& ciphertext,
                                                           const std::array<uint8_t, 32>& key,
                                                           const std::array<uint8_t, 16>& nonce) {
    return aesCtrDecryptImpl<32>(ciphertext, key, nonce);
}