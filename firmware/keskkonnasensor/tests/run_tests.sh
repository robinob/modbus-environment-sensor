#!/usr/bin/env bash
# Build and run all PC unit tests.
#
# Usage (from Git Bash, in any folder):
#   firmware/keskkonnasensor/tests/run_tests.sh
#
# Needs gcc on PATH (MSYS2: C:\msys64\ucrt64\bin).

# Stop at the first command that fails, instead of carrying on.
set -e

# Work from the firmware folder, wherever the script was started from.
# $0 is the path of this script; its parent's parent is the firmware folder.
cd "$(dirname "$0")/.."

CFLAGS="-std=c11 -Wall -Wextra -Werror -IApp -IApp/modbus"
mkdir -p build/tests

echo "== CRC =="
gcc $CFLAGS -o build/tests/test_modbus_crc.exe \
    tests/test_modbus_crc.c App/modbus/modbus_crc.c
./build/tests/test_modbus_crc.exe

echo "== Frame parser =="
gcc $CFLAGS -o build/tests/test_modbus_rtu.exe \
    tests/test_modbus_rtu.c App/modbus/modbus_rtu.c App/modbus/modbus_crc.c
./build/tests/test_modbus_rtu.exe

echo "== Frame assembler =="
gcc $CFLAGS -o build/tests/test_modbus_rx.exe \
    tests/test_modbus_rx.c App/modbus/modbus_rx.c
./build/tests/test_modbus_rx.exe

echo "== Register map =="
gcc $CFLAGS -o build/tests/test_regmap.exe \
    tests/test_regmap.c App/regmap.c App/modbus/modbus_rtu.c App/modbus/modbus_crc.c
./build/tests/test_regmap.exe

echo "All tests passed."
