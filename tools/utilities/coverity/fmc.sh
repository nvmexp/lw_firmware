#!/bin/bash

#
# @file:     fmc.sh
# @location: <BRANCH>/uproc/utilities/coverity
# 
# Created on: 9/22/2021
#
# This script file is written to commit local changes on coverity server.
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
export COVERITY_DIR=$P4ROOT/sw/$GPGPU_BRANC/uproc/utilities/coverity/_out
export COVERITY_DIR_CONFIG=$COVERITY_DIR/config
export COVERITY_DIR_EMIT=$COVERITY_DIR/emit
export COVERITY_DIR_HTML=$COVERITY_DIR/html

#----------------------PHASE-2----------------------
export AGGRESSIVENESS_LEVEL="high"                          
#You can change high/medium/low

export STREAM="FMC_gpu_drv_chips"                                 
#Stream must be specified so that can be filtered @server

export TARGET_PROFILE="fmc_coverity_$(date)"    
#Must Specify as will be added to DESCRIPTION

export PROFILE=""    
#Comment it if you want to build all profiles

export BUILD_PATH=$P4ROOT/sw/$GPGPU_BRANCH/uproc/sec2_riscv/fmc/
#---------------------------------------------------

rm -rf $COVERITY_DIR
mkdir -p $COVERITY_DIR
mkdir $COVERITY_DIR/config
mkdir $COVERITY_DIR/emit
mkdir $COVERITY_DIR/html

#----------------------PHASE-3----------------------
STEP=0
((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|------------------STEP $STEP: Config--------------------|"
printf "\n|----------------------------------------------------|\n"

$COVERITY_BIN_PATH/cov-configure --compiler riscv64-elf-gcc \
    --comptype gcc \
    --config $COVERITY_DIR_CONFIG/coverity_config.xml \
    --template

((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|-------------------STEP $STEP: Build--------------------|"
printf "\n|----------------------------------------------------|\n"
cd $BUILD_PATH
$COVERITY_BIN_PATH/cov-build --dir $COVERITY_DIR_EMIT \
    --config $COVERITY_DIR_CONFIG/coverity_config.xml lwmake clean
$COVERITY_BIN_PATH/cov-build --dir $COVERITY_DIR_EMIT \
    --config $COVERITY_DIR_CONFIG/coverity_config.xml lwmake clobber
$COVERITY_BIN_PATH/cov-build --dir $COVERITY_DIR_EMIT \
    --config $COVERITY_DIR_CONFIG/coverity_config.xml lwmake
cd $COVERITY_DIR/..

((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|------------------STEP $STEP: Analyze-------------------|"
printf "\n|----------------------------------------------------|\n"
$COVERITY_BIN_PATH/cov-analyze \
    --dir $COVERITY_DIR_EMIT \
    --all --enable USER_POINTER \
    --enable SELWRE_CODING \
    --aggressiveness-level $AGGRESSIVENESS_LEVEL \
    --enable-constraint-fpp --enable-exceptions \
    --enable-fnptr --enable-virtual \
    --inherit-taint-from-unions \
    --enable TAINTED_SCALAR

((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|-------------------STEP $STEP: HTML---------------------|"
printf "\n|----------------------------------------------------|\n"
$COVERITY_BIN_PATH/cov-format-errors --dir $COVERITY_DIR_EMIT \
    --html-output $COVERITY_DIR_HTML

((STEP++))
printf "\n|----------------------------------------------------|"
printf "\n|------------------STEP $STEP: COMMIT--------------------|"
printf "\n|----------------------------------------------------|\n"
#$COVERITY_BIN_PATH/cov-commit-defects --dir $COVERITY_DIR_EMIT \
#    --url commit://reporter:coverity@txcovlind:9090 --stream $STREAM --description "$TARGET_PROFILE" --certs $P4ROOT/sw/tools/scripts/CoverityAutomation/SSLCert/ca-chain.crt
#---------------------------------------------------
