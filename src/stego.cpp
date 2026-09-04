#include "stego.h"

#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
constexpr std::array<unsigned char, 4> V2_MAGIC{{'P', 'P', 'X', '2'}};
constexpr std::array<unsigned char, 4> LEGACY_MAGIC{{0x50, 0x61, 0x73, 0x73}};
constexpr std::uint8_t KDF_ARGON2ID13 = 1;

constexpr std::size_t LEGACY_METADATA_BOUNDARY = 300;
constexpr std::size_t LEGACY_DATA_START = 1500;
constexpr std::size_t LEGACY_SALT_OFFSET = 20;
constexpr std::size_t LEGACY_MAGIC_POSITIONS = 512;
constexpr std::size_t LEGACY_REDUNDANCY = 16;
constexpr std::size_t LEGACY_REDUNDANCY_STRIDE = 1024;
constexpr std::size_t LEGACY_MAX_CIPHERTEXT = 10000;

void appendU32(std::vector<unsigned char> &out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<unsigned char>((value >> shift) & 0xffU));
  }
}

void appendU64(std::vector<unsigned char> &out, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) {
    out.push_back(static_cast<unsigned char>((value >> shift) & 0xffU));
  }
}

std::uint32_t readU32(const unsigned char *data) {
  std::uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i)
    value |= static_cast<std::uint32_t>(data[i]) << (i * 8U);
  return value;
}

std::uint64_t readU64(const unsigned char *data) {
  std::uint64_t value = 0;
  for (unsigned i = 0; i < 8; ++i)
    value |= static_cast<std::uint64_t>(data[i]) << (i * 8U);
  return value;
}

std::size_t carrierOffset(std::size_t slot) {
  return (slot / 3U) * 4U + (slot % 3U);
}

bool requiredSlots(std::size_t byteCount, std::size_t &slots) {
  if (byteCount >
      std::numeric_limits<std::size_t>::max() / (8U * BIT_REDUNDANCY))
    return false;
  slots = byteCount * 8U * BIT_REDUNDANCY;
  return true;
}

bool writeRedundantBytes(std::vector<unsigned char> &image,
                         std::size_t firstSlot,
                         const std::vector<unsigned char> &bytes) {
  std::size_t slots = 0;
  if (!requiredSlots(bytes.size(), slots))
    return false;
  const std::size_t carrierSlots = (image.size() / 4U) * 3U;
  if (firstSlot > carrierSlots || slots > carrierSlots - firstSlot)
    return false;

  for (std::size_t i = 0; i < bytes.size(); ++i) {
    for (unsigned bit = 0; bit < 8; ++bit) {
      const unsigned char value =
          static_cast<unsigned char>((bytes[i] >> bit) & 1U);
      const std::size_t bitSlot = firstSlot + (i * 8U + bit) * BIT_REDUNDANCY;
      for (std::size_t copy = 0; copy < BIT_REDUNDANCY; ++copy) {
        unsigned char &channel = image[carrierOffset(bitSlot + copy)];
        channel = static_cast<unsigned char>((channel & 0xfeU) | value);
      }
    }
  }
  return true;
}

bool readRedundantBytes(const std::vector<unsigned char> &image,
                        std::size_t firstSlot, std::size_t byteCount,
                        std::vector<unsigned char> &bytes) {
  std::size_t slots = 0;
  if (!requiredSlots(byteCount, slots))
    return false;
  const std::size_t carrierSlots = (image.size() / 4U) * 3U;
  if (firstSlot > carrierSlots || slots > carrierSlots - firstSlot)
    return false;

  bytes.assign(byteCount, 0);
  for (std::size_t i = 0; i < byteCount; ++i) {
    for (unsigned bit = 0; bit < 8; ++bit) {
      const std::size_t bitSlot = firstSlot + (i * 8U + bit) * BIT_REDUNDANCY;
      unsigned votes = 0;
      for (std::size_t copy = 0; copy < BIT_REDUNDANCY; ++copy) {
        votes += image[carrierOffset(bitSlot + copy)] & 1U;
      }
      if (votes >= 2)
        bytes[i] |= static_cast<unsigned char>(1U << bit);
    }
  }
  return true;
}

bool legacyMagic(const std::vector<unsigned char> &image) {
  if (image.size() < 8)
    return false;
  const std::size_t stride = image.size() / LEGACY_MAGIC_POSITIONS;
  if (stride == 0 || image.size() <= LEGACY_MAGIC.size())
    return false;

  std::size_t matches = 0;
  for (std::size_t p = 0; p < LEGACY_MAGIC_POSITIONS; ++p) {
    std::size_t index = (p * stride) % (image.size() - LEGACY_MAGIC.size());
    if (index % 4U == 0)
      ++index;
    if (index > image.size() - LEGACY_MAGIC.size())
      continue;
    if (std::equal(LEGACY_MAGIC.begin(), LEGACY_MAGIC.end(),
                   image.begin() + index))
      ++matches;
  }
  return matches * 100U / LEGACY_MAGIC_POSITIONS >= 60U;
}

