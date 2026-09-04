#include "crypto_utils.h"
#include "image_utils.h"
#include "terminal_utils.h"

#include <iostream>
#include <limits>
#include <string>

namespace {
void discardLine() {
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void showMenu() {
  std::cout << "\n=== PassPix Menu ===\n"
            << "1. Encrypt Password\n"
            << "2. Decrypt Password\n"
            << "3. Exit\n"
            << "Enter your choice: ";
}

void encryptFlow() {
  SensitiveString masterPassphrase;
  SensitiveString confirmation;
  SensitiveString password;
  if (!readSecret("Enter master passphrase: ", masterPassphrase) ||
      !readSecret("Confirm master passphrase: ", confirmation))
    return;
  if (masterPassphrase.value() != confirmation.value()) {
    std::cout << "Passphrases do not match. Aborting.\n";
    return;
  }
  if (masterPassphrase.value().empty()) {
    std::cout << "Master passphrase cannot be empty. Aborting.\n";
    return;
  }
  if (!readSecret("Enter password to encrypt: ", password))
    return;
  if (password.value().empty()) {
    std::cout << "Password cannot be empty. Aborting.\n";
    return;
  }

  std::string filename;
  std::string error;
  if (encryptPassword(masterPassphrase.value(), password.value(), filename,
                      error)) {
    std::cout << "Password encrypted to file: " << filename << '\n';
  } else {
    std::cout << "Encryption failed: " << error << '\n';
  }
}

void decryptFlow() {
  const auto files = listEncFiles();
  if (files.empty()) {
    std::cout << "No PassPix files found.\n";
    return;
  }
  std::cout << "Select a file to decrypt:\n";
  for (std::size_t i = 0; i < files.size(); ++i) {
    std::cout << (i + 1) << ". " << files[i] << '\n';
  }

  std::size_t fileIndex = 0;
  std::cout << "Enter file number: ";
  if (!(std::cin >> fileIndex)) {
    std::cin.clear();
    discardLine();
    std::cout << "Invalid input. Please enter a number.\n";
    return;
  }
  discardLine();
  if (fileIndex < 1 || fileIndex > files.size()) {
    std::cout << "Invalid selection.\n";
    return;
  }

  SensitiveString masterPassphrase;
  SensitiveString password;
  if (!readSecret("Enter master passphrase: ", masterPassphrase))
    return;
  std::string error;
  if (!decryptPassword(masterPassphrase.value(), files[fileIndex - 1],
                       password.value(), error)) {
    std::cout << "Decryption failed: " << error << '\n';
    return;
  }
  password.lock();

  std::cout << "Password decrypted. Reveal it now? [y/N]: ";
  std::string answer;
  std::getline(std::cin, answer);
  if (answer == "y" || answer == "Y") {
    std::cout << "Decrypted password: " << password.value() << '\n';
  } else {
    std::cout << "Password was not displayed.\n";
  }
}
} // namespace

int main() {
  try {
    initCrypto();
  } catch (const std::exception &exception) {
    std::cerr << "Fatal: " << exception.what() << '\n';
    return 1;
  }

  std::cout << "PassPix\n" << std::string(55, '=') << '\n';
  for (;;) {
    showMenu();
    int choice = 0;
    if (!(std::cin >> choice)) {
      if (std::cin.eof())
        break;
      std::cin.clear();
      discardLine();
      std::cout << "Invalid input. Please enter a number.\n";
      continue;
    }
    discardLine();
    if (choice == 1)
      encryptFlow();
    else if (choice == 2)
      decryptFlow();
    else if (choice == 3)
      break;
    else
      std::cout << "Invalid choice.\n";
  }
  return 0;
}
