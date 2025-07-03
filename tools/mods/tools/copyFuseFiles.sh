#!/bin/bash

BoardHalDir=${BOARD_HAL_DIR-$P4ROOT/sw/mods/boardhal}
echo "Copying files from \"$BoardHalDir\""

if [ -d $BoardHalDir ] ; then
    echo "Copying e821B00.js (Fuse Daughter Card)"
    cp $BoardHalDir/devices/e812B00.js ./

    # R384
    echo "Copying e3600.js (LwSwitch)"
    cp $BoardHalDir/boards/e3600.js ./

    echo "Copying e3620.js (GV100)"
    cp $BoardHalDir/boards/e3620.js ./
else
    echo "Missing BoardHal Directory!"
    echo "Please sync //sw/mods/boardhal/..."
fi

