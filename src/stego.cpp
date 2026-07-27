#include "stego.h"
#include "crypto_utils.h"
#include <algorithm>
#include <map>
#include <random>

void embedMagic(std::vector<unsigned char>& image, unsigned width, unsigned height) {
    const unsigned totalPixels = width * height;
    const size_t imageSize = static_cast<size_t>(totalPixels) * 4;
    const size_t stride = imageSize / MAGIC_POSITIONS;

    for (size_t p = 0; p < MAGIC_POSITIONS; p++) {
        size_t idx = (p * stride) % (imageSize - 4);
        if (idx % 4 == 0) idx += 1;
        for (int b = 0; b < 4; b++) {
            image[idx + b] = MAGIC_BYTES[b];
        }
    }
}

bool checkMagic(const std::vector<unsigned char>& image, unsigned width, unsigned height) {
    const unsigned totalPixels = width * height;
    const size_t imageSize = static_cast<size_t>(totalPixels) * 4;
    const size_t stride = imageSize / MAGIC_POSITIONS;

    size_t matches = 0;
    for (size_t p = 0; p < MAGIC_POSITIONS; p++) {
        size_t idx = (p * stride) % (imageSize - 4);
        if (idx % 4 == 0) idx += 1;
        bool ok = true;
        for (int b = 0; b < 4; b++) {
            if (image[idx + b] != MAGIC_BYTES[b]) { ok = false; break; }
        }
        if (ok) matches++;
    }
    return (matches * 100 / MAGIC_POSITIONS) >= 60;
}

void embedPayload(std::vector<unsigned char>& image, unsigned width, unsigned height,
                  const EncryptedPayload& payload, const std::string& key) {
    const unsigned totalPixels = width * height;
    const size_t imageSize = static_cast<size_t>(totalPixels) * 4;

    const unsigned encLen = static_cast<unsigned>(payload.encryptedData.size());
    const unsigned char encLenBytes[4] = {
        static_cast<unsigned char>(encLen & 0xFF),
        static_cast<unsigned char>((encLen >> 8) & 0xFF),
        static_cast<unsigned char>((encLen >> 16) & 0xFF),
        static_cast<unsigned char>((encLen >> 24) & 0xFF)
    };
    for (int row = 0; row < 8; row++) {
        size_t offset = static_cast<size_t>(row * 2 * width) * 4;
        for (int i = 0; i < 4; i++) {
            image[offset + i] = encLenBytes[i];
        }
    }

    for (int copy = 0; copy < 8; copy++) {
        size_t offset = SALT_OFFSET + static_cast<size_t>(copy) * 64;
        for (size_t i = 0; i < payload.salt.length(); i++) {
            image[offset + i] = static_cast<unsigned char>(payload.salt[i]);
        }
    }

    for (int copy = 0; copy < 8; copy++) {
        size_t offset = IV_OFFSET + static_cast<size_t>(copy) * 64;
        for (int i = 0; i < NONCE_SIZE; i++) {
            image[offset + i] = payload.iv[i];
        }
    }

    for (int copy = 0; copy < 8; copy++) {
        size_t offset = HASH_OFFSET + static_cast<size_t>(copy) * 64;
        const size_t hashLen = std::min(static_cast<size_t>(32), payload.passwordHash.length());
        for (size_t i = 0; i < hashLen; i++) {
            image[offset + i] = static_cast<unsigned char>(payload.passwordHash[i]);
        }
    }

    std::vector<size_t> indices;
    indices.reserve(totalPixels / 2);
    for (size_t idx = DATA_EMBEDDING_START; idx < imageSize - METADATA_BOUNDARY; idx += 4) {
        indices.push_back(idx);
    }

    const unsigned seed = deriveSeedFromKey(key, payload.salt);
    std::mt19937 g(seed);
    std::shuffle(indices.begin(), indices.end(), g);

    const size_t encDataSize = payload.encryptedData.size();
    for (size_t copy = 0; copy < static_cast<size_t>(REDUNDANCY_FACTOR) && copy * encDataSize < indices.size(); copy++) {
        size_t base = (copy * REDUNDANCY_STRIDE) % (indices.size() - encDataSize);
        for (size_t i = 0; i < encDataSize && (base + i) < indices.size(); i++) {
            size_t idx = indices[base + i];
            size_t channelOffset = copy % 3;
            image[idx + channelOffset] = payload.encryptedData[i];
        }
    }

    embedMagic(image, width, height);
}

