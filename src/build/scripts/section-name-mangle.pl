#!/usr/bin/perl -w
#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
#
# Example usage :
#   > perl section-name-mangle.pl <ELF_TARGET_STEM>
#
use strict;
use warnings 'all';
my ($targetStem) = @ARGV;
die 'perl section-name-mangle.pl <ELF_TARGET_STEM>' if (! defined $targetStem);
my $skipSectionName = ".LWPU.mpu_info";
my $sectionsFn = $targetStem . '.sections';
my $mangleFn   = $targetStem . '.mangle';
my $elfFn      = $targetStem . '_nosym.elf';
open(SECTIONS,'<:encoding(UTF-8)', $sectionsFn) or die "Couldn't open $sectionsFn for read";
open(MANGLE,'>:encoding(UTF-8)', $mangleFn) or die "Couldn't open $mangleFn for write";
my $line = '';
# Skip to section headers portion
$line = <SECTIONS> while ($line !~ /^\s+\[Nr\]/);
# Parse headers
while ($line = <SECTIONS>) {
    last if ($line =~ /^Key to Flags:$/);
    if ($line =~ m/^\s+\[\s*(\d+)]\s+(\S+)\s+(\S+)/) {
        my ($no, $section, $type) = ($1, $2, $3);
        next if $type !~ /(PROGBITS|NOBITS)/;
        next if $section eq $skipSectionName;
        my $dst = "s$no";
        print(MANGLE "--rename-section $section=$dst\n");
    }
}
print(MANGLE "$elfFn\n");
close(MANGLE);
