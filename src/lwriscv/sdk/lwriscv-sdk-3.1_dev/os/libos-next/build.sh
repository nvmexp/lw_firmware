#!/bin/sh
cd profiles
make clean
make || exit
cd ..

cd kernel
make clean
make || exit
cd ..

cd tools/simulator
make clean
make  || exit
cd ../..

cd tools/libosmc
make clean
make  || exit
cd ../..

cd tests
make clean
make  || exit 
