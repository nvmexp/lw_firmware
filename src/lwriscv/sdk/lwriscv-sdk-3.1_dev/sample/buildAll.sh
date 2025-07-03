#!/bin/sh
set -e
./build-liblwriscv-samples.sh
./build-sepkern-samples.sh
./build-libos-samples.sh
