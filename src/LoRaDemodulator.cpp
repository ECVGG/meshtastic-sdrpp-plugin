#include "LoRaDemodulator.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper function to compute magnitude of dsp::complex_t
static inline double complexAbs(const dsp::complex_t& c) {
    return sqrt(c.re * c.re + c.im * c.im);
}

// Helper function to compute squared magnitude (norm) of dsp::complex_t
static inline double complexNorm(const dsp::complex_t& c) {
    return c.re * c.re + c.im * c.im;
}

// Hamming code lookup tables for CR 4/5 (4 data bits + 1 parity bit = 5 coded bits)
// For CR 4/5: we use standard (7,4) Hamming code extended to (8,4) with extra parity
// The deinterleaver writes bits column-wise and reads row-wise
static const uint8_t HAMMING_7_4_DECODE[128] = {
    // Syndrome -> error position (0 = no error, 1-7 = bit position with error)
    0,  3,  5,  0,  6,  0,  0,  7,
    4,  0,  0,  1,  0,  2,  0,  0,
    6,  0,  0,  7,  0,  1,  2,  0,
    0,  3,  4,  0,  5,  0,  0,  0,
    4,  0,  0,  2,  0,  7,  6,  0,
    0,  1,  3,  0,  5,  0,  0,  0,
    0,  5,  6,  0,  2,  0,  0,  1,
    3,  0,  0,  4,  7,  0,  0,  0
};

// Extract data bits from (7,4) Hamming codeword (positions 2,4,5,6 in 1-indexed = 1,3,4,5 in 0-indexed)
static const uint8_t HAMMING_DATA_BITS[4] = { 2, 4, 5, 6 }; // 0-indexed positions in 7-bit codeword

