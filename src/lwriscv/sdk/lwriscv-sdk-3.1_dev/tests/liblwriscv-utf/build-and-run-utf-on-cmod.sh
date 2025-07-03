#!/bin/bash

# Builds UTF with libfsp and runs the tests on cmodel
# Prerequisites:
#   export the following environment variables:
#     LW_SOURCE should point to your source branch in P4 (eg: chips_a)
#     CMOD should point to your copy of the cmodel.
#          Otherwise, it will use the default one at /home/ip/ap/mobile/peregrine2.0/cmod/53865352/bin
#
# Arguments:
#   ./build-and-run-utf-cmod.sh
#       Build UTF with liblwriscv and run it on cmod
#
#   ./build-and-run-utf-cmod.sh FSPTOP=/path/to/fsp/tree
#       Build UTF with libfsp and run it on cmod

if [ -z "${CMOD}" ]; then
    CMOD=/home/ip/ap/mobile/peregrine2.0/cmod/53865352/bin
    echo "using default CMOD: ${CMOD}"
fi

if [ -z "${P4ROOT}" ] ; then
    echo "P4ROOT not defined"
    exit 1
fi

if [ ! -d "${P4ROOT}/sw/dev/gpu_drv/chips_a/tools/UTF" ] ; then
    echo "${P4ROOT}/sw/dev/gpu_drv/chips_a/tools/UTF doesn't exist. Add it to your clientspec."
    exit 1
fi

if [ ! -d "${CMOD}" ] ; then
    echo "${CMOD} doesn't exist. mount it (Or run on LSF) (Or export a CMOD elw variable to point to your cmodel)."
    exit 1
fi

make -C ${P4ROOT}/sw/dev/gpu_drv/chips_a/tools/UTF/ UTFCFG_PROFILE=gh100-fsp-cmodel $1
if [ $? -ne 0 ]; then
    echo "UTF build failed!"
    exit 1
fi

pushd ${CMOD}
export LD_LIBRARY_PATH=`pwd`:$LD_LIBRARY_PATH
./run_fastsim --load_hex ${P4ROOT}/sw/dev/gpu_drv/chips_a/tools/UTF/_out/Linux_amd64/riscv/utf-tests-gh100-fsp-cmodel.hex 0x7400000080000000 --prgn_bringup riscv_bringup --start_riscv_at 0x100000
popd
