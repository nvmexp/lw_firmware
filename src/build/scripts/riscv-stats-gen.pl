#!/usr/bin/perl -w

#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# Example usage :
#   > perl riscv-stats-gen.pl $(ELF_TARGET) output.txt
#

use strict;
use warnings 'all';

use Carp;
use File::Basename;
use File::Spec;

my ($targetStem, $outFn) = @ARGV;

die 'perl riscv-stats-gen.pl <ELF_TARGET> <stats.txt>' if ((! defined $targetStem) or (! defined $outFn));

my $sectionsFn = $targetStem . '.sections';
my $nmFn       = $targetStem . '.nm';
my $elfFn      = $targetStem . '_nosym.elf';

open(SECTIONS,'<:encoding(UTF-8)', $sectionsFn) or die "Couldn't open $sectionsFn for read";
open(NM,      '<:encoding(UTF-8)', $nmFn) or die "Couldn't open $nmFn for read";
open(OUTFH,   '>:encoding(UTF-8)', $outFn) or die "Couldn't open $outFn for write";

my $line = '';

# Skip to section headers portion
$line = <SECTIONS> while ($line !~ /^\s+\[Nr\]/);

my %sectionHeaders;
my $largestNameLength = 0;

# Large arbitrary value, used for sorting.
my $fakeIndex = 100000;
while ($line = <SECTIONS>) {
    last if ($line =~ /^Key to Flags:$/);

    $line =~ m/^\s+\[\s?([0-9]+)\] ([^\s]*)\s+([^\s]+)\s+([0-9a-fA-F]{8})([0-9a-fA-F]{8})\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)/;
    my ($index, $name, $type, $addrhi, $addrlo, $offset, $size) = ($1, $2, $3, $4, $5, $6, $7);

    next if int($index) eq 0;

    my $header = {};
    $header->{IS_CHILD} = 0;
    $header->{NAME} = $name;
    $header->{SNAME} = substr $name, 1;
    $header->{TYPE} = $type;
    $header->{ADDRESS} = oct('0x'.$addrlo);
    $header->{OFFSET} = oct('0x'.$offset);
    $header->{REAL_SIZE} = oct('0x'.$size);
    $header->{INDEX} = $fakeIndex;
    $header->{SIZE} = $header->{REAL_SIZE};
    $header->{MAX_SIZE} = $header->{REAL_SIZE};
    $header->{HEAP_SIZE} = 0;
    $header->{LOCATION} = -1;
    $header->{PERMISSION} = 0;

    if ($header->{ADDRESS} != 0) {
        $sectionHeaders{$header->{SNAME}} = $header;
    }
    $fakeIndex += 1;

    # Update longest name length
    if (length($header->{NAME}) > $largestNameLength) {
        $largestNameLength = length($header->{NAME});
    }
}

my %odpStats;
my $odpSharedMpuEntries;

# Skip header line
$line = <NM>;

