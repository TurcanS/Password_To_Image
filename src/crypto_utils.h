#pragma once

#include <string>
#include <vector>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>

const int AES_KEY_SIZE = 32;
const int AES_BLOCK_SIZE = 16;
const int HMAC_SIZE = 32; // SHA-256 HMAC size

std::string sha256(const std::string& data);
std::string deriveKey(const std::string& password, size_t keyLength);
std::vector<unsigned char> aesEncrypt(const std::string& plaintext, const std::string& key, std::vector<unsigned char>& iv);
std::string aesDecrypt(const std::vector<unsigned char>& ciphertext, const std::string& key, const std::vector<unsigned char>& iv);
std::string generateRandomString(size_t length);
bool checkAccessPassword(const std::string& password);

// New HMAC functions
std::vector<unsigned char> generateHMAC(const std::vector<unsigned char>& data, const std::string& key);
bool verifyHMAC(const std::vector<unsigned char>& data, const std::vector<unsigned char>& hmac, const std::string& key);