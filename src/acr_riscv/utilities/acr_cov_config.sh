export MODULE=ACR
export MODULE_HOME=${BRANCH}/uproc/acr_riscv
export MODULE_SRC=${MODULE_HOME}/src
export MODULE_COMPONENT=Coverity_Linux_unix-build_ACR
export MODULE_EMIT_DIR=${P4_ROOT}/emit-${MODULE_COMPONENT}
export MODULE_COV_STREAM=unix-build_ACR
export MODULE_COV_AGGRESSIVENESS=high
export MODULE_PROFILE_FILE=${MODULE_HOME}/config/acr-profiles.mk
export MODULE_PROFILE_ALL=ACRCFG_PROFILES_ALL
export MODULE_PROFILE=ACRCFG_PROFILE

