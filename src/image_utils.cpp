#include "image_utils.h"
#include "crypto_utils.h"
#include <iostream>
#include <random>
#include <algorithm>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <map>
#include <cmath>

using namespace std;

const string PROGRAM_PASSWORD = "admin"; // Moving this constant here since it's used in the image functions

// Helper function to generate a smooth gradient
void generateGradient(vector<unsigned char>& image, unsigned width, unsigned height, 
                     const vector<unsigned char>& color1, const vector<unsigned char>& color2) {
    // Pre-calculate width factor to avoid repeated division
    const float widthInv = 1.0f / width;
    const float heightInv = 1.0f / height;
    
    for (unsigned y = 0; y < height; y++) {
        const float factor2 = y * heightInv;
        size_t rowOffset = y * width * 4;
        
        for (unsigned x = 0; x < width; x++) {
            const float factor = x * widthInv;
            const float blend = 0.5f * (factor + factor2);
            const float oneMinusBlend = 1.0f - blend;
            
            size_t idx = rowOffset + x * 4;
            // Unroll loop for better performance
            image[idx] = static_cast<unsigned char>(color1[0] * oneMinusBlend + color2[0] * blend);
            image[idx + 1] = static_cast<unsigned char>(color1[1] * oneMinusBlend + color2[1] * blend);
            image[idx + 2] = static_cast<unsigned char>(color1[2] * oneMinusBlend + color2[2] * blend);
            image[idx + 3] = 255; // Alpha channel
        }
    }
}

// Helper function to add some smooth noise to make it look natural
void addNaturalNoise(vector<unsigned char>& image, unsigned width, unsigned height, float intensity) {
    random_device rd;
    mt19937 rng(rd());
    normal_distribution<float> dist(0.0f, intensity);
    
    const size_t totalPixels = width * height;
    for (size_t pixel = 0; pixel < totalPixels; pixel++) {
        size_t idx = pixel * 4;
        // Unroll loop and combine operations
        float noise1 = dist(rng);
        float noise2 = dist(rng);
        float noise3 = dist(rng);
        
        int newValue1 = static_cast<int>(image[idx]) + static_cast<int>(noise1);
        int newValue2 = static_cast<int>(image[idx + 1]) + static_cast<int>(noise2);
        int newValue3 = static_cast<int>(image[idx + 2]) + static_cast<int>(noise3);
        
        image[idx] = static_cast<unsigned char>(max(0, min(255, newValue1)));
        image[idx + 1] = static_cast<unsigned char>(max(0, min(255, newValue2)));
        image[idx + 2] = static_cast<unsigned char>(max(0, min(255, newValue3)));
    }
}

// Helper function to add simple shapes for natural look
void addShapes(vector<unsigned char>& image, unsigned width, unsigned height, int numShapes, mt19937& rng) {
    uniform_int_distribution<int> xDist(0, width - 1);
    uniform_int_distribution<int> yDist(0, height - 1);
    uniform_int_distribution<int> radiusDist(30, 150);
    uniform_int_distribution<int> colorDist(0, 255);
    uniform_real_distribution<float> opacityDist(0.1f, 0.3f);
    
    for (int s = 0; s < numShapes; s++) {
        const int centerX = xDist(rng);
        const int centerY = yDist(rng);
        const int radius = radiusDist(rng);
        const unsigned char shapeColor[3] = {
            static_cast<unsigned char>(colorDist(rng)),
            static_cast<unsigned char>(colorDist(rng)),
            static_cast<unsigned char>(colorDist(rng))
        };
        const float opacity = opacityDist(rng);
        
        const int minY = max(0, centerY - radius);
        const int maxY = min(static_cast<int>(height), centerY + radius);
        const int minX = max(0, centerX - radius);
        const int maxX = min(static_cast<int>(width), centerX + radius);
        const float radiusSquared = static_cast<float>(radius * radius);
        
        for (int y = minY; y < maxY; y++) {
            const int dy = y - centerY;
            const int dySquared = dy * dy;
            const size_t rowOffset = y * width * 4;
            
            for (int x = minX; x < maxX; x++) {
                const int dx = x - centerX;
                const float distanceSquared = static_cast<float>(dx * dx + dySquared);
                
                if (distanceSquared < radiusSquared) {
                    const float distance = sqrt(distanceSquared);
                    float factor = 1.0f - (distance / radius);
                    factor = factor * factor * opacity; // Squared factor times opacity
                    const float oneMinusFactor = 1.0f - factor;
                    
                    const size_t idx = rowOffset + x * 4;
                    // Unroll loop for RGB channels
                    image[idx] = static_cast<unsigned char>(
                        image[idx] * oneMinusFactor + shapeColor[0] * factor);
                    image[idx + 1] = static_cast<unsigned char>(
                        image[idx + 1] * oneMinusFactor + shapeColor[1] * factor);
                    image[idx + 2] = static_cast<unsigned char>(
                        image[idx + 2] * oneMinusFactor + shapeColor[2] * factor);
                }
            }
        }
    }
}

