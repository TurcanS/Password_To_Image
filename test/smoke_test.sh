#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./passpix}"
PASS="smoke-test-master"
SECRET="smoke-secret-123"
TESTDIR=$(mktemp -d)
trap "rm -rf $TESTDIR" EXIT

cd "$TESTDIR"
cp "$OLDPWD/$BINARY" ./passpix 2>/dev/null || cp "$OLDPWD/$BINARY" ./passpix
BIN=./passpix

# Test encrypt
ENCRYPT_OUT=$(echo -e "1\n$PASS\n$PASS\n$SECRET\n3\n" | $BIN 2>&1)
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
RESULT=$(echo -e "2\n1\n$PASS\n3\n" | $BIN 2>&1)
echo "$RESULT" | grep -q "$SECRET"
echo "PASS: decryption returned correct password"

echo "All smoke tests passed."