while ($line = <NM>) {
    last if ($line !~ /./); # End on empty line

    $line =~ m/^([0-9a-fA-F]{8})([0-9a-fA-F]{8})( ([0-9a-fA-F]{8})([0-9a-fA-F]{8}))? (.) (.+)$/;

    my ($valuehi, $valuelo, $sizehi, $sizelo, $type, $name) = ($1, $2, $4, $5, $6, $7);

    if ($name =~ /^__section_/) {
        $name =~ m/^__section_([0-9a-zA-Z]+)_(.+)$/;
        my ($sectionName, $setting) = ($1, $2);
        # If the section name is prefixed, we need to use a different regex
        if (($sectionName eq 'dmem') || ($sectionName eq 'imem')) {
            $name =~ m/^__section_([id]mem_[0-9a-zA-Z]+)_(.+)$/;
            ($sectionName, $setting) = ($1, $2);
        }
        elsif ($sectionName eq 'LWPU') {
            $name =~ m/^__section_LWIDIA_mpu_info_(.+)$/;
            ($sectionName, $setting) = ('LWPU.mpu_info', $1);
        }

        if (! exists $sectionHeaders{$sectionName}) {
            my $header = {};
            $header->{IS_CHILD} = 1;
            $header->{NAME} = "_".$sectionName;
            $header->{SNAME} = $sectionName;

            # Child sections have empty heaps
            $header->{HEAP_SIZE} = 0;

            #
            # A child section is not a real section so it has no
            # meaningful TYPE or OFFSET
            #
            $header->{TYPE} = "PROGBITS"; # a reasonable default
            $header->{OFFSET} = -1;

            $sectionHeaders{$header->{SNAME}} = $header;

            # Update longest name length
            if (length($header->{NAME}) > $largestNameLength) {
                $largestNameLength = length($header->{NAME});
            }
        }

        my $header = $sectionHeaders{$sectionName};
        if ($setting eq 'max_size') {
            $header->{MAX_SIZE} = oct('0x'.$valuelo);

            if ($header->{IS_CHILD}) {
                #
                # Child sections are not real sections, and
                # they have no padding or alignment, so
                # real size is always going to be same as
                # max size.
                #
                $header->{REAL_SIZE} = $header->{MAX_SIZE};
                $header->{SIZE} = $header->{REAL_SIZE};
            }
        }
        elsif ($setting eq 'initial_size') {
            $header->{SIZE} = oct('0x'.$valuelo);
        }
        elsif ($setting eq 'heap_size') {
            $header->{HEAP_SIZE} = oct('0x'.$valuelo);
        }
        elsif ($setting eq 'index') {
            $header->{INDEX} = oct('0x'.$valuelo);
        }
        elsif ($setting eq 'location') {
            $header->{LOCATION} = oct('0x'.$valuelo);
        }
        elsif ($setting eq 'permission') {
            $header->{PERMISSION} = oct('0x'.$valuelo);
        }

        if ($header->{IS_CHILD}) {
            if ($setting eq 'start') {
                $header->{ADDRESS} = oct('0x'.$valuelo);
            }
        }
    } elsif ($name =~ /^__odp_(.+)$/) {
        if ($name eq '__odp_config_mpu_shared_count') {
            $odpSharedMpuEntries = oct('0x'.$valuelo);
        } elsif ($name =~ /^__odp_([id]mem)_(.+)$/) {
            $odpStats{$1}{$2} = oct('0x'.$valuelo);
        }
    }
}

my @locationNames = (
    'Paged DTCM',
    'Paged ITCM',
    'Resident DTCM',
    'Resident ITCM',
    'EMEM',
    'FB',
    'In-Place ELF',
);

sub locationForHeader {
    my ($locatiolwalue, $name) = @_;

    if ($locatiolwalue == -1) {
        my %autoSectionHash = (
            'text'      => 5,
            'rodata'    => 5,
            'data'      => 5,
            'page_pool' => 5,
            'sysStack'  => 2,
        );
        $locatiolwalue = $autoSectionHash{$name} if (defined $autoSectionHash{$name});
    }
    return $locatiolwalue;
}

# Build statistics
my @regionUsage = (
    { # Paged DMEM
        REAL => 0,
        HEAP => 0,
        USED => 0,
    },
    { # Paged IMEM
        REAL => 0,
        HEAP => 0,
        USED => 0,
    },
    { # Resident DMEM
        REAL => 0,
        HEAP => 0,
        USED => 0,
    },
    { # Resident IMEM
        REAL => 0,
        HEAP => 0,
        USED => 0,
    },
    { # EMEM
        REAL => 0,
        HEAP => 0,
        USED => 0,
    },
    { # FB
        REAL => 0,
        HEAP => 0,
        USED => 0,
    },
);

foreach my $sectionName (keys %sectionHeaders) {
    my $header = $sectionHeaders{$sectionName};
    my $locationIdx = locationForHeader($header->{LOCATION}, $sectionName);
    if ($locationIdx != -1 && !$header->{IS_CHILD}) {
        $regionUsage[$locationIdx]->{REAL} += $header->{REAL_SIZE};
        $regionUsage[$locationIdx]->{HEAP} += $header->{HEAP_SIZE};
        $regionUsage[$locationIdx]->{USED} += $header->{SIZE};
    }
}

printf OUTFH "Region name   :  Used Size :  Real Size :\n";
printf OUTFH "--------------:------------:------------:\n";
foreach my $i (0 .. $#regionUsage) {
    if ($regionUsage[$i]->{REAL} > 0) {
        printf OUTFH "%-13s : %10s : %10s :\n",
            $locationNames[$i],
            sprintf("0x%08X", $regionUsage[$i]->{USED}),
            sprintf("0x%08X", $regionUsage[$i]->{REAL});
    }
}
printf OUTFH "\n";

