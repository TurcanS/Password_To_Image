#include "image_utils.h"
#include "crypto_utils.h"
#include <iostream>
#include <random>
#include <algorithm>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <map>

using namespace std;

const string PROGRAM_PASSWORD = "admin"; // Moving this constant here since it's used in the image functions

void encryptPassword(const string& password) {
    unsigned width = 720;
    unsigned height = 720;
    
    vector<unsigned char> image;
    image.resize(width * height * 4);
    
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<unsigned char> dist(0, 255);
    
    for (size_t i = 0; i < image.size(); i++) {
        image[i] = dist(rng);
    }
    
    string salt = generateRandomString(16);
    string key = deriveKey(PROGRAM_PASSWORD + salt, AES_KEY_SIZE);
    
    vector<unsigned char> iv(AES_BLOCK_SIZE);
    RAND_bytes(iv.data(), AES_BLOCK_SIZE);
    
    string passwordHash = sha256(password);
    
    vector<unsigned char> encryptedData = aesEncrypt(password, key, iv);
    
    unsigned encLen = encryptedData.size();
    for (int i = 0; i < 4; i++) {
        image[i] = (encLen >> (i * 8)) & 0xFF;
        image[width*4 - 4 + i] = (encLen >> (i * 8)) & 0xFF;
        image[width*8 + i] = (encLen >> (i * 8)) & 0xFF;
    }
    
    for (size_t i = 0; i < salt.length(); i++) {
        image[20 + i] = static_cast<unsigned char>(salt[i]);
        image[width * 4 - 20 - i] = static_cast<unsigned char>(salt[i]);
    }
    
    for (int i = 0; i < AES_BLOCK_SIZE; i++) {
        image[100 + i] = iv[i];
        image[width * 4 - 100 - i] = iv[i];
        image[(height/2 * width + width/2) * 4 + i] = iv[i];
    }
    
    for (int i = 0; i < 32 && i < passwordHash.length(); i++) {
        image[200 + i] = static_cast<unsigned char>(passwordHash[i]);
        image[width * height * 4 - 200 - i] = static_cast<unsigned char>(passwordHash[i]);
    }
    
    vector<size_t> indices;
    indices.reserve(width * height / 2);
    
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 4;
            if (idx > 300 && idx < width * height * 4 - 300) {
                indices.push_back(idx);
            }
        }
    }
    
    string seedStr = PROGRAM_PASSWORD;
    seed_seq seed(seedStr.begin(), seedStr.end());
    mt19937 g(seed);
    shuffle(indices.begin(), indices.end(), g);
    
    for (size_t i = 0; i < encryptedData.size() && i < indices.size(); i++) {
        image[indices[i]] = encryptedData[i];
        
        if (i + indices.size()/3 < indices.size()) {
            image[indices[i + indices.size()/3]] = encryptedData[i];
        }
    }
    
    // Replace XOR checksum with HMAC
    vector<unsigned char> hmac = generateHMAC(encryptedData, key);
    
    // Store HMAC at multiple places for redundancy
    for (size_t i = 0; i < min(hmac.size(), (size_t)HMAC_SIZE); i++) {
        image[width * height * 4 - 40 - i] = hmac[i];
        image[width * height * 4 - 40 - HMAC_SIZE - i] = hmac[i];
        image[400 + i] = hmac[i];
    }
    
    string filename = "enc_" + generateRandomString(10) + ".png";
    
    unsigned error = lodepng::encode(filename, image, width, height);
    if (error) {
        cout << "Error encoding image: " << lodepng_error_text(error) << endl;
    } else {
        cout << "Password encrypted to file: " << filename << endl;
    }
}

