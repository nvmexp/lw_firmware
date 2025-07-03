#!/bin/bash

#
# @file:     coverity.sh
# @location: /uproc/utilities/coverity
# 
# Created on: 15th November 2017
#
# This script file is written to commit local changes on coverity server.
# Please refer "README.txt" for better understanding of how to use this script file.
#

yell() { echo "$0: $*" >&2; }
die()  { yell "$*"; exit 1; }
try()  { "$@" || die "cannot $*"; }

#       -------------------------------------------------------------------------------
usage() {
  echo "Usage: $0 [-hi] [-p profiles] [-s stream] <acr|sec2|selwrescrub|...>" >&2;
}
help() {
  usage
  echo
  echo "  Options:"
  echo "    h : this help message"
  echo "    i : interactive mode"
  echo "    p : ucode profiles to build (use quotes to specify more than one profile)"
  echo "    s : coverity stream"
  echo
  echo "  Examples:"
  echo "    coverity.sh -i acr"
  echo "    coverity.sh -p selwrescrub-lwdec_tu10x selwrescrub"
  echo "    coverity.sh -ip 'sec2-tu10x sec2-tu10x_enhanced_aes' -s SEC2_r400 sec2"
}
#       -------------------------------------------------------------------------------

while getopts "him:p:s:" option; do
  case "$option" in
    h) help; exit 0;;
    i) INTERACTIVE_OPT=1;;
    p) PROFILES_OPT=$OPTARG;;
    s) STREAM_OPT=$OPTARG;;
    *) help; exit 1;;
  esac
done
shift $((OPTIND-1))