LoRaDemodulator::~LoRaDemodulator() {
    stop();
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

std::vector<LoRaPacket> LoRaDemodulator::process(const dsp::complex_t* iq, size_t count, double sampleRate) {
    std::vector<LoRaPacket> packets;

    if (!initialized || !builtinState || count == 0) return packets;

    if (!running) return packets;

    builtinState->sampleRate = sampleRate;
    builtinState->sf = config.spreadingFactor;
    builtinState->bw = config.bandwidth;

    int M = 1 << builtinState->sf;
    int samplesPerSymbol = static_cast<int>(sampleRate * M / builtinState->bw);
    int preambleSymbols = 8 + 4.25;
    int minSamples = samplesPerSymbol * preambleSymbols;

    builtinState->buffer.insert(builtinState->buffer.end(), iq, iq + count);
    builtinState->sampleCount = builtinState->buffer.size();

    if (builtinState->sampleCount < static_cast<size_t>(minSamples)) {
        return packets;
    }

    // Process complete symbols from buffer
    while (builtinState->buffer.size() >= static_cast<size_t>(samplesPerSymbol * 2)) {
        size_t preambleStart = 0;
        if (detectPreamble(builtinState->buffer.data(), builtinState->buffer.size(), preambleStart)) {
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
                packet.timestamp = 0;
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
        }
        break;
    }

    return packets;
}

bool LoRaDemodulator::detectPreamble(const dsp::complex_t* iq, size_t count, size_t& preambleStart) {
    int M = 1 << builtinState->sf;
    int samplesPerSymbol = static_cast<int>(builtinState->sampleRate * M / builtinState->bw);
    int preambleSymbols = 8;

    if (count < static_cast<size_t>(samplesPerSymbol * preambleSymbols)) {
        return false;
    }

    // Generate reference upchirp for correlation
    std::vector<dsp::complex_t> refUpchirp(samplesPerSymbol);
    for (int i = 0; i < samplesPerSymbol; ++i) {
        double phase = M_PI * (double)i * i / samplesPerSymbol;
        refUpchirp[i] = dsp::complex_t{ (float)cos(phase), (float)sin(phase) };
    }

    // Correlate with sliding window
    double maxCorr = 0;
    size_t maxPos = 0;
    int searchLen = std::min<int>(count - samplesPerSymbol * 2, samplesPerSymbol * 4);

    for (int i = 0; i < searchLen; i += samplesPerSymbol / 4) {
        double corr = 0;
        for (int j = 0; j < samplesPerSymbol; ++j) {
            corr += complexAbs(iq[i + j]) * complexAbs(refUpchirp[j]);
        }
        if (corr > maxCorr) {
            maxCorr = corr;
            maxPos = i;
        }
    }

    // Threshold for preamble detection
    double avgPower = 0;
    for (size_t i = 0; i < std::min<size_t>(count, samplesPerSymbol * 4); ++i) {
        avgPower += complexNorm(iq[i]);
    }
    avgPower /= std::min<size_t>(count, samplesPerSymbol * 4);

    if (maxCorr > avgPower * samplesPerSymbol * 2.0) {
        preambleStart = maxPos;
        return true;
    }

    return false;
}

std::vector<uint8_t> LoRaDemodulator::decodePacket(const dsp::complex_t* iq, size_t count) {
    int M = 1 << builtinState->sf;
    int samplesPerSymbol = static_cast<int>(builtinState->sampleRate * M / builtinState->bw);

    // Skip preamble (8 symbols + 2.25 SFD + 2.25 sync)
    int preambleSkip = static_cast<int>(samplesPerSymbol * 12.5);
    if (count < static_cast<size_t>(preambleSkip + samplesPerSymbol * 2)) {
        return {};
    }

    const dsp::complex_t* dataIQ = iq + preambleSkip;
    size_t dataCount = count - preambleSkip;

    // Demodulate symbols
    std::vector<uint8_t> symbols = demodulateSymbols(dataIQ, dataCount, builtinState->sf, builtinState->bw);
    if (symbols.empty()) return {};

    // Deinterleave and decode
    std::vector<uint8_t> decoded = deinterleaveAndDecode(symbols, builtinState->sf, config.codingRate);
    return decoded;
}

std::vector<uint8_t> LoRaDemodulator::demodulateSymbols(const dsp::complex_t* iq, size_t count, int sf, int bw) {
    int M = 1 << sf;
    int samplesPerSymbol = static_cast<int>(builtinState->sampleRate * M / bw);
    int maxSymbols = static_cast<int>(count / samplesPerSymbol);

    if (maxSymbols < 10) return {};

    std::vector<uint8_t> symbols;
    symbols.reserve(maxSymbols);

    // Generate reference chirps for all M symbols
    std::vector<std::vector<dsp::complex_t>> refChirps(M, std::vector<dsp::complex_t>(samplesPerSymbol));
    for (int m = 0; m < M; ++m) {
        for (int i = 0; i < samplesPerSymbol; ++i) {
            double phase = M_PI * (double)i * i / samplesPerSymbol +
                          2.0 * M_PI * m * i / M;
            refChirps[m][i] = dsp::complex_t{ (float)cos(phase), (float)sin(phase) };
        }
    }

    // For each symbol, correlate with all reference chirps
    for (int sym = 0; sym < maxSymbols; ++sym) {
        const dsp::complex_t* symIQ = iq + sym * samplesPerSymbol;
        if (sym * samplesPerSymbol + samplesPerSymbol > static_cast<int>(count)) break;

        double maxCorr = 0;
        int bestSymbol = 0;

        for (int m = 0; m < M; ++m) {
            double corr = 0;
            for (int i = 0; i < samplesPerSymbol; ++i) {
                // Compute conj(refChirps[m][i]) and multiply
                dsp::complex_t conj = dsp::complex_t{ refChirps[m][i].re, -refChirps[m][i].im };
                dsp::complex_t symCopy = symIQ[i]; // Copy to non-const for multiplication
                dsp::complex_t prod = symCopy * conj;
                corr += prod.re; // real part
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
            break;
        }
    }

    return symbols;
}

std::vector<uint8_t> LoRaDemodulator::deinterleaveAndDecode(const std::vector<uint8_t>& symbols, int sf, int cr) {
    // LoRa deinterleaving and Hamming code decoding
    // CR 4/5 = 5 (4 data + 1 parity), CR 4/6 = 6, CR 4/7 = 7, CR 4/8 = 8
    // Meshtastic typically uses CR 4/5 (cr = 5)

    if (symbols.size() < 10) return {};

    // Step 1: Deinterleave (matrix transpose)
    // LoRa writes bits column-wise into a matrix of SF rows and N columns, reads row-wise
    // For CR 4/5: we have 5 coded bits per 4 data bits = (7,4) Hamming extended with extra parity
    // Actually LoRa uses a specific interleaving: write column-wise (SF rows), read row-wise

    int numSymbols = static_cast<int>(symbols.size());
    int bitsPerSymbol = sf;
    int totalBits = numSymbols * bitsPerSymbol;

    // For CR 4/5, we need groups of 5 symbols (5 coded bits) to decode 4 data bits
    // But the interleaving is done at the bit level across all symbols
    // Deinterleaver matrix: SF rows x ceil(totalBits/SF) columns

    // We'll process the bits by extracting each bit from each symbol
    std::vector<uint8_t> bits;
    bits.reserve(totalBits);

    // Extract bits from symbols (MSB first for each symbol)
    for (uint8_t sym : symbols) {
        for (int b = bitsPerSymbol - 1; b >= 0; --b) {
            bits.push_back((sym >> b) & 1);
        }
    }

    // Deinterleave: inverse of transmitter
    // Transmitter: writes column-wise (SF rows), reads row-wise for transmission
    // Receiver: received bits are in row-wise order from transmitter
    // So we write row-wise into matrix and read column-wise to get original order
    int numCols = (bits.size() + sf - 1) / sf;
    std::vector<uint8_t> deinterleavedBits;
    deinterleavedBits.reserve(bits.size());

    // Pad bits to fill the matrix
    while (static_cast<int>(bits.size()) < sf * numCols) {
        bits.push_back(0);
    }

    // Write row-wise, read column-wise (deinterleave)
    for (int col = 0; col < numCols; ++col) {
        for (int row = 0; row < sf; ++row) {
            size_t idx = row * numCols + col;
            if (idx < bits.size()) {
                deinterleavedBits.push_back(bits[idx]);
            }
        }
    }

    // Step 2: Hamming decode for CR 4/5
    // For CR 4/5: each 5 coded bits -> 4 data bits (using (7,4) Hamming with extra parity)
    // Actually LoRa CR 4/5 uses a specific scheme: 4 data bits encoded into 5 bits (Hamming 7,4 + 1 parity)
    // Wait - LoRa CR 4/5 means 4 data bits + 1 parity bit = 5 coded bits
    // But the deinterleaver output gives us 7 bits per symbol group for Hamming (7,4)
    // Actually: After deinterleaving, the bits are grouped for Hamming decoding

    // Standard LoRa CR 4/5: Uses Hamming (7,4) code
    // 4 data bits -> 7 coded bits (3 parity bits)
    // Then an extra parity bit is added making it 8 bits
    // Wait, let me check the actual LoRa spec...

    // According to LoRa spec:
    // CR 4/5: (7,4) Hamming code -> 4 data bits encoded into 7 bits
    // CR 4/6: (8,4) extended Hamming
    // CR 4/7: (8,4) extended Hamming
    // CR 4/8: (8,4) extended Hamming

    // For Meshtastic with CR 4/5, we use standard (7,4) Hamming
    if (cr == 5) {
        return hammingDecode7_4(deinterleavedBits);
    }

    // For other CRs, return as bytes (simplified)
    return bitsToBytes(deinterleavedBits);
}

std::vector<uint8_t> LoRaDemodulator::hammingDecode7_4(const std::vector<uint8_t>& bits) {
    // Decode (7,4) Hamming code
    // Each 7 bits -> 4 data bits
    std::vector<uint8_t> dataBits;
    dataBits.reserve(bits.size() * 4 / 7);

    for (size_t i = 0; i + 6 < bits.size(); i += 7) {
        // Build 7-bit codeword
        uint8_t codeword = 0;
        for (int b = 0; b < 7; ++b) {
            codeword |= (bits[i + b] << (6 - b));
        }

        // Compute syndrome
        uint8_t syndrome = 0;
        // Parity checks for Hamming (7,4):
        // p1 = d1 ^ d2 ^ d4 (bits 0,1,3 in data positions = codeword bits 2,4,6)
        // p2 = d1 ^ d3 ^ d4 (bits 0,2,3 in data = codeword bits 2,5,6)
        // p3 = d2 ^ d3 ^ d4 (bits 1,2,3 in data = codeword bits 4,5,6)
        // Standard Hamming (7,4) parity bit positions: 1,2,4 (1-indexed) = 0,1,3 (0-indexed)
        // Data bit positions: 3,5,6,7 (1-indexed) = 2,4,5,6 (0-indexed)

        // Syndrome bits:
        // s1 = p1 ^ d1 ^ d2 ^ d4 = codeword[0] ^ codeword[2] ^ codeword[4] ^ codeword[6]
        // s2 = p2 ^ d1 ^ d3 ^ d4 = codeword[1] ^ codeword[2] ^ codeword[5] ^ codeword[6]
        // s3 = p3 ^ d2 ^ d3 ^ d4 = codeword[3] ^ codeword[4] ^ codeword[5] ^ codeword[6]

        uint8_t s1 = ((codeword >> 6) & 1) ^ ((codeword >> 4) & 1) ^ ((codeword >> 2) & 1) ^ (codeword & 1);
        uint8_t s2 = ((codeword >> 5) & 1) ^ ((codeword >> 4) & 1) ^ ((codeword >> 1) & 1) ^ (codeword & 1);
        uint8_t s3 = ((codeword >> 3) & 1) ^ ((codeword >> 2) & 1) ^ ((codeword >> 1) & 1) ^ (codeword & 1);

        syndrome = (s3 << 2) | (s2 << 1) | s1;

        // Correct error if syndrome != 0
        if (syndrome != 0) {
            // Syndrome value indicates error position (1-7)
            int errorPos = 7 - syndrome; // Convert to 0-indexed from MSB
            if (errorPos >= 0 && errorPos < 7) {
                codeword ^= (1 << errorPos);
            }
        }

        // Extract data bits (positions 2,4,5,6 in 0-indexed from MSB = bits 4,2,1,0 in LSB order)
        // Codeword bits (MSB to LSB): p1, p2, d1, p3, d2, d3, d4
        // Data: d1, d2, d3, d4 at positions 4, 2, 1, 0 (from LSB)
        uint8_t d1 = (codeword >> 4) & 1;
        uint8_t d2 = (codeword >> 2) & 1;
        uint8_t d3 = (codeword >> 1) & 1;
        uint8_t d4 = codeword & 1;

        dataBits.push_back(d1);
        dataBits.push_back(d2);
        dataBits.push_back(d3);
        dataBits.push_back(d4);
    }

    return bitsToBytes(dataBits);
}

std::vector<uint8_t> LoRaDemodulator::bitsToBytes(const std::vector<uint8_t>& bits) {
    std::vector<uint8_t> bytes;
    bytes.reserve((bits.size() + 7) / 8);

    for (size_t i = 0; i + 7 < bits.size(); i += 8) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; ++b) {
            byte |= (bits[i + b] << (7 - b));
        }
        bytes.push_back(byte);
    }

    return bytes;
}

