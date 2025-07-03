#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#
#
#
# This file contains exception list of global initialized data in fub binaries.
#
# This file is used by check_global_data.pl.
# 
# WARNING:
# Without proper analysis and review, exceptions should not be added to this file.
#

use warnings 'all';
package globalDataExceptions;

# Exception list of global initialized data.
# Build will not fail for these variables.
our %excp_vars = (
    'g_signature'     => 1,
    'g_desc'          => 1,
    'g_reserved'      => 1,
    'g_canFalcHalt'   => 1,
);
