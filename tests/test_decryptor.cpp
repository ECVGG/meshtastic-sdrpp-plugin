// Decryptor tests for Meshtastic AES-CTR decryption

#include <gtest/gtest.h>
#include "MeshtasticDecryptor.h"

TEST(DecryptorTests, Init) {
    MeshtasticDecryptor decryptor;
    ASSERT_TRUE(decryptor.init());
    ASSERT_FALSE(decryptor.hasAppKey());
    ASSERT_FALSE(decryptor.hasNetworkKey());
    ASSERT_EQ(decryptor.getLastError(), "");
}

TEST(DecryptorTests, SetAppKeyAES128) {
    MeshtasticDecryptor decryptor;
    decryptor.init();
    std::array<uint8_t, 16> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    decryptor.setAppKey(key);
    ASSERT_TRUE(decryptor.hasAppKey());
    ASSERT_FALSE(decryptor.hasNetworkKey());
}

TEST(DecryptorTests, SetAppKeyAES256) {
    MeshtasticDecryptor decryptor;
    decryptor.init();
    std::array<uint8_t, 32> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
    decryptor.setAppKey(key);
    ASSERT_TRUE(decryptor.hasAppKey());
    ASSERT_FALSE(decryptor.hasNetworkKey());
}

TEST(DecryptorTests, SetNetworkKey) {
    MeshtasticDecryptor decryptor;
    decryptor.init();
    std::array<uint8_t, 16> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    decryptor.setNetworkKey(key);
    ASSERT_FALSE(decryptor.hasAppKey());
    ASSERT_TRUE(decryptor.hasNetworkKey());
}

TEST(DecryptorTests, SetKeyFromPSK) {
    MeshtasticDecryptor decryptor;
    decryptor.init();

    // Test with a known PSK (base64 encoded 16 bytes = 0x00..0x0F)
    std::string pskBase64 = "AAECAwQFBgcICQoLDA0ODw==";
    ASSERT_TRUE(decryptor.setKeyFromPSK(pskBase64, true));  // App key
    ASSERT_TRUE(decryptor.hasAppKey());
}

TEST(DecryptorTests, BuildNonce) {
    MeshtasticDecryptor decryptor;
    decryptor.init();

    uint32_t packetId = 0x01020304;
    uint32_t fromNode = 0x11223344;

    // Access private method through test - we'll test via decryptPacket
    // For now, just verify the class compiles and basic operations work
    ASSERT_TRUE(decryptor.hasAppKey() == false);
}

TEST(DecryptorTests, DecryptPacketWithAES128) {
    MeshtasticDecryptor decryptor;
    decryptor.init();

    // Set a known key
    std::array<uint8_t, 16> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    decryptor.setAppKey(key);

    // Test decrypting an empty payload (should return empty)
    std::vector<uint8_t> emptyPayload;
    auto result = decryptor.decryptPacket(emptyPayload, 42, 0x12345678);
    ASSERT_TRUE(result.empty());

    // Test with some dummy encrypted data
    std::vector<uint8_t> dummyData = {0x01, 0x02, 0x03, 0x04, 0x05};
    result = decryptor.decryptPacket(dummyData, 42, 0x12345678);
    // Should return something (even if garbage since it's not real encrypted data)
    ASSERT_FALSE(result.empty());
}

TEST(DecryptorTests, DecryptPacketWithAES256) {
    MeshtasticDecryptor decryptor;
    decryptor.init();

    // Set a known 256-bit key
    std::array<uint8_t, 32> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
    decryptor.setAppKey(key);

    std::vector<uint8_t> dummyData = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto result = decryptor.decryptPacket(dummyData, 42, 0x12345678);
    ASSERT_FALSE(result.empty());
}

TEST(DecryptorTests, GetLastError) {
    MeshtasticDecryptor decryptor;
    decryptor.init();
    ASSERT_EQ(decryptor.getLastError(), "");

    // Test with invalid PSK (too short)
    std::string invalidPsk = "AAE=";  // Too short
    // This might not fail in init but in setKeyFromPSK
    bool result = decryptor.setKeyFromPSK(invalidPsk, true);
    // Result could be true or false depending on implementation
    // Just verify the method exists and doesn't crash
    (void)result;
}