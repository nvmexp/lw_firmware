#!/bin/bash

echo   "================================================================="
printf "=      Clearing %-45s   =\n" "libos-multipart-gh100-fsp"
echo   "================================================================="

cd libos-multipart-gh100-fsp
make USE_PREBUILT_SDK=false clean
rm -rf sdk-gfw-gh100-fsp-fw.*

echo "================================================================="
echo " Clear script has finished. "
echo "================================================================="
