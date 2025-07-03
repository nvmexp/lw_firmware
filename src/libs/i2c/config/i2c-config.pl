#!/usr/bin/perl -w

use strict;
use warnings "all";

use File::Basename;

#
# drivers-config.pl and other LWRISCV Libraries share one chip-config client.
# This file is a warpper script to trigger chip-config.pl
#

BEGIN {
    # Adding the directory path of xxxconfig.pl ( __FILE__ ) to @INC so that
    # we can access default config file - xxxconfig.cfg
    unshift @INC, dirname __FILE__      if (__FILE__ && -f __FILE__);

    # Adding the major directory of riscvlib-config to @INC so that
    # We can access the shared configuration files, e.g. Features.pm
    my $cfgFilePath = (dirname __FILE__).'/../../flcnlib_config';
    unshift @INC, $cfgFilePath;
}

my $dir = dirname __FILE__;
my $chipConfig = "$dir/../../../../drivers/common/chip-config/chip-config.pl";

# append --source-root if not specified
if (!grep {$_ eq "--source-root"} @ARGV) {
    push @ARGV, "--source-root";
    push @ARGV, "../src/";
}

# append --output-dir if not specified
if (!grep {$_ =~ m/--output-dir/} @ARGV) {
    # create deafult output directory
    my $outDir = '_out';
    if (not -e $outDir) {
        print "[i2c-config] creating output directory '$outDir'\n";
        mkdir($outDir)      or die "Unabled to create directory '$outDir'\n";
    }
    push @ARGV, "--output-dir";
    push @ARGV, "_out";
}

if (!grep {$_ eq "--source-root"} @ARGV) {
}

# Util function to set breakpoint.
# usage:
#  - add main:bp() to somewhere in the code
#  - in debugger (perl -d), set breakpoint at bp
sub bp {}

# "--mode flcnlib-config" must be the first argument
unshift @ARGV, "--config", "i2c-config.cfg";
unshift @ARGV, "--mode", "flcnlib-config";

require $chipConfig;

