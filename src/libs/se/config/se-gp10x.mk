#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

###############################################################################
# Some modules will complain when its internal jump tables refer to the address
# beyond the linker can address. (the linker usually points to somewhere in
# rodata with 'no_symbol')
###############################################################################

# See the description of LARGE_SRC_FILES in uproc/libs/se/src/make-profile.lwmk
