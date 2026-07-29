#include "crypto_utils.h"
#include <iostream>
#include <random>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <sodium.h>

void initCrypto() {
    if (sodium_init() < 0) {
        std::cerr << "Fatal: libsodium initialization failed" << std::endl;
        std::exit(1);
    }
}

std::string generateRandomString(size_t length) {
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::vector<unsigned char> randBytes(length);
    randombytes_buf(randBytes.data(), length);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += chars[randBytes[i] % chars.size()];
    }
    return result;
}

std::string deriveKey(const std::string& password, const std::string& salt, size_t keyLength) {
    std::vector<unsigned char> key(keyLength);

    if (crypto_pwhash(
            key.data(), keyLength,
            password.c_str(), password.length(),
            reinterpret_cast<const unsigned char*>(salt.c_str()),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE,
            crypto_pwhash_ALG_ARGON2ID13) != 0) {
        std::cerr << "Error: Argon2id key derivation failed" << std::endl;
        return "";
    }

    return std::string(reinterpret_cast<char*>(key.data()), keyLength);
}

std::string sha256(const std::string& data) {
    unsigned char hash[crypto_hash_sha256_BYTES];

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        std::cerr << "Error: Failed to create EVP_MD_CTX" << std::endl;
        return "";
    }

    const EVP_MD* md = EVP_sha256();

    if (EVP_DigestInit_ex(context, md, nullptr) != 1) {
        std::cerr << "Error: Failed to initialize digest" << std::endl;
        EVP_MD_CTX_free(context);
        return "";
    }

    if (EVP_DigestUpdate(context, data.c_str(), data.size()) != 1) {
        std::cerr << "Error: Failed to update digest" << std::endl;
        EVP_MD_CTX_free(context);
        return "";
    }

    if (EVP_DigestFinal_ex(context, hash, nullptr) != 1) {
        std::cerr << "Error: Failed to finalize digest" << std::endl;
        EVP_MD_CTX_free(context);
        return "";
    }

    EVP_MD_CTX_free(context);

    std::stringstream ss;
    for (int i = 0; i < static_cast<int>(crypto_hash_sha256_BYTES); i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::vector<unsigned char> encrypt(const std::string& plaintext, const std::string& key) {
    std::vector<unsigned char> nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext(plaintext.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);
    unsigned long long ciphertextLen;

    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext.data(), &ciphertextLen,
            reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size(),
            nullptr, 0,
            nullptr,
            nonce.data(),
            reinterpret_cast<const unsigned char*>(key.data())) != 0) {
        std::cerr << "Error: Encryption failed" << std::endl;
        return {};
    }

    std::vector<unsigned char> result;
    result.reserve(nonce.size() + ciphertextLen);
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertextLen);

    return result;
}

std::string decrypt(const std::vector<unsigned char>& data, const std::string& key) {
    if (data.size() < static_cast<size_t>(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES + crypto_aead_xchacha20poly1305_ietf_ABYTES)) {
        std::cout << "Error: Ciphertext too short." << std::endl;
        return "";
    }

    const unsigned char* nonce = data.data();
    const unsigned char* ciphertext = data.data() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    size_t ciphertextLen = data.size() - crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

    std::vector<unsigned char> plaintext(ciphertextLen);
    unsigned long long plaintextLen;

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext.data(), &plaintextLen,
            nullptr,
            ciphertext, ciphertextLen,
            nullptr, 0,
            nonce,
            reinterpret_cast<const unsigned char*>(key.data())) != 0) {
        std::cout << "Error: Decryption failed, possibly due to corrupted data or incorrect key." << std::endl;
        return "";
    }

    plaintext.resize(plaintextLen);
    return std::string(plaintext.begin(), plaintext.end());
}

void secureWipe(void* data, size_t size) {
    sodium_memzero(data, size);
}

void lockMem(void* data, size_t size) {
    sodium_mlock(data, size);
}

void unlockMem(void* data, size_t size) {
    sodium_munlock(data, size);
}

unsigned deriveSeedFromKey(const std::string& key, const std::string& salt) {
    unsigned char hash[crypto_hash_sha256_BYTES];

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        std::cerr << "Error: Failed to create EVP_MD_CTX" << std::endl;
        return 0;
    }

    const EVP_MD* md = EVP_sha256();

    if (EVP_DigestInit_ex(context, md, nullptr) != 1) {
        std::cerr << "Error: Failed to initialize digest" << std::endl;
        EVP_MD_CTX_free(context);
        return 0;
    }

    if (EVP_DigestUpdate(context, key.data(), key.size()) != 1) {
        std::cerr << "Error: Failed to update digest with key" << std::endl;
        EVP_MD_CTX_free(context);
        return 0;
    }

    if (EVP_DigestUpdate(context, salt.data(), salt.size()) != 1) {
        std::cerr << "Error: Failed to update digest with salt" << std::endl;
        EVP_MD_CTX_free(context);
        return 0;
    }

    if (EVP_DigestFinal_ex(context, hash, nullptr) != 1) {
        std::cerr << "Error: Failed to finalize digest" << std::endl;
        EVP_MD_CTX_free(context);
        return 0;
    }

    EVP_MD_CTX_free(context);

    unsigned seed = 0;
    for (int i = 0; i < 4; i++) {
        seed |= (static_cast<unsigned>(hash[i]) << (i * 8));
    }

    return seed;
}