void encryptPassword(const string& password) {
    const unsigned width = 720;
    const unsigned height = 720;
    const unsigned totalPixels = width * height;
    
    vector<unsigned char> image;
    image.resize(totalPixels * 4);
    
    // Create a random seed for the image generation
    random_device rd;
    mt19937 rng(rd());
    
    // Generate natural looking colors for the gradient
    uniform_int_distribution<int> colorDist(0, 255);
    vector<unsigned char> color1 = {
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng))
    };
    
    vector<unsigned char> color2 = {
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng))
    };
    
    // Create gradient background
    generateGradient(image, width, height, color1, color2);
    
    // Add some shapes to make it look more like a photograph
    uniform_int_distribution<int> numShapesDist(10, 25);
    int numShapes = numShapesDist(rng);
    addShapes(image, width, height, numShapes, rng);
    
    // Add subtle noise to make it look more natural
    addNaturalNoise(image, width, height, 10.0f);
    
    // Continue with encryption as before
    string salt = generateRandomString(16);
    string key = pbkdf2(salt, salt, AES_KEY_SIZE, PBKDF2_ITERATIONS);
    
    vector<unsigned char> iv(AES_BLOCK_SIZE);
    RAND_bytes(iv.data(), AES_BLOCK_SIZE);
    
    string passwordHash = sha256(password);
    
    vector<unsigned char> encryptedData = aesEncrypt(password, key, iv);
    
    // Store the encryption metadata in the image
    const unsigned encLen = encryptedData.size();
    const unsigned char encLenBytes[4] = {
        static_cast<unsigned char>((encLen) & 0xFF),
        static_cast<unsigned char>((encLen >> 8) & 0xFF),
        static_cast<unsigned char>((encLen >> 16) & 0xFF),
        static_cast<unsigned char>((encLen >> 24) & 0xFF)
    };
    
    // Store encryption length at three locations (combined loop)
    for (int i = 0; i < 4; i++) {
        image[i] = encLenBytes[i];
        image[width*4 - 4 + i] = encLenBytes[i];
        image[width*8 + i] = encLenBytes[i];
    }
    
    // Store salt at two locations (combined loop)
    for (size_t i = 0; i < salt.length(); i++) {
        const unsigned char saltChar = static_cast<unsigned char>(salt[i]);
        image[20 + i] = saltChar;
        image[width * 4 - 20 - i] = saltChar;
    }
    
    // Store IV at three locations (combined loop)
    for (int i = 0; i < AES_BLOCK_SIZE; i++) {
        const unsigned char ivByte = iv[i];
        image[100 + i] = ivByte;
        image[width * 4 - 100 - i] = ivByte;
        image[(height/2 * width + width/2) * 4 + i] = ivByte;
    }
    
    // Store password hash at two locations (combined loop)
    const size_t hashLen = min(static_cast<size_t>(32), passwordHash.length());
    for (size_t i = 0; i < hashLen; i++) {
        const unsigned char hashChar = static_cast<unsigned char>(passwordHash[i]);
        image[200 + i] = hashChar;
        image[totalPixels * 4 - 200 - i] = hashChar;
    }
    
    // Create indices for embedding encrypted data
    vector<size_t> indices;
    indices.reserve(totalPixels / 2);
    
    const size_t imageSize = totalPixels * 4;
    for (size_t idx = 304; idx < imageSize - 300; idx += 4) {
        indices.push_back(idx);
    }
    
    unsigned seed = deriveSeedFromKey(key, salt);
    mt19937 g(seed);
    shuffle(indices.begin(), indices.end(), g);
    
    // Embed encrypted data with redundancy (combined loop)
    const size_t indicesThird = indices.size() / 3;
    const size_t encDataSize = encryptedData.size();
    for (size_t i = 0; i < encDataSize && i < indices.size(); i++) {
        const unsigned char dataByte = encryptedData[i];
        image[indices[i]] = dataByte;
        
        if (i + indicesThird < indices.size()) {
            image[indices[i + indicesThird]] = dataByte;
        }
    }
    
    vector<unsigned char> hmac = generateHMAC(encryptedData, key);
    
    // Store HMAC at three locations (combined loop)
    const size_t hmacSize = min(hmac.size(), static_cast<size_t>(HMAC_SIZE));
    for (size_t i = 0; i < hmacSize; i++) {
        const unsigned char hmacByte = hmac[i];
        image[imageSize - 40 - i] = hmacByte;
        image[imageSize - 40 - HMAC_SIZE - i] = hmacByte;
        image[400 + i] = hmacByte;
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
    
    const unsigned totalPixels = width * height;
    const size_t imageSize = totalPixels * 4;
    
    // Extract encryption length from three locations (combined loop)
    unsigned encLen1 = 0, encLen2 = 0, encLen3 = 0;
    for (int i = 0; i < 4; i++) {
        const unsigned shift = i * 8;
        encLen1 |= (static_cast<unsigned>(image[i]) << shift);
        encLen2 |= (static_cast<unsigned>(image[width*4 - 4 + i]) << shift);
        encLen3 |= (static_cast<unsigned>(image[width*8 + i]) << shift);
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
    
    // Extract salt from two locations with error recovery (combined loop)
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
    
    // Extract IV from three locations with error recovery (combined loop)
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
    
    // Create indices for extracting encrypted data (optimized)
    vector<size_t> indices;
    indices.reserve(totalPixels / 2);
    
    for (size_t idx = 304; idx < imageSize - 300; idx += 4) {
        indices.push_back(idx);
    }
    
    // Use PBKDF2 with just salt, not mixing the program password for decryption
    string key = pbkdf2(salt, salt, AES_KEY_SIZE, PBKDF2_ITERATIONS);
    
    // Derive the seed from key and salt instead of reading it from the image
    unsigned seed = deriveSeedFromKey(key, salt);
    mt19937 g(seed);
    shuffle(indices.begin(), indices.end(), g);
    
    // Extract encrypted data with frequency analysis (optimized)
    vector<map<unsigned char, int>> byteFrequencies(encLen);
    const size_t indicesThird = indices.size() / 3;
    
    for (size_t i = 0; i < encLen && i < indices.size(); i++) {
        byteFrequencies[i][image[indices[i]]]++;
        
        if (i + indicesThird < indices.size() && i < encLen) {
            byteFrequencies[i][image[indices[i + indicesThird]]]++;
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
    
    // Extract stored HMAC from multiple locations (combined into arrays)
    vector<unsigned char> storedHmac1(HMAC_SIZE), storedHmac2(HMAC_SIZE), storedHmac3(HMAC_SIZE);
    for (int i = 0; i < HMAC_SIZE; i++) {
        storedHmac1[i] = image[imageSize - 40 - i];
        storedHmac2[i] = image[imageSize - 40 - HMAC_SIZE - i];
        storedHmac3[i] = image[400 + i];
    }
    
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
            
            // Verify password hash (combined loop)
            const size_t hashLen = min(static_cast<size_t>(32), passwordHash.length());
            for (size_t i = 0; i < hashLen; i++) {
                totalChecks += 2;
                
                unsigned char hash1 = image[200 + i];
                unsigned char hash2 = image[imageSize - 200 - i];
                
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
