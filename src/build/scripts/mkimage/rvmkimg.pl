#!/usr/bin/perl -w
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

use strict;
use warnings "all";

use IO::File;
use Getopt::Long;
use File::Basename;
use File::Spec;

#
# Define the const values
#
use constant RISCV_MKIMG_TOOL_VERSION => 4;         # increment the number when we change the output field format

# option hash structure to hold all command line option inputs
my %gOptions;
# config hash structure to hold config flags (bUseSwbrom, bUseFmc, bUseCoreBootloader, bUseApp etc.)
my %gConfig;

my $g_programName = basename($0);
$g_programName =~ s/\.\w+$//;

#
# functions
#
BEGIN {
    # Save and show the original command line when the exelwtion fails.
    our $g_orgCmdLine = '';
    $g_orgCmdLine  = join ' ', $0, @ARGV;
}

END {
    our $g_orgCmdLine;      # original command line cached in BEGIN block

    # if exit code is not zero, dump the original command line to facilitate debugging
    # default disabled
    if (0 || $?) {
        print "\nFailed command:\n  ";
        print 'perl '.$g_orgCmdLine."\n";
        usage();
    }
}

sub usage {

    print <<__USAGE__;

Options:

  --help                                    : Help message
  --swbromCode FILE                         : swbrom code image
  --swbromData FILE                         : swbrom data image
  --fmcCode|monitorCode|partitionCode  FILE : fmc code image
  --fmcData|monitorData|partitionData  FILE : fmc data image
  --manifest  FILE                          : manifest file
  --bootloader|loaderFile|bl  FILE          : core team bootloader binary
  --app|elfFile  FILE                       : FMC or bootloader payload, could be elf file for bootloader or payload binary for lwstome fmc
  --readelf  FILE                           : .readelf of the ELF binary
  --outFilePrefix STRING                    : String prefix of generated files
  --outDir|out STRING                       : Output directory for generated files
  --verbose                                 : Verbose debug spew
  --useMonitor                              : [deprecated, set automatically when fmcCode provided] use monitor in the image
  --appVersion CHANGELIST                   : Changelist number associated with the image
  --fmcCodeFirst                            : Places FMC code ahead of data (in the image)

__USAGE__

    exit 1;
}

