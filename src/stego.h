#pragma once

#include <vector>
#include <string>

const size_t METADATA_BOUNDARY = 300;
const size_t DATA_EMBEDDING_START = 1500;
const size_t SALT_OFFSET = 20;
const size_t IV_OFFSET = 500;
const size_t HASH_OFFSET = 1000;

const unsigned char MAGIC_BYTES[4] = {0x50, 0x61, 0x73, 0x73};
const size_t MAGIC_POSITIONS = 512;
const int REDUNDANCY_FACTOR = 16;
const size_t REDUNDANCY_STRIDE = 1024;

struct EncryptedPayload {
    std::vector<unsigned char> encryptedData;
    std::string salt;
    std::vector<unsigned char> iv;
    std::string passwordHash;
};

void embedMagic(std::vector<unsigned char>& image, unsigned width, unsigned height);
bool checkMagic(const std::vector<unsigned char>& image, unsigned width, unsigned height);

void embedPayload(std::vector<unsigned char>& image, unsigned width, unsigned height,
                  const EncryptedPayload& payload, const std::string& key);
EncryptedPayload extractPayload(const std::vector<unsigned char>& image, unsigned width,
                                 unsigned height, const std::string& key);
