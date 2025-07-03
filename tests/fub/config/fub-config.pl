#!/usr/bin/perl -w

#
# Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

# 
# fub-config.pl
#
# This utility script is a simple wrapper to chip-config that shorten the 
# verbose command line from
#   > perl ./../../drivers/common/chip-config/chip-config.pl --mode fub-config --config fub-config.cfg --source-root ../prod_app --profile fub-sec2_tu10x --output-dir _out
# to
#   > perl fub-config.pl --profile fub-sec2_tu10x
#

use strict;
use warnings "all";

use File::Basename;

my $dir = dirname __FILE__;
my $chipConfig = "$dir/../../../drivers/common/chip-config/chip-config.pl";

push @ARGV, '--mode', 'fub-config';
push @ARGV, '--config', 'fub-config.cfg';
push @ARGV, '--source-root', '../src';

if (!grep {$_ eq "--profile"} @ARGV) {
    # use fub-sec2_tu10x as default
    push @ARGV, '--profile', 'fub-sec2_tu10x';
}

# append --output-dir if not specified
if (!grep {$_ =~ m/--output-dir/} @ARGV) {
    # create deafult output directory
    my $outDir = '_out';
    if (not -e $outDir) {
        print "[fub-config] creating output directory '$outDir'\n";
        mkdir($outDir)      or die "Unabled to create directory '$outDir'\n";
    }
    push @ARGV, "--output-dir";
    push @ARGV, "_out";
}

require $chipConfig;
