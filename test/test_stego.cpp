#include "catch_amalgamated.hpp"
#include "image_utils.h"
#include "lodepng.h"
#include "stego.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <random>

namespace {
std::size_t carrierOffset(std::size_t slot) {
  return (slot / 3U) * 4U + slot % 3U;
}

PayloadHeader makeHeader(std::size_t plaintextSize) {
  PayloadHeader header;
  header.kdf = legacyKdfParams();
  header.salt = randomBytes(SALT_SIZE);
  header.nonce = randomBytes(NONCE_SIZE);
  header.ciphertextLength =
      static_cast<std::uint32_t>(plaintextSize + MAC_SIZE);
  return header;
}

std::filesystem::path temporaryPng() {
  return std::filesystem::temp_directory_path() /
         ("passpix-test-" + generateRandomString(20) + ".png");
}

void createLegacyImage(const std::string &path, const std::string &master,
                       const std::string &plaintext) {
  constexpr unsigned width = 320;
  constexpr unsigned height = 240;
  std::vector<unsigned char> image(
      static_cast<std::size_t>(width) * height * 4U, 127);
  for (std::size_t i = 3; i < image.size(); i += 4)
    image[i] = 255;
  const std::string saltText = generateRandomString(SALT_SIZE);
  const std::vector<unsigned char> salt(saltText.begin(), saltText.end());
  const auto nonce = randomBytes(NONCE_SIZE);
  SecureBuffer key = deriveKey(master, salt, legacyKdfParams());
  const auto ciphertext = encrypt(plaintext, key, nonce, {});
  std::vector<unsigned char> blob(nonce);
  blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());

  const std::uint32_t length = static_cast<std::uint32_t>(blob.size());
  for (std::size_t row = 0; row < 8; ++row) {
    const std::size_t offset = row * 2U * width * 4U;
    for (unsigned i = 0; i < 4; ++i)
      image[offset + i] = (length >> (i * 8U)) & 0xffU;
  }
  for (std::size_t copy = 0; copy < 8; ++copy) {
    std::copy(salt.begin(), salt.end(), image.begin() + 20 + copy * 64U);
  }
  std::vector<std::size_t> indices;
  for (std::size_t i = 1500; i < image.size() - 300; i += 4)
    indices.push_back(i);
  std::mt19937 generator(deriveLegacySeedFromKey(key, salt));
  std::shuffle(indices.begin(), indices.end(), generator);
  for (std::size_t copy = 0; copy < 16; ++copy) {
    const std::size_t base = (copy * 1024U) % (indices.size() - blob.size());
    for (std::size_t i = 0; i < blob.size(); ++i)
      image[indices[base + i] + copy % 3U] = blob[i];
  }
  const std::array<unsigned char, 4> magic{{'P', 'a', 's', 's'}};
  const std::size_t stride = image.size() / 512U;
  for (std::size_t p = 0; p < 512; ++p) {
    std::size_t index = (p * stride) % (image.size() - 4U);
    if (index % 4U == 0)
      ++index;
    std::copy(magic.begin(), magic.end(), image.begin() + index);
  }
  REQUIRE(lodepng::encode(path, image, width, height) == 0);
}
} // namespace

TEST_CASE("V2 header serializes and parses exactly", "[stego]") {
  const PayloadHeader expected = makeHeader(42);
  const auto bytes = serializeHeader(expected);
  PayloadHeader actual;
  std::string error;
  REQUIRE(bytes.size() == HEADER_SIZE);
  REQUIRE(parseHeader(bytes, actual, error));
  REQUIRE(actual.kdf.opsLimit == expected.kdf.opsLimit);
  REQUIRE(actual.kdf.memLimit == expected.kdf.memLimit);
  REQUIRE(actual.salt == expected.salt);
  REQUIRE(actual.nonce == expected.nonce);
  REQUIRE(actual.ciphertextLength == expected.ciphertextLength);
}

TEST_CASE("V2 LSB embedding round trips and tolerates one damaged copy",
          "[stego]") {
  constexpr unsigned width = 200;
  constexpr unsigned height = 100;
  std::vector<unsigned char> image(
      static_cast<std::size_t>(width) * height * 4U, 128);
  const std::string plaintext = "test secret";
  PayloadHeader header = makeHeader(plaintext.size());
  SecureBuffer key = deriveKey("master", header.salt, header.kdf);
  const auto aad = serializeHeader(header);
  const auto ciphertext = encrypt(plaintext, key, header.nonce, aad);
  std::string error;
  REQUIRE(embedPayload(image, width, height, header, ciphertext, error));
  REQUIRE(detectFormat(image, width, height) == PassPixFormat::V2);

  const std::size_t firstPayloadSlot = HEADER_SIZE * 8U * BIT_REDUNDANCY;
  image[carrierOffset(firstPayloadSlot)] ^= 1U;
  PayloadHeader extractedHeader;
  std::vector<unsigned char> extractedCiphertext;
  REQUIRE(extractPayload(image, width, height, extractedHeader,
                         extractedCiphertext, error));
  REQUIRE(extractedCiphertext == ciphertext);
}

