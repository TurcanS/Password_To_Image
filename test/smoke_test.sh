#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./passpix}"
PASS="smoke-test-master"
SECRET="smoke-secret-123"
TESTDIR=$(mktemp -d)
trap 'rm -rf -- "$TESTDIR"' EXIT

if [[ "$BINARY" = /* ]]; then
    SOURCE_BINARY="$BINARY"
else
    SOURCE_BINARY="$(pwd)/$BINARY"
fi

cd "$TESTDIR"
if [[ "$SOURCE_BINARY" = *.exe ]]; then
    LOCAL_BINARY=./passpix.exe
else
    LOCAL_BINARY=./passpix
fi
cp "$SOURCE_BINARY" "$LOCAL_BINARY"
BIN="$LOCAL_BINARY"

# Test encrypt
ENCRYPT_OUT=$(printf '1\n%s\n%s\n%s\n3\n' "$PASS" "$PASS" "$SECRET" | "$BIN" 2>&1)
echo "$ENCRYPT_OUT" | grep -q "Password encrypted"
echo "PASS: encryption"

# Get generated filename
ENC_FILE=$(ls img_*.png 2>/dev/null | head -1)
if [ -z "$ENC_FILE" ]; then
    echo "FAIL: no encrypted file generated"
    exit 1
fi
echo "PASS: file generated: $ENC_FILE"

# Test decrypt
RESULT=$(printf '2\n1\n%s\ny\n3\n' "$PASS" | "$BIN" 2>&1)
echo "$RESULT" | grep -q "$SECRET"
echo "PASS: decryption returned correct password"

echo "All smoke tests passed."