string decryptPassword(const string& filename) {
    vector<unsigned char> image;
    unsigned width, height;
    
    unsigned error = lodepng::decode(image, width, height, filename);
    if (error) {
        cout << "Error decoding image: " << lodepng_error_text(error) << endl;
        return "";
    }
    
    unsigned encLen1 = 0, encLen2 = 0, encLen3 = 0;
    for (int i = 0; i < 4; i++) {
        encLen1 |= (static_cast<unsigned>(image[i]) << (i * 8));
        encLen2 |= (static_cast<unsigned>(image[width*4 - 4 + i]) << (i * 8));
        encLen3 |= (static_cast<unsigned>(image[width*8 + i]) << (i * 8));
    }
    
    unsigned encLen;
    if (encLen1 == encLen2 || encLen1 == encLen3) {
        encLen = encLen1;
    } else if (encLen2 == encLen3) {
        encLen = encLen2;
    } else {
        vector<unsigned> lens = {encLen1, encLen2, encLen3};
        sort(lens.begin(), lens.end());
        encLen = lens[1];
        
        if (encLen > 10000) {
            encLen = min({encLen1, encLen2, encLen3});
            if (encLen > 10000) encLen = 1000;
        }
    }
    
    if (encLen1 != encLen2 || encLen2 != encLen3) {
        cout << "Warning: Size data was corrupted but recovered using redundancy." << endl;
    }
    
    string salt;
    salt.reserve(16);
    bool saltCorrupted = false;
    
    for (size_t i = 0; i < 16; i++) {
        unsigned char salt1 = image[20 + i];
        unsigned char salt2 = image[width * 4 - 20 - i];
        
        if (salt1 == salt2) {
            salt.push_back(salt1);
        } else {
            saltCorrupted = true;
            salt.push_back((salt1 != 0) ? salt1 : salt2);
        }
    }
    
    if (saltCorrupted) {
        cout << "Warning: Salt was corrupted but attempted recovery." << endl;
    }
    
    vector<unsigned char> iv(AES_BLOCK_SIZE);
    bool ivCorrupted = false;
    
    for (int i = 0; i < AES_BLOCK_SIZE; i++) {
        unsigned char iv1 = image[100 + i];
        unsigned char iv2 = image[width * 4 - 100 - i];
        unsigned char iv3 = image[(height/2 * width + width/2) * 4 + i];
        
        if (iv1 == iv2 || iv1 == iv3) {
            iv[i] = iv1;
        } else if (iv2 == iv3) {
            iv[i] = iv2;
        } else {
            ivCorrupted = true;
            iv[i] = (iv1 != 0) ? iv1 : ((iv2 != 0) ? iv2 : iv3);
        }
    }
    
    if (ivCorrupted) {
        cout << "Warning: IV was corrupted but repaired using redundant data." << endl;
    }
    
    vector<size_t> indices;
    indices.reserve(width * height / 2);
    
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 4;
            if (idx > 300 && idx < width * height * 4 - 300) {
                indices.push_back(idx);
            }
        }
    }
    
    string seedStr = PROGRAM_PASSWORD;
    seed_seq seed(seedStr.begin(), seedStr.end());
    mt19937 g(seed);
    shuffle(indices.begin(), indices.end(), g);
    
    vector<map<unsigned char, int>> byteFrequencies(encLen);
    
    for (size_t i = 0; i < encLen && i < indices.size(); i++) {
        byteFrequencies[i][image[indices[i]]]++;
        
        if (i + indices.size()/3 < indices.size() && i < encLen) {
            byteFrequencies[i][image[indices[i + indices.size()/3]]]++;
        }
    }
    
    vector<unsigned char> encryptedData(encLen);
    for (size_t i = 0; i < encLen; i++) {
        unsigned char mostCommon = 0;
        int maxCount = 0;
        
        for (const auto& pair : byteFrequencies[i]) {
            if (pair.second > maxCount) {
                maxCount = pair.second;
                mostCommon = pair.first;
            }
        }
        
        encryptedData[i] = mostCommon;
    }
    
    // Extract stored HMAC from multiple locations
    vector<unsigned char> storedHmac1(HMAC_SIZE), storedHmac2(HMAC_SIZE), storedHmac3(HMAC_SIZE);
    for (int i = 0; i < HMAC_SIZE; i++) {
        storedHmac1[i] = image[width * height * 4 - 40 - i];
        storedHmac2[i] = image[width * height * 4 - 40 - HMAC_SIZE - i];
        storedHmac3[i] = image[400 + i];
    }
    
    string key = deriveKey(PROGRAM_PASSWORD + salt, AES_KEY_SIZE);
    
    // Verify with each HMAC and consider verification successful if any match
    bool hmacVerified = 
        verifyHMAC(encryptedData, storedHmac1, key) ||
        verifyHMAC(encryptedData, storedHmac2, key) ||
        verifyHMAC(encryptedData, storedHmac3, key);
    
    if (!hmacVerified) {
        cout << "Warning: HMAC verification failed. Data integrity cannot be guaranteed." << endl;
    } else {
        cout << "HMAC verification successful. Data integrity confirmed." << endl;
    }
    
    try {
        string password = aesDecrypt(encryptedData, key, iv);
        
        if (!password.empty()) {
            string passwordHash = sha256(password);
            int validHashes = 0;
            int totalChecks = 0;
            
            for (int i = 0; i < 32 && i < passwordHash.length(); i++) {
                totalChecks += 2;
                
                unsigned char hash1 = image[200 + i];
                unsigned char hash2 = image[width * height * 4 - 200 - i];
                
                unsigned char expectedHash = static_cast<unsigned char>(passwordHash[i]);
                
                if (expectedHash == hash1) validHashes++;
                if (expectedHash == hash2) validHashes++;
            }
            
            float hashValidityPercentage = (float)validHashes / totalChecks * 100.0f;
            cout << "Password hash verification: " << hashValidityPercentage << "% valid" << endl;
            
            if (hashValidityPercentage < 30.0f) {
                cout << "Warning: Password verification failed. The data may be corrupted." << endl;
            }
            
            return password;
        }
        return password;
    } catch (...) {
        cout << "Error: Exception during decryption process." << endl;
        return "";
    }
}

vector<string> listEncFiles() {
    vector<string> files;
    DIR* dir;
    struct dirent* ent;
    
    if ((dir = opendir(".")) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            string filename = ent->d_name;
            if (filename.length() > 4 && 
                filename.substr(0, 4) == "enc_" &&
                filename.substr(filename.length() - 4) == ".png") {
                files.push_back(filename);
            }
        }
        closedir(dir);
    }
    
    return files;
}