unsigned majorityByte(const std::array<unsigned char, 8> &values) {
  std::array<unsigned, 256> counts{};
  unsigned bestValue = 0;
  unsigned bestCount = 0;
  for (unsigned char value : values) {
    const unsigned count = ++counts[value];
    if (count > bestCount) {
      bestCount = count;
      bestValue = value;
    }
  }
  return bestValue;
}
} // namespace

bool validateRgbaImage(const std::vector<unsigned char> &image, unsigned width,
                       unsigned height, std::string &error) {
  if (width == 0 || height == 0 ||
      width > std::numeric_limits<std::size_t>::max() / height ||
      static_cast<std::size_t>(width) * height >
          std::numeric_limits<std::size_t>::max() / 4U) {
    error = "invalid or overflowing image dimensions";
    return false;
  }
  const std::size_t expected = static_cast<std::size_t>(width) * height * 4U;
  if (image.size() != expected) {
    error = "decoded image size does not match its dimensions";
    return false;
  }
  return true;
}

std::vector<unsigned char> serializeHeader(const PayloadHeader &header) {
  if (header.salt.size() != SALT_SIZE || header.nonce.size() != NONCE_SIZE) {
    throw std::invalid_argument("invalid payload header field length");
  }
  std::vector<unsigned char> bytes;
  bytes.reserve(HEADER_SIZE);
  bytes.insert(bytes.end(), V2_MAGIC.begin(), V2_MAGIC.end());
  bytes.push_back(FORMAT_VERSION);
  bytes.push_back(KDF_ARGON2ID13);
  appendU64(bytes, header.kdf.opsLimit);
  appendU64(bytes, header.kdf.memLimit);
  bytes.insert(bytes.end(), header.salt.begin(), header.salt.end());
  bytes.insert(bytes.end(), header.nonce.begin(), header.nonce.end());
  appendU32(bytes, header.ciphertextLength);
  return bytes;
}

bool parseHeader(const std::vector<unsigned char> &bytes, PayloadHeader &header,
                 std::string &error) {
  if (bytes.size() != HEADER_SIZE) {
    error = "invalid PassPix header length";
    return false;
  }
  if (!std::equal(V2_MAGIC.begin(), V2_MAGIC.end(), bytes.begin())) {
    error = "not a PassPix V2 image";
    return false;
  }
  if (bytes[4] != FORMAT_VERSION) {
    error = "unsupported PassPix format version";
    return false;
  }
  if (bytes[5] != KDF_ARGON2ID13) {
    error = "unsupported key derivation algorithm";
    return false;
  }
  header.kdf = {readU64(bytes.data() + 6), readU64(bytes.data() + 14)};
  header.salt.assign(bytes.begin() + 22, bytes.begin() + 38);
  header.nonce.assign(bytes.begin() + 38, bytes.begin() + 62);
  header.ciphertextLength = readU32(bytes.data() + 62);
  if (header.ciphertextLength < MAC_SIZE ||
      header.ciphertextLength > MAX_SECRET_SIZE + MAC_SIZE) {
    error = "invalid encrypted payload length";
    return false;
  }
  return true;
}

bool embedPayload(std::vector<unsigned char> &image, unsigned width,
                  unsigned height, const PayloadHeader &header,
                  const std::vector<unsigned char> &ciphertext,
                  std::string &error) {
  if (!validateRgbaImage(image, width, height, error))
    return false;
  if (ciphertext.size() != header.ciphertextLength ||
      ciphertext.size() < MAC_SIZE ||
      ciphertext.size() > MAX_SECRET_SIZE + MAC_SIZE) {
    error = "encrypted payload length is invalid";
    return false;
  }
  const std::vector<unsigned char> headerBytes = serializeHeader(header);
  std::size_t headerSlots = 0;
  requiredSlots(headerBytes.size(), headerSlots);
  if (!writeRedundantBytes(image, 0, headerBytes) ||
      !writeRedundantBytes(image, headerSlots, ciphertext)) {
    error = "image does not have enough capacity for this secret";
    return false;
  }
  return true;
}

bool extractPayload(const std::vector<unsigned char> &image, unsigned width,
                    unsigned height, PayloadHeader &header,
                    std::vector<unsigned char> &ciphertext,
                    std::string &error) {
  if (!validateRgbaImage(image, width, height, error))
    return false;
  std::vector<unsigned char> headerBytes;
  if (!readRedundantBytes(image, 0, HEADER_SIZE, headerBytes) ||
      !parseHeader(headerBytes, header, error))
    return false;

  std::size_t headerSlots = 0;
  requiredSlots(HEADER_SIZE, headerSlots);
  if (!readRedundantBytes(image, headerSlots, header.ciphertextLength,
                          ciphertext)) {
    error = "encrypted payload exceeds image capacity";
    return false;
  }
  return true;
}

