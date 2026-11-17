#pragma once

#include <complex>
#include <vector>
#include <cstdint>

using Complex = std::complex<float>;

struct LoRaModemConfig {
    int spreadingFactor = 7;        // SF7-SF12
    int bandwidth = 125000;         // Hz
    int codingRate = 5;             // 4/5, 4/6, 4/7, 4/8
    int preambleLength = 8;         // symbols
    uint8_t syncWord = 0x34;        // 0x12 for LoRaWAN, 0x34 for Meshtastic
    bool crcEnabled = true;
    bool implicitHeader = false;
    bool iqInverted = false;
    double centerFrequency = 915000000.0; // Hz
    double sampleRate = 1000000.0;  // Hz
};

struct LoRaPacket {
    std::vector<uint8_t> payload;
    int rssi = 0;
    float snr = 0.0f;
    double timestamp = 0;
    int spreadingFactor = 7;
    int bandwidth = 125000;
    bool crcValid = false;
};

class LoRaDemodulator {
public:
    LoRaDemodulator() : initialized(false) {}
    ~LoRaDemodulator();

    bool init(const LoRaModemConfig& config);
    void configure(const LoRaModemConfig& config);
    bool isInitialized() const { return initialized; }

    // Process IQ samples and return detected packets
    std::vector<LoRaPacket> process(const std::complex<float>* iq, size_t count, double sampleRate);

    // Get configuration
    LoRaModemConfig getConfig() const { return config; }

    void start() { running = true; }
    void stop() { running = false; }
    bool isRunning() const { return running; }

private:
    LoRaModemConfig config;
    bool initialized = false;
    bool running = false;

    // Built-in demodulator state (fallback when liquid-dsp not available)
    struct BuiltinState {
        std::vector<std::complex<float>> buffer;
        size_t sampleCount = 0;
        double freqOffset = 0;
        int sf = 7;
        int bw = 125000;
        double sampleRate = 1000000.0;
        double centerFreq = 915000000.0;
    };
    std::unique_ptr<BuiltinState> builtinState;

    // Built-in LoRa demodulation (software fallback)
    std::vector<uint8_t> processBuiltin(const std::complex<float>* iq, size_t count, double sampleRate);

    // Preamble detection
    bool detectPreamble(const std::complex<float>* iq, size_t count, size_t& preambleStart);

    // Symbol demodulation
    std::vector<uint8_t> demodulateSymbols(const std::complex<float>* iq, size_t count, int sf, int bw);

    // Deinterleaving and decoding
    std::vector<uint8_t> deinterleaveAndDecode(const std::vector<uint8_t>& symbols, int sf, int cr);

    // Packet decoding
    std::vector<uint8_t> decodePacket(const std::complex<float>* iq, size_t count);

    // CRC check
    bool checkCRC(const std::vector<uint8_t>& data);

    // RSSI/SNR estimation
    int estimateRSSI(const std::complex<float>* iq, size_t count);
    float estimateSNR(const std::complex<float>* iq, size_t count);
};