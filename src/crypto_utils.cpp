#include "crypto_utils.h"

#include <array>
#include <limits>
#include <sodium.h>
#include <stdexcept>
#include <utility>

namespace {
constexpr char FILENAME_ALPHABET[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr std::size_t FILENAME_ALPHABET_SIZE = sizeof(FILENAME_ALPHABET) - 1;
} // namespace

SecureBuffer::SecureBuffer(std::size_t size) : bytes_(size) {
  if (!bytes_.empty())
    locked_ = sodium_mlock(bytes_.data(), bytes_.size()) == 0;
}

SecureBuffer::~SecureBuffer() { clear(); }

SecureBuffer::SecureBuffer(SecureBuffer &&other) noexcept
    : bytes_(std::move(other.bytes_)), locked_(other.locked_) {
  other.locked_ = false;
}

SecureBuffer &SecureBuffer::operator=(SecureBuffer &&other) noexcept {
  if (this != &other) {
    clear();
    bytes_ = std::move(other.bytes_);
    locked_ = other.locked_;
    other.locked_ = false;
  }
  return *this;
}

void SecureBuffer::clear() noexcept {
  if (!bytes_.empty()) {
    sodium_memzero(bytes_.data(), bytes_.size());
    if (locked_)
      sodium_munlock(bytes_.data(), bytes_.size());
  }
  bytes_.clear();
  locked_ = false;
}

unsigned char *SecureBuffer::data() noexcept { return bytes_.data(); }
const unsigned char *SecureBuffer::data() const noexcept {
  return bytes_.data();
}
std::size_t SecureBuffer::size() const noexcept { return bytes_.size(); }
bool SecureBuffer::empty() const noexcept { return bytes_.empty(); }

void initCrypto() {
  if (sodium_init() < 0)
    throw std::runtime_error("libsodium initialization failed");
}

KdfParams defaultKdfParams() {
  return {crypto_pwhash_OPSLIMIT_MODERATE, crypto_pwhash_MEMLIMIT_MODERATE};
}

KdfParams legacyKdfParams() {
  return {crypto_pwhash_OPSLIMIT_INTERACTIVE,
          crypto_pwhash_MEMLIMIT_INTERACTIVE};
}

std::vector<unsigned char> randomBytes(std::size_t length) {
  std::vector<unsigned char> result(length);
  if (!result.empty())
    randombytes_buf(result.data(), result.size());
  return result;
}

std::string generateRandomString(std::size_t length) {
  std::string result;
  result.reserve(length);
  constexpr unsigned int limit = 256 - (256 % FILENAME_ALPHABET_SIZE);
  while (result.size() < length) {
    const unsigned int value = randombytes_uniform(256);
    if (value < limit)
      result.push_back(FILENAME_ALPHABET[value % FILENAME_ALPHABET_SIZE]);
  }
  return result;
}

SecureBuffer deriveKey(const std::string &password,
                       const std::vector<unsigned char> &salt,
                       const KdfParams &params) {
  if (salt.size() != crypto_pwhash_SALTBYTES) {
    throw std::invalid_argument("invalid Argon2id salt length");
  }
  if (params.opsLimit < crypto_pwhash_OPSLIMIT_MIN ||
      params.opsLimit > crypto_pwhash_OPSLIMIT_MAX ||
      params.memLimit < crypto_pwhash_MEMLIMIT_MIN ||
      params.memLimit > crypto_pwhash_MEMLIMIT_MAX ||
      params.memLimit >
          static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw std::invalid_argument("unsupported Argon2id parameters");
  }

  SecureBuffer key(KEY_SIZE);
  if (crypto_pwhash(key.data(), key.size(), password.data(), password.size(),
                    salt.data(), params.opsLimit,
                    static_cast<std::size_t>(params.memLimit),
                    crypto_pwhash_ALG_ARGON2ID13) != 0) {
    throw std::runtime_error("Argon2id key derivation failed");
  }
  return key;
}

std::vector<unsigned char>
encrypt(const std::string &plaintext, const SecureBuffer &key,
        const std::vector<unsigned char> &nonce,
        const std::vector<unsigned char> &associatedData) {
  if (key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES ||
      nonce.size() != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES) {
    throw std::invalid_argument(
        "invalid XChaCha20-Poly1305 key or nonce length");
  }
  if (plaintext.size() > crypto_aead_xchacha20poly1305_ietf_MESSAGEBYTES_MAX) {
    throw std::length_error("plaintext is too large");
  }

  std::vector<unsigned char> ciphertext(plaintext.size() + MAC_SIZE);
  unsigned long long ciphertextLength = 0;
  if (crypto_aead_xchacha20poly1305_ietf_encrypt(
          ciphertext.data(), &ciphertextLength,
          reinterpret_cast<const unsigned char *>(plaintext.data()),
          plaintext.size(),
          associatedData.empty() ? nullptr : associatedData.data(),
          associatedData.size(), nullptr, nonce.data(), key.data()) != 0) {
    throw std::runtime_error("encryption failed");
  }
  ciphertext.resize(static_cast<std::size_t>(ciphertextLength));
  return ciphertext;
}

bool decrypt(const std::vector<unsigned char> &ciphertext,
             const SecureBuffer &key, const std::vector<unsigned char> &nonce,
             const std::vector<unsigned char> &associatedData,
             std::string &plaintext) {
  plaintext.clear();
  if (key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES ||
      nonce.size() != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES ||
      ciphertext.size() < MAC_SIZE)
    return false;

  std::vector<unsigned char> output(ciphertext.size());
  unsigned long long plaintextLength = 0;
  const int status = crypto_aead_xchacha20poly1305_ietf_decrypt(
      output.data(), &plaintextLength, nullptr, ciphertext.data(),
      ciphertext.size(),
      associatedData.empty() ? nullptr : associatedData.data(),
      associatedData.size(), nonce.data(), key.data());
  if (status != 0) {
    sodium_memzero(output.data(), output.size());
    return false;
  }
  plaintext.assign(reinterpret_cast<const char *>(output.data()),
                   static_cast<std::size_t>(plaintextLength));
  sodium_memzero(output.data(), output.size());
  return true;
}

unsigned deriveLegacySeedFromKey(const SecureBuffer &key,
                                 const std::vector<unsigned char> &salt) {
  crypto_hash_sha256_state state;
  std::array<unsigned char, crypto_hash_sha256_BYTES> hash{};
  crypto_hash_sha256_init(&state);
  crypto_hash_sha256_update(&state, key.data(), key.size());
  crypto_hash_sha256_update(&state, salt.data(), salt.size());
  crypto_hash_sha256_final(&state, hash.data());
  return static_cast<unsigned>(hash[0]) |
         (static_cast<unsigned>(hash[1]) << 8U) |
         (static_cast<unsigned>(hash[2]) << 16U) |
         (static_cast<unsigned>(hash[3]) << 24U);
}

void secureWipe(void *data, std::size_t size) noexcept {
  if (data != nullptr && size != 0)
    sodium_memzero(data, size);
}
