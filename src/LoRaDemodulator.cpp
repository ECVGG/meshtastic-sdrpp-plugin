#include "LoRaDemodulator.h"
#include "MeshtasticProtocol.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
#include <numeric>
#include <array>

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
    builtinState->sampleRate = config.sampleRate;
    builtinState->centerFreq = config.centerFrequency;
    initialized = true;
    return true;
}

void LoRaDemodulator::configure(const LoRaModemConfig& config) {
    this->config = config;
    if (builtinState) {
        builtinState->sf = config.spreadingFactor;
        builtinState->bw = config.bandwidth;
        builtinState->sampleRate = config.sampleRate;
        builtinState->centerFreq = config.centerFrequency;
    }
}

std::vector<LoRaPacket> LoRaDemodulator::process(const Complex* iq, size_t count, double sampleRate) {
    if (!initialized || !builtinState) return {};

    return processBuiltin(iq, count, sampleRate);
}

std::vector<LoRaPacket> LoRaDemodulator::processBuiltin(const Complex* iq, size_t count, double sampleRate) {
    std::vector<LoRaPacket> packets;

    if (!builtinState || count == 0) return packets;

    builtinState->sampleRate = sampleRate;
    builtinState->sf = config.spreadingFactor;
    builtinState->bw = config.bandwidth;

    builtinState->buffer.insert(builtinState->buffer.end(), iq, iq + count);
    builtinState->sampleCount = builtinState->buffer.size();

    int M = 1 << builtinState->sf;
    int samplesPerSymbol = static_cast<int>(sampleRate * M / builtinState->bw);
    int preambleSymbols = 8 + 4.25;
    int minSamples = samplesPerSymbol * preambleSymbols;

    if (builtinState->sampleCount < static_cast<size_t>(minSamples)) {
        return packets;
    }

    // Process complete symbols from buffer
    while (builtinState->buffer.size() >= static_cast<size_t>(samplesPerSymbol * 2)) {
        // Try to detect preamble and decode packet
        size_t preambleStart = 0;
        if (detectPreamble(builtinState->buffer.data(), builtinState->buffer.size(), preambleStart)) {
            // Found preamble, try to decode packet
            std::vector<uint8_t> payload = decodePacket(builtinState->buffer.data() + preambleStart,
                                                        builtinState->buffer.size() - preambleStart);
            if (!payload.empty()) {
                LoRaPacket packet;
                packet.payload = payload;
                packet.spreadingFactor = builtinState->sf;
                packet.bandwidth = builtinState->bw;
                packet.crcValid = checkCRC(payload);
                packet.rssi = estimateRSSI(builtinState->buffer.data() + preambleStart, samplesPerSymbol * 10);
                packet.snr = estimateSNR(builtinState->buffer.data() + preambleStart, samplesPerSymbol * 10);
                packet.timestamp = 0; // Would need system time
                packets.push_back(packet);

                // Remove processed samples from buffer
                size_t consumed = preambleStart + payload.size() * samplesPerSymbol * 2;
                if (consumed < builtinState->buffer.size()) {
                    builtinState->buffer.erase(builtinState->buffer.begin(),
                        builtinState->buffer.begin() + consumed);
                } else {
                    builtinState->buffer.clear();
                }
                builtinState->sampleCount = builtinState->buffer.size();
                continue;
            }
        }

        // No packet found, remove some samples to prevent buffer overflow
        if (builtinState->buffer.size() > static_cast<size_t>(samplesPerSymbol * 50)) {
            builtinState->buffer.erase(builtinState->buffer.begin(),
                builtinState->buffer.begin() + builtinState->buffer.size() / 2);
            builtinState->sampleCount = builtinState->buffer.size();
        }
        break;
    }

    return packets;
}

bool LoRaDemodulator::detectPreamble(const Complex* iq, size_t count, size_t& preambleStart) {
    int M = 1 << builtinState->sf;
    int samplesPerSymbol = static_cast<int>(builtinState->sampleRate * M / builtinState->bw);
    int preambleSymbols = 8;

    if (count < static_cast<size_t>(samplesPerSymbol * preambleSymbols)) {
        return false;
    }

    // Generate reference upchirp for correlation
    std::vector<Complex> refUpchirp(samplesPerSymbol);
    for (int i = 0; i < samplesPerSymbol; ++i) {
        double phase = M_PI * (double)i * i / samplesPerSymbol;
        refUpchirp[i] = Complex(cos(phase), sin(phase));
    }

    // Correlate with sliding window
    double maxCorr = 0;
    size_t maxPos = 0;
    int searchLen = std::min<int>(count - samplesPerSymbol * 2, samplesPerSymbol * 4);

    for (int i = 0; i < searchLen; i += samplesPerSymbol / 4) {
        double corr = 0;
        for (int j = 0; j < samplesPerSymbol; ++j) {
            corr += std::abs(iq[i + j]) * std::abs(refUpchirp[j]);
        }
        if (corr > maxCorr) {
            maxCorr = corr;
            maxPos = i;
        }
    }

    // Threshold for preamble detection
    double avgPower = 0;
    for (size_t i = 0; i < std::min<size_t>(count, samplesPerSymbol * 4); ++i) {
        avgPower += std::norm(iq[i]);
    }
    avgPower /= std::min<size_t>(count, samplesPerSymbol * 4);

    if (maxCorr > avgPower * samplesPerSymbol * 2.0) {
        preambleStart = maxPos;
        return true;
    }

    return false;
}

std::vector<uint8_t> LoRaDemodulator::decodePacket(const Complex* iq, size_t count) {
    int M = 1 << builtinState->sf;
    int samplesPerSymbol = static_cast<int>(builtinState->sampleRate * M / builtinState->bw);

    // Skip preamble (8 symbols + 2.25 SFD + 2.25 sync)
    int preambleSkip = static_cast<int>(samplesPerSymbol * 12.5);
    if (count < static_cast<size_t>(preambleSkip + samplesPerSymbol * 2)) {
        return {};
    }

    const Complex* dataIQ = iq + preambleSkip;
    size_t dataCount = count - preambleSkip;

    // Demodulate symbols
    std::vector<uint8_t> symbols = demodulateSymbols(dataIQ, dataCount, builtinState->sf, builtinState->bw);
    if (symbols.empty()) return {};

    // Deinterleave and decode
    std::vector<uint8_t> decoded = deinterleaveAndDecode(symbols, builtinState->sf, config.codingRate);
    return decoded;
}

std::vector<uint8_t> LoRaDemodulator::demodulateSymbols(const Complex* iq, size_t count, int sf, int bw) {
    int M = 1 << sf;
    int samplesPerSymbol = static_cast<int>(builtinState->sampleRate * M / bw);
    int maxSymbols = static_cast<int>(count / samplesPerSymbol);

    if (maxSymbols < 10) return {};

    std::vector<uint8_t> symbols;
    symbols.reserve(maxSymbols);

    // Generate reference chirps for all M symbols
    std::vector<std::vector<Complex>> refChirps(M, std::vector<Complex>(samplesPerSymbol));
    for (int m = 0; m < M; ++m) {
        for (int i = 0; i < samplesPerSymbol; ++i) {
            double phase = M_PI * (double)i * i / samplesPerSymbol +
                          2.0 * M_PI * m * i / M;
            refChirps[m][i] = Complex(cos(phase), sin(phase));
        }
    }

    // For each symbol, correlate with all reference chirps
    for (int sym = 0; sym < maxSymbols; ++sym) {
        const Complex* symIQ = iq + sym * samplesPerSymbol;
        if (sym * samplesPerSymbol + samplesPerSymbol > static_cast<int>(count)) break;

        double maxCorr = 0;
        int bestSymbol = 0;

        for (int m = 0; m < M; ++m) {
            double corr = 0;
            for (int i = 0; i < samplesPerSymbol; ++i) {
                corr += std::real(symIQ[i] * std::conj(refChirps[m][i]));
            }
            if (corr > maxCorr) {
                maxCorr = corr;
                bestSymbol = m;
            }
        }

        // Simple threshold
        if (maxCorr > samplesPerSymbol * 0.1) {
            symbols.push_back(static_cast<uint8_t>(bestSymbol));
        } else {
            // Weak signal, stop
            break;
        }
    }

    return symbols;
}

std::vector<uint8_t> LoRaDemodulator::deinterleaveAndDecode(const std::vector<uint8_t>& symbols, int sf, int cr) {
    // LoRa deinterleaving and Hamming code decoding
    // This is a simplified implementation for CR 4/5 (cr=5)

    if (symbols.size() < 10) return {};

    // For now, return raw symbols (no deinterleaving/FEC)
    // Real implementation would:
    // 1. Deinterleave (matrix transpose)
    // 2. Hamming decode (CR 4/5, 4/6, 4/7, 4/8)
    // 3. Extract payload bytes

    // Placeholder: convert symbols to bytes (each symbol carries sf bits)
    // For SF7, each symbol = 7 bits, need to pack into bytes
    // This is just a placeholder - real implementation needs proper deinterleaving

    return symbols; // Return as-is for now, CRC check will fail if wrong
}

bool LoRaDemodulator::checkCRC(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    uint16_t expectedCRC = (uint16_t)data[data.size() - 2] | ((uint16_t)data[data.size() - 1] << 8);
    uint16_t calcCRC = MeshtasticProtocol::MeshHeader::calculateCRC(data.data(), data.size() - 2);
    return calcCRC == expectedCRC;
}

int LoRaDemodulator::estimateRSSI(const Complex* iq, size_t count) {
    double power = 0;
    for (size_t i = 0; i < count; ++i) {
        power += std::norm(iq[i]);
    }
    power /= count;
    // Rough conversion to dBm (calibration needed)
    return static_cast<int>(10 * log10(power + 1e-10) - 50);
}

float LoRaDemodulator::estimateSNR(const Complex* iq, size_t count) {
    double signal = 0, noise = 0;
    for (size_t i = 0; i < count; ++i) {
        double mag = std::abs(iq[i]);
        signal += mag;
        noise += mag * mag;
    }
    signal /= count;
    noise = sqrt(noise / count - signal * signal);
    if (noise < 1e-10) return 30.0f;
    return static_cast<float>(20 * log10(signal / noise));
}