my $staticMpuEntriesInfoSection = "LWPU.mpu_info";

if (exists $sectionHeaders{$staticMpuEntriesInfoSection}) {
    # Subtract 4 bytes for setting type and 4 bytes for count
    my $mpuEntriesSize = $sectionHeaders{$staticMpuEntriesInfoSection}->{SIZE} - 8;
    # Each MPU entry in mpu_info is 4 dwords, so divide by 4*8
    printf OUTFH "Static MPU entries mapped at init: %d\n", $mpuEntriesSize / (4 * 8);
}
printf OUTFH "ODP shared MPU entries config: %d\n", $odpSharedMpuEntries;
printf OUTFH "ODP region :   Page Size : Total Pages :   Used Size :   Real Size : Wasted Size :\n";
printf OUTFH "-----------:-------------:-------------:-------------:-------------:-------------:\n";
foreach my $region (sort keys %odpStats) {
    my $maxNumPages = $odpStats{$region}{'max_num_pages'};
    my $configNumPagesUpperBound = $odpStats{$region}{'max_config_num_pages'};

    if ($maxNumPages > $configNumPagesUpperBound) {
        printf "WARNING: ODP region %s has upper bound set to %d, ".
               "lower than max possible number of pages: %d\n",
               $region, $configNumPagesUpperBound, $maxNumPages;

        $maxNumPages = $configNumPagesUpperBound;
    }

    my $pageSize = $odpStats{$region}{'page_size'};
    my $realSize = $odpStats{$region}{'region_size'};

    printf OUTFH "%-10s : %11s : %11s : %11s : %11s : %11s :\n",
        $region,
        sprintf("0x%08X", $pageSize),
        sprintf("%d", $maxNumPages),
        sprintf("0x%08X", $pageSize * $maxNumPages),
        sprintf("0x%08X", $realSize),
        sprintf("0x%08X", $realSize - ($pageSize * $maxNumPages));
}
printf OUTFH "\n";

printf OUTFH "%-${\($largestNameLength + 1)}s: Index : Permission :      Location :  Used Size :  Real Size :  Heap Size :\n", "Section name";
printf OUTFH ("-"x($largestNameLength + 1)).":-------:------------:---------------:------------:------------:------------:\n";
foreach my $sectionName (sort { $sectionHeaders{$a}{INDEX} <=> $sectionHeaders{$b}{INDEX}     ||
                                $sectionHeaders{$a}{ADDRESS} <=> $sectionHeaders{$b}{ADDRESS} ||
                                $sectionHeaders{$a}{IS_CHILD} <=> $sectionHeaders{$b}{IS_CHILD} } keys %sectionHeaders) {
    my $header = $sectionHeaders{$sectionName};
    my $index = '-';
    if ($header->{INDEX} < 100000) {
        $index = sprintf("0x%03X", $header->{INDEX});
    }
    my $size = sprintf("0x%08X", $header->{SIZE});
    my $heapsize = sprintf("0x%08X", $header->{HEAP_SIZE});
    my $realsize = sprintf("0x%08X", $header->{REAL_SIZE});
    my $location = '';
    my $locationIdx = locationForHeader($header->{LOCATION}, $sectionName);
    if ($locationIdx != -1) {
        $location = $locationNames[$locationIdx];
    }
    my $permission = 'k';
    $permission .= ($header->{PERMISSION} & ( 8|1)) ? 'R' : '-';
    $permission .= ($header->{PERMISSION} & (16|2)) ? 'W' : '-';
    $permission .= ($header->{PERMISSION} & (32|4)) ? 'X' : '-';
    $permission .= '  u';
    $permission .= ($header->{PERMISSION} &  1) ? 'R' : '-';
    $permission .= ($header->{PERMISSION} &  2) ? 'W' : '-';
    $permission .= ($header->{PERMISSION} &  4) ? 'X' : '-';
    printf OUTFH "%-${\($largestNameLength + 1)}s: %5s : %s : %13s : %10s : %10s : %10s :\n",
        $header->{NAME}, $index, $permission, $location, $size, $realsize, $heapsize;
}
