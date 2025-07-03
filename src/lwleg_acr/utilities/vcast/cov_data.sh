#!/bin/bash

# Copyright (c) 2019, LWPU CORPORATION.  All rights reserved.

# Utilize PMU VC utils to colwert TESTINSS.BIN to TESTINSS.DAT
# Run from build dir!

PMU_VC_UTILS=$P4ROOT/sw/apps/gpu/drivers/resman/pmu/vcExtract
cp TESTINSS.BIN vcast_elw
cd vcast_elw
$VECTORCAST_DIR/vpython $PMU_VC_UTILS/vcast_c_options_preprocess.py
$VECTORCAST_DIR/vpython $PMU_VC_UTILS/expand_binary_coverage.py TESTINSS.BIN > TESTINSS.DAT
cp TESTINSS.DAT ..
cd ..

