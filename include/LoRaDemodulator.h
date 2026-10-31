#pragma once

#include <dsp/complex.h>
#include <vector>
#include <cstdint>
#include <memory>

// LoRa modem configuration
struct LoRaModemConfig {
    int spreadingFactor = 7;      // SF7-SF12
    int bandwidth = 125000;       // 125k, 250k, 500k Hz
    int codingRate = 5;           // 5=4/5, 6=4/6, 7=4/7, 8=4/8
    int preambleLength = 8;
    uint8_t syncWord = 0x12;      // 0x12 public, 0x2B private
    bool crcEnabled = true;
    bool implicitHeader = false;
    bool iqInverted = false;
    double centerFrequency = 915000000.0;
    double sampleRate = 1000000.0; // IQ sample rate
};

// LoRa packet detection result
struct LoRaPacket {
    std::vector<uint8_t> payload;
    float snr = 0;
    int rssi = 0;
    double frequencyOffset = 0;
    uint32_t timestamp = 0;
    bool crcValid = false;
    int spreadingFactor = 0;
    int bandwidth = 0;
};

// LoRa demodulator class
// Supports both liquid-dsp backend and built-in fallback implementation
class LoRaDemodulator {
public:
    LoRaDemodulator();
    ~LoRaDemodulator();

    // Initialize demodulator with configuration
    bool init(const LoRaModemConfig& config);

    // Update configuration (reinitializes if parameters change)
    void configure(const LoRaModemConfig& config);

    // Process IQ samples and return detected packets
    // iq: pointer to complex IQ samples (interleaved I/Q)
    // count: number of complex samples
    // Returns vector of detected packets
    std::vector<LoRaPacket> process(const dsp::complex_t* iq, size_t count);

    // Process IQ samples with explicit sample rate
    std::vector<LoRaPacket> process(const dsp::complex_t* iq, size_t count, double sampleRate);

    // Get current configuration
    const LoRaModemConfig& getConfig() const { return config; }

    // Get last measured signal quality
    float getLastSNR() const { return lastSNR; }
    int getLastRSSI() const { return lastRSSI; }

    // Check if demodulator is initialized
    bool isInitialized() const { return initialized; }

    // Reset internal state
    void reset();

    // Set callback for packet detection (optional, for real-time processing)
    using PacketCallback = std::function<void(const LoRaPacket&)>;
    void setPacketCallback(PacketCallback cb) { packetCallback = cb; }

private:
    LoRaModemConfig config;
    bool initialized = false;
    float lastSNR = 0;
    int lastRSSI = 0;
    PacketCallback packetCallback;

    // Backend selection
    enum class Backend {
        NONE,
        LIQUID_DSP,
        BUILTIN
    };
    Backend backend = Backend::NONE;

    // liquid-dsp backend state
    struct LiquidDSPState {
        void* modem = nullptr;
        void* sync = nullptr;
        void* nco = nullptr;
        void* fir = nullptr;
        std::vector<float> preambleWaveform;
        float phase = 0;
        float freqOffset = 0;
    };
    std::unique_ptr<LiquidDSPState> liquidState;

    // Built-in backend state (simplified LoRa detection)
    struct BuiltinState {
        // Chirp detection via FFT
        std::vector<dsp::complex_t> fftBuffer;
        std::vector<float> magBuffer;
        std::vector<float> preambleCorr;
        size_t fftSize = 1024;
        size_t hopSize = 256;

        // Symbol timing recovery
        float symbolRate = 0;
        float sampleRate = 0;
        int samplesPerSymbol = 0;

        // Preamble detection
        std::vector<float> chirpTemplate;
        float detectionThreshold = 0.3f;
        bool inPreamble = false;
        size_t preambleStart = 0;
        int preambleDetections = 0;

        // Packet assembly
        std::vector<uint8_t> currentPacket;
        size_t expectedPayloadLen = 0;
        bool headerDecoded = false;
        int symbolsRemaining = 0;

        // Deinterleaving / decoding tables
        std::vector<uint8_t> deinterleaveTable;
        std::vector<uint8_t> whiteningSequence;
    };
    std::unique_ptr<BuiltinState> builtinState;

    // Private methods
    bool initLiquidDSP();
    bool initBuiltin();
    void cleanupLiquidDSP();
    void cleanupBuiltin();

    // Liquid-dsp processing
    std::vector<LoRaPacket> processLiquidDSP(const dsp::complex_t* iq, size_t count);

    // Built-in processing
    std::vector<LoRaPacket> processBuiltin(const dsp::complex_t* iq, size_t count);

    // Built-in helper methods
    void generateChirpTemplate();
    float correlatePreamble(const float* input, size_t len);
    bool detectPreamble(const dsp::complex_t* iq, size_t count, size_t& startIdx);
    bool decodePHYHeader(const dsp::complex_t* iq, size_t startIdx, LoRaPacket& packet);
    bool decodePayload(const dsp::complex_t* iq, size_t startIdx, LoRaPacket& packet);
    bool deinterleaveAndDecode(const uint8_t* encoded, size_t len, std::vector<uint8_t>& decoded);
    void dewhiten(uint8_t* data, size_t len);
    uint16_t calculateCRC(const uint8_t* data, size_t len);

    // Signal quality estimation
    void estimateSignalQuality(const dsp::complex_t* iq, size_t count, float& snr, int& rssi);
};

// Utility functions for LoRa parameter calculations
namespace LoRaUtils {
    // Calculate symbol duration in seconds
    double symbolDuration(int sf, int bw);

    // Calculate chip rate (chips per second)
    double chipRate(int sf, int bw);

    // Calculate samples per symbol at given sample rate
    int samplesPerSymbol(int sf, int bw, double sampleRate);

    // Calculate preamble length in samples
    int preambleSamples(int sf, int bw, int preambleSymbols, double sampleRate);

    // Get spreading factor name
    const char* sfName(int sf);

    // Get bandwidth name
    const char* bwName(int bw);

    // Validate LoRa parameters
    bool validateConfig(const LoRaModemConfig& config);
}