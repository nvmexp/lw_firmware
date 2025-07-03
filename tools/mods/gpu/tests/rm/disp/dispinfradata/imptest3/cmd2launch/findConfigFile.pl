#!/usr/bin/perl
#
# Copyright (c) 2013 LWPU Corporation. All Rights Reserved
#
# File Name:   findConfigFile.pl
#
# This script return proper config file for imptest3.
#

use strict;

use warnings 'all';

use Cwd;
use File::Basename;             # base directory name for files
use File::Find;                 # Traverse a directory tree

my $configRootDir = (dirname __FILE__) . "/../configs";

# collect all .js config files 
my @searchDirs = ();
push @searchDirs, $configRootDir;

my @configFiles = ();

find(\&findAllConfigFile, @searchDirs);

sub findAllConfigFile()
{
    return if -d $_; # if it's path of dir, just ignore it

    # skip the non-js file
    my (undef, $ext) = split('\\.');

    return if !defined($ext);
    return if lc($ext) ne 'js';

    push @configFiles, $_;
}

# check the input parameter
if (@ARGV != 4)
{
    die "Please pass 4 parameters - Chip, BusinessUnit, Memory Type, Memory address range\n" .
        "Chip - GF100, GK107 etc...\n".
        "BusinessUnit - NB, DT, Qdro etc...\n".
        "Memory Type - SDDR3, GDDR5 etc...\n".
        "Memory address range - 128, 192 etc...\n";
}


my $chip = $ARGV[0];
my $bu = $ARGV[1];
my $memType = $ARGV[2];
my $memRange = $ARGV[3];

my $exitCode = 0;

foreach my $config (@configFiles)
{
    if ($config =~ /$chip.*$bu.*$memType.*$memRange/i)
    {
        print $config;
        open(DATA,">imptest3ConfigForThisGpu.txt"); 
        print DATA $config; 
        close (DATA); 
        $exitCode = 1;
        last;
    }
}

exit $exitCode;

