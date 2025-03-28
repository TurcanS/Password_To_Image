#include "crypto_utils.h"
#include <iostream>
#include <random>
#include <iomanip>
#include <sstream>
#include <algorithm>

using namespace std;

const string PROGRAM_PASSWORD = "admin";

bool checkAccessPassword(const string& password) {
    return password == PROGRAM_PASSWORD;
}

string generateRandomString(size_t length) {
    const string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<> distribution(0, chars.size() - 1);
    
    string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += chars[distribution(generator)];
    }
    
    return result;
}

string deriveKey(const string& password, size_t keyLength) {
    string key = sha256(password + PROGRAM_PASSWORD);
    
    while (key.length() < keyLength) {
        key += sha256(key);
    }
    
    return key.substr(0, keyLength);
}

string sha256(const string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    
    EVP_DigestInit_ex(context, md, nullptr);
    EVP_DigestUpdate(context, data.c_str(), data.size());
    EVP_DigestFinal_ex(context, hash, nullptr);
    
    EVP_MD_CTX_free(context);
    
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

vector<unsigned char> aesEncrypt(const string& plaintext, const string& key, vector<unsigned char>& iv) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;
    
    vector<unsigned char> ciphertext(plaintext.length() + AES_BLOCK_SIZE);
    
    ctx = EVP_CIPHER_CTX_new();
    
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, 
                      reinterpret_cast<const unsigned char*>(key.c_str()), 
                      iv.data());
    
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, 
                     reinterpret_cast<const unsigned char*>(plaintext.c_str()), 
                     plaintext.length());
    ciphertext_len = len;
    
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    ciphertext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    
    ciphertext.resize(ciphertext_len);
    
    return ciphertext;
}

string aesDecrypt(const vector<unsigned char>& ciphertext, const string& key, const vector<unsigned char>& iv) {
    if (ciphertext.empty()) {
        cout << "Error: Empty ciphertext." << endl;
        return "";
    }
    
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    
    vector<unsigned char> plaintext_buf(ciphertext.size() + AES_BLOCK_SIZE);
    
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        cout << "Error: Failed to create cipher context." << endl;
        return "";
    }
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, 
                         reinterpret_cast<const unsigned char*>(key.c_str()), 
                         iv.data()) != 1) {
        cout << "Error: Failed to initialize decryption." << endl;
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    if (EVP_DecryptUpdate(ctx, plaintext_buf.data(), &len, 
                       ciphertext.data(), ciphertext.size()) != 1) {
        cout << "Error: Failed during decryption update." << endl;
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    plaintext_len = len;
    
    int finalResult = EVP_DecryptFinal_ex(ctx, plaintext_buf.data() + len, &len);
    
    EVP_CIPHER_CTX_free(ctx);
    
    if (finalResult <= 0) {
        cout << "Error: Decryption failed, possibly due to corrupted data or incorrect key/IV." << endl;
        return "";
    }
    
    plaintext_len += len;
    
    bool isPrintable = true;
    for (int i = 0; i < plaintext_len; i++) {
        if (plaintext_buf[i] == 0) {
            plaintext_len = i;
            break;
        }
        
        if (!isprint(plaintext_buf[i]) && !isspace(plaintext_buf[i])) {
            isPrintable = false;
            break;
        }
    }
    
    if (!isPrintable) {
        cout << "Warning: Decrypted data contains non-printable characters, which may indicate corruption." << endl;
    }
    
    return string(plaintext_buf.begin(), plaintext_buf.begin() + plaintext_len);
}
