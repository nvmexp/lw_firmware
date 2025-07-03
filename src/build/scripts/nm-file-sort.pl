#!/usr/bin/perl -w

#
# Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# We need this because GNU nm can sort by address or by symbol name but
# not both at once, and we sometimes get different orderings for symbols
# with the same value/address but a different name, which causes a
# spurious "only symbols changed" release (for example on RISCV builds
# on Linux vs Windows). This script can be used to prevent that scenario.
#
# Example usage :
#   > perl nm-file-sort.pl output.nm
#

use strict;
use warnings 'all';

my ($outFn) = @ARGV;

die 'perl nm-file-sort.pl <NM_TARGET>' if (! defined $outFn);

open(INFH, '<:encoding(UTF-8)', $outFn) or die "Couldn't open $outFn for read";

my @symbols;
my $line;
my $processedFirstLine = 0;
my $appVersionLine;

while ($line = <INFH>) {
    if (!$processedFirstLine) {
        if ($line =~ /^(AppVersion:\s.*?)\s+$/) {
            $appVersionLine = $1;
            next;
        }

        $processedFirstLine = 1;
    }

    if ($line =~ /libos_pvt_file\./) {
        # Special case: libos_pvt_file symbols use __FILE__, so they can be volatile depending
        # on the build config. Strip them out of the .nm to avoid "only symbols changed"
        # releases.
        next;
    }

    last if ($line !~ m/^([0-9a-fA-F]+)\s(.*?)\s+$/);
    my ($addr, $data) = ($1, $2);

    push @symbols, { ADDR => $addr, DATA => $data };
}

close(INFH);

open(OUTFH, '>:encoding(UTF-8)', $outFn) or die "Couldn't open $outFn for write";

if (defined $appVersionLine) {
    printf OUTFH "%s\n", $appVersionLine;
}

foreach my $symbol (sort { $a->{ADDR} cmp $b->{ADDR} || $a->{DATA} cmp $b->{DATA} } @symbols) {
    printf OUTFH "%s %s\n", $symbol->{ADDR}, $symbol->{DATA};
}

close(OUTFH);
