#!/usr/bin/elw perl

#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 1999-2003 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#-------------------------------------------------------------------------------
# 45678901234567890123456789012345678901234567890123456789012345678901234567890

use strict;

my ($nobios, $nochip, $amod, $basename, $ext, $verbose, $help);
my ($bios, $chip);

# set defaults
$bios = "lw41n.rom";
$chip = "lw41_cmodel";

$nobios = 0;
$nochip = 0;
$amod = 1;
$basename = "lw_amodel";
$verbose = 0;
$help =0;

while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-help") {
        print "Determines the mods cmdline parameters for amodel, prints to stdout.\n";
        print "usage: sim_amodel_args.pl [options]\n";
        print "       -verbose: print info/warnings to stderr, best to put this as first argument if supplied\n";
        print "       -nobios: do not add the -gpubios option, the caller will do it\n";
        print "       -nochip: do not add the -chip option, the caller will do it\n";
        print "       -basename \"string\": override the default lib basename \"lw_amodel\"\n";
        print "       -ext \"string\": specify lib extension, e.g. \".so\" or \".dll\"\n";
        print "       -help\n";
        exit 0;
    } elsif ($ARGV[0] eq "-verbose") {
        # best to put this arg first because it allows to print warnings on unused args
        $verbose = 1;
    } elsif ($ARGV[0] eq "-nobios") {
        $nobios = 1;
    } elsif ($ARGV[0] eq "-nochip") {
        $nochip = 1;
    } elsif ($ARGV[0] eq "-basename") {
        shift(@ARGV);
        $basename = $ARGV[0];
    } elsif ($ARGV[0] eq "-ext") {
        shift(@ARGV);
        $ext = $ARGV[0];
    } else {
        print STDERR "warning: unrecognized arg $ARGV[0]\n" if $verbose;
    }
    shift(@ARGV);
}

# Detect Windows vs. Unix
unless ($ext)
{
    if ($^O eq "MSWin32") {
        $ext = ".dll";
    } elsif ($^O eq "cygwin") {
        $ext = ".dll";
    } elsif ($^O eq "linux") {
        $ext = ".so";
    } else {
        print STDERR "warning: could not identify operating system $^O, defaulting extension to .so\n" if $verbose;
        $ext = ".so";
    }
}


my $modsarg = "";
$modsarg .= " -chip " . $basename . $ext unless $nochip;

print "$modsarg";
exit 0;
