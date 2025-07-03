#!/bin/bash
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019 by LWPU Corporation. All rights reserved. All
# information contained herein is proprietary and confidential to LWPU
# Corporation. Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

cov_type=$VC_INST_TYPE
files="$@"

if [ -z $P4 ]; then
	P4="/usr/local/bin/p4"
fi

for f in $files; do
	echo "Adding unit $f ..."
	$P4 edit $f
	$VECTORCAST_DIR/clicast -e vcast_elw cover source add $f

	echo "Instrumenting $f ..."
	$VECTORCAST_DIR/clicast -e vcast_elw -u $f cover instrument $cov_type
done

