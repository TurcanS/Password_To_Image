#include "terminal_utils.h"

#include "crypto_utils.h"

#include <iostream>
#include <sodium.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

SensitiveString::~SensitiveString() {
  if (!value_.empty()) {
    if (locked_)
      sodium_munlock(value_.data(), value_.size());
    else
      secureWipe(value_.data(), value_.size());
  }
}

std::string &SensitiveString::value() noexcept { return value_; }
const std::string &SensitiveString::value() const noexcept { return value_; }

bool SensitiveString::lock() noexcept {
  if (value_.empty() || locked_)
    return true;
  locked_ = sodium_mlock(value_.data(), value_.size()) == 0;
  return locked_;
}

bool readSecret(const std::string &prompt, SensitiveString &destination) {
  std::cout << prompt << std::flush;
#ifdef _WIN32
  if (_isatty(_fileno(stdin))) {
    std::string &value = destination.value();
    value.clear();
    for (;;) {
      const int character = _getch();
      if (character == '\r' || character == '\n')
        break;
      if ((character == '\b' || character == 127) && !value.empty())
        value.pop_back();
      else if (character >= 32 && character <= 255)
        value.push_back(static_cast<char>(character));
    }
    std::cout << '\n';
    destination.lock();
    return true;
  }
#else
  if (isatty(STDIN_FILENO)) {
    termios original{};
    if (tcgetattr(STDIN_FILENO, &original) == 0) {
      termios hidden = original;
      hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
      if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden) == 0) {
        const bool success =
            static_cast<bool>(std::getline(std::cin, destination.value()));
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        std::cout << '\n';
        destination.lock();
        return success;
      }
    }
  }
#endif
  const bool success =
      static_cast<bool>(std::getline(std::cin, destination.value()));
  destination.lock();
  return success;
}