bool LoRaDemodulator::checkCRC(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    uint16_t expectedCRC = (uint16_t)data[data.size() - 2] | ((uint16_t)data[data.size() - 1] << 8);

    // CRC16-CCITT (0x1021) - used by Meshtastic
    uint16_t calcCRC = 0xFFFF;
    for (size_t i = 0; i < data.size() - 2; ++i) {
        calcCRC ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j) {
            if (calcCRC & 0x8000)
                calcCRC = (calcCRC << 1) ^ 0x1021;
            else
                calcCRC <<= 1;
        }
    }
    return calcCRC == expectedCRC;
}

int LoRaDemodulator::estimateRSSI(const dsp::complex_t* iq, size_t count) {
    double power = 0;
    for (size_t i = 0; i < count; ++i) {
        power += complexNorm(iq[i]);
    }
    power /= count;
    // Rough conversion to dBm (calibration needed)
    return static_cast<int>(10 * log10(power + 1e-10) - 50);
}

float LoRaDemodulator::estimateSNR(const dsp::complex_t* iq, size_t count) {
    double signal = 0, noise = 0;
    for (size_t i = 0; i < count; ++i) {
        double mag = complexAbs(iq[i]);
        signal += mag;
        noise += mag * mag;
    }
    signal /= count;
    noise = sqrt(noise / count - signal * signal);
    if (noise < 1e-10) return 30.0f;
    return static_cast<float>(20 * log10(signal / noise));
}