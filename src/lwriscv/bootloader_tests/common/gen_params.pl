#!/usr/bin/perl -w
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# Generates a binary that can be loaded into DMEM to boot an ELF with the
# LWRISCV bootloader.
#

use strict;
use warnings "all";

use IO::File;
use Getopt::Long;
use File::Basename;
use File::Spec;

my %opt;
my $g_usageExample = 'perl '.basename($0).' --outFile <name> [options]';

my $g_programName = basename($0);
$g_programName =~ s/\.\w+$//;


#
# functions
#
sub usage {

    print <<__USAGE__;

Usage:

   $g_usageExample

Options:

  --help               : Help message
  --verbose            : Verbose debug spew
  --outFile            : Path for generated parameter file
  --loadBases          : List of 64 bit load bases, e.g. "0x6180000000800000,0x0000000100800000"
  --bootMagic          : Magic number for boot type
  --debugBufferAddr    : Physical address of debug buffer
  --debugBufferSize    : Size of debug buffer (disabled if unspecified)

__USAGE__

    exit 1;
}

sub verbose {
    print(@_, "\n")     if $opt{verbose};
}

sub fatalWithUsage {
    die "\n[$g_programName] ERROR: @_\n".
        "    $g_usageExample\n\n";
}

sub fatal {
    die "\n[$g_programName] ERROR: @_\n\n";
}

sub parseArgs {
    my %opt_raw;

    # load options
    # allow bundling of single char forms and case matters
    Getopt::Long::Configure("bundling", "no_ignore_case");
    # keep support short parameter 'bl', 'app' and 'out' for users
    GetOptions(\%opt_raw,
            "h|help"                           => \$opt{help},
            "verbose"                          => \$opt{verbose},
            "outFile|out=s"                    => \$opt{outFile},
            "loadBases=s"                      => \$opt{loadBases},
            "bootMagic=s"                      => \$opt{bootMagic},
            "debugBufferAddr=s"                => \$opt{dbgBufAddr},
            "debugBufferSize=s"                => \$opt{dbgBufSize},
                   )
        or usage();

    # check for --help/-h option
    if ($opt{help}) {
        usage();
    }

    # fatal error for required parameters
    fatalWithUsage ("Require output file specified with '--outFile FILE_PATH'.")         unless $opt{outFile};

    $opt{loadBases}     = ''                    unless defined $opt{loadBases}; # check with 'defined' as loadBases might be empty string
    $opt{dbgBufAddr}    = '0x0000000000000000'  unless $opt{dbgBufAddr};
    $opt{dbgBufSize}    = '0'                   unless $opt{dbgBufSize};
    $opt{bootMagic}     = '0x12345678'          unless $opt{bootMagic};
}

# dump parameters to a file
sub createParamBinary {
    my $outFilePath = shift;

    open IMAGE_FILE, ">$outFilePath" or fatal("Can't open output file: $outFilePath");
    binmode IMAGE_FILE;

    my @loadBases;
    push @loadBases, split ',', $opt{loadBases};

    fatal("Incorrect boot magic format: $opt{bootMagic} from '--bootMagic $opt{bootMagic}'") unless ($opt{bootMagic} =~ m/(0x)?([0-9a-fA-F]{8})/);
    my $bootMagic = hex($2);

    # Magic number for non-RM boot
    print IMAGE_FILE pack("L<", $bootMagic);
    # Number of load bases
    print IMAGE_FILE pack("L<", scalar @loadBases);
    # Load bases themselves
    foreach my $baseAddr (@loadBases) {
        # some perl version in use doesn't support 64bit pack 'Q<'
        # colwert a 64bit address to two 32bit value
        fatal("Incorrect Base Address format: $baseAddr from '--loadBase $opt{loadBases}'") unless ($baseAddr =~ m/(0x)?([0-9a-fA-F]{8})([0-9a-fA-F]{8})/);
        my $hi32 = hex($2);
        my $lo32 = hex($3);

        printf ("BaseAddr($baseAddr), LO32(0x%x), HI32(0x%x)", $lo32, $hi32) if $opt{verbose};

        print IMAGE_FILE pack("L<L<", $lo32, $hi32);
    }
    # Debug buffer parameters
    fatal("Incorrect address format: '--debugBufferAddr $opt{dbgBufAddr}'") unless ($opt{dbgBufAddr} =~ m/(0x)?([0-9a-fA-F]{8})([0-9a-fA-F]{8})/);
    my $hi32 = hex($2);
    my $lo32 = hex($3);
    print IMAGE_FILE pack("L<", $lo32, $hi32, $opt{dbgBufSize}, 0);

    close IMAGE_FILE;
}

#-----------------------------------------
# start to run
#-----------------------------------------
parseArgs();

# create the image and dump desc file
createParamBinary($opt{outFile});

# done!
