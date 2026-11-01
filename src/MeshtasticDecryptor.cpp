#include "MeshtasticDecryptor.h"
#include "MeshtasticProtocol.h"
#include "mesh.pb.h"
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/base64.h>
#include <cryptopp/secblock.h>
#include <algorithm>
#include <vector>
#include <array>

namespace MeshtasticDecryptor {

MeshtasticDecryptor::MeshtasticDecryptor() : keyType(KeyType::AES128), hasKey_(false) {
}

MeshtasticDecryptor::~MeshtasticDecryptor() {
    // Clear key material
    if (!key.empty()) {
        std::fill(key.begin(), key.end(), 0);
    }
}

bool MeshtasticDecryptor::setKey(const std::array<uint8_t, 16>& key128) {
    key.assign(key128.begin(), key128.end());
    keyType = KeyType::AES128;
    hasKey_ = true;
    return true;
}

bool MeshtasticDecryptor::setKey(const std::array<uint8_t, 32>& key256) {
    key.assign(key256.begin(), key256.end());
    keyType = KeyType::AES256;
    hasKey_ = true;
    return true;
}

bool MeshtasticDecryptor::decrypt(google::protobuf::Message& packet) {
    if (!hasKey_) return false;

    // Cast to MeshPacket to access fields
    auto* meshPacket = dynamic_cast<meshtastic::MeshPacket*>(&packet);
    if (!meshPacket) return false;

    if (!meshPacket->encrypted()) return false;

    // Get encrypted payload
    const std::string& encryptedPayload = meshPacket->payload();
    if (encryptedPayload.empty()) return false;

    // Build nonce: packet_id (4 bytes LE) + from_node (4 bytes LE) + 4 zero bytes
    std::array<uint8_t, 12> nonce = buildNonce(meshPacket->id(), meshPacket->from());

    // Prepare output buffer
    std::vector<uint8_t> decrypted(encryptedPayload.size());

    // Decrypt using AES-CTR
    if (!decryptCTR(
            reinterpret_cast<const uint8_t*>(encryptedPayload.data()),
            encryptedPayload.size(),
            decrypted.data(),
            nonce)) {
        return false;
    }

    // Replace payload with decrypted data
    meshPacket->set_payload(decrypted.data(), decrypted.size());
    meshPacket->set_encrypted(false);

    return true;
}

std::vector<uint8_t> MeshtasticDecryptor::deriveKeyFromPSK(const std::string& pskBase64, KeyType keyType) {
    // Decode base64 PSK
    std::string pskDecoded;
    try {
        CryptoPP::StringSource ss(pskBase64, true,
            new CryptoPP::Base64Decoder(
                new CryptoPP::StringSink(pskDecoded)
            )
        );
    } catch (...) {
        return {};
    }

    // Meshtastic uses PBKDF2-HMAC-SHA256 with salt "Meshtastic" and 1000 iterations
    const std::string salt = MeshtasticProtocol::PBKDF2_SALT;
    const unsigned int iterations = 1000;
    const size_t keyLength = (keyType == KeyType::AES256) ? 32 : 16;

    std::vector<uint8_t> derivedKey(keyLength);

    try {
        CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;
        pbkdf2.DeriveKey(
            derivedKey.data(), derivedKey.size(),
            0,  // default key derivation parameters
            reinterpret_cast<const CryptoPP::byte*>(pskDecoded.data()), pskDecoded.size(),
            reinterpret_cast<const CryptoPP::byte*>(salt.data()), salt.size(),
            iterations
        );
    } catch (...) {
        return {};
    }

    return derivedKey;
}

std::array<uint8_t, 12> MeshtasticDecryptor::buildNonce(uint32_t packetId, uint32_t fromNode) {
    return MeshtasticProtocol::MeshtasticCrypto::buildNonce(packetId, fromNode);
}

bool MeshtasticDecryptor::decryptCTR(const uint8_t* input, size_t len, uint8_t* output, const std::array<uint8_t, 12>& nonce) {
    try {
        // CryptoPP AES-CTR mode
        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption decryptor;

        // Key length: 16 bytes for AES-128, 32 bytes for AES-256
        size_t keyLen = (keyType == KeyType::AES256) ? 32 : 16;
        if (key.size() < keyLen) return false;

        // Set key and IV (nonce)
        decryptor.SetKeyWithIV(key.data(), keyLen, nonce.data(), nonce.size());

        // Process data
        decryptor.ProcessData(output, input, len);

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace MeshtasticDecryptor