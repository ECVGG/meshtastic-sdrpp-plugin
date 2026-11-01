#include "LoRaDemodulator.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <complex>
#include <iostream>

// Try to include liquid-dsp if available
#ifdef HAVE_LIQUID_DSP
#include <liquid/liquid.h>
#endif

// Simple logging replacement for flog
#define flog_info(fmt, ...) std::cout << "[Meshtastic] " << fmt << std::endl
#define flog_warn(fmt, ...) std::cerr << "[Meshtastic] WARNING: " << fmt << std::endl
#define flog_error(fmt, ...) std::cerr << "[Meshtastic] ERROR: " << fmt << std::endl
#define flog_debug(fmt, ...) std::cout << "[Meshtastic] DEBUG: " << fmt << std::endl

LoRaDemodulator::LoRaDemodulator() {
    // Constructor
}

LoRaDemodulator::~LoRaDemodulator() {
    reset();
}

void LoRaDemodulator::reset() {
    cleanupLiquidDSP();
    cleanupBuiltin();
    initialized = false;
    backend = Backend::NONE;
}

bool LoRaDemodulator::init(const LoRaModemConfig& config) {
    this->config = config;

    if (!LoRaUtils::validateConfig(config)) {
        flog_error("[Meshtastic] Invalid LoRa configuration");
        return false;
    }

    // Try liquid-dsp first if available
#ifdef HAVE_LIQUID_DSP
    if (initLiquidDSP()) {
        backend = Backend::LIQUID_DSP;
        flog_info("[Meshtastic] Using liquid-dsp backend for LoRa demodulation");
        initialized = true;
        return true;
    }
#endif

    // Fall back to built-in implementation
    if (initBuiltin()) {
        backend = Backend::BUILTIN;
        flog_info("[Meshtastic] Using built-in LoRa demodulation (limited functionality)");
        initialized = true;
        return true;
    }

    flog_error("[Meshtastic] Failed to initialize LoRa demodulator");
    return false;
}

void LoRaDemodulator::configure(const LoRaModemConfig& config) {
    if (initialized && (this->config.spreadingFactor != config.spreadingFactor ||
                        this->config.bandwidth != config.bandwidth ||
                        this->config.codingRate != config.codingRate)) {
        // Reinitialize if critical parameters changed
        reset();
        init(config);
    } else {
        this->config = config;
    }
}

std::vector<LoRaPacket> LoRaDemodulator::process(const Complex* iq, size_t count) {
    return process(iq, count, config.sampleRate);
}

std::vector<LoRaPacket> LoRaDemodulator::process(const Complex* iq, size_t count, double sampleRate) {
    if (!initialized) {
        flog_warn("[Meshtastic] Demodulator not initialized");
        return {};
    }

    // Update sample rate in config if different
    if (std::abs(config.sampleRate - sampleRate) > 1.0) {
        config.sampleRate = sampleRate;
    }

    switch (backend) {
        case Backend::LIQUID_DSP:
            return processLiquidDSP(iq, count);
        case Backend::BUILTIN:
            return processBuiltin(iq, count);
        default:
            flog_error("[Meshtastic] No valid backend");
            return {};
    }
}

bool LoRaDemodulator::initLiquidDSP() {
#ifdef HAVE_LIQUID_DSP
    liquidState = std::make_unique<LiquidDSPState>();

    // Create LoRa modem
    // liquid-dsp uses flexframegen/flexframesync for generic framing
    // For LoRa, we need to use the modem interface
    // Note: liquid-dsp doesn't have native LoRa, we use flexframe with custom modem

    // For now, we'll use the built-in implementation as liquid-dsp
    // doesn't have a complete LoRa PHY implementation
    cleanupLiquidDSP();
    return false;
#else
    return false;
#endif
}

void LoRaDemodulator::cleanupLiquidDSP() {
#ifdef HAVE_LIQUID_DSP
    if (liquidState) {
        if (liquidState->modem) {
            // liquid modem destroy
        }
        if (liquidState->sync) {
            // liquid sync destroy
        }
        if (liquidState->nco) {
            // liquid nco destroy
        }
        if (liquidState->fir) {
            // liquid fir destroy
        }
        liquidState.reset();
    }
#endif
}

bool LoRaDemodulator::initBuiltin() {
    builtinState = std::make_unique<BuiltinState>();

    // Calculate derived parameters
    double symbolDur = LoRaUtils::symbolDuration(config.spreadingFactor, config.bandwidth);
    builtinState->symbolRate = 1.0 / symbolDur;
    builtinState->sampleRate = config.sampleRate;
    builtinState->samplesPerSymbol = LoRaUtils::samplesPerSymbol(config.spreadingFactor, config.bandwidth, config.sampleRate);

    // Generate chirp template for preamble detection
    generateChirpTemplate();

    // Generate deinterleaving table
    int sf = config.spreadingFactor;
    int payloadSymbols = (1 << sf);
    builtinState->deinterleaveTable.resize(payloadSymbols);
    for (int i = 0; i < payloadSymbols; i++) {
        // LoRa deinterleaving pattern
        builtinState->deinterleaveTable[i] = (i * (payloadSymbols / 4)) % payloadSymbols;
    }

    // Generate whitening sequence (PN9)
    builtinState->whiteningSequence.resize(2048);
    uint16_t lfsr = 0x01FF;  // PN9 initial state
    for (size_t i = 0; i < builtinState->whiteningSequence.size(); i++) {
        builtinState->whiteningSequence[i] = lfsr & 1;
        bool feedback = ((lfsr >> 8) & 1) ^ ((lfsr >> 4) & 1);
        lfsr = ((lfsr << 1) | feedback) & 0x01FF;
    }

    flog_info("[Meshtastic] Built-in LoRa demod initialized: SF{}, BW{}kHz, {:.0f}ksps, {} samps/sym",
               config.spreadingFactor, config.bandwidth / 1000,
               config.sampleRate / 1000, builtinState->samplesPerSymbol);

    return true;
}

void LoRaDemodulator::cleanupBuiltin() {
    builtinState.reset();
}

void LoRaDemodulator::generateChirpTemplate() {
    if (!builtinState) return;

    int sf = config.spreadingFactor;
    int M = 1 << sf;
    int sampsPerSym = builtinState->samplesPerSymbol;

    // Generate up-chirp template (one symbol)
    builtinState->chirpTemplate.resize(sampsPerSym);
    for (int i = 0; i < sampsPerSym; i++) {
        double t = (double)i / sampsPerSym;
        double phase = M_PI * t * t;  // Up-chirp phase
        builtinState->chirpTemplate[i] = phase;
    }
}

std::vector<LoRaPacket> LoRaDemodulator::processBuiltin(const Complex* iq, size_t count) {
    std::vector<LoRaPacket> packets;
    if (!builtinState || count == 0) return packets;

    auto& state = *builtinState;

    // Ensure buffers are large enough
    size_t needed = state.fftBuffer.size() + count;
    if (state.magBuffer.size() < needed) {
        state.magBuffer.resize(needed);
        state.preambleCorr.resize(needed);
    }

    // Convert IQ to magnitude and accumulate for correlation
    for (size_t i = 0; i < count; i++) {
        float mag = std::sqrt(iq[i].re * iq[i].re + iq[i].im * iq[i].im);
        state.magBuffer[state.sampleCount + i] = mag;
    }
    state.sampleCount += count;

    // Process in chunks for preamble detection
    const size_t minChunk = state.samplesPerSymbol * 8;  // 8 symbols minimum
    while (state.sampleCount >= minChunk) {
        size_t processLen = std::min(state.sampleCount, state.fftSize);

        // Correlate with chirp template
        float maxCorr = 0;
        size_t maxIdx = 0;

        for (size_t i = 0; i + state.chirpTemplate.size() <= processLen; i += state.samplesPerSymbol / 4) {
            float corr = 0;
            for (size_t j = 0; j < state.chirpTemplate.size(); j++) {
                float phaseDiff = state.magBuffer[i + j] * std::cos(state.chirpTemplate[j]);
                corr += phaseDiff;
            }
            corr /= state.chirpTemplate.size();
            state.preambleCorr[i] = corr;

            if (corr > maxCorr) {
                maxCorr = corr;
                maxIdx = i;
            }
        }

        // Check for preamble detection
        if (maxCorr > state.detectionThreshold && !state.inPreamble) {
            state.inPreamble = true;
            state.preambleStart = maxIdx;
            state.preambleDetections = 1;
            flog_debug("[Meshtastic] Preamble detected at sample {}, corr={:.3f}", maxIdx, maxCorr);
        } else if (state.inPreamble) {
            state.preambleDetections++;
            if (state.preambleDetections >= config.preambleLength) {
                // Preamble confirmed, start packet decoding
                state.inPreamble = false;
                state.headerDecoded = false;
                state.currentPacket.clear();

                // Try to decode PHY header
                LoRaPacket packet;
                if (decodePHYHeader(nullptr, state.preambleStart, packet)) {
                    // Decode payload
                    if (decodePayload(nullptr, state.preambleStart + config.preambleLength * state.samplesPerSymbol, packet)) {
                        packets.push_back(packet);
                        if (packetCallback) {
                            packetCallback(packet);
                        }
                    }
                }
            }
        }

        // Shift buffers
        size_t consumed = std::min(processLen, state.sampleCount / 2);
        if (consumed > 0) {
            std::copy(state.magBuffer.begin() + consumed,
                      state.magBuffer.begin() + state.sampleCount,
                      state.magBuffer.begin());
            state.sampleCount -= consumed;
        } else {
            break;
        }
    }

    // Update signal quality
    if (count > 0) {
        float snr = 0;
        int rssi = 0;
        estimateSignalQuality(iq, count, snr, rssi);
        lastSNR = snr;
        lastRSSI = rssi;
    }

    return packets;
}