EncryptedPayload extractPayload(const std::vector<unsigned char>& image, unsigned width,
                                 unsigned height, const std::string& key) {
    const unsigned totalPixels = width * height;
    const size_t imageSize = static_cast<size_t>(totalPixels) * 4;
    EncryptedPayload result;

    unsigned encLenValues[8];
    for (int row = 0; row < 8; row++) {
        size_t offset = static_cast<size_t>(row * 2 * width) * 4;
        encLenValues[row] = 0;
        for (int i = 0; i < 4; i++) {
            encLenValues[row] |= (static_cast<unsigned>(image[offset + i]) << static_cast<unsigned>(i * 8));
        }
    }
    std::map<unsigned, int> encLenFreq;
    for (int row = 0; row < 8; row++) {
        encLenFreq[encLenValues[row]]++;
    }
    unsigned encLen = 0;
    int maxFreq = 0;
    for (const auto& pair : encLenFreq) {
        if (pair.second > maxFreq) {
            maxFreq = pair.second;
            encLen = pair.first;
        }
    }
    if (encLen > 10000) encLen = 1000;

    result.salt.reserve(16);
    for (size_t i = 0; i < 16; i++) {
        std::map<unsigned char, int> saltFreq;
        for (int copy = 0; copy < 8; copy++) {
            size_t offset = SALT_OFFSET + static_cast<size_t>(copy) * 64;
            saltFreq[image[offset + i]]++;
        }
        unsigned char best = 0;
        int bestCount = 0;
        for (const auto& pair : saltFreq) {
            if (pair.second > bestCount) {
                bestCount = pair.second;
                best = pair.first;
            }
        }
        result.salt.push_back(best);
    }

    result.iv.resize(NONCE_SIZE);
    for (int i = 0; i < NONCE_SIZE; i++) {
        std::map<unsigned char, int> ivFreq;
        for (int copy = 0; copy < 8; copy++) {
            size_t offset = IV_OFFSET + static_cast<size_t>(copy) * 64;
            ivFreq[image[offset + i]]++;
        }
        unsigned char best = 0;
        int bestCount = 0;
        for (const auto& pair : ivFreq) {
            if (pair.second > bestCount) {
                bestCount = pair.second;
                best = pair.first;
            }
        }
        result.iv[i] = best;
    }

    std::vector<size_t> indices;
    indices.reserve(totalPixels / 2);
    for (size_t idx = DATA_EMBEDDING_START; idx < imageSize - METADATA_BOUNDARY; idx += 4) {
        indices.push_back(idx);
    }

    const unsigned seed = deriveSeedFromKey(key, result.salt);
    std::mt19937 g(seed);
    std::shuffle(indices.begin(), indices.end(), g);

    std::vector<std::map<unsigned char, int>> byteFrequencies(encLen);
    for (size_t copy = 0; copy < static_cast<size_t>(REDUNDANCY_FACTOR); copy++) {
        size_t base = (copy * REDUNDANCY_STRIDE) % (indices.size() - encLen);
        size_t channelOffset = copy % 3;
        for (size_t i = 0; i < encLen && (base + i) < indices.size(); i++) {
            size_t idx = indices[base + i] + channelOffset;
            byteFrequencies[i][image[idx]]++;
        }
    }

    result.encryptedData.resize(encLen);
    for (size_t i = 0; i < encLen; i++) {
        unsigned char mostCommon = 0;
        int maxCount = 0;
        for (const auto& pair : byteFrequencies[i]) {
            if (pair.second > maxCount) {
                maxCount = pair.second;
                mostCommon = pair.first;
            }
        }
        result.encryptedData[i] = mostCommon;
    }

    result.passwordHash.reserve(32);
    for (size_t i = 0; i < 32; i++) {
        std::map<unsigned char, int> hashFreq;
        for (int copy = 0; copy < 8; copy++) {
            size_t offset = HASH_OFFSET + static_cast<size_t>(copy) * 64;
            hashFreq[image[offset + i]]++;
        }
        unsigned char best = 0;
        int bestCount = 0;
        for (const auto& pair : hashFreq) {
            if (pair.second > bestCount) {
                bestCount = pair.second;
                best = pair.first;
            }
        }
        result.passwordHash.push_back(static_cast<char>(best));
    }

    return result;
}
