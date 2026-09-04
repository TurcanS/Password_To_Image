# PassPix

**Encrypt a secret and store the authenticated ciphertext inside a generated PNG image.**

PassPix creates an abstract 1920×1080 image, derives an encryption key from a master
passphrase with Argon2id, encrypts the secret with XChaCha20-Poly1305, and embeds the
result in the RGB least-significant bits of the PNG.

> **Beta software:** review and test the source before trusting important data. Keep a
> separate backup of every secret. PassPix images are recognizable as PassPix containers;
> steganography is not a security boundary.

## Security update

The V2 format removes the unauthenticated SHA-256 password verifier used by V1. That
legacy verifier allowed inexpensive offline guessing of the stored password without the
master passphrase. New files are always written as V2. Existing V1 files can still be
read through a bounds-checked compatibility path and should be re-encrypted as V2.

Published v0.3.x binaries create the legacy format and should not be used for new secrets.

## How it works

```text
master passphrase + secret
          |
          v
Argon2id (explicit per-file parameters and 128-bit random salt)
          |
          v
XChaCha20-Poly1305 (192-bit random nonce)
          |
          v
versioned header + authenticated ciphertext
          |
          v
RGB least-significant-bit embedding with triple redundancy
          |
          v
opaque 1920x1080 PNG
```

The V2 header contains the format version, KDF identifier and parameters, salt, nonce,
and ciphertext length. The complete header is authenticated as AEAD associated data.
No hash or verifier derived from the plaintext secret is stored.

## Build

### Dependencies

- A C++17 compiler
- libsodium 1.0.18 or newer
- GNU Make, or CMake 3.16+ and pkg-config

On Debian or Ubuntu:

```bash
sudo apt install build-essential cmake pkg-config libsodium-dev
git clone https://github.com/TurcanS/PassPix.git
cd PassPix
make
./passpix
```

With CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows, use an MSYS2 UCRT64 terminal:

```bash
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-libsodium
make
./passpix.exe
```

## Usage

Run `./passpix` and select:

1. **Encrypt Password** — enter the master passphrase twice, then the secret.
2. **Decrypt Password** — choose a discovered PassPix PNG and enter its passphrase.
3. **Exit**.

Interactive secret entry is hidden when the terminal supports it. After successful
decryption, PassPix asks for explicit confirmation before displaying the secret.
Generated filenames use the form `img_<20 random characters>.png`; existing files are
never intentionally overwritten. Secrets are limited to 64 KiB.

## Testing

```bash
# Unit, integration, legacy compatibility, and CLI smoke tests
make test

# AddressSanitizer and UndefinedBehaviorSanitizer
make clean
make SANITIZE=1
make test SANITIZE=1
```

CI runs Linux, Windows, smoke, and Linux sanitizer coverage. Tagged release artifacts
are tested before upload.

## Technical details

| Component | V2 choice |
|---|---|
| Encryption | XChaCha20-Poly1305 AEAD |
| Key derivation | Argon2id, libsodium moderate preset (currently 3 operations / 256 MiB) |
| Salt / nonce | 16 raw random bytes / 24 raw random bytes |
| Header integrity | Entire versioned header supplied as AEAD associated data |
| Embedding | RGB LSBs, three copies per bit with majority recovery |
| Image | 1920×1080 RGBA PNG, alpha fixed at 255 |
| Input limits | 64 MiB encoded PNG, 16 megapixels decoded, 64 KiB secret |
| Legacy support | Read-only V1 compatibility with strict bounds checking |

KDF parameters are read from the authenticated header, but defensive upper limits are
applied before key derivation to prevent malicious images from requesting excessive CPU
or memory.

## Security notes

- Use a long, unique master passphrase. The encrypted payload still permits offline
  passphrase guessing; Argon2id only makes each attempt more expensive.
- The `PPX2` format marker is intentionally identifiable. Do not rely on the image to
  conceal that encrypted data exists.
- Lossy conversion, resizing, cropping, or editing can destroy embedded bits. Preserve
  the original PNG bytes.
- PassPix attempts to lock derived keys and interactive secret strings in memory and
  wipes managed sensitive buffers on destruction. Memory locking can be unavailable due
  to operating-system policy, and C++/terminal/runtime copies cannot be comprehensively
  guaranteed absent.
- A correct AEAD tag is the only plaintext-integrity check. Wrong passphrases, altered
  headers, and altered ciphertext are rejected with the same generic failure.
- The bundled LodePNG source reports version `20241228`; review dependency updates as
  part of each release.

## Project structure

```text
src/
  crypto_utils.*    libsodium initialization, Argon2id, AEAD, secure buffers
  image_gen.*       abstract cover-image generation
  image_utils.*     validated PNG I/O and encrypt/decrypt workflows
  stego.*           V2 format plus bounds-checked V1 reader
  terminal_utils.*  hidden input and sensitive interactive strings
  main.cpp          CLI
Include/lodepng.*   bundled PNG codec
test/               unit, integration, compatibility, and smoke tests
```

## Pre-built binaries

Release artifacts and their SHA-256 hashes are published on the
[Releases](https://github.com/TurcanS/PassPix/releases) page. Verify hashes from the
release that you download. Do not use a v0.3.x binary to create new secrets.

## License

MIT — see [LICENSE](LICENSE).
