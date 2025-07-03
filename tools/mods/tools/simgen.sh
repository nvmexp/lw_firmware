#!/bin/bash

edids=`cat edid.js | grep \"..DS_[0-9]*x[0-9]*\" | awk '{print $1}' | sed -e s/\"//g`

for i in $edids; do
    ./mods gpugen.js -simulate_all_dfps $i $*
done
