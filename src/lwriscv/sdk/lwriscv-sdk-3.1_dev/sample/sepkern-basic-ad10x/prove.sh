#!/usr/bin/elw bash
#set -x

APP_DIR=`pwd`

make -C ../../monitor/sepkern/ prove ENGINE=$1 POLICY_PATH=$APP_DIR POLICY_FILE_NAME=policies_$1_ad10x.ads CHIP=ad10x