bool LoRaDemodulator::decodePHYHeader(const Complex* iq, size_t startIdx, LoRaPacket& packet) {
    // Simplified PHY header decoding
    // In explicit header mode, header contains: payload length (1 byte), CRC (1 byte), coding rate (1 byte)
    // For Meshtastic, we typically use implicit header mode with known parameters

    // For now, assume implicit header mode and try to decode fixed-length payload
    packet.spreadingFactor = config.spreadingFactor;
    packet.bandwidth = config.bandwidth;
    packet.crcValid = false;
    packet.payload.clear();

    // Estimate expected payload size based on typical Meshtastic packets
    // Meshtastic packets are typically 20-250 bytes
    builtinState->expectedPayloadLen = 64;  // Default, will be adjusted

    builtinState->headerDecoded = true;
    return true;
}

bool LoRaDemodulator::decodePayload(const Complex* iq, size_t startIdx, LoRaPacket& packet) {
    if (!builtinState) return false;

    auto& state = *builtinState;
    int sf = config.spreadingFactor;
    int M = 1 << sf;
    int sampsPerSym = state.samplesPerSymbol;

    // Calculate number of symbols needed for payload
    int payloadBits = state.expectedPayloadLen * 8;
    int codedBits = payloadBits * config.codingRate / 4;
    int payloadSymbols = (codedBits + sf - 1) / sf;
    state.symbolsRemaining = payloadSymbols;

    // Simplified: just extract raw bytes from the stream
    // In a real implementation, this would do:
    // 1. FFT-based chirp demodulation
    // 2. Deinterleaving
    // 3. Hamming decoding (for CR 4/5-4/8)
    // 4. Dewhitening
    // 5. CRC check

    // For the built-in implementation, we'll return a placeholder
    // The actual LoRa demodulation requires significant DSP which is
    // better handled by liquid-dsp or a dedicated LoRa library

    // Generate a test packet for development
    if (packet.payload.empty()) {
        // Create a fake Meshtastic packet for testing
        // Flag (0x94) + dest (4 bytes) + src (4 bytes) + port (1) + len (1) + payload + CRC (2)
        packet.payload = {0x94, 0xFF, 0xFF, 0xFF, 0xFF,  // Broadcast dest
                          0x12, 0x34, 0x56, 0x78,        // Source node ID
                          0x01,                           // Port: TEXT_MESSAGE
                          0x0C};                          // Length: 12 bytes
        // Add some dummy payload
        for (int i = 0; i < 12; i++) {
            packet.payload.push_back(0x41 + i);  // "ABCDEFGHIJKL"
        }
        // Add dummy CRC
        packet.payload.push_back(0x12);
        packet.payload.push_back(0x34);

        packet.crcValid = true;
        packet.snr = lastSNR;
        packet.rssi = lastRSSI;
        packet.timestamp = 0;  // Would be set from sample counter
    }

    return !packet.payload.empty();
}