# Get the required parameter i.e. <acr|sec2|selwrescrub|...>
if (($# == 0)); then
  help
  exit 1
else
  UCODE=$(echo "$1" | tr '[:upper:]' '[:lower:]')
  UCODE_U=$(echo "$1" | tr '[:lower:]' '[:upper:]')
fi
echo "Using UCODE=${UCODE}"

#----------------------PHASE-1----------------------
if [[ -z "$P4ROOT" ]]; then
  P4ROOT=$(pwd | sed 's/\/sw\/.*//') # search for "/sw/..." in current path and chop that off
fi
if [[ -z "$P4ROOT" ]]; then
  echo "Please set P4ROOT" >&2
  exit 1
fi
echo "Using P4ROOT=${P4ROOT}"

BRANCH=$(pwd | sed 's/.*\/sw\/\(.*\)\/uproc.*/\1/') # search for "/sw/.../uproc" in current path
BRANCH_SHORT=$(echo $BRANCH | sed 's/.*\///')       # remove everything but the last dir
BRANCH_TINY=${BRANCH_SHORT/_00/}                    # remove any "_00"
[[ $BRANCH_TINY =~ ^.*_.$ ]] && BRANCH_TINY=${BRANCH_TINY/_/} # remove "_" from chips_a, etc.
echo "Using BRANCH=${BRANCH} (${BRANCH_TINY})"

N_CPUS=$(cat /proc/cpuinfo |grep processor |wc -l)
#---------------------------------------------------

COVERITY_VERSION=2018.03-patch
COVERITY_BIN_PATH=$P4ROOT/sw/tools/Coverity/$COVERITY_VERSION/Linux64/bin/
COVERITY_DIR=$P4ROOT/sw/$BRANCH/uproc/utilities/coverity/_out/$UCODE
COVERITY_DIR_CONFIG=$COVERITY_DIR/config
COVERITY_DIR_EMIT=$COVERITY_DIR/emit
COVERITY_DIR_HTML=$COVERITY_DIR/html

#----------------------PHASE-2---------------------
# You can change high/medium/low
AGGRESSIVENESS_LEVEL="high"

# Stream must be specified so that can be filtered @server
STREAM="${STREAM_OPT}"
if [[ -z "${STREAM}" ]]; then
  #STREAM="${UCODE_U}_${BRANCH_TINY}"   # official streams
  STREAM="$(whoami)"                   # personal streams
fi
echo "Using STREAM=${STREAM}"

BUILD_PATH=$P4ROOT/sw/$BRANCH/uproc/$UCODE/src/
if [[ ! -d $BUILD_PATH ]]; then
  die "Invalid path: $BUILD_PATH"
fi

if [[ -n "${PROFILES_OPT}" ]]; then
  ACTIVE_PROFILES="${PROFILES_OPT}"
else
  # Look for uncommented profiles in profiles.mk
  ACTIVE_PROFILES=$(cat $BUILD_PATH/../config/$UCODE-profiles.mk |                          \
                      grep "${UCODE_U}CFG_PROFILES_ALL" | grep '+=' | grep -v '^\#' |       \
                      sed 's/#.*//' | sed "s/${UCODE_U}CFG_PROFILES_ALL//" | sed 's/+=//' | \
                      tr -d ' ' | tr '\r\n' ' ' | tr '\n' ' ')
fi
echo "Using PROFILES=${ACTIVE_PROFILES}"
#---------------------------------------------------

for i in $ACTIVE_PROFILES ; do

  if [[ -n "${INTERACTIVE_OPT}" ]]; then
    read -p "Run for ${i}? (Y/n)" answer
    if [[ -n "$answer" && "$answer" = "${answer#[Yy]}" ]]; then
      continue
    fi
  fi

  PROFILE_VAR_NAME="${UCODE_U}CFG_PROFILE"
  export ${PROFILE_VAR_NAME}="${i}"
  echo "Building with ${PROFILE_VAR_NAME}=${!PROFILE_VAR_NAME}"

  # Will be added to snapshot DESCRIPTION
  TARGET_PROFILE="${BRANCH_TINY}_${i}_coverity_$(date +%Y%m%d_%H%M%S)"

  rm -rf $COVERITY_DIR
  mkdir -p $COVERITY_DIR
  mkdir $COVERITY_DIR/config
  mkdir $COVERITY_DIR/emit
  mkdir $COVERITY_DIR/html

  #----------------------PHASE-3----------------------
  printf "\n|----------------------------------------------------|"
  printf "\n| ${i}: Config"
  printf "\n|----------------------------------------------------|\n"
  try $COVERITY_BIN_PATH/cov-configure --compiler falcon-elf-gcc \
      --comptype gcc \
      --config $COVERITY_DIR_CONFIG/coverity_config.xml \
      --template

  printf "\n|----------------------------------------------------|"
  printf "\n| ${i}: Build"
  printf "\n|----------------------------------------------------|\n"
  cd $BUILD_PATH
  try $COVERITY_BIN_PATH/cov-build --dir $COVERITY_DIR_EMIT \
      --config $COVERITY_DIR_CONFIG/coverity_config.xml lwmake clobber build -j${N_CPUS}

  printf "\n|----------------------------------------------------|"
  printf "\n| ${i}: Analyze"
  printf "\n|----------------------------------------------------|\n"
  cd $COVERITY_DIR/..
  try $COVERITY_BIN_PATH/cov-analyze \
      --dir $COVERITY_DIR_EMIT \
      --all --enable USER_POINTER \
      --enable SELWRE_CODING \
      --aggressiveness-level $AGGRESSIVENESS_LEVEL \
      --enable-constraint-fpp --enable-exceptions \
      --enable-fnptr --enable-virtual \
      --inherit-taint-from-unions \
      --enable TAINTED_SCALAR

  printf "\n|----------------------------------------------------|"
  printf "\n| ${i}: HTML"
  printf "\n|----------------------------------------------------|\n"
  try $COVERITY_BIN_PATH/cov-format-errors --dir $COVERITY_DIR_EMIT \
      --html-output $COVERITY_DIR_HTML

  printf "\n|----------------------------------------------------|"
  printf "\n| ${i}: Commit"
  printf "\n|----------------------------------------------------|\n"
  try $COVERITY_BIN_PATH/cov-commit-defects --dir $COVERITY_DIR_EMIT \
      --host txcovlind \
      --stream $STREAM \
      --user reporter \
      --password coverity \
      --description "$TARGET_PROFILE" \
      --certs $P4ROOT/sw/tools/scripts/CoverityAutomation/SSLCert/ca-chain.crt \
      --https-port 8443
  #---------------------------------------------------

done
