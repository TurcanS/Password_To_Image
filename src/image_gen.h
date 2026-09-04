#pragma once

#include <random>
#include <vector>

const unsigned IMAGE_WIDTH = 1920;
const unsigned IMAGE_HEIGHT = 1080;

void generateGradient(std::vector<unsigned char> &image, unsigned width,
                      unsigned height, const std::vector<unsigned char> &color1,
                      const std::vector<unsigned char> &color2);
void addNaturalNoise(std::vector<unsigned char> &image, unsigned width,
                     unsigned height, float intensity);
void addShapes(std::vector<unsigned char> &image, unsigned width,
               unsigned height, int numShapes, std::mt19937 &rng);
