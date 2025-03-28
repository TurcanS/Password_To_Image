#include "../Include/lodepng.h"
#include "image_utils.h"
#include "crypto_utils.h"
#include <iostream>
#include <vector>
#include <string>

using namespace std;

void showMenu();

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
    
    cout << "Enter program access password: ";
    string inputPassword;
    cin >> inputPassword;
    
    if (!checkAccessPassword(inputPassword)) {
        cout << "Invalid password. Exiting..." << endl;
        return 1;
    }
    
    cout << "Access granted." << endl;
    
    while (true) {
        showMenu();
        
        int choice;
        cin >> choice;
        cin.ignore();
        
        if (choice == 1) {
            cout << "Enter password to encrypt: ";
            string password;
            getline(cin, password);
            encryptPassword(password);
        } else if (choice == 2) {
            auto files = listEncFiles();
            if (files.empty()) {
                cout << "No encrypted files found." << endl;
                continue;
            }
            
            cout << "Select a file to decrypt:" << endl;
            for (size_t i = 0; i < files.size(); i++) {
                cout << (i + 1) << ". " << files[i] << endl;
            }
            
            size_t fileIndex;
            cout << "Enter file number: ";
            cin >> fileIndex;
            cin.ignore();
            
            if (fileIndex < 1 || fileIndex > files.size()) {
                cout << "Invalid selection." << endl;
                continue;
            }
            
            string decrypted = decryptPassword(files[fileIndex - 1]);
            cout << "Decrypted password: " << decrypted << endl;
        } else if (choice == 3) {
            break;
        } else {
            cout << "Invalid choice." << endl;
        }
    }
    
    return 0;
}

void showMenu() {
    cout << "\n=== Password Encryption/Decryption ===" << endl;
    cout << "1. Encrypt Password" << endl;
    cout << "2. Decrypt Password" << endl;
    cout << "3. Exit" << endl;
    cout << "Enter your choice: ";
}
