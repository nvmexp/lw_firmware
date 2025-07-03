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

if [ "$1" == de ]; then
	$VECTORCAST_DIR/clicast -e vcast_elw cover instrument none	
	$VECTORCAST_DIR/clicast -e vcast_elw cover elw delete
else
	$VECTORCAST_DIR/clicast cover elw create vcast_elw
	$VECTORCAST_DIR/clicast -e vcast_elw cover options in_place y
fi
