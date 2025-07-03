#! /bin/bash

if [ "$#" -lt 2 ]; then
    echo "Usage: prof.sh [TRACE_FILE] [OBJECT_FILE]+"
    exit 1
fi

CPP_BIN="/home/utils/gcc-4.9.3/bin"

if [ ! -d ${CPP_BIN} ]; then
   echo "Could not find ${CPP_BIN}"
   exit 1
fi

export PATH=${CPP_BIN}:${PATH}   

make

FALCON_ELF_OBJDUMP=sw/tools/falcon-gcc/falcon6/6.2.1/Linux/falcon-elf-gcc/bin/falcon-elf-objdump

TRACE_FILE=$1
shift

if [ -z ${P4ROOT} ]; then
    echo "P4ROOT not defined"
    exit 1
fi

if [ ! -e ${P4ROOT}/${FALCON_ELF_OBJDUMP} ]; then
    echo "Please add //${FALCON_ELF_OBJDUMP}"
    exit 1
fi

if [ ! -e ${TRACE_FILE} ]; then
    echo "Trace file ${TRACE_FILE} does not exist"
    exit 1
fi

OBJDUMP_FILE=objects.objdump

rm -f ${OBJDUMP_FILE}

until [ -z "$1" ]  # Until all parameters used up . . .
do
    ${P4ROOT}/${FALCON_ELF_OBJDUMP} -d -l $1 >> ${OBJDUMP_FILE}
    if [ $? -ne 0 ]; then
        echo "falcon-elf-objdump failed"
        exit 1
    fi
    shift
done

CALLGRIND_FILE=callgrind.out.`date +%s.%N`

# The trace file can be dynamically growing as the profiler is running.
# This means you can run fmodel and this profiler simultaneously.
cat ${TRACE_FILE} | ./prof ${OBJDUMP_FILE} ${CALLGRIND_FILE}

if [ $? -ne 0 ]; then
    echo "Profiler failed"
    exit 1
fi

if [ ! -d bin ]; then
    mkdir bin
fi

rm -f bin/objdump
ln -s "${P4ROOT}/${FALCON_ELF_OBJDUMP}" bin/objdump

export PATH=`pwd`/bin:${PATH}

GRAPHVIZ_BIN=/home/utils/graphviz-2.38.0/bin

if [ ! -d ${GRAPHVIZ_BIN} ]; then
    echo "Could not find ${GRAPHVIZ_BIN}"
    exit 1
fi

export PATH=${GRAPHVIZ_BIN}:${PATH}

kcachegrind ${CALLGRIND_FILE}
