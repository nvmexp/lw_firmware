#!/usr/bin/sh

if [ -f "./tmpsign.sh" ]
then 
    rm -f tmpsign.sh
fi
make clean
make clobber
make
if [ -f "./tmpsign.sh" ]
then 
sh tmpsign.sh
rm -f tmpsign.sh
fi
