#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr std::size_t KEY_SIZE = 32;
inline constexpr std::size_t NONCE_SIZE = 24;
inline constexpr std::size_t MAC_SIZE = 16;
inline constexpr std::size_t SALT_SIZE = 16;

struct KdfParams {
  std::uint64_t opsLimit;
  std::uint64_t memLimit;
};

class SecureBuffer {
public:
  explicit SecureBuffer(std::size_t size = 0);
  ~SecureBuffer();
  SecureBuffer(const SecureBuffer &) = delete;
  SecureBuffer &operator=(const SecureBuffer &) = delete;
  SecureBuffer(SecureBuffer &&other) noexcept;
  SecureBuffer &operator=(SecureBuffer &&other) noexcept;

  unsigned char *data() noexcept;
  const unsigned char *data() const noexcept;
  std::size_t size() const noexcept;
  bool empty() const noexcept;

private:
  void clear() noexcept;
  std::vector<unsigned char> bytes_;
  bool locked_ = false;
};

void initCrypto();
KdfParams defaultKdfParams();
KdfParams legacyKdfParams();
std::vector<unsigned char> randomBytes(std::size_t length);
std::string generateRandomString(std::size_t length);

SecureBuffer deriveKey(const std::string &password,
                       const std::vector<unsigned char> &salt,
                       const KdfParams &params);
std::vector<unsigned char>
encrypt(const std::string &plaintext, const SecureBuffer &key,
        const std::vector<unsigned char> &nonce,
        const std::vector<unsigned char> &associatedData);
bool decrypt(const std::vector<unsigned char> &ciphertext,
             const SecureBuffer &key, const std::vector<unsigned char> &nonce,
             const std::vector<unsigned char> &associatedData,
             std::string &plaintext);

unsigned deriveLegacySeedFromKey(const SecureBuffer &key,
                                 const std::vector<unsigned char> &salt);
void secureWipe(void *data, std::size_t size) noexcept;
