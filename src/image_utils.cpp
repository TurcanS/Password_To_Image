#include "image_utils.h"

#include "../Include/lodepng.h"
#include "crypto_utils.h"
#include "image_gen.h"
#include "stego.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <limits>
#include <random>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t FILENAME_RANDOM_LENGTH = 20;
constexpr std::uint64_t MAX_ACCEPTED_KDF_OPS = 4;
constexpr std::uint64_t MAX_ACCEPTED_KDF_MEMORY = 512ULL * 1024ULL * 1024ULL;
constexpr std::uintmax_t MAX_PNG_FILE_SIZE = 64ULL * 1024ULL * 1024ULL;
constexpr std::size_t MAX_IMAGE_PIXELS = 16ULL * 1024ULL * 1024ULL;

bool decodeImage(const std::string &filename, std::vector<unsigned char> &image,
                 unsigned &width, unsigned &height, std::string &error) {
  namespace fs = std::filesystem;
  std::error_code filesystemError;
  if (!fs::is_regular_file(filename, filesystemError)) {
    error = "input is not a regular file";
    return false;
  }
  const std::uintmax_t fileSize = fs::file_size(filename, filesystemError);
  if (filesystemError || fileSize == 0 || fileSize > MAX_PNG_FILE_SIZE) {
    error = "PNG file size is invalid or exceeds the 64 MiB limit";
    return false;
  }

  std::vector<unsigned char> png;
  unsigned status = lodepng::load_file(png, filename);
  if (status != 0) {
    error = std::string("cannot read PNG: ") + lodepng_error_text(status);
    return false;
  }
  lodepng::State state;
  status = lodepng_inspect(&width, &height, &state, png.data(), png.size());
  if (status != 0) {
    error = std::string("invalid PNG header: ") + lodepng_error_text(status);
    return false;
  }
  if (width == 0 || height == 0 ||
      static_cast<std::size_t>(width) > MAX_IMAGE_PIXELS / height) {
    error = "PNG dimensions exceed the safe decoding limit";
    return false;
  }
  status = lodepng::decode(image, width, height, png);
  if (status != 0) {
    error = std::string("cannot decode PNG: ") + lodepng_error_text(status);
    return false;
  }
  return validateRgbaImage(image, width, height, error);
}

std::string chooseOutputFilename(const std::string &requested,
                                 std::string &error) {
  namespace fs = std::filesystem;
  std::error_code filesystemError;
  if (!requested.empty()) {
    if (fs::path(requested).extension() != ".png") {
      error = "output filename must use the .png extension";
      return {};
    }
    if (fs::exists(requested, filesystemError)) {
      error = "refusing to overwrite an existing output file";
      return {};
    }
    if (filesystemError) {
      error = "cannot check the requested output path";
      return {};
    }
    return requested;
  }
  for (unsigned attempt = 0; attempt < 100; ++attempt) {
    std::string candidate =
        "img_" + generateRandomString(FILENAME_RANDOM_LENGTH) + ".png";
    if (!fs::exists(candidate, filesystemError) && !filesystemError)
      return candidate;
    filesystemError.clear();
  }
  error = "could not allocate a unique output filename";
  return {};
}

bool saveExclusive(const std::string &filename,
                   const std::vector<unsigned char> &png, std::string &error) {
#ifdef _WIN32
  const int descriptor =
      _open(filename.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
            _S_IREAD | _S_IWRITE);
#else
  const int descriptor =
      open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
  if (descriptor < 0) {
    error = std::string("cannot create output file exclusively: ") +
            std::strerror(errno);
    return false;
  }

  std::size_t total = 0;
  bool success = true;
  while (total < png.size()) {
#ifdef _WIN32
    const unsigned int chunk = static_cast<unsigned int>(std::min<std::size_t>(
        png.size() - total, std::numeric_limits<unsigned int>::max()));
    const int written = _write(descriptor, png.data() + total, chunk);
#else
    const ssize_t written =
        write(descriptor, png.data() + total, png.size() - total);
#endif
    if (written < 0) {
      if (errno == EINTR)
        continue;
      success = false;
      error = std::string("cannot write output file: ") + std::strerror(errno);
      break;
    }
    if (written == 0) {
      success = false;
      error = "cannot write complete output file";
      break;
    }
    total += static_cast<std::size_t>(written);
  }
#ifdef _WIN32
  if (_close(descriptor) != 0 && success) {
#else
  if (close(descriptor) != 0 && success) {
#endif
    success = false;
    error = std::string("cannot finalize output file: ") + std::strerror(errno);
  }
  if (!success) {
    std::error_code ignored;
    std::filesystem::remove(filename, ignored);
  }
  return success;
}

bool decryptV2(const std::vector<unsigned char> &image, unsigned width,
               unsigned height, const std::string &masterPassphrase,
               std::string &password, std::string &error) {
  PayloadHeader header;
  std::vector<unsigned char> ciphertext;
  if (!extractPayload(image, width, height, header, ciphertext, error))
    return false;
  if (header.kdf.opsLimit == 0 || header.kdf.opsLimit > MAX_ACCEPTED_KDF_OPS ||
      header.kdf.memLimit == 0 ||
      header.kdf.memLimit > MAX_ACCEPTED_KDF_MEMORY) {
    error = "image requests unsupported key derivation limits";
    return false;
  }
  SecureBuffer key = deriveKey(masterPassphrase, header.salt, header.kdf);
  const std::vector<unsigned char> associatedData = serializeHeader(header);
  if (!decrypt(ciphertext, key, header.nonce, associatedData, password)) {
    error = "incorrect passphrase or corrupted image";
    return false;
  }
  return true;
}

bool decryptLegacy(const std::vector<unsigned char> &image, unsigned width,
                   unsigned height, const std::string &masterPassphrase,
                   std::string &password, std::string &error) {
  std::vector<unsigned char> salt;
  if (!extractLegacySalt(image, width, height, salt, error))
    return false;
  SecureBuffer key = deriveKey(masterPassphrase, salt, legacyKdfParams());
  std::vector<unsigned char> blob;
  if (!extractLegacyCiphertext(image, width, height, key, salt, blob, error))
    return false;
  if (blob.size() < NONCE_SIZE + MAC_SIZE) {
    error = "legacy encrypted payload is truncated";
    return false;
  }
  std::vector<unsigned char> nonce(blob.begin(), blob.begin() + NONCE_SIZE);
  std::vector<unsigned char> ciphertext(blob.begin() + NONCE_SIZE, blob.end());
  if (!decrypt(ciphertext, key, nonce, {}, password)) {
    error = "incorrect passphrase or corrupted legacy image";
    return false;
  }
  return true;
}
} // namespace

