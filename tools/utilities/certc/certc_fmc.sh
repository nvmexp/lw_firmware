#!/bin/bash

#
# @file:     certc_fmc.sh
# @location: <BRANCH>/uproc/utilities/certc
# 
# Created on: 09/22/2021
#
# This script file is written to run CERTC and commit defects on server for sec2_riscv ucode.
# Please refer "README.txt" for better understanding of how to use this script file.
#

#----------------------PHASE-1----------------------
if [ -n "$P4ROOT" ]
then
    echo $P4ROOT
else
    echo "Please set P4ROOT"
    exit
fi
#---------------------------------------------------
export COVERITY_VERSION=2021.06
export GPGPU_BRANCH=dev/gpu_drv/chips_a
export COVERITY_BIN_PATH=$LW_TOOLS/Coverity/$COVERITY_VERSION/Linux64/bin/
export CERTC_DIR=$P4ROOT/sw/$GPGPU_BRANCH/uproc/utilities/certc/_out
export CERTC_DIR_CONFIG=$CERTC_DIR/config
export CERTC_DIR_EMIT=$CERTC_DIR/emit
export CERTC_DIR_HTML=$CERTC_DIR/html
export CERTC_L_CONFIG=$LW_TOOLS/Coverity/$COVERITY_VERSION/Linux64/config/coding-standards/cert-c/
#----------------------PHASE-2----------------------

export STREAM="FMC_gpu_drv_chips"
#Stream must be specified so that can be filtered @server

export TARGET_PROFILE="fmc_certc_$(date)"
#Must Specify as will be added to DESCRIPTION

export PROFILE=""
#Comment it if you want to build all profiles


export BUILD_PATH=$P4ROOT/sw/$GPGPU_BRANCH/uproc/sec2_riscv/fmc/
#---------------------------------------------------

rm -rf $CERTC_DIR
mkdir -p $CERTC_DIR
mkdir $CERTC_DIR/config
mkdir $CERTC_DIR/emit
mkdir $CERTC_DIR/html

#----------------------PHASE-3----------------------
STEP=0
((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|------------------STEP $STEP: Config--------------------|"
printf "\n|----------------------------------------------------|\n"

$COVERITY_BIN_PATH/cov-configure --compiler riscv64-elf-gcc \
    --comptype gcc \
    --config $CERTC_DIR_CONFIG/coverity_config.xml \
    --template

((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|-------------------STEP $STEP: Build--------------------|"
printf "\n|----------------------------------------------------|\n"
cd $BUILD_PATH
$COVERITY_BIN_PATH/cov-build --dir $CERTC_DIR_EMIT \
    --config $CERTC_DIR_CONFIG/coverity_config.xml --emit-complementary-info lwmake clean
$COVERITY_BIN_PATH/cov-build --dir $CERTC_DIR_EMIT \
    --config $CERTC_DIR_CONFIG/coverity_config.xml --emit-complementary-info lwmake clobber
$COVERITY_BIN_PATH/cov-build --dir $CERTC_DIR_EMIT \
    --config $CERTC_DIR_CONFIG/coverity_config.xml --emit-complementary-info lwmake
cd $CERTC_DIR/..

((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|------------------STEP $STEP: Analyze-------------------|"
printf "\n|----------------------------------------------------|\n"
$COVERITY_BIN_PATH/cov-analyze \
    --dir $CERTC_DIR_EMIT \
    --coding-standard-config $CERTC_L_CONFIG\cert-c-L1-L2.config \

((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|-------------------STEP $STEP: HTML---------------------|"
printf "\n|----------------------------------------------------|\n"
$COVERITY_BIN_PATH/cov-format-errors --dir $CERTC_DIR_EMIT \
    --html-output $CERTC_DIR_HTML

((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|------------------STEP $STEP: COMMIT--------------------|"
printf "\n|----------------------------------------------------|\n"
$COVERITY_BIN_PATH/cov-commit-defects --dir $CERTC_DIR_EMIT \
    --url commit://reporter:coverity@txcovlind:9090 --stream $STREAM --description "$TARGET_PROFILE" --certs $LW_TOOLS/scripts/CoverityAutomation/SSLCert/ca-chain.crt
#---------------------------------------------------
