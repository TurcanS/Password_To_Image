# PassPix

**Encrypt passwords and hide them inside natural-looking PNG images.**

PassPix generates a unique abstract gradient image, encrypts your password with XChaCha20-Poly1305, then steganographically embeds the ciphertext into the image's pixels. The resulting PNG looks like a normal abstract photo — only someone with the correct master passphrase can extract the secret back out.

> ⚠️ **Beta software.** Use at your own risk. Review the source before trusting sensitive data.

---

## How It Works

```
You enter a master passphrase + password to store
       ↓
    PassPix generates a random 1920×1080 gradient image
    with soft shapes and natural noise
       ↓
    Your password is encrypted with XChaCha20-Poly1305
    (key derived from your master passphrase via Argon2id with 64 MiB memory)
       ↓
    The ciphertext + salt + nonce are
    embedded at pseudo-random pixel positions
       ↓
    Output: a .png file that looks like abstract art
```

To recover the password, you provide the same master passphrase — the correct key is re-derived, and the ciphertext is extracted and decrypted.

---

## Pre-built Binaries

Pre-compiled binaries are available on the [Releases](https://github.com/TurcanS/PassPix/releases) page. To verify integrity, check that your downloaded file matches the SHA-256 hash below:

> **Note:** These hashes are for the CI-built release binaries compiled with `-march=x86-64-v2`. Locally compiled binaries will have different hashes due to `-march=native` CPU-specific optimizations.

<!-- RELEASE_TABLE -->
| Version | Platform | Binary | SHA-256 Hash |
|---|---|---|---|
| <!-- VERSION -->v0.3.0<!-- /VERSION --> | Linux | `passpix` | <!-- HASH_LINUX -->`f1745d715ef787408370cf19a0433ff835da2176fc2fc257ebc9b3ed8d337d33`<!-- /HASH_LINUX --> |
| <!-- VERSION -->v0.3.0<!-- /VERSION --> | Windows | `passpix.exe` | <!-- HASH_WIN -->`2feb96da5e56f404eb5023440060fd7c42abe45c14cfeb9ce95a3e21747fbd8c`<!-- /HASH_WIN --> |

---

## Compile from Source

### Dependencies

- **C++ compiler** — g++ with C++17 support or newer
- **OpenSSL** — 1.1.0 or newer (for SHA-256 only)
  - Linux: `libssl-dev`
  - Windows (MSYS2): `mingw-w64-ucrt-x86_64-openssl`
- **libsodium** — 1.0.18 or newer (Argon2id + XChaCha20-Poly1305)
  - Linux: `libsodium-dev`
  - Windows (MSYS2): `mingw-w64-ucrt-x86_64-libsodium`
- **Make** or **CMake** (3.10+)

### Build

#### Linux

```bash
# Install dependencies
sudo apt install build-essential libssl-dev libsodium-dev

# Clone and build
git clone https://github.com/TurcanS/PassPix.git
cd PassPix
make

# Run
./passpix

# Run tests
make test
```

#### Windows (MSYS2)

```bash
# Install MSYS2 from https://www.msys2.org/

# Open UCRT64 terminal and install dependencies
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-libsodium

# Clone and build
git clone https://github.com/TurcanS/PassPix.git
cd PassPix
make

# Run
./passpix

# Run tests
make test
```

#### Linux with CMake

```bash
sudo apt install build-essential libssl-dev libsodium-dev cmake
git clone https://github.com/TurcanS/PassPix.git
cd PassPix
mkdir build && cd build
cmake .. && make
ctest
```

### Usage

```bash
# Run the program
./passpix

# Follow the interactive menu:
#   1. Encrypt a password → enter master passphrase (twice) + password to store
#   2. Decrypt a password → enter master passphrase + select a file

# Output files are named enc_<random>.png
```

### Testing

```bash
# Run all tests (unit + integration + smoke)
make test

# Or with CMake
cd build && cmake .. && make && ctest
```

---

## Technical Details

| Component | Choice |
|---|---|
| **Encryption** | XChaCha20-Poly1305 AEAD (authenticated encryption) |
| **Key derivation** | Argon2id, 64 MiB memory, 3 iterations |
| **Integrity** | Poly1305 MAC (built into the AEAD cipher) |
| **Steganography** | Pseudorandom byte embedding with 2× redundancy + frequency recovery |
| **Image format** | PNG via [LodePNG](https://lodev.org/lodepng/) |
| **Image size** | 1920×1080 RGBA (8-bit per channel) |
| **Memory safety** | Key material locked with `sodium_mlock`, zeroed after use |

### Image embedding layout

Metadata (salt, nonce, password hash) is stored at multiple redundant locations within the first/last few hundred pixels. The encrypted payload is shuffled across the remaining pixel data using a deterministic seed derived from the key. Redundancy and majority-vote recovery protect against pixel corruption.

---

## Project Structure

```
PassPix/
├── src/
│   ├── main.cpp            # CLI menu and user flow
│   ├── crypto_utils.h/.cpp  # Argon2id, XChaCha20-Poly1305, SHA-256
│   ├── image_gen.h/.cpp     # Gradient, shapes, noise generation
│   ├── stego.h/.cpp         # Steganographic embed/extract with redundancy
│   └── image_utils.h/.cpp   # Legacy compatibility wrapper
├── Include/
│   ├── lodepng.cpp          # PNG encoding/decoding (bundled)
│   └── lodepng.h
├── test/
│   ├── test_crypto.cpp      # Unit tests for crypto functions
│   ├── test_stego.cpp       # Integration tests for encrypt/decrypt roundtrip
│   └── smoke_test.sh        # CLI smoke test
├── Makefile                 # Make build
├── CMakeLists.txt           # CMake build
├── LICENSE                  # MIT license
└── README.md
```

---

## Security Notes

- **Master passphrase strength matters.** Use a long, unique passphrase — it's the single key to all stored passwords. Argon2id with 64 MiB memory makes brute-force attacks expensive, but a weak passphrase can still be cracked.
- **Key material is protected in memory.** Derived keys and passphrases are locked from swapping (`sodium_mlock`) and zeroed after use (`sodium_memzero`).
- **The generated images are not encrypted containers.** The pixel data looks random but the embedding pattern is deterministic. Security relies on the cryptographic key, not obscurity.
- **Verify pre-built binaries.** If you use pre-compiled releases, verify the SHA-256 hash matches the table above before trusting the binary.

---

## License

MIT — see [LICENSE](LICENSE).
