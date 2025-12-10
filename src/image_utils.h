#pragma once

#include <vector>
#include <string>
#include <random>
#include "../Include/lodepng.h"

// Constants for data embedding boundaries
const size_t METADATA_BOUNDARY = 300;
const size_t DATA_EMBEDDING_START = 304; // METADATA_BOUNDARY + 4 bytes

void encryptPassword(const std::string& password);
std::string decryptPassword(const std::string& filename);
std::vector<std::string> listEncFiles();
void generateGradient(std::vector<unsigned char>& image, unsigned width, unsigned height, 
                     const std::vector<unsigned char>& color1, const std::vector<unsigned char>& color2);
void addNaturalNoise(std::vector<unsigned char>& image, unsigned width, unsigned height, float intensity);
void addShapes(std::vector<unsigned char>& image, unsigned width, unsigned height, int numShapes, std::mt19937& rng);