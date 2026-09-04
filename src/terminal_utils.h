#pragma once

#include <string>

class SensitiveString {
public:
  SensitiveString() = default;
  ~SensitiveString();
  SensitiveString(const SensitiveString &) = delete;
  SensitiveString &operator=(const SensitiveString &) = delete;

  std::string &value() noexcept;
  const std::string &value() const noexcept;
  bool lock() noexcept;

private:
  std::string value_;
  bool locked_ = false;
};

bool readSecret(const std::string &prompt, SensitiveString &destination);
