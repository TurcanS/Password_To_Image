# Password_To_Image (BETA)
**USE AT YOUR OWN RISK**

Master password: "admin"

## Overview
Password_To_Image is a C++ project designed to transform passwords into images. This can be used for various purposes such as secure password storage and visual encoding.

## Prerequisites

### Required Dependencies
- **C++ Compiler**: g++ with C++11 support or newer
- **OpenSSL**: Version 1.1.0 or newer (3.0+ recommended)
  - Linux: `libssl-dev` and `libcrypto-dev`
  - Windows (MinGW): OpenSSL libraries for MinGW

### Optional Build Tools
- **CMake**: Version 3.10 or newer (for CMake builds)
- **Make**: GNU Make or MinGW32-make (for Makefile builds)

## Building the Project

### Method 1: Using Make (Recommended for Windows with MinGW)

The project includes an optimized Makefile that works with both native builds on Linux and cross-platform builds on Windows using MinGW.

#### On Windows with MinGW:
```bash
# Install MinGW-w64 and ensure OpenSSL is available
# Add MinGW bin directory to PATH

# Build the project
mingw32-make

# Clean build artifacts
mingw32-make clean

# Rebuild from scratch
mingw32-make rebuild

# Build with debug symbols
mingw32-make DEBUG=1
```

#### On Linux:
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential libssl-dev

# Build the project
make

# Clean build artifacts
make clean

# Rebuild from scratch
make rebuild

# Build with debug symbols
make DEBUG=1

# Install to /usr/local/bin (requires sudo)
sudo make install
```

#### Build Options:
- `make` - Build optimized release version with -O3 and LTO
- `make DEBUG=1` - Build with debug symbols and no optimizations
- `make clean` - Remove all build artifacts
- `make rebuild` - Clean and build from scratch
- `make help` - Display help information

### Method 2: Using CMake

CMake provides a cross-platform build configuration.

#### On Windows:
```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

#### On Linux:
```bash
mkdir build
cd build
cmake ..
make
```

## Performance Optimizations

The project includes several performance optimizations (detailed in PERFORMANCE_IMPROVEMENTS.md):
- Link-Time Optimization (LTO) enabled by default
- Native CPU optimization (-march=native -mtune=native)
- Optimized gradient generation and image processing
- Efficient HMAC generation using OpenSSL
- Reduced memory allocations and improved cache locality

Typical performance improvement: **30-40% reduction in execution time** compared to non-optimized builds.

## Running the Program

After building, run the executable:

```bash
# On Linux
./enc_dec

# On Windows
enc_dec.exe
```

Enter the master password ("admin") when prompted to access the program features.

## Build Output

- **Executable**: `enc_dec` (Linux) or `enc_dec.exe` (Windows)
- **Build Directory**: `build/` (contains object files and intermediate artifacts)
- **Optimizations**: -O3, native CPU tuning, Link-Time Optimization (LTO)

## Troubleshooting

### OpenSSL Not Found
- **Linux**: Install OpenSSL development packages: `sudo apt-get install libssl-dev`
- **Windows**: Ensure OpenSSL is installed and library paths are configured for MinGW

### Compilation Errors
- Verify you have C++11 support: `g++ --version` should show GCC 4.8.1 or newer
- Ensure all dependencies are installed correctly
- Try building with DEBUG=1 for more verbose output

### MinGW Issues on Windows
- Ensure MinGW bin directory is in your PATH
- Use `mingw32-make` instead of `make`
- Verify OpenSSL libraries are compatible with MinGW

## Project Structure

```
Password_To_Image/
├── src/                    # Source files
│   ├── main.cpp           # Main program entry
│   ├── crypto_utils.cpp   # Cryptographic functions
│   ├── crypto_utils.h
│   ├── image_utils.cpp    # Image generation/manipulation
│   └── image_utils.h
├── Include/               # Third-party libraries
│   ├── lodepng.cpp       # PNG encoding/decoding
│   └── lodepng.h
├── Makefile              # Optimized Makefile for Make builds
├── CMakeLists.txt        # CMake configuration
└── README.md             # This file
```

## License

See LICENSE file for details.
