#include "../Include/lodepng.h"
#include "image_utils.h"
#include "crypto_utils.h"
#include <iostream>
#include <vector>
#include <string>
#include <limits>



void showMenu();

int main() {
    initCrypto();
    std::cout << "PassPix" << std::endl;
    std::cout << std::string(55, '=') << std::endl;
    
    while (true) {
        showMenu();
        
        int choice;
        std::cin >> choice;
        
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input. Please enter a number." << std::endl;
            continue;
        }
        std::cin.ignore();
        
        if (choice == 1) {
            std::string masterPassphrase, masterConfirm, password;
            
            std::cout << "Enter master passphrase: ";
            getline(std::cin, masterPassphrase);
            std::cout << "Confirm master passphrase: ";
            getline(std::cin, masterConfirm);
            
            if (masterPassphrase != masterConfirm) {
                std::cout << "Passphrases do not match. Aborting." << std::endl;
                continue;
            }
            
            if (masterPassphrase.empty()) {
                std::cout << "Master passphrase cannot be empty. Aborting." << std::endl;
                continue;
            }
            
            std::cout << "Enter password to encrypt: ";
            getline(std::cin, password);
            
            if (password.empty()) {
                std::cout << "Password cannot be empty. Aborting." << std::endl;
                continue;
            }
            
            encryptPassword(masterPassphrase, password);
            
        } else if (choice == 2) {
            auto files = listEncFiles();
            if (files.empty()) {
                std::cout << "No PassPix files found." << std::endl;
                continue;
            }
            
            std::cout << "Select a file to decrypt:" << std::endl;
            for (size_t i = 0; i < files.size(); i++) {
                std::cout << (i + 1) << ". " << files[i] << std::endl;
            }
            
            size_t fileIndex;
            std::cout << "Enter file number: ";
            std::cin >> fileIndex;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input. Please enter a number." << std::endl;
                continue;
            }
            std::cin.ignore();

            std::cout << "Enter master passphrase: ";
            std::string masterPassphrase;
            getline(std::cin, masterPassphrase);
            
            if (fileIndex < 1 || fileIndex > files.size()) {
                std::cout << "Invalid selection." << std::endl;
                continue;
            }
            
            std::string decrypted = decryptPassword(masterPassphrase, files[fileIndex - 1]);
            
            if (!decrypted.empty()) {
                std::cout << "Decrypted password: " << decrypted << std::endl;
            }
            
        } else if (choice == 3) {
            break;
        } else {
            std::cout << "Invalid choice." << std::endl;
        }
    }
    
    return 0;
}

void showMenu() {
    std::cout << "\n=== PassPix Menu ===" << std::endl;
    std::cout << "1. Encrypt Password" << std::endl;
    std::cout << "2. Decrypt Password" << std::endl;
    std::cout << "3. Exit" << std::endl;
    std::cout << "Enter your choice: ";
}
