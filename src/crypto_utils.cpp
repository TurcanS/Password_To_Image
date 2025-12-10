#include "crypto_utils.h"
#include <iostream>
#include <random>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>

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
    // Use PBKDF2 instead of simple SHA256
    string salt = generateRandomString(16);
    return pbkdf2(password, salt, keyLength, PBKDF2_ITERATIONS);
}

string pbkdf2(const string& password, const string& salt, size_t keyLength, int iterations) {
    unsigned char* key = new unsigned char[keyLength];
    
    // Use PKCS5_PBKDF2_HMAC with SHA-256
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), password.length(),
            reinterpret_cast<const unsigned char*>(salt.c_str()), salt.length(),
            iterations,
            EVP_sha256(),
            keyLength, key) != 1) {
        cerr << "Error generating key using PBKDF2" << endl;
        delete[] key;
        return "";
    }
    
    // Convert to string for our API
    string result(reinterpret_cast<char*>(key), keyLength);
    
    // Clean up
    delete[] key;
    return result;
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
    
    // Explicitly set padding mode (PKCS7 padding is the default)
    EVP_CIPHER_CTX_set_padding(ctx, 1);
    
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
    
    // Explicitly set padding mode (PKCS7 padding is the default)
    EVP_CIPHER_CTX_set_padding(ctx, 1);
    
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
    
    // Check if plaintext contains only valid characters
    bool isPrintable = true;
    for (int i = 0; i < plaintext_len; i++) {
        if (!isprint(plaintext_buf[i]) && !isspace(plaintext_buf[i])) {
            isPrintable = false;
            break;
        }
    }
    
    if (!isPrintable) {
        cout << "Warning: Decrypted data contains non-printable characters, which may indicate corruption." << endl;
    }
    
    // The padding is automatically removed by EVP_DecryptFinal_ex, so we don't need to handle null bytes
    return string(plaintext_buf.begin(), plaintext_buf.begin() + plaintext_len);
}

vector<unsigned char> generateHMAC(const vector<unsigned char>& data, const string& key) {
    vector<unsigned char> hmac(HMAC_SIZE);
    unsigned int len = 0;
    
    // Use proper HMAC function instead of manual concatenation
    HMAC(EVP_sha256(), 
         key.c_str(), 
         key.length(),
         data.data(), 
         data.size(),
         hmac.data(), 
         &len);
    
    hmac.resize(len);
    return hmac;
}

bool verifyHMAC(const vector<unsigned char>& data, const vector<unsigned char>& hmac, const string& key) {
    vector<unsigned char> computedHmac = generateHMAC(data, key);
    
    if (hmac.size() != computedHmac.size()) {
        return false;
    }
    
    // Use OpenSSL's constant-time comparison
    return CRYPTO_memcmp(hmac.data(), computedHmac.data(), hmac.size()) == 0;
}

unsigned deriveSeedFromKey(const string& key, const string& salt) {
    // Use SHA-256 to create a hash of key and salt
    unsigned char hash[SHA256_DIGEST_LENGTH];
    
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    
    EVP_DigestInit_ex(context, md, nullptr);
    EVP_DigestUpdate(context, key.data(), key.size());
    EVP_DigestUpdate(context, salt.data(), salt.size());
    EVP_DigestFinal_ex(context, hash, nullptr);
    
    EVP_MD_CTX_free(context);
    
    // Convert first 4 bytes of hash to an unsigned int for the seed
    // Use memcpy to avoid potential alignment issues
    unsigned seed;
    memcpy(&seed, hash, sizeof(unsigned));
    
    return seed;
}