TEST_CASE("V2 authentication rejects header and ciphertext changes",
          "[stego]") {
  PayloadHeader header = makeHeader(6);
  SecureBuffer key = deriveKey("master", header.salt, header.kdf);
  const auto aad = serializeHeader(header);
  auto ciphertext = encrypt("secret", key, header.nonce, aad);
  std::string output;
  ciphertext[0] ^= 1U;
  REQUIRE_FALSE(decrypt(ciphertext, key, header.nonce, aad, output));
  ciphertext[0] ^= 1U;
  auto changedAad = aad;
  changedAad[10] ^= 1U;
  REQUIRE_FALSE(decrypt(ciphertext, key, header.nonce, changedAad, output));
}

TEST_CASE("Stego parser rejects malformed sizes and excessive lengths",
          "[stego]") {
  std::string error;
  REQUIRE_FALSE(
      validateRgbaImage(std::vector<unsigned char>(16), 100, 1, error));
  REQUIRE_FALSE(checkMagic(std::vector<unsigned char>(4), 1, 1));

  PayloadHeader header = makeHeader(1);
  auto bytes = serializeHeader(header);
  bytes[62] = 0xff;
  bytes[63] = 0xff;
  bytes[64] = 0xff;
  bytes[65] = 0x7f;
  REQUIRE_FALSE(parseHeader(bytes, header, error));
}

TEST_CASE("Stego entry points safely reject randomized malformed buffers",
          "[stego][fuzz]") {
  std::mt19937 generator(0x50505832U);
  std::uniform_int_distribution<std::size_t> sizeDistribution(0, 4096);
  std::uniform_int_distribution<unsigned> dimensionDistribution(0, 1000);
  for (unsigned iteration = 0; iteration < 500; ++iteration) {
    std::vector<unsigned char> bytes(sizeDistribution(generator));
    for (unsigned char &byte : bytes)
      byte = static_cast<unsigned char>(generator());
    const unsigned width = dimensionDistribution(generator);
    const unsigned height = dimensionDistribution(generator);
    REQUIRE_NOTHROW(checkMagic(bytes, width, height));
    PayloadHeader header;
    std::vector<unsigned char> ciphertext;
    std::string error;
    REQUIRE_NOTHROW(
        extractPayload(bytes, width, height, header, ciphertext, error));
  }
}

TEST_CASE("Full V2 encrypt-decrypt round trip uses opaque alpha",
          "[integration]") {
  const auto path = temporaryPng();
  std::string filename;
  std::string error;
  std::string decrypted;
  REQUIRE(encryptPassword("integration master", "my secret", filename, error,
                          path.string()));
  REQUIRE(filename == path.string());
  std::string duplicateFilename;
  REQUIRE_FALSE(encryptPassword("integration master", "replacement",
                                duplicateFilename, error, path.string()));
  REQUIRE(duplicateFilename.empty());
#ifndef _WIN32
  const auto permissions = std::filesystem::status(path).permissions();
  REQUIRE((permissions & (std::filesystem::perms::group_all |
                          std::filesystem::perms::others_all)) ==
          std::filesystem::perms::none);
#endif
  REQUIRE_FALSE(decryptPassword("wrong master", filename, decrypted, error));
  REQUIRE(decrypted.empty());
  REQUIRE(decryptPassword("integration master", filename, decrypted, error));
  REQUIRE(decrypted == "my secret");

  std::vector<unsigned char> image;
  unsigned width = 0;
  unsigned height = 0;
  REQUIRE(lodepng::decode(image, width, height, filename) == 0);
  REQUIRE(width == 1920);
  REQUIRE(height == 1080);
  bool alphaIsOpaque = true;
  for (std::size_t i = 3; i < image.size(); i += 4)
    alphaIsOpaque &= image[i] == 255;
  REQUIRE(alphaIsOpaque);
  std::filesystem::remove(path);
}

TEST_CASE("Legacy V1 images remain decryptable", "[integration][legacy]") {
  const auto path = temporaryPng();
  createLegacyImage(path.string(), "legacy master", "legacy secret");
  std::string password;
  std::string error;
  REQUIRE(decryptPassword("legacy master", path.string(), password, error));
  REQUIRE(password == "legacy secret");
  std::filesystem::remove(path);
}

TEST_CASE("Encryption rejects invalid and oversized inputs before writing",
          "[integration]") {
  std::string filename;
  std::string error;
  REQUIRE_FALSE(encryptPassword("", "secret", filename, error));
  REQUIRE_FALSE(encryptPassword("master", "", filename, error));
  REQUIRE_FALSE(encryptPassword("master", std::string(MAX_SECRET_SIZE + 1, 'x'),
                                filename, error));
  REQUIRE(filename.empty());
}