PassPixFormat detectFormat(const std::vector<unsigned char> &image,
                           unsigned width, unsigned height) {
  std::string ignored;
  if (!validateRgbaImage(image, width, height, ignored))
    return PassPixFormat::None;
  std::vector<unsigned char> magic;
  if (readRedundantBytes(image, 0, V2_MAGIC.size(), magic) &&
      std::equal(V2_MAGIC.begin(), V2_MAGIC.end(), magic.begin()))
    return PassPixFormat::V2;
  return legacyMagic(image) ? PassPixFormat::LegacyV1 : PassPixFormat::None;
}

bool checkMagic(const std::vector<unsigned char> &image, unsigned width,
                unsigned height) {
  return detectFormat(image, width, height) != PassPixFormat::None;
}

bool extractLegacySalt(const std::vector<unsigned char> &image, unsigned width,
                       unsigned height, std::vector<unsigned char> &salt,
                       std::string &error) {
  if (!validateRgbaImage(image, width, height, error) || !legacyMagic(image)) {
    if (error.empty())
      error = "not a legacy PassPix image";
    return false;
  }
  const std::size_t finalOffset = LEGACY_SALT_OFFSET + 7U * 64U + SALT_SIZE;
  if (finalOffset > image.size()) {
    error = "legacy image is too small for its metadata";
    return false;
  }
  salt.assign(SALT_SIZE, 0);
  for (std::size_t i = 0; i < SALT_SIZE; ++i) {
    std::array<unsigned char, 8> values{};
    for (std::size_t copy = 0; copy < values.size(); ++copy) {
      values[copy] = image[LEGACY_SALT_OFFSET + copy * 64U + i];
    }
    salt[i] = static_cast<unsigned char>(majorityByte(values));
  }
  return true;
}

bool extractLegacyCiphertext(const std::vector<unsigned char> &image,
                             unsigned width, unsigned height,
                             const SecureBuffer &key,
                             const std::vector<unsigned char> &salt,
                             std::vector<unsigned char> &encryptedBlob,
                             std::string &error) {
  if (!validateRgbaImage(image, width, height, error) ||
      salt.size() != SALT_SIZE) {
    if (error.empty())
      error = "invalid legacy payload inputs";
    return false;
  }
  std::array<std::uint32_t, 8> lengths{};
  for (std::size_t row = 0; row < lengths.size(); ++row) {
    const std::size_t rowStride = static_cast<std::size_t>(width) * 8U;
    if (rowStride / 8U != width ||
        (row != 0 &&
         rowStride > std::numeric_limits<std::size_t>::max() / row)) {
      error = "legacy metadata offset overflow";
      return false;
    }
    const std::size_t offset = row * rowStride;
    if (offset > image.size() || image.size() - offset < 4U) {
      error = "legacy image is truncated";
      return false;
    }
    lengths[row] = readU32(image.data() + offset);
  }
  std::uint32_t encryptedLength = 0;
  unsigned bestCount = 0;
  for (std::uint32_t candidate : lengths) {
    unsigned count = 0;
    for (std::uint32_t value : lengths)
      count += value == candidate;
    if (count > bestCount) {
      bestCount = count;
      encryptedLength = candidate;
    }
  }
  if (encryptedLength < NONCE_SIZE + MAC_SIZE ||
      encryptedLength > LEGACY_MAX_CIPHERTEXT) {
    error = "invalid legacy encrypted payload length";
    return false;
  }
  if (image.size() <= LEGACY_DATA_START + LEGACY_METADATA_BOUNDARY) {
    error = "legacy image has no payload capacity";
    return false;
  }

  std::vector<std::size_t> indices;
  for (std::size_t index = LEGACY_DATA_START;
       index < image.size() - LEGACY_METADATA_BOUNDARY; index += 4U)
    indices.push_back(index);
  if (indices.size() <= encryptedLength) {
    error = "legacy encrypted payload exceeds image capacity";
    return false;
  }
  std::mt19937 generator(deriveLegacySeedFromKey(key, salt));
  std::shuffle(indices.begin(), indices.end(), generator);

  encryptedBlob.assign(encryptedLength, 0);
  for (std::size_t i = 0; i < encryptedLength; ++i) {
    std::array<unsigned, 256> counts{};
    unsigned mostCommon = 0;
    unsigned mostCount = 0;
    for (std::size_t copy = 0; copy < LEGACY_REDUNDANCY; ++copy) {
      const std::size_t base = (copy * LEGACY_REDUNDANCY_STRIDE) %
                               (indices.size() - encryptedLength);
      const unsigned value = image[indices[base + i] + copy % 3U];
      const unsigned count = ++counts[value];
      if (count > mostCount) {
        mostCount = count;
        mostCommon = value;
      }
    }
    encryptedBlob[i] = static_cast<unsigned char>(mostCommon);
  }
  return true;
}
