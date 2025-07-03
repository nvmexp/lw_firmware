#!/usr/bin/sh

# Usage     : cov.sh <cov_connect_config.sh> <module_config.sh>
# Example   : sh cov.sh dvs_connect_config.sh uproc/acr/utilities/acr_cov_config.sh 

# 
# Checking Arguments
#
if [ $# -ne 2 ]
then
    echo 'Few Arguments provided. See usage and examples in file.'
    exit 1
fi

if [[ ! -f $(dirname "$0")/$1 ]]
then
    echo 'Not a cov connect config file. See usage and examples in file.'
    exit 1
fi

if [[ ! -f ${BRANCH}/$2 ]]
then
    echo 'Not a cov module config file. See usage and examples in file.'
    exit 1
fi

# Apply coverity connect settings
. $(dirname "$0")/$1

# Apply Module specific settings
. ${BRANCH}/$2

# Create output file
mkdir -p ${MODULE_EMIT_DIR}

# Parse module profiles.mk file and get profiles list
grep "${MODULE_PROFILE_ALL}" ${MODULE_PROFILE_FILE} | awk '{if(/^[^#]/ && /=/) print $3}' | sed '/^$/d' | while read PROFILE ; do
	echo "Processing Profile: $PROFILE"
	
	#
	# Creating Build Script
	#
	echo cd ${MODULE_SRC} > ${P4_ROOT}/temp_build_${PROFILE}.sh
	echo make clean >> ${P4_ROOT}/temp_build_${PROFILE}.sh
	echo make clobber >> ${P4_ROOT}/temp_build_${PROFILE}.sh
	echo export ${MODULE_PROFILE}=${PROFILE} >> ${P4_ROOT}/temp_build_${PROFILE}.sh
	echo make >> ${P4_ROOT}/temp_build_${PROFILE}.sh
	chmod 777 ${P4_ROOT}/temp_build_${PROFILE}.sh


	# Conguring Coverity
	${COV_PATH}/cov-configure --compiler falcon-elf-gcc --comptype gcc --template

	# Initiating Coverity Build
	${COV_PATH}/cov-build --dir ${MODULE_EMIT_DIR} ${P4_ROOT}/temp_build_${PROFILE}.sh

	# Start Coverity Analyze
	${COV_PATH}/cov-analyze --dir ${MODULE_EMIT_DIR} --aggressiveness-level ${MODULE_COV_AGGRESSIVENESS} --security --enable-fnptr --enable-virtual --enable-constraint-fpp -j auto --strip-path ${P4_ROOT}

	# Commit to server
	${COV_PATH}/cov-commit-defects --on-new-cert trust --user ${COV_CONNECT_USER} --password ${COV_CONNECT_PASSWORD} --scm perforce --host ${COV_CONNECT_HOST} --port  ${COV_CONNECT_PORT} --stream	${MODULE_COV_STREAM} --dir ${MODULE_EMIT_DIR} --description ${MODULE_COMPONENT}_${PROFILE}

	rm -f ${P4_ROOT}/temp_build_${PROFILE}.sh
done
