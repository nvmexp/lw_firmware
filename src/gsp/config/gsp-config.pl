#!/usr/bin/perl -w

use strict;
use warnings "all";

use File::Basename;

# Adding the directory path of rmconfig.pl ( __FILE__ ) to @INC so that
# we can access default config file - rmconfig.cfg
BEGIN {
    unshift @INC, dirname __FILE__      if (__FILE__ && -f __FILE__);
}

my $dir = dirname __FILE__;
my $chipConfig = "$dir/../../../drivers/common/chip-config/chip-config.pl";

# append --source-root if not specified
if (!grep {$_ eq "--source-root"} @ARGV) {
    push @ARGV, "--source-root";
    push @ARGV, "$dir/../src";
}

# append --output-dir if not specified
if (!grep {$_ =~ m/--output-dir/} @ARGV) {
    # create deafult output directory
    my $outDir = '_out';
    if (not -e $outDir) {
        print "[gsp-config] creating output directory '$outDir'\n";
        mkdir($outDir)      or die "Unabled to create directory '$outDir'\n";
    }
    push @ARGV, "--output-dir";
    push @ARGV, "_out";
}

push @ARGV, "--mode", "gsp-config";
push @ARGV, "--config", "gsp-config.cfg";

require $chipConfig;

