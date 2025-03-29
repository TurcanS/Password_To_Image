#pragma once

#include <vector>
#include <string>
#include <random>
#include "../Include/lodepng.h"

void encryptPassword(const std::string& password);
std::string decryptPassword(const std::string& filename);
std::vector<std::string> listEncFiles();
void generateGradient(std::vector<unsigned char>& image, unsigned width, unsigned height, 
                     const std::vector<unsigned char>& color1, const std::vector<unsigned char>& color2);
void addNaturalNoise(std::vector<unsigned char>& image, unsigned width, unsigned height, float intensity);
void addShapes(std::vector<unsigned char>& image, unsigned width, unsigned height, int numShapes, std::mt19937& rng);