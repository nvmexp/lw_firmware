#!/bin/bash

CHIPS="ampere/ga102 t23x/t234 hopper/gh100"
ENGINES="pmu sec lwdec gsp"
GRP="MMODE RISCV_CTL PIC TIMER HOSTIF DMA PMB DIO KEY  DEBUG SHA KMEM BROM ROM_PATCH IOPMP NOACCESS SCP FBIF FALCON_ONLY PRGN_CTL SCRATCH_GROUP0 SCRATCH_GROUP1 SCRATCH_GROUP2 SCRATCH_GROUP3 PLM HUB_DIO"

HWREF_DIR="${P4ROOT}/sw/resman/manuals"
OUTPUT_DIR="${P4ROOT}/sw/lwriscv/main/tools/device-map-analyzer/db"

mkdir -p ${OUTPUT_DIR}

for chip in ${CHIPS}; do
    for engine in ${ENGINES}; do
        engineFile="${HWREF_DIR}/${chip}/dev_${engine}_prgnlcl.h"
        if [ -r "${engineFile}" ]; then
            for g in ${GRP}; do
                    outputFile="${OUTPUT_DIR}/${chip//\//-}-${engine}-${g}"
                    # grep __DEVICE_MAP "${engineFile}"  | grep "GROUP_${g}"  | cut -d ' ' -f 2 | sed 's/__.*//' | cut -d '_' -f 3- > ${outputFile}
                    REGS=`grep __DEVICE_MAP "${engineFile}" | grep "GROUP_${g}" | cut -d " " -f 2 | sed 's/__.*//' | sed 's/^/-e /'`
                    REGS_SIZE=`grep __DEVICE_MAP "${engineFile}" | grep "GROUP_${g}" | cut -d " " -f 2 | sed 's/__.*//' | sed 's/$/__SIZE_1/' | sed 's/^/-e /'`
                    echo "Generating ${outputFile}"
                    if [ -n "${REGS}" ] ; then
                        grep -w ${REGS} "${engineFile}" | tr -s " " | cut -d " " -f 2-3 > "${outputFile}"
                    else
                        touch ${outputFile}
                    fi
                    if [ -n "${REGS_SIZE}" ]; then
                        grep -w ${REGS_SIZE} "${engineFile}" | tr -s " " | cut -d " " -f 2-3 > "${outputFile}-sz"
                    else
                        touch "${outputFile}-sz"
                    fi
            done
        fi
    done
done