bool encryptPassword(const std::string &masterPassphrase,
                     const std::string &password, std::string &outputFilename,
                     std::string &error, const std::string &requestedFilename) {
  outputFilename.clear();
  error.clear();
  if (masterPassphrase.empty()) {
    error = "master passphrase cannot be empty";
    return false;
  }
  if (password.empty()) {
    error = "password cannot be empty";
    return false;
  }
  if (password.size() > MAX_SECRET_SIZE) {
    error = "password exceeds the 64 KiB limit";
    return false;
  }

  try {
    const std::string chosenFilename =
        chooseOutputFilename(requestedFilename, error);
    if (chosenFilename.empty())
      return false;
    const unsigned width = IMAGE_WIDTH;
    const unsigned height = IMAGE_HEIGHT;
    std::vector<unsigned char> image(static_cast<std::size_t>(width) * height *
                                     4U);
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int> colorDistribution(0, 255);
    const std::vector<unsigned char> color1{
        static_cast<unsigned char>(colorDistribution(generator)),
        static_cast<unsigned char>(colorDistribution(generator)),
        static_cast<unsigned char>(colorDistribution(generator)), 255};
    const std::vector<unsigned char> color2{
        static_cast<unsigned char>(colorDistribution(generator)),
        static_cast<unsigned char>(colorDistribution(generator)),
        static_cast<unsigned char>(colorDistribution(generator)), 255};
    generateGradient(image, width, height, color1, color2);
    std::uniform_int_distribution<int> shapeDistribution(10, 25);
    addShapes(image, width, height, shapeDistribution(generator), generator);
    addNaturalNoise(image, width, height, 10.0f);
    for (std::size_t alpha = 3; alpha < image.size(); alpha += 4)
      image[alpha] = 255;

    PayloadHeader header;
    header.kdf = defaultKdfParams();
    header.salt = randomBytes(SALT_SIZE);
    header.nonce = randomBytes(NONCE_SIZE);
    header.ciphertextLength =
        static_cast<std::uint32_t>(password.size() + MAC_SIZE);
    const std::vector<unsigned char> associatedData = serializeHeader(header);
    SecureBuffer key = deriveKey(masterPassphrase, header.salt, header.kdf);
    const std::vector<unsigned char> ciphertext =
        encrypt(password, key, header.nonce, associatedData);
    if (!embedPayload(image, width, height, header, ciphertext, error))
      return false;

    std::vector<unsigned char> png;
    const unsigned encodingStatus = lodepng::encode(png, image, width, height);
    if (encodingStatus != 0) {
      error = std::string("cannot encode output PNG: ") +
              lodepng_error_text(encodingStatus);
      return false;
    }
    if (!saveExclusive(chosenFilename, png, error)) {
      return false;
    }
    outputFilename = chosenFilename;
    return true;
  } catch (const std::exception &exception) {
    error = exception.what();
    return false;
  }
}

bool decryptPassword(const std::string &masterPassphrase,
                     const std::string &filename, std::string &password,
                     std::string &error) {
  password.clear();
  error.clear();
  if (masterPassphrase.empty()) {
    error = "master passphrase cannot be empty";
    return false;
  }
  try {
    std::vector<unsigned char> image;
    unsigned width = 0;
    unsigned height = 0;
    if (!decodeImage(filename, image, width, height, error))
      return false;
    switch (detectFormat(image, width, height)) {
    case PassPixFormat::V2:
      return decryptV2(image, width, height, masterPassphrase, password, error);
    case PassPixFormat::LegacyV1:
      return decryptLegacy(image, width, height, masterPassphrase, password,
                           error);
    case PassPixFormat::None:
      error = "file is not a supported PassPix image";
      return false;
    }
  } catch (const std::exception &exception) {
    error = exception.what();
    return false;
  }
  error = "unrecognized PassPix format";
  return false;
}

std::vector<std::string> listEncFiles() {
  namespace fs = std::filesystem;
  std::vector<std::string> files;
  std::error_code error;
  for (const auto &entry : fs::directory_iterator(".", error)) {
    if (error)
      break;
    if (!entry.is_regular_file(error) || error) {
      error.clear();
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (entry.path().extension() != ".png")
      continue;
    std::vector<unsigned char> image;
    unsigned width = 0;
    unsigned height = 0;
    std::string decodeError;
    if (decodeImage(filename, image, width, height, decodeError) &&
        checkMagic(image, width, height))
      files.push_back(filename);
  }
  std::sort(files.begin(), files.end());
  return files;
}
