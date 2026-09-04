#include "image_gen.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
constexpr int MIN_RADIUS = 30;
constexpr int MAX_RADIUS = 150;
constexpr float MIN_OPACITY = 0.1f;
constexpr float MAX_OPACITY = 0.3f;

void validateImageBuffer(const std::vector<unsigned char> &image,
                         unsigned width, unsigned height) {
  if (width == 0 || height == 0 ||
      static_cast<std::size_t>(width) >
          std::numeric_limits<std::size_t>::max() / height / 4U ||
      image.size() != static_cast<std::size_t>(width) * height * 4U) {
    throw std::invalid_argument("invalid RGBA image buffer");
  }
}
} // namespace

void generateGradient(std::vector<unsigned char> &image, unsigned width,
                      unsigned height, const std::vector<unsigned char> &color1,
                      const std::vector<unsigned char> &color2) {
  validateImageBuffer(image, width, height);
  if (color1.size() != 4 || color2.size() != 4) {
    throw std::invalid_argument("gradient colors must be RGBA values");
  }
  const float widthInv = 1.0f / static_cast<float>(width);
  const float heightInv = 1.0f / static_cast<float>(height);

  for (unsigned y = 0; y < height; y++) {
    const float factor2 = static_cast<float>(y) * heightInv;
    const size_t rowOffset = static_cast<size_t>(y) * width * 4;

    for (unsigned x = 0; x < width; x++) {
      const float factor = static_cast<float>(x) * widthInv;
      const float blend = 0.5f * (factor + factor2);
      const float oneMinusBlend = 1.0f - blend;

      const size_t idx = rowOffset + static_cast<size_t>(x) * 4;
      image[idx] = static_cast<unsigned char>(
          static_cast<float>(color1[0]) * oneMinusBlend +
          static_cast<float>(color2[0]) * blend);
      image[idx + 1] = static_cast<unsigned char>(
          static_cast<float>(color1[1]) * oneMinusBlend +
          static_cast<float>(color2[1]) * blend);
      image[idx + 2] = static_cast<unsigned char>(
          static_cast<float>(color1[2]) * oneMinusBlend +
          static_cast<float>(color2[2]) * blend);
      image[idx + 3] = static_cast<unsigned char>(
          static_cast<float>(color1[3]) * oneMinusBlend +
          static_cast<float>(color2[3]) * blend);
    }
  }
}

void addNaturalNoise(std::vector<unsigned char> &image, unsigned width,
                     unsigned height, float intensity) {
  validateImageBuffer(image, width, height);
  if (!std::isfinite(intensity) || intensity < 0.0f) {
    throw std::invalid_argument(
        "noise intensity must be finite and non-negative");
  }
  std::random_device rd;
  std::mt19937 rng(rd());
  std::normal_distribution<float> dist(0.0f, intensity);

  const size_t totalPixels = static_cast<size_t>(width) * height;
  for (size_t pixel = 0; pixel < totalPixels; pixel++) {
    const size_t idx = pixel * 4;
    const int newR = static_cast<int>(image[idx]) + static_cast<int>(dist(rng));
    const int newG =
        static_cast<int>(image[idx + 1]) + static_cast<int>(dist(rng));
    const int newB =
        static_cast<int>(image[idx + 2]) + static_cast<int>(dist(rng));

    image[idx] = static_cast<unsigned char>(std::max(0, std::min(255, newR)));
    image[idx + 1] =
        static_cast<unsigned char>(std::max(0, std::min(255, newG)));
    image[idx + 2] =
        static_cast<unsigned char>(std::max(0, std::min(255, newB)));
  }
}

void addShapes(std::vector<unsigned char> &image, unsigned width,
               unsigned height, int numShapes, std::mt19937 &rng) {
  validateImageBuffer(image, width, height);
  if (numShapes < 0)
    throw std::invalid_argument("shape count cannot be negative");
  std::uniform_int_distribution<int> xDist(0, static_cast<int>(width) - 1);
  std::uniform_int_distribution<int> yDist(0, static_cast<int>(height) - 1);
  std::uniform_int_distribution<int> radiusDist(MIN_RADIUS, MAX_RADIUS);
  std::uniform_int_distribution<int> colorDist(0, 255);
  std::uniform_real_distribution<float> opacityDist(MIN_OPACITY, MAX_OPACITY);

  for (int s = 0; s < numShapes; s++) {
    const int centerX = xDist(rng);
    const int centerY = yDist(rng);
    const int radius = radiusDist(rng);
    const unsigned char shapeColor[3] = {
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng)),
        static_cast<unsigned char>(colorDist(rng))};
    const float opacity = opacityDist(rng);

    const int minY = std::max(0, centerY - radius);
    const int maxY = std::min(static_cast<int>(height), centerY + radius);
    const int minX = std::max(0, centerX - radius);
    const int maxX = std::min(static_cast<int>(width), centerX + radius);
    const float radiusSquared = static_cast<float>(radius * radius);

    for (int y = minY; y < maxY; y++) {
      const int dy = y - centerY;
      const int dySquared = dy * dy;
      const size_t rowOffset = static_cast<size_t>(y) * width * 4;

      for (int x = minX; x < maxX; x++) {
        const int dx = x - centerX;
        const float distanceSquared = static_cast<float>(dx * dx + dySquared);

        if (distanceSquared < radiusSquared) {
          const float distance = std::sqrt(distanceSquared);
          float factor = 1.0f - (distance / static_cast<float>(radius));
          factor = factor * factor * opacity;
          const float oneMinusFactor = 1.0f - factor;

          const size_t idx = rowOffset + static_cast<size_t>(x) * 4;
          image[idx] = static_cast<unsigned char>(
              static_cast<float>(image[idx]) * oneMinusFactor +
              static_cast<float>(shapeColor[0]) * factor);
          image[idx + 1] = static_cast<unsigned char>(
              static_cast<float>(image[idx + 1]) * oneMinusFactor +
              static_cast<float>(shapeColor[1]) * factor);
          image[idx + 2] = static_cast<unsigned char>(
              static_cast<float>(image[idx + 2]) * oneMinusFactor +
              static_cast<float>(shapeColor[2]) * factor);
        }
      }
    }
  }
}
