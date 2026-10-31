// Decryptor tests for Meshtastic AES-CTR decryption

#include <gtest/gtest.h>
#include "MeshtasticDecryptor.h"
#include "mesh.pb.h"

using namespace meshtastic;

TEST(DecryptorTests, SetKeyAES128) {
    MeshtasticDecryptor decryptor;
    std::array<uint8_t, 16> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    ASSERT_TRUE(decryptor.setKey(key));
    ASSERT_TRUE(decryptor.hasKey());
    ASSERT_EQ(decryptor.getKeyType(), MeshtasticDecryptor::KeyType::AES128);
}

TEST(DecryptorTests, SetKeyAES256) {
    MeshtasticDecryptor decryptor;
    std::array<uint8_t, 32> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
    ASSERT_TRUE(decryptor.setKey(key));
    ASSERT_TRUE(decryptor.hasKey());
    ASSERT_EQ(decryptor.getKeyType(), MeshtasticDecryptor::KeyType::AES256);
}

TEST(DecryptorTests, SetKeyFromPSK) {
    MeshtasticDecryptor decryptor;

    // Test with a known PSK (base64 encoded 16 bytes)
    std::string pskBase64 = "AAECAwQFBgcICQoLDA0ODw==";  // 0x00..0x0F base64
    ASSERT_TRUE(decryptor.setKeyFromPSK(pskBase64, MeshtasticDecryptor::KeyType::AES128));
    ASSERT_TRUE(decryptor.hasKey());
}

TEST(DecryptorTests, BuildNonce) {
    uint64_t packetId = 0x0102030405060708;
    uint64_t fromNode = 0x1122334455667788;

    auto nonce = MeshtasticDecryptor::buildNonce(packetId, fromNode);

    // Check little-endian encoding
    ASSERT_EQ(nonce[0], 0x08);
    ASSERT_EQ(nonce[1], 0x07);
    ASSERT_EQ(nonce[2], 0x06);
    ASSERT_EQ(nonce[3], 0x05);
    ASSERT_EQ(nonce[4], 0x04);
    ASSERT_EQ(nonce[5], 0x03);
    ASSERT_EQ(nonce[6], 0x02);
    ASSERT_EQ(nonce[7], 0x01);

    ASSERT_EQ(nonce[8], 0x88);
    ASSERT_EQ(nonce[9], 0x77);
    ASSERT_EQ(nonce[10], 0x66);
    ASSERT_EQ(nonce[11], 0x55);
    ASSERT_EQ(nonce[12], 0x44);
    ASSERT_EQ(nonce[13], 0x33);
    ASSERT_EQ(nonce[14], 0x22);
    ASSERT_EQ(nonce[15], 0x11);
}

TEST(DecryptorTests, DeriveKeyFromPSK) {
    std::string pskBase64 = "AAECAwQFBgcICQoLDA0ODw==";  // 16 bytes

    auto key128 = MeshtasticDecryptor::deriveKeyFromPSK(pskBase64, MeshtasticDecryptor::KeyType::AES128);
    ASSERT_EQ(key128.size(), 16);

    auto key256 = MeshtasticDecryptor::deriveKeyFromPSK(pskBase64, MeshtasticDecryptor::KeyType::AES256);
    ASSERT_EQ(key256.size(), 32);

    // Keys should be deterministic
    auto key128_2 = MeshtasticDecryptor::deriveKeyFromPSK(pskBase64, MeshtasticDecryptor::KeyType::AES128);
    ASSERT_EQ(key128, key128_2);
}

TEST(DecryptorTests, EncryptDecryptRoundTrip) {
    MeshtasticDecryptor encryptor;
    MeshtasticDecryptor decryptor;

    std::array<uint8_t, 16> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    ASSERT_TRUE(encryptor.setKey(key));
    ASSERT_TRUE(decryptor.setKey(key));

    // Create test packet
    MeshPacket packet;
    packet.set_from(0x12345678);
    packet.set_to(0xFFFFFFFF);
    packet.set_id(42);
    packet.set_channel(0);
    packet.set_hop_limit(3);
    packet.set_portnum(1);
    packet.set_encrypted(true);

    Data data;
    data.set_text("Hello encrypted world!");
    packet.set_payload(data.SerializeAsString());

    std::string originalPayload = packet.payload();

    // Encrypt (using decryptor with CTR mode - same operation for encrypt/decrypt)
    ASSERT_TRUE(decryptor.decrypt(packet));
    ASSERT_NE(packet.payload(), originalPayload);

    // Decrypt
    ASSERT_TRUE(decryptor.decrypt(packet));
    ASSERT_EQ(packet.payload(), originalPayload);

    // Verify content
    MeshPacket verified;
    ASSERT_TRUE(verified.ParseFromString(packet.payload()));
    ASSERT_EQ(verified.data().text(), "Hello encrypted world!");
}

TEST(DecryptorTests, DecryptPayloadRaw) {
    MeshtasticDecryptor decryptor;
    std::array<uint8_t, 16> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    ASSERT_TRUE(decryptor.setKey(key));

    std::string plaintext = "Test plaintext for decryption";
    std::vector<uint8_t> ciphertext(plaintext.size());

    auto nonce = MeshtasticDecryptor::buildNonce(42, 0x12345678);

    // Encrypt
    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption encryptor;
    encryptor.SetKeyWithIV(key.data(), key.size(), nonce.data());
    encryptor.ProcessData(ciphertext.data(),
                          reinterpret_cast<const uint8_t*>(plaintext.data()),
                          plaintext.size());

    // Decrypt using raw function
    std::vector<uint8_t> decrypted(plaintext.size());
    ASSERT_TRUE(decryptor.decryptPayload(ciphertext.data(), ciphertext.size(),
                                         decrypted.data(), nonce));

    std::string result(decrypted.begin(), decrypted.end());
    ASSERT_EQ(result, plaintext);
}

// MeshtasticCrypto namespace tests
TEST(DecryptorTests, Base64EncodeDecode) {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

    std::string encoded = MeshtasticCrypto::base64Encode(data.data(), data.size());
    ASSERT_EQ(encoded, "AAECAwQFBgcICQoLDA0ODw==");

    auto decoded = MeshtasticCrypto::base64Decode(encoded);
    ASSERT_EQ(decoded, data);
}

TEST(DecryptorTests, ParsePSK) {
    std::string pskBase64 = "AAECAwQFBgcICQoLDA0ODw==";
    auto parsed = MeshtasticCrypto::parsePSK(pskBase64);
    ASSERT_EQ(parsed.size(), 16);
}

TEST(DecryptorTests, VerifyDecryption) {
    // Valid protobuf (MeshPacket with field 1 = varint)
    std::vector<uint8_t> validProtobuf = {0x08, 0x78, 0x56, 0x34, 0x12};  // field 1, value 0x12345678
    ASSERT_TRUE(MeshtasticCrypto::verifyDecryption(validProtobuf.data(), validProtobuf.size()));

    // Invalid - wrong wire type
    std::vector<uint8_t> invalidProtobuf = {0x09, 0x78, 0x56, 0x34, 0x12};  // field 1, wire type 1 (64-bit)
    ASSERT_FALSE(MeshtasticCrypto::verifyDecryption(invalidProtobuf.data(), invalidProtobuf.size()));

    // Too short
    std::vector<uint8_t> tooShort = {0x08};
    ASSERT_FALSE(MeshtasticCrypto::verifyDecryption(tooShort.data(), tooShort.size()));
}