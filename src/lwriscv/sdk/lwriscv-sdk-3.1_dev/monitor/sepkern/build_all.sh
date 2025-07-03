#!/bin/bash

# supported chips
chips="orin ga10x gh100 ls10 ad10x"

# supported engines on each chip
orin_engines="gsp lwdec pmu tsec"
ga10x_engines="pmu gsp sec"
gh100_engines="pmu gsp sec fsp lwdec"
ls10_engines="soe fsp"
ad10x_engines="pmu gsp sec"

for chip in ${chips} ; do
    eval engine_list=${chip}_engines
    for engine in ${!engine_list} ; do
        make CHIP=${chip} ENGINE=${engine}
        if [ $? -ne 0 ] ; then
            echo "Build for chip:${chip} engine:${engine} failed!"
            exit -1;
        fi
    done
done
