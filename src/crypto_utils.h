#pragma once

#include <string>
#include <vector>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

const int AES_KEY_SIZE = 32;
const int AES_BLOCK_SIZE = 16;

std::string sha256(const std::string& data);
std::string deriveKey(const std::string& password, size_t keyLength);
std::vector<unsigned char> aesEncrypt(const std::string& plaintext, const std::string& key, std::vector<unsigned char>& iv);
std::string aesDecrypt(const std::vector<unsigned char>& ciphertext, const std::string& key, const std::vector<unsigned char>& iv);
std::string generateRandomString(size_t length);
bool checkAccessPassword(const std::string& password);