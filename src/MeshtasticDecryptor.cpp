#include "MeshtasticDecryptor.h"
#include "MeshtasticProtocol.h"
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <cryptopp/base64.h>
#include <cryptopp/secblock.h>
#include <cryptopp/osrng.h>
#include <algorithm>
#include <cstring>

MeshtasticDecryptor::MeshtasticDecryptor() {
    // Constructor
}

MeshtasticDecryptor::~MeshtasticDecryptor() {
    cleanup();
}

void MeshtasticDecryptor::cleanup() {
    if (aesContext) {
        delete static_cast<CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption*>(aesContext);
        aesContext = nullptr;
    }
    key.clear();
    keySet = false;
}

bool MeshtasticDecryptor::initAESCTR() {
    cleanup();

    try {
        if (keyType == KeyType::AES128) {
            if (key.size() != 16) return false;
            aesContext = new CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption(
                key.data(), 16, std::array<uint8_t, 16>{}.data()  // IV will be set per packet
            );
        } else {
            if (key.size() != 32) return false;
            aesContext = new CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption(
                key.data(), 32, std::array<uint8_t, 16>{}.data()
            );
        }
        return true;
    } catch (const std::exception& e) {
        cleanup();
        return false;
    }
}

bool MeshtasticDecryptor::setKey(const std::array<uint8_t, 16>& key) {
    this->key.assign(key.begin(), key.end());
    this->keyType = KeyType::AES128;
    this->keySet = true;
    return initAESCTR();
}

bool MeshtasticDecryptor::setKey(const std::array<uint8_t, 32>& key) {
    this->key.assign(key.begin(), key.end());
    this->keyType = KeyType::AES256;
    this->keySet = true;
    return initAESCTR();
}

bool MeshtasticDecryptor::setKeyFromPSK(const std::string& pskBase64, KeyType type) {
    auto derived = deriveKeyFromPSK(pskBase64, type);
    if (derived.empty()) return false;

    if (type == KeyType::AES128) {
        if (derived.size() < 16) return false;
        std::array<uint8_t, 16> key;
        std::copy(derived.begin(), derived.begin() + 16, key.begin());
        return setKey(key);
    } else {
        if (derived.size() < 32) return false;
        std::array<uint8_t, 32> key;
        std::copy(derived.begin(), derived.begin() + 32, key.begin());
        return setKey(key);
    }
}

std::vector<uint8_t> MeshtasticDecryptor::deriveKeyFromPSK(const std::string& pskBase64,
                                                            KeyType type,
                                                            int iterations) {
    try {
        // Decode base64 PSK
        std::string decoded;
        CryptoPP::StringSource(pskBase64, true,
            new CryptoPP::Base64Decoder(
                new CryptoPP::StringSink(decoded)
            )
        );

        if (decoded.empty()) {
            return {};
        }

        // Meshtastic uses PBKDF2-HMAC-SHA256
        // Salt is "meshtastic" (10 bytes)
        // Iterations: 100000 (configurable)
        // Key length: 16 bytes (AES-128) or 32 bytes (AES-256)
        size_t keyLen = (type == KeyType::AES128) ? 16 : 32;
        std::vector<uint8_t> derivedKey(keyLen);

        CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;
        pbkdf2.DeriveKey(
            derivedKey.data(), derivedKey.size(),
            0,  // id (not used)
            reinterpret_cast<const CryptoPP::byte*>(decoded.data()), decoded.size(),
            reinterpret_cast<const CryptoPP::byte*>(PBKDF2_SALT), strlen(PBKDF2_SALT),
            iterations
        );

        return derivedKey;
    } catch (const std::exception&) {
        return {};
    }
}

bool MeshtasticDecryptor::decrypt(meshtastic::MeshPacket& packet) {
    if (!keySet || !aesContext || packet.payload().empty()) {
        return false;
    }

    try {
        // Build nonce from packet ID and source node
        // Meshtastic nonce format: 8 bytes packet_id (LE) + 8 bytes from_node (LE)
        std::array<uint8_t, 16> nonce = buildNonce(packet.id(), packet.from());

        // Decrypt payload
        std::string decrypted;
        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption* decryptor =
            static_cast<CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption*>(aesContext);

        // Resync with new nonce
        decryptor->Resynchronize(nonce.data(), nonce.size());

        CryptoPP::StringSource ss(
            packet.payload().data(), packet.payload().size(), true,
            new CryptoPP::StreamTransformationFilter(*decryptor,
                new CryptoPP::StringSink(decrypted)
            )
        );

        // Replace payload with decrypted data
        packet.set_payload(decrypted);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool MeshtasticDecryptor::decryptPayload(const uint8_t* ciphertext, size_t len,
                                         uint8_t* plaintext,
                                         const std::array<uint8_t, 16>& nonce) {
    if (!keySet || !aesContext || !ciphertext || !plaintext || len == 0) {
        return false;
    }

    try {
        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption* decryptor =
            static_cast<CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption*>(aesContext);

        // Resync with new nonce
        decryptor->Resynchronize(nonce.data(), nonce.size());

        // Decrypt directly to output buffer
        decryptor->ProcessData(plaintext, ciphertext, len);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::array<uint8_t, 16> MeshtasticDecryptor::buildNonce(uint64_t packetId, uint64_t fromNode) {
    std::array<uint8_t, 16> nonce = {};

    // Meshtastic nonce: 8 bytes packet_id (little-endian) + 8 bytes from_node (little-endian)
    for (int i = 0; i < 8; i++) {
        nonce[i] = static_cast<uint8_t>((packetId >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; i++) {
        nonce[8 + i] = static_cast<uint8_t>((fromNode >> (i * 8)) & 0xFF);
    }

    return nonce;
}

// ============================================================================
// MeshtasticCrypto namespace implementation
// ============================================================================

namespace MeshtasticCrypto {

std::vector<uint8_t> base64Decode(const std::string& input) {
    std::string decoded;
    try {
        CryptoPP::StringSource(input, true,
            new CryptoPP::Base64Decoder(
                new CryptoPP::StringSink(decoded)
            )
        );
        return std::vector<uint8_t>(decoded.begin(), decoded.end());
    } catch (const std::exception&) {
        return {};
    }
}

std::string base64Encode(const uint8_t* data, size_t len) {
    std::string encoded;
    try {
        CryptoPP::StringSource(data, len, true,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(encoded),
                false  // no line breaks
            )
        );
        return encoded;
    } catch (const std::exception&) {
        return "";
    }
}

std::vector<uint8_t> parsePSK(const std::string& pskBase64) {
    return base64Decode(pskBase64);
}

bool verifyDecryption(const uint8_t* data, size_t len) {
    if (!data || len < 4) return false;

    // Quick check: Meshtastic packets start with 0x94 flag
    // But after decryption, it's a protobuf which doesn't have fixed magic
    // Check for valid protobuf varint structure

    // Try to parse as protobuf MeshPacket
    // First byte should be a valid field tag (field 1 = from node, wire type 0 = varint)
    // So first byte should have lower 3 bits = 0 (varint) and field number >= 1
    uint8_t firstByte = data[0];
    uint8_t wireType = firstByte & 0x07;
    uint8_t fieldNum = firstByte >> 3;

    // Valid mesh packet fields: 1=from, 2=to, 3=id, 4=channel, etc.
    // All are varint (wire type 0)
    if (wireType != 0 || fieldNum == 0 || fieldNum > 25) {
        return false;
    }

    return true;
}

} // namespace MeshtasticCrypto