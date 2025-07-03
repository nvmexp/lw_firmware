#!/usr/bin/perl -w

#
# Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

# 
# pmu-config.pl
#
# Version 1.00
#
# This utility script is a simple wrapper to chip-config that shorten the 
# verbose command line from
#   > perl ./../../drivers/common/chip-config/chip-config.pl --mode pmu-config --config pmu-config.cfg --source-root ../prod_app --profile pmu-gp105 --output-dir _out
# to
#   > perl pmu-config.pl --profile pmu-gp105
#

use strict;
use warnings "all";

use File::Basename;

my $dir = dirname __FILE__;
my $chipConfig = "$dir/../../drivers/common/chip-config/chip-config.pl";

push @ARGV, '--mode', 'pmu-config';
push @ARGV, '--config', 'pmu-config.cfg';
push @ARGV, '--source-root', '../prod_app';

if (!grep {$_ eq "--profile"} @ARGV) {
    # use pmu-gp100 as default instead of the oldest profile
    push @ARGV, '--profile', 'pmu-gp100';
}

# append --output-dir if not specified
if (!grep {$_ =~ m/--output-dir/} @ARGV) {
    # create deafult output directory
    my $outDir = '_out';
    if (not -e $outDir) {
        print "[pmu-config] creating output directory '$outDir'\n";
        mkdir($outDir)      or die "Unabled to create directory '$outDir'\n";
    }
    push @ARGV, "--output-dir";
    push @ARGV, "_out";
}

require $chipConfig;
