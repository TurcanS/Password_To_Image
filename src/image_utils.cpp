#include "image_utils.h"
#include "image_gen.h"
#include "stego.h"
#include "crypto_utils.h"
#include "../Include/lodepng.h"
#include <iostream>
#include <random>
#include <filesystem>
#include <algorithm>

namespace {
    constexpr size_t FILENAME_RANDOM_LENGTH = 10;
    constexpr size_t SALT_LENGTH = 16;
}

void encryptPassword(const std::string& masterPassphrase, const std::string& password) {
    const unsigned width = IMAGE_WIDTH;
    const unsigned height = IMAGE_HEIGHT;

    std::vector<unsigned char> image;
    image.resize(static_cast<size_t>(width) * height * 4);

    std::random_device rd;
    std::mt19937 rng(rd());

    std::uniform_int_distribution<int> colorDist(0, 255);
    std::vector<unsigned char> color1 = {
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng))
    };
    std::vector<unsigned char> color2 = {
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng))
    };

    generateGradient(image, width, height, color1, color2);

    std::uniform_int_distribution<int> numShapesDist(10, 25);
    int numShapes = numShapesDist(rng);
    addShapes(image, width, height, numShapes, rng);
    addNaturalNoise(image, width, height, 10.0f);

    std::string salt = generateRandomString(SALT_LENGTH);

    std::string key = deriveKey(masterPassphrase, salt, KEY_SIZE);
    lockMem(&key[0], key.size());

    std::vector<unsigned char> encryptedBlob = encrypt(password, key);

    std::vector<unsigned char> nonce(encryptedBlob.begin(),
                                      encryptedBlob.begin() + NONCE_SIZE);

    std::string passwordHash = sha256(password);

    EncryptedPayload payload;
    payload.encryptedData = encryptedBlob;
    payload.salt = salt;
    payload.iv = nonce;
    payload.passwordHash = passwordHash;

    embedPayload(image, width, height, payload, key);
    embedMagic(image, width, height);

    secureWipe(&key[0], key.size());
    unlockMem(&key[0], key.size());

    std::string filename = "img_" + generateRandomString(FILENAME_RANDOM_LENGTH) + ".png";
    unsigned error = lodepng::encode(filename, image, width, height);
    if (error) {
        std::cout << "Error encoding image: " << lodepng_error_text(error) << std::endl;
    } else {
        std::cout << "Password encrypted to file: " << filename << std::endl;
    }
}

std::string decryptPassword(const std::string& masterPassphrase, const std::string& filename) {
    std::vector<unsigned char> image;
    unsigned width, height;

    unsigned error = lodepng::decode(image, width, height, filename);
    if (error) {
        std::cout << "Error decoding image: " << lodepng_error_text(error) << std::endl;
        return "";
    }

    std::string salt;
    salt.reserve(SALT_LENGTH);
    for (size_t i = 0; i < SALT_LENGTH; i++) {
        unsigned char s = image[SALT_OFFSET + i];
        salt.push_back(static_cast<char>(s));
    }

    std::string key = deriveKey(masterPassphrase, salt, KEY_SIZE);
    lockMem(&key[0], key.size());

    EncryptedPayload payload = extractPayload(image, width, height, key);

    key = deriveKey(masterPassphrase, payload.salt, KEY_SIZE);

    try {
        std::string password = decrypt(payload.encryptedData, key);

        if (!password.empty()) {
            std::string passwordHash = sha256(password);
            const size_t hashLen = std::min(static_cast<size_t>(32), passwordHash.length());
            int validHashes = 0;
            for (size_t i = 0; i < hashLen; i++) {
                if (static_cast<int>(passwordHash[i]) == static_cast<int>(payload.passwordHash[i]))
                    validHashes++;
            }
            const float hashValidity = static_cast<float>(validHashes) / static_cast<float>(hashLen) * 100.0f;
            std::cout << "Password hash verification: " << hashValidity << "% valid" << std::endl;
            if (hashValidity < 30.0f) {
                std::cout << "Warning: Password verification failed. The data may be corrupted." << std::endl;
            }
        }

        secureWipe(&key[0], key.size());
        unlockMem(&key[0], key.size());
        return password;
    } catch (...) {
        secureWipe(&key[0], key.size());
        unlockMem(&key[0], key.size());
        std::cout << "Error: Exception during decryption process." << std::endl;
        return "";
    }
}

std::vector<std::string> listEncFiles() {
    std::vector<std::string> files;
    namespace fs = std::filesystem;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(".", ec)) {
        if (ec) break;
        std::string filename = entry.path().filename().string();
        if (filename.length() < 5 || filename.substr(filename.length() - 4) != ".png")
            continue;

        // Quick check: skip already-found enc_ files
        bool alreadyListed = false;
        for (const auto& f : files) {
            if (f == filename) { alreadyListed = true; break; }
        }
        if (alreadyListed) continue;

        // Try magic check
        std::vector<unsigned char> img;
        unsigned w, h;
        if (lodepng::decode(img, w, h, filename) != 0) continue;
        if (checkMagic(img, w, h)) {
            files.push_back(filename);
        }
    }

    return files;
}
