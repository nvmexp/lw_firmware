#!/usr/bin/perl -w

# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2016 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_

#
# pmu-script.pl - Wrapper script of rtos-flcn-script.pl for PMU module
#
# This wrapper script is to facilitate quick query, testing and development. 
# In makefiles, please use rtos-flcn-script.pl directly instead of this wrapper
# file. 

use strict;
use warnings "all";

use File::Basename;

my $dir = dirname __FILE__;
my $flcnScript = "$dir/../../uproc/build/scripts/rtos-flcn-script.pl";

if (!grep {$_ eq '--profile'} @ARGV) {
    my $default_profile = 'pmu-gm20x';
    print "Use default profile '$default_profile'.\n";
    push @ARGV, "--profile";
    push @ARGV, $default_profile;
}

require $flcnScript;