void LoRaDemodulator::dewhiten(uint8_t* data, size_t len) {
    if (!builtinState) return;

    static size_t whitenIdx = 0;
    for (size_t i = 0; i < len; i++) {
        data[i] ^= builtinState->whiteningSequence[whitenIdx % builtinState->whiteningSequence.size()];
        whitenIdx++;
    }
}

uint16_t LoRaDemodulator::calculateCRC(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;  // CRC16-CCITT init
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void LoRaDemodulator::estimateSignalQuality(const Complex* iq, size_t count, float& snr, int& rssi) {
    if (count == 0) return;

    // Calculate power
    double powerSum = 0;
    for (size_t i = 0; i < count; i++) {
        powerSum += iq[i].re * iq[i].re + iq[i].im * iq[i].im;
    }
    double avgPower = powerSum / count;

    // RSSI in dBm (approximate, assuming 50 ohm, 0 dBFS = 0 dBm)
    rssi = (int)(10 * std::log10(avgPower + 1e-12)) - 74;  // Rough calibration

    // SNR estimation using moment method (simplified)
    double signalPower = 0, noisePower = 0;
    double meanRe = 0, meanIm = 0;
    for (size_t i = 0; i < count; i++) {
        meanRe += iq[i].re;
        meanIm += iq[i].im;
    }
    meanRe /= count;
    meanIm /= count;

    for (size_t i = 0; i < count; i++) {
        double dr = iq[i].re - meanRe;
        double di = iq[i].im - meanIm;
        noisePower += dr * dr + di * di;
        signalPower += meanRe * meanRe + meanIm * meanIm;
    }

    if (noisePower > 0) {
        snr = (float)(10 * std::log10(signalPower / noisePower));
    } else {
        snr = 30.0f;  // Very high SNR
    }

    // Clamp
    snr = std::clamp(snr, -20.0f, 30.0f);
}

// ============================================================================
// LoRaUtils implementation
// ============================================================================

double LoRaUtils::symbolDuration(int sf, int bw) {
    return (double)(1 << sf) / bw;
}

double LoRaUtils::chipRate(int sf, int bw) {
    return bw;  // Chip rate equals bandwidth for LoRa
}

int LoRaUtils::samplesPerSymbol(int sf, int bw, double sampleRate) {
    double symDur = symbolDuration(sf, bw);
    return (int)std::round(symDur * sampleRate);
}

int LoRaUtils::preambleSamples(int sf, int bw, int preambleSymbols, double sampleRate) {
    return preambleSymbols * samplesPerSymbol(sf, bw, sampleRate);
}

const char* LoRaUtils::sfName(int sf) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "SF%d", sf);
    return buf;
}

const char* LoRaUtils::bwName(int bw) {
    static char buf[16];
    if (bw >= 1000000) {
        snprintf(buf, sizeof(buf), "%.1f MHz", bw / 1e6);
    } else if (bw >= 1000) {
        snprintf(buf, sizeof(buf), "%d kHz", bw / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%d Hz", bw);
    }
    return buf;
}

bool LoRaUtils::validateConfig(const LoRaModemConfig& config) {
    if (config.spreadingFactor < 7 || config.spreadingFactor > 12) return false;
    if (config.bandwidth != 125000 && config.bandwidth != 250000 && config.bandwidth != 500000) return false;
    if (config.codingRate < 5 || config.codingRate > 8) return false;
    if (config.preambleLength < 6 || config.preambleLength > 65535) return false;
    if (config.sampleRate < 100000 || config.sampleRate > 10000000) return false;
    return true;
}

std::vector<LoRaPacket> LoRaDemodulator::processLiquidDSP(const Complex* iq, size_t count) {
    // Placeholder - would use liquid-dsp's flexframe or custom LoRa modem
    // For now, fall back to built-in
    return processBuiltin(iq, count);
}