#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "crypto_utils.h"

#include <cstring>
#include <sodium.h>

namespace {
struct CryptoInitializer {
  CryptoInitializer() { initCrypto(); }
} cryptoInitializer;
} // namespace

TEST_CASE("Argon2id derives deterministic, input-dependent keys", "[crypto]") {
  const auto salt1 = randomBytes(SALT_SIZE);
  auto salt2 = salt1;
  salt2[0] ^= 1U;
  SecureBuffer key1 = deriveKey("password", salt1, legacyKdfParams());
  SecureBuffer key2 = deriveKey("password", salt1, legacyKdfParams());
  SecureBuffer key3 = deriveKey("password", salt2, legacyKdfParams());
  REQUIRE(key1.size() == KEY_SIZE);
  REQUIRE(std::memcmp(key1.data(), key2.data(), KEY_SIZE) == 0);
  REQUIRE(std::memcmp(key1.data(), key3.data(), KEY_SIZE) != 0);
}

TEST_CASE("Argon2id rejects invalid inputs", "[crypto]") {
  REQUIRE_THROWS_AS(
      deriveKey("password", randomBytes(SALT_SIZE - 1), legacyKdfParams()),
      std::invalid_argument);
  REQUIRE_THROWS_AS(deriveKey("password", randomBytes(SALT_SIZE), {0, 0}),
                    std::invalid_argument);
}

TEST_CASE("XChaCha20-Poly1305 round trips authenticated plaintext",
          "[crypto]") {
  const auto salt = randomBytes(SALT_SIZE);
  SecureBuffer key = deriveKey("password", salt, legacyKdfParams());
  const auto nonce = randomBytes(NONCE_SIZE);
  const std::vector<unsigned char> aad{'h', 'e', 'a', 'd', 'e', 'r'};
  const std::string plaintext("binary\0secret", 13);
  const auto ciphertext = encrypt(plaintext, key, nonce, aad);
  std::string decrypted;
  REQUIRE(ciphertext.size() == plaintext.size() + MAC_SIZE);
  REQUIRE(decrypt(ciphertext, key, nonce, aad, decrypted));
  REQUIRE(decrypted == plaintext);
  secureWipe(decrypted.data(), decrypted.size());
}

TEST_CASE("XChaCha20-Poly1305 rejects tampering and wrong inputs", "[crypto]") {
  const auto salt = randomBytes(SALT_SIZE);
  SecureBuffer key = deriveKey("password", salt, legacyKdfParams());
  SecureBuffer wrongKey = deriveKey("wrong", salt, legacyKdfParams());
  const auto nonce = randomBytes(NONCE_SIZE);
  const std::vector<unsigned char> aad{'a'};
  auto ciphertext = encrypt("secret", key, nonce, aad);
  std::string output;
  REQUIRE_FALSE(decrypt(ciphertext, wrongKey, nonce, aad, output));
  ciphertext[0] ^= 1U;
  REQUIRE_FALSE(decrypt(ciphertext, key, nonce, aad, output));
  ciphertext[0] ^= 1U;
  REQUIRE_FALSE(decrypt(ciphertext, key, nonce, {'b'}, output));

  SecureBuffer shortKey(1);
  REQUIRE_THROWS_AS(encrypt("secret", shortKey, nonce, aad),
                    std::invalid_argument);
  REQUIRE_FALSE(decrypt(ciphertext, shortKey, nonce, aad, output));
}

TEST_CASE("Random helpers return requested sizes and filename-safe characters",
          "[crypto]") {
  REQUIRE(randomBytes(16).size() == 16);
  REQUIRE(randomBytes(0).empty());
  const std::string value = generateRandomString(1000);
  REQUIRE(value.size() == 1000);
  REQUIRE(
      value.find_first_not_of(
          "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") ==
      std::string::npos);
}

TEST_CASE("secureWipe clears memory", "[crypto]") {
  std::vector<unsigned char> bytes{1, 2, 3, 4};
  secureWipe(bytes.data(), bytes.size());
  REQUIRE(bytes == std::vector<unsigned char>{0, 0, 0, 0});
}
