#!/bin/bash
# References
# 1. https://wiki.lwpu.com/engwiki/index.php/EmbeddedHome/SoftwareTools/Coverity
# 2. http://bobr-dt:8080/CoverityWebHelp/webhelp/en/docs/docs/bk02pt01ch01.html
# 3. \\bobr-dt contains installer files. Useful for installing coverity connect
# 4. D:\sw\automation\DVS 2.0\Build System\Classes\Database_Mappings\gpu_drv_chips\Coverity_Windows_wddm-x64_lwlddmkm.txt
# 5. \\netapp37\scratch.dkumar_mods\t2\sw\pvt\gsamaiya\Coverity Setup.txt
export COV_EXE_ROOT=$P4ROOT/sw/tools/Coverity/2019.12/Linux64
#export LW_SOURCE=$ROOT/sw/rel/gpu_drv/r367/r367_00

# lwmake profile
export TARGET_PROFILE=fbfalcon-gh100-gddr
export AGGRESIVENESS_LEVEL="high"
DTIME=`date +'%m%d%y%H%M%S'`
export COV_BASE_DIR=$P4ROOT/sw/dev/gpu_drv/chips_a/uproc/fbflcn/src/_out/cov-${TARGET_PROFILE}-${AGGRESIVENESS_LEVEL}-${DTIME}
export COV_CONFIG_DIR=$COV_BASE_DIR/config
export COV_EMIT_DIR=$COV_BASE_DIR/emit
export COV_HTML_DIR=$COV_BASE_DIR/html
export PATH=$PATH:$COV_EXE_ROOT/bin
export COV_CONNECT_HOST=txcovlinc
#export COV_CONNECT_PORT=8181
export COV_CONNECT_PORT=8443
export COV_CONNECT_USER=reporter
export COV_CONNECT_PASSWD=coverity
#export COV_CONNECT_STREAM=acr-r370-${TARGET_PROFILE}-${AGGRESIVENESS_LEVEL}
export COV_CONNECT_STREAM=FBFALCON_chips_a
export COMPILER=falcon-elf-gcc
export CLEAN_CMD="make clean"
export BUILD_CMD="make"

# for Cert-C
export CERTC_CFG=$COV_EXE_ROOT/config/coding-standards/cert-c/cert-c-all.config
#export CERTC_CFG=$COV_EXE_ROOT/config/coding-standards/cert-c/cert-cpp-L1-L2.config
#for advisory items
#export CERTC_CFG=$COV_EXE_ROOT/config/coding-standards/cert-c/cert-cpp-all.config

# for MISRA
export MISRA_CFG=$COV_EXE_ROOT/config/coding-standards/misac2012/misrac2012-all.config

# check that we run in the correct location
LOCATION="$PWD"
if [ "$LOCATION" != "$P4ROOT/sw/dev/gpu_drv/chips_a/uproc/fbflcn/tools" ]; then
  echo "WRONG LOCATION: your P4ROOT does not seem to match to current path"
  exit 0
fi

  
mkdir -p $COV_BASE_DIR
rm -rf $COV_CONFIG_DIR
mkdir $COV_CONFIG_DIR
rm -rf $COV_EMIT_DIR
mkdir $COV_EMIT_DIR
rm -rf $COV_HTML_DIR
mkdir $COV_HTML_DIR

cd ../src/
STEP=0
  
((STEP++))
echo "-------------------------------------"
echo "STEP $STEP: Configuring compiler........."
echo "-------------------------------------"
cov-configure --compiler $COMPILER --comptype gcc --config $COV_CONFIG_DIR/coverity_config.xml --template
 
((STEP++))
echo "-------------------------------------"
echo "STEP $STEP: Cleaning the build..........."
echo "-------------------------------------"
$CLEAN_CMD
 
((STEP++))
echo ""
echo "-------------------------------------"
echo "STEP $STEP: Running build cmd............"
echo "-------------------------------------"
cov-build --dir $COV_EMIT_DIR --config $COV_CONFIG_DIR/coverity_config.xml --emit-complementary-info $BUILD_CMD
 
((STEP++))
echo ""
echo "-------------------------------------"
echo "STEP $STEP: Running cov-analyze.........."
echo "-------------------------------------"
#for Coverity
#cov-analyze --dir $COV_EMIT_DIR --all --enable USER_POINTER --enable SELWRE_CODING --aggressiveness-level ${AGGRESIVENESS_LEVEL} --enable-constraint-fpp --enable-exceptions --enable-fnptr --enable-virtual --inherit-taint-from-unions --enable TAINTED_SCALAR
 
#for Cert-C
#cov-analyze --dir $COV_EMIT_DIR --all --enable USER_POINTER --enable SELWRE_CODING --aggressiveness-level ${AGGRESIVENESS_LEVEL} --enable-constraint-fpp --enable-exceptions --enable-fnptr --enable-virtual --inherit-taint-from-unions --enable TAINTED_SCALAR --coding-standard-config $CERTC_CFG

#for MISRA
cov-analyze --dir $COV_EMIT_DIR --all --enable USER_POINTER --enable SELWRE_CODING --aggressiveness-level ${AGGRESIVENESS_LEVEL} --enable-constraint-fpp --enable-exceptions --enable-fnptr --enable-virtual --inherit-taint-from-unions --enable TAINTED_SCALAR --coding-standard-config $MISRA_CFG

((STEP++))
echo ""
echo "-------------------------------------"
echo "STEP $STEP: Formatting defects as html..."
echo "-------------------------------------"
cov-format-errors --dir $COV_EMIT_DIR --html-output $COV_HTML_DIR
 
((STEP++))
echo ""
echo "-------------------------------------"
echo "STEP $STEP: Committing defects..........."
echo "-------------------------------------"
 
# For TLS connection
# Linux:
# mkdir -p $HOME/.coverity/certs/ca
# Copy ca-certs.pem (https://confluence.lwpu.com/display/~dkumar/Coverity?preview=%2F45754579%2F447370769%2Fca-certs.pem) into the dir above
#
# Windows: Add LWCA to key store (if Chrome doesn't give you a warning when navigating to https://dkumar-s1:8182, you're good
#
# References
# 1. https://dkumar-s1:8182/doc/en/cov_platform_use_and_admin_guide.html#cim_config_ssl
# 2. https://dkumar-s1:8182/doc/en/cov_platform_use_and_admin_guide.html#N432A5
# NOTE: LWCA signed cert is self signed. So, cov-commit-defects will fail by default.
# Using "--on-new-cert trust" did not work as suggested in coverity documentation did not work with coverity version 8.7.1
#
# Run cov-commit-defects as follows
#cov-commit-defects --dir $COV_EMIT_DIR --stream $COV_CONNECT_STREAM --user $COV_CONNECT_USER --password $COV_CONNECT_PASSWD -host $COV_CONNECT_HOST --https-port $COV_CONNECT_PORT --encryption preferred
 
 
# For non-TLS connection
cov-commit-defects --dir $COV_EMIT_DIR --stream $COV_CONNECT_STREAM --user $COV_CONNECT_USER --password $COV_CONNECT_PASSWD -host $COV_CONNECT_HOST --https-port $COV_CONNECT_PORT
