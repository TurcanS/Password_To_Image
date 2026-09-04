#pragma once

#include "crypto_utils.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr std::size_t MAX_SECRET_SIZE = 64 * 1024;
inline constexpr std::uint8_t FORMAT_VERSION = 2;
inline constexpr std::size_t HEADER_SIZE = 66;
inline constexpr std::size_t BIT_REDUNDANCY = 3;

enum class PassPixFormat { None, LegacyV1, V2 };

struct PayloadHeader {
  KdfParams kdf{};
  std::vector<unsigned char> salt;
  std::vector<unsigned char> nonce;
  std::uint32_t ciphertextLength = 0;
};

bool validateRgbaImage(const std::vector<unsigned char> &image, unsigned width,
                       unsigned height, std::string &error);
std::vector<unsigned char> serializeHeader(const PayloadHeader &header);
bool parseHeader(const std::vector<unsigned char> &bytes, PayloadHeader &header,
                 std::string &error);

bool embedPayload(std::vector<unsigned char> &image, unsigned width,
                  unsigned height, const PayloadHeader &header,
                  const std::vector<unsigned char> &ciphertext,
                  std::string &error);
bool extractPayload(const std::vector<unsigned char> &image, unsigned width,
                    unsigned height, PayloadHeader &header,
                    std::vector<unsigned char> &ciphertext, std::string &error);

PassPixFormat detectFormat(const std::vector<unsigned char> &image,
                           unsigned width, unsigned height);
bool checkMagic(const std::vector<unsigned char> &image, unsigned width,
                unsigned height);

bool extractLegacySalt(const std::vector<unsigned char> &image, unsigned width,
                       unsigned height, std::vector<unsigned char> &salt,
                       std::string &error);
bool extractLegacyCiphertext(const std::vector<unsigned char> &image,
                             unsigned width, unsigned height,
                             const SecureBuffer &key,
                             const std::vector<unsigned char> &salt,
                             std::vector<unsigned char> &encryptedBlob,
                             std::string &error);
