#!/bin/bash

# Copyright (c) 2019, LWPU CORPORATION.  All rights reserved.

# Common environment variables to be exported for VectorCAST builds

export VECTOR_LICENSE_FILE=1683@indc-lic-01:1787@sc-lic-25
export VECTORCAST_DIR=${P4ROOT}/sw/tools/VectorCAST/linux/2019
export PATH=${VECTORCAST_DIR}:${PATH}
export TEGRA_SRC=${P4ROOT}/sw/dev/gpu_drv/chips_a/uproc/tegra_acr/src/
export VC_UTILS=$TEGRA_SRC/../utilities/vcast/
echo "Found VC_UTILS, moving to TEGRA_SRC"
cd $TEGRA_SRC

