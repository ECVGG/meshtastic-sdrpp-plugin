#include "LoRaDemodulator.h"
#include "MeshtasticProtocol.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LoRaDemodulator::LoRaDemodulator() : initialized(false) {
}

LoRaDemodulator::~LoRaDemodulator() {
}

bool LoRaDemodulator::init(const LoRaModemConfig& config) {
    this->config = config;
    builtinState = std::make_unique<BuiltinState>();
    builtinState->sf = config.spreadingFactor;
    builtinState->bw = config.bandwidth;
    initialized = true;
    return true;
}

void LoRaDemodulator::configure(const LoRaModemConfig& config) {
    this->config = config;
    if (builtinState) {
        builtinState->sf = config.spreadingFactor;
        builtinState->bw = config.bandwidth;
    }
}

std::vector<LoRaPacket> LoRaDemodulator::process(const Complex* iq, size_t count, double sampleRate) {
    if (!initialized || !builtinState) return {};

    return processBuiltin(iq, count, sampleRate);
}

std::vector<LoRaPacket> LoRaDemodulator::processBuiltin(const Complex* iq, size_t count, double sampleRate) {
    std::vector<LoRaPacket> packets;

    if (!builtinState || count == 0) return packets;

    // Simple energy detection for preamble
    // In a real implementation, this would do proper LoRa demodulation
    // For now, we just buffer samples and do basic detection

    builtinState->buffer.insert(builtinState->buffer.end(), iq, iq + count);
    builtinState->sampleCount = builtinState->buffer.size();

    // Need at least a few symbols worth of samples
    int samplesPerSymbol = static_cast<int>(sampleRate * (1 << builtinState->sf) / builtinState->bw);
    int minSamples = samplesPerSymbol * 10; // ~10 symbols for preamble

    if (builtinState->sampleCount < static_cast<size_t>(minSamples)) {
        return packets;
    }

    // Simple energy detection - look for LoRa chirp pattern
    // This is a placeholder - real LoRa demodulation is complex
    // We'll just return empty for now and let the user know

    // Clear buffer to prevent memory growth
    if (builtinState->buffer.size() > static_cast<size_t>(samplesPerSymbol * 100)) {
        builtinState->buffer.erase(builtinState->buffer.begin(),
            builtinState->buffer.begin() + builtinState->buffer.size() / 2);
        builtinState->sampleCount = builtinState->buffer.size();
    }

    return packets;
}

bool LoRaDemodulator::detectPreamble(const Complex* iq, size_t count, size_t& preambleStart) {
    // Placeholder for preamble detection
    // Real implementation would correlate with known chirp pattern
    return false;
}

std::vector<uint8_t> LoRaDemodulator::demodulateSymbols(const Complex* iq, size_t count, int sf, int bw) {
    // Placeholder for symbol demodulation
    return {};
}

std::vector<uint8_t> LoRaDemodulator::deinterleaveAndDecode(const std::vector<uint8_t>& symbols, int sf, int cr) {
    // Placeholder for deinterleaving and FEC decoding
    return {};
}

bool LoRaDemodulator::checkCRC(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    uint16_t expectedCRC = (uint16_t)data[data.size() - 2] | ((uint16_t)data[data.size() - 1] << 8);
    uint16_t calcCRC = MeshtasticProtocol::MeshHeader::calculateCRC(data.data(), data.size() - 2);
    return calcCRC == expectedCRC;
}