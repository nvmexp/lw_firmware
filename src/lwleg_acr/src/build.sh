#!/usr/bin/sh

rm -f tmpsign.sh
make clean
make clobber
make
sh tmpsign.sh
rm -f tmpsign.sh