sub verbose {
    printf($_[0]."\n", @_[1..$#_]) if $gOptions{verbose};
}

sub fatal {
    die "\n[$g_programName] ERROR: @_\n\n";
}

sub parseArgs {
    # set default parameters
    $gOptions{outFilePrefix} = 'g_'.$g_programName;
    $gOptions{outDir}        = '.';
    $gOptions{appVersion}    = '0';

    # load options
    # allow bundling of single char forms and case matters
    Getopt::Long::Configure("bundling", "no_ignore_case");
    # keep support short parameter 'bl', 'app' and 'out' for users
    GetOptions(\%gOptions,
        "help",
        "swbromCode=s",
        "swbromData=s",
        "fmcCode|monitorCode|partitionCode=s",
        "fmcData|monitorData|partitionData=s",
        "manifest=s",
        "bootloader|loaderFile|bl=s",
        "app|elfFile=s",
        "readelf=s",
        "outFilePrefix=s",
        "outDir|out=s",
        "verbose",
        "useMonitor",
        "appVersion=s",
        "fmcCodeFirst",
    );

    # check for --help/-h option
    if ($gOptions{help}) {
        usage();
    }

    $gConfig{bUseSwbrom}         = defined($gOptions{swbromCode});
    $gConfig{bUseFmc}            = defined($gOptions{fmcData});
    $gConfig{bUseCoreBootloader} = defined($gOptions{bootloader});
    $gConfig{bBootloaderWithElf} = defined($gOptions{bootloader}) && defined($gOptions{app});
    $gConfig{bUseApp}            = defined($gOptions{app}) && (!defined($gOptions{bootloader})) && (!defined($gOptions{readelf}));
    $gConfig{bUseManifest}       = defined($gOptions{manifest});
    $gConfig{bFmcCodeFirst}      = defined($gOptions{fmcCodeFirst});

    # fatal error for required parameters
    if ($gConfig{bUseSwbrom}) {
        checkRequiredFile($gOptions{swbromCode}, "swbromCode");
        checkRequiredFile($gOptions{swbromData}, "swbromData");
        fatal("Fmc/monitor files missing.") unless $gConfig{bUseFmc};
    }

    if ($gConfig{bUseFmc}) {
        checkRequiredFile($gOptions{fmcCode}, "fmcCode|monitorCode|appCode");
        checkRequiredFile($gOptions{fmcData}, "fmcData|monitorData|appData");
        if ($gConfig{bUseManifest}) {
            checkRequiredFile($gOptions{manifest}, "manifest");
        }
    }

    if ($gConfig{bUseCoreBootloader}) {
        if ($gConfig{bBootloaderWithElf}) {
            checkRequiredFile($gOptions{app}, "elfFile");
            checkRequiredFile($gOptions{readelf}, "readelf");
        }
        checkRequiredFile($gOptions{bootloader}, "bootloader|loaderFile");
    }

    if ($gConfig{bUseApp}) {
        checkRequiredFile($gOptions{app}, "app");
    }
}

#
# check if file is provided and valid.
# If not, print the error message for the corresponding option
# usage: checkRequiredFile(FILE_PATH, option text)
#
sub checkRequiredFile {
    my $file = shift;
    my $option = shift;
    fatal("Missing required option: --$option FILE") unless $file;
    fatal("Invalid file: --$option $file") unless -f $file;
    fatal("Unreadable file: --$option $file") unless -r $file;
}

# read a binary file a return the content and size
sub readFileBytes {
  my $fileName = shift;
  my $bytes;
  my $size;

  open BINFILE, '<:raw', $fileName          or fatal("Couldn't open and read '$fileName'");
  binmode BINFILE;

  $size = -s $fileName;
  read(BINFILE, $bytes, $size) == $size     or fatal("Couldn't read entirity of '$fileName'");

  close BINFILE;

  return ($bytes, $size);
}

sub alignPageUp {
    my $size = shift;

    return $size + ((0x1000 - ($size % 0x1000)) % 0x1000);
}

sub alignPageUp256 {
    my $size = shift;

    return $size + ((0x100 - ($size % 0x100)) % 0x100);
}

sub getUcodeLoadSizeFromELF {
    my $loadSize = 0;
    open READELF, '<', $gOptions{readelf}         or fatal("Couldn't open and read '$gOptions{readelf}'");
    my $line = '';

    # Parsing ReadELF file and get the load section
    #
    # Program Headers:
    # Type           Offset             VirtAddr           PhysAddr
    #                 FileSiz            MemSiz              Flags  Align
    # LOAD           0x0000000000001000 0x0000000001fc0000 0x0000000000000000
    #                 0x0000000000005000 0x0000000000005000  R E    0x1000
    # LOAD           0x0000000000006000 0x0000000001f80000 0x0000000100000000
    #                 0x0000000000001000 0x0000000000001000  RW     0x1000
    # LOAD           0x0000000000007000 0x0000000002000000 0xff00000000000000    <= PhysAddr starts with 0xFF
    #                 0x000000000000b000 0x000000000010b000  RWE    0x1000       <= Get MemSiz
    # LOAD           0x0000000000012000 0x000000000210b000 0xff0000000010b000    <= and this one
    #                 0x0000000000002000 0x0000000000104000  RW     0x1000
    # LOAD           0x0000000000014000 0x000000000220f000 0xff0000000020f000    <= and this one
    #                 0x0000000000003000 0x000000000001f000  RW     0x1000

    # drop everything until it reaches the Program Headers and the line before first entry
    $line = <READELF>       while ($line !~ /^\s+FileSiz\s+MemSiz\s+Flags/);

    while ($line = <READELF>) {
        # end the loop when it reaches an empty line
        last unless $line =~ m/./;

        # LOAD           0x0000000000012000 0x000000000210b000 0xff0000000010b000
        $line =~ m/LOAD\s+0x\w+ 0x\w+ (0x\w+)/              or fatal("Unexpected format when parsing .readelf file to get LOAD line:\n  $line");

        my $physAddr = $1;
        my $nextLine = <READELF>;     # fetch the 2nd Line

        # Only callwlate entries where PhysAddr starts with 0xFF0 or 0xFF2
        # (0xFF2 is for the elf-in-place ODP copy-on-write "fake" load-base)
        next unless ($physAddr =~ m/^0xff[02]/);

        # Assume the MemSiz never exceed 32 bit range.  Give fatal error when it breaks the assumption
        #       FileSiz            MemSiz              Flags  Align
        #      0x0000000000001000 0x0000000000001000  RW     0x1000
        $nextLine =~ m/^\s+ 0x\w+ 0x0{8}([0-9a-f]{8})\s/    or fatal("Unexpected format when parsing .readelf file to get MemSiz line:\n  $nextLine");
        my $memSize = hex($1);
        $physAddr =~ m/^0xff[02]00000([0-9a-f]{8})/         or fatal("Unexpected format when parsing .readelf file to get PhysAddr line:\n  $physAddr");
        my $fbOffset = hex($1);

        if (($fbOffset + $memSize) > $loadSize) {
            $loadSize = $fbOffset + $memSize;
        }
    }

    verbose("getUcodeLoadSizeFromELF(): \$loadSize = 0x%x\n", $loadSize);

    close READELF;

    return $loadSize;
}

#
# dump swbrom + fmc + bootloader + param + elf to one image file and generate the descriptor
# warning: do not change the order of section
#
sub createRiscvImage {
    #
    # Riscv Ucode Descriptor fields and format
    #
    my %riscvFields =
    (
        TOOL_VERSION       => {NAME=>"version"              , FMT=>"%d"  , TYPE=>"LwU32", SIZE=>4, VAL=>RISCV_MKIMG_TOOL_VERSION},
        LOADER_OFFS        => {NAME=>"bootloaderOffset"     , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        LOADER_SIZE        => {NAME=>"bootloaderSize"       , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        LOAD_PARAM_OFFS    => {NAME=>"bootloaderParamOffset", FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        LOAD_PARAM_SIZE    => {NAME=>"bootloaderParamSize"  , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        RISCV_ELF_OFFS     => {NAME=>"riscvElfOffset"       , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        RISCV_ELF_SIZE     => {NAME=>"riscvElfSize"         , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        APP_VERSION        => {NAME=>"appVersion"           , FMT=>"%d"  , TYPE=>"LwU32", SIZE=>4, VAL=>$gOptions{appVersion}},
        MANIFEST_OFFS      => {NAME=>"manifestOffset"       , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        MANIFEST_SIZE      => {NAME=>"manifestSize"         , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        MONITOR_CODE_OFFS  => {NAME=>"monitoCodeOffset"     , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        MONITOR_CODE_SIZE  => {NAME=>"monitorCodeSize"      , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        MONITOR_DATA_OFFS  => {NAME=>"monitorDataOffset"    , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        MONITOR_DATA_SIZE  => {NAME=>"monitorDataSize"      , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        IS_MONITOR_ENABLED => {NAME=>"bIsMonitorEnabled"    , FMT=>"%d"  , TYPE=>"LwU32", SIZE=>4, VAL=>$gConfig{bUseFmc}},
        SWBROM_CODE_OFFS   => {NAME=>"swbromCodeOffset"     , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        SWBROM_CODE_SIZE   => {NAME=>"swbromCodeSize"       , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        SWBROM_DATA_OFFS   => {NAME=>"swbromDataOffset"     , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
        SWBROM_DATA_SIZE   => {NAME=>"swbromDataSize"       , FMT=>"0x%x", TYPE=>"LwU32", SIZE=>4, VAL=>0},
    );

    # Use an array for the order of fields in descriptor
    my @fields = qw (
        TOOL_VERSION
        LOADER_OFFS         LOADER_SIZE         LOAD_PARAM_OFFS
        LOAD_PARAM_SIZE     RISCV_ELF_OFFS      RISCV_ELF_SIZE
        APP_VERSION
        MANIFEST_OFFS       MANIFEST_SIZE
        MONITOR_DATA_OFFS   MONITOR_DATA_SIZE
        MONITOR_CODE_OFFS   MONITOR_CODE_SIZE
        IS_MONITOR_ENABLED
        SWBROM_CODE_OFFS    SWBROM_CODE_SIZE
        SWBROM_DATA_OFFS    SWBROM_DATA_SIZE
    );

    my $movingOffset = 0;
    my $loaderParamSize = 0;
    my $outImagePath = File::Spec->catfile($gOptions{outDir},
                                           $gOptions{outFilePrefix}.'_image.bin');
    open IMAGE_FILE, ">$outImagePath"   or fatal("Can't open output file: $outImagePath");
    binmode IMAGE_FILE;

    # dump swbrom section
    my $swbromSectionSize = 0;
    if ($gOptions{swbromCode})
    {
        my ($swbromCodeBytes, $swbromCodeSize) = readFileBytes($gOptions{swbromCode});
        my ($swbromDataBytes, $swbromDataSize) = readFileBytes($gOptions{swbromData});
        print IMAGE_FILE $swbromCodeBytes;
        $riscvFields{SWBROM_CODE_OFFS}{VAL} = $movingOffset;
        $riscvFields{SWBROM_CODE_SIZE}{VAL} = $swbromCodeSize;
        $movingOffset += $swbromCodeSize;

        print IMAGE_FILE $swbromDataBytes;
        $riscvFields{SWBROM_DATA_OFFS}{VAL} = $movingOffset;
        $riscvFields{SWBROM_DATA_SIZE}{VAL} = $swbromDataSize;
        $movingOffset += $swbromDataSize;

        $swbromSectionSize = $swbromCodeSize + $swbromDataSize;
        my $swbromPadding = alignPageUp($swbromSectionSize) - $swbromSectionSize;
        print IMAGE_FILE pack("C", 0) x $swbromPadding;
        $swbromSectionSize += $swbromPadding;
        $movingOffset += $swbromPadding;
    }

    # dump fmc section
    my $fmcSectionSize = 0;
    if($gOptions{fmcCode})
    {
        my ($manifestBytes, $manifestSize) = (0, 0);
        my ($fmcDataBytes, $fmcDataSize) = readFileBytes($gOptions{fmcData});
        my ($fmcCodeBytes, $fmcCodeSize) = readFileBytes($gOptions{fmcCode});

        if ($gConfig{bUseManifest}) {
            ($manifestBytes, $manifestSize) = readFileBytes($gOptions{manifest});
            print IMAGE_FILE $manifestBytes;
        }

        $riscvFields{MANIFEST_OFFS}{VAL} = $movingOffset;
        $riscvFields{MANIFEST_SIZE}{VAL} = $manifestSize;
        $movingOffset += $manifestSize;

        # Align to 256
        my $manifestPadding = alignPageUp256($manifestSize) - $manifestSize;
        print IMAGE_FILE pack("C", 0) x $manifestPadding;
        $movingOffset += $manifestPadding;

        if ($gConfig{bFmcCodeFirst})
        {
            print IMAGE_FILE $fmcCodeBytes;
            $riscvFields{MONITOR_CODE_OFFS}{VAL} = $movingOffset;
            $riscvFields{MONITOR_CODE_SIZE}{VAL} = $fmcCodeSize;
            $movingOffset += $fmcCodeSize;
        }

        print IMAGE_FILE $fmcDataBytes;
        $riscvFields{MONITOR_DATA_OFFS}{VAL} = $movingOffset;
        $riscvFields{MONITOR_DATA_SIZE}{VAL} = $fmcDataSize;
        $movingOffset += $fmcDataSize;

        if (!$gConfig{bFmcCodeFirst})
        {
            print IMAGE_FILE $fmcCodeBytes;
            $riscvFields{MONITOR_CODE_OFFS}{VAL} = $movingOffset;
            $riscvFields{MONITOR_CODE_SIZE}{VAL} = $fmcCodeSize;
            $movingOffset += $fmcCodeSize;
        }

        $fmcSectionSize = $manifestSize + $manifestPadding + $fmcDataSize + $fmcCodeSize;
        # Align to 4K, WPR requirement
        my $fmcPadding = alignPageUp($fmcSectionSize) - $fmcSectionSize;
        print IMAGE_FILE pack("C", 0) x $fmcPadding;
        $fmcSectionSize += $fmcPadding;
        $movingOffset += $fmcPadding;
    }

    # dump bootloader and elf section
    my $loaderSectionSize = 0;
    my $appSectionSize = 0;
    if($gConfig{bUseCoreBootloader}) {
        my ($loaderBytes, $loaderSize) = readFileBytes($gOptions{bootloader});
        my ($elfBytes, $elfSize) = (0, 0);
        if ($gConfig{bBootloaderWithElf}) {
            ($elfBytes, $elfSize) = readFileBytes($gOptions{app});
        }
        # dump bootloader part of image
        print IMAGE_FILE $loaderBytes;
        $riscvFields{LOADER_OFFS}{VAL} = $movingOffset;
        $riscvFields{LOADER_SIZE}{VAL} = $loaderSize;
        $movingOffset += $loaderSize;

        # populata FLCN_RISCV_BOOTLDR_UCODE (must be aligned to 16 bytes)
        $loaderParamSize = 4         # (DWORD) Total FB footprint
                         + 4         # (DWORD) Size of full image
                         + 4         # (DWORD) Size of ELF
                         + 4;        # (DWORD) Size of padding
        my $imageSize = $swbromSectionSize + $fmcSectionSize + $loaderSize + $loaderParamSize + $elfSize;
        my $padSize = alignPageUp($imageSize) - $imageSize;
        $imageSize += $padSize;

        # need additional space for elf load section
        my $fbReservedSize = $imageSize;
        if ($gConfig{bBootloaderWithElf}) {
            $fbReservedSize = $fbReservedSize + alignPageUp(getUcodeLoadSizeFromELF());
        }

        print IMAGE_FILE pack("L<", $fbReservedSize);
        print IMAGE_FILE pack("L<", $imageSize);
        print IMAGE_FILE pack("L<", $elfSize);
        print IMAGE_FILE pack("L<", $padSize);
        $riscvFields{LOAD_PARAM_OFFS}{VAL} = $movingOffset;
        $riscvFields{LOAD_PARAM_SIZE}{VAL} = $loaderParamSize;
        $movingOffset += $loaderParamSize;
        $loaderSectionSize = $loaderSize + $loaderParamSize;

        # dump ELF part
        if ($gConfig{bBootloaderWithElf}) {
            print IMAGE_FILE $elfBytes;
            $riscvFields{RISCV_ELF_OFFS}{VAL} = $movingOffset;
            $riscvFields{RISCV_ELF_SIZE}{VAL} = $elfSize;
            $movingOffset += $elfSize;
        }

        # dump padding
        print IMAGE_FILE pack("C", 0) x $padSize;   # '$padSize' times of '0'
        $movingOffset += $padSize;

        $appSectionSize = $elfSize + $padSize;
    }
    # dump app section if core team bootloader is not used but app image still provided
    elsif ($gConfig{bUseApp}) {
        my ($appBytes, $appSize) = readFileBytes($gOptions{app});
        # dump app image
        print IMAGE_FILE $appBytes;
        $riscvFields{RISCV_ELF_OFFS}{VAL} = $movingOffset;
        $riscvFields{RISCV_ELF_SIZE}{VAL} = $appSize;
        $movingOffset += $appSize;
        my $padSize = alignPageUp($appSize) - $appSize;

        # dump padding
        print IMAGE_FILE pack("C", 0) x $padSize;
        $movingOffset += $padSize;
        $appSectionSize = $appSize + $padSize;
    }

    close IMAGE_FILE;
    dumpDescriptor($gOptions{outFilePrefix}.'_desc', \@fields, \%riscvFields);

    #
    # Dump signing descriptor & image in bin-file format. This file is sent to the
    # signing server for signing the code and data segments. The signing
    # descriptor contains the code offset -- with loader (4 bytes), code size
    # with loader (4 bytes), data offset (4 bytes), data size (4 bytes), and last,
    # the image size (4 bytes).
    #
    my $codeSize = $swbromSectionSize + $fmcSectionSize + $loaderSectionSize;
    my $dataSize = $appSectionSize;
    dumpBinaryWithSigningDescriptor(
        $outImagePath,
        {
            codeOffs   => 0,
            codeSize   => $codeSize,
            dataOffs   => $codeSize,
            dataSize   => $dataSize,
        },
    );
}

#
# Dump signing descriptor & image in bin-file format. This file is sent to the
# signing server for signing the code and the data segments. The signing
# descriptor contains the code offset -- with loader (4 bytes), code size
# with loader (4 bytes), data offset (4 bytes), data size (4 bytes), and last,
# the image size (4 bytes).
#
#   @param[in] outImagePath destination file
#   @param[in] argHash descriptor values
#
sub dumpBinaryWithSigningDescriptor {
    my $outImagePath = shift;
    my $argHash      = shift;

    my $binSignFile = File::Spec->catfile($gOptions{outDir},
                                          $gOptions{outFilePrefix}.'_sign.bin');

    # get the size and the content of the output image
    my ($imgBytes, $imgSize) = readFileBytes($outImagePath);

    open(FDES, ">", $binSignFile)
        or die "can't open '$binSignFile': $!";
    binmode(FDES);

    # First, write descriptor
    print FDES pack("L", $argHash->{codeOffs});         # code start
    print FDES pack("L", $argHash->{codeSize});         # code size (padded)
    print FDES pack("L", $argHash->{dataOffs});         # data start
    print FDES pack("L", $argHash->{dataSize});         # data size (padded)
    print FDES pack("L", $imgSize);                     # image size (padded)

    # append the image to _sign.bin that is required by sign script
    print FDES $imgBytes;

    # Then, dump the image blob and close the file.
    close FDES or die "can't close $binSignFile: $!";
}

sub dumpDescriptor {
    my $fileNamePrefix = shift;
    my $fieldArray = shift;
    my $fieldHash = shift;
    my $flags = shift;

    my $fileName = $gOptions{outDir}."/$fileNamePrefix";
    open DESC_HEADER, '>', "$fileName.h"        or fatal ("can't open $fileName.h: $!");

    #
    # Dump out fields to a header file
    #
    print DESC_HEADER <<__DESC_HEADER__;
/*
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 */


/*
 * DO NOT EDIT - THIS FILE WAS AUTOMATICALLY GENERATED by $0
 */

#include "rmRiscvUcode.h"

RM_RISCV_UCODE_DESC $fileNamePrefix =
{
__DESC_HEADER__

    foreach my $f (@$fieldArray) {
        printf DESC_HEADER "        /* .%-22s = */ " .
            $fieldHash->{$f}{FMT} . ",\n",
            $fieldHash->{$f}{NAME},
            $fieldHash->{$f}{VAL};
    }

    print DESC_HEADER "};\n";
    close DESC_HEADER;

    #
    # Dump out all fields to .bin file
    #
    open(DESC_BIN, '>', $fileName.'.bin')
        or fatal ("can't open $fileName.bin: $!");
    binmode(DESC_BIN);

    foreach my $f (@$fieldArray)
    {
        if ($fieldHash->{$f}{TYPE} eq "LwU32") {
            print DESC_BIN pack("L", $fieldHash->{$f}{VAL});
        }

        if ($fieldHash->{$f}{TYPE} eq "char[]") {
            my $str = $fieldHash->{$f}{VAL};
            $str    =~ s/"//g;
            my $count  = length $str;

            # append zero to fill the char array
            print DESC_BIN $str;
            while ($count < $fieldHash->{$f}{SIZE}) {
                print DESC_BIN pack("C", 0);
                $count++;
            }
        }
    }

    close DESC_BIN;
}

#-----------------------------------------
# start to run
#-----------------------------------------
parseArgs();

# create the image and dump desc file
createRiscvImage();

# done!
