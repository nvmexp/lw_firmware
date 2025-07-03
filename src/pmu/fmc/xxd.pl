#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

# MMINTS-TODO: move to a shared ucode dir later if this is ever needed elsewhere.

local $/;
my $unp = unpack "H*", <>;
$unp =~ s/(.{32})/$1\n/mxsg;
$unp =~ s/([a-fA-F0-9]{2})/ $1/mxsg;
print $unp;
