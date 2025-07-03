#!/bin/bash

# Purpose of this script : To check if variables defined in globalDataExceptions.pm file are
# actually used/defined somewhere in the ACR code base.

# Usage : ./checkGlobalDataExceptiolwariables.sh

PARENT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
cd "$PARENT_PATH"

ACR_CODE_BASE="../"
VARIABLES=`cat ./globalDataExceptions.pm | grep '=>' | cut -f2 -d\' | tr -s '\n' ' '`
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

for VARIABLE in $VARIABLES
do
    FILE_LIST=`grep -ril "$VARIABLE" "$ACR_CODE_BASE" | grep -v _out`
    FILE_COUNT=`echo "$FILE_LIST" | grep -v _out | wc -l`

    # There will always be 1 count from the globalDataExceptions.pm file itself, so
    # check less than 2
    if [ $FILE_COUNT -lt 2 ];
    then
        echo -e "${RED}$VARIABLE not used!${NC}"
    else
        echo -e "${GREEN}$VARIABLE used in $FILE_COUNT files!${NC}"
    fi

done
