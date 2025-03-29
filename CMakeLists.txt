cmake_minimum_required(VERSION 3.10)
project(enc_dec)

set(CMAKE_CXX_STANDARD 11)

# Include OpenSSL
find_package(OpenSSL REQUIRED)

# Add source files
set(SOURCES
    src/main.cpp
    src/crypto_utils.cpp
    src/image_utils.cpp
    Include/lodepng.cpp
)

# Define the executable
add_executable(enc_dec ${SOURCES})

# Link OpenSSL libraries
target_link_libraries(enc_dec OpenSSL::SSL OpenSSL::Crypto)