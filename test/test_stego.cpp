#include "catch_amalgamated.hpp"
#include "image_utils.h"
#include "lodepng.h"
#include "stego.h"
#include <filesystem>
#include <fstream>

static std::string getGeneratedFilename() {
    namespace fs = std::filesystem;
    for (const auto& entry : fs::directory_iterator(".")) {
        std::string name = entry.path().filename().string();
        if (name.size() > 4 && name.substr(0, 4) == "img_" && name.substr(name.size() - 4) == ".png") {
            return name;
        }
    }
    return "";
}

TEST_CASE("Full encrypt-decrypt roundtrip", "[integration]") {
    std::string masterPass = "integration-test-master-passphrase";
    std::string password = "my-secret-password-123";

    std::string existing = getGeneratedFilename();
    while (!existing.empty()) {
        std::filesystem::remove(existing);
        existing = getGeneratedFilename();
    }

    encryptPassword(masterPass, password);

    std::string filename = getGeneratedFilename();
    REQUIRE_FALSE(filename.empty());
    REQUIRE(std::filesystem::exists(filename));

    std::string decrypted = decryptPassword(masterPass, filename);
    REQUIRE(decrypted == password);

    std::filesystem::remove(filename);
}

TEST_CASE("Decrypt with wrong passphrase produces wrong result", "[integration]") {
    std::string masterPass = "correct-master-passphrase";
    std::string wrongPass = "wrong-master-passphrase";
    std::string password = "another-secret";

    std::string existing = getGeneratedFilename();
    while (!existing.empty()) {
        std::filesystem::remove(existing);
        existing = getGeneratedFilename();
    }

    encryptPassword(masterPass, password);
    std::string filename = getGeneratedFilename();
    REQUIRE_FALSE(filename.empty());

    std::string decrypted = decryptPassword(wrongPass, filename);
    REQUIRE(decrypted != password);

    std::filesystem::remove(filename);
}

TEST_CASE("Encrypted PNG is valid and has correct dimensions", "[integration]") {
    std::string masterPass = "dimension-test-passphrase";
    std::string password = "test-password";

    std::string existing = getGeneratedFilename();
    while (!existing.empty()) {
        std::filesystem::remove(existing);
        existing = getGeneratedFilename();
    }

    encryptPassword(masterPass, password);
    std::string filename = getGeneratedFilename();
    REQUIRE_FALSE(filename.empty());

    std::vector<unsigned char> img;
    unsigned w, h;
    unsigned error = lodepng::decode(img, w, h, filename);
    REQUIRE(error == 0);
    REQUIRE(w == 1920);
    REQUIRE(h == 1080);

    std::filesystem::remove(filename);
}

TEST_CASE("Generated files are named img_<random>.png", "[integration]") {
    std::string masterPass = "naming-test";
    std::string password = "test";

    std::string existing = getGeneratedFilename();
    while (!existing.empty()) {
        std::filesystem::remove(existing);
        existing = getGeneratedFilename();
    }

    encryptPassword(masterPass, password);
    std::string filename = getGeneratedFilename();
    REQUIRE_FALSE(filename.empty());
    REQUIRE(filename.substr(0, 4) == "img_");
    REQUIRE(filename.substr(filename.size() - 4) == ".png");

    std::filesystem::remove(filename);
}

TEST_CASE("Magic signature is embedded and detected", "[integration]") {
    std::string masterPass = "magic-test-passphrase";
    std::string password = "test";

    std::string existing = getGeneratedFilename();
    while (!existing.empty()) {
        std::filesystem::remove(existing);
        existing = getGeneratedFilename();
    }

    encryptPassword(masterPass, password);
    std::string filename = getGeneratedFilename();
    REQUIRE_FALSE(filename.empty());

    std::vector<unsigned char> img;
    unsigned w, h;
    unsigned error = lodepng::decode(img, w, h, filename);
    REQUIRE(error == 0);
    REQUIRE(checkMagic(img, w, h));

    std::filesystem::remove(filename);
}

TEST_CASE("Non-PassPix PNG does not match magic", "[integration]") {
    std::vector<unsigned char> img = {255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255};
    REQUIRE_FALSE(checkMagic(img, 2, 2));
}
