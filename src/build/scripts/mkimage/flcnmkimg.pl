#!/usr/bin/perl -w
#
# Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

use strict;
no strict qw(vars);
use warnings "all";

use IO::File;
use Getopt::Long;
use File::Basename;

#
# Start by getting the date and time
#
my @months   = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
my @weekDays = qw(Sun Mon Tue Wed Thu Fri Sat Sun);
my ($second, $minute, $hour, $dayOfMonth, $month,
    $yearOffset, $dayOfWeek, $dayOfYear, $daylightSavings) = localtime();
my $year = 1900 + $yearOffset;

my @header =
(
    "/*"                                                                        ,
    " * Copyright 2007-$year by LWPU Corporation.  All rights reserved.  All" ,
    " * information contained herein is proprietary and confidential to LWPU" ,
    " * Corporation.  Any use, reproduction, or disclosure without the written" ,
    " * permission of LWPU Corporation is prohibited."                        ,
    " */"                                                                       ,
    ""                                                                          ,
    ""                                                                          ,
    "/*"                                                                        ,
    " * DO NOT EDIT - THIS FILE WAS AUTOMATICALLY GENERATED"                    ,
    " */"                                                                       ,
);

my @includes =
(
    "lwrm.h",
    "rmflcnucode.h"
);

my $fdes;
my $elf          = "";
my $blFilename   = "bootloader";
my $blImemOffs   = 0;
my $blStackOffs  = 0;
# TODO: imgHFile: remove the image part from the header dump after binres
#       colwersion
my $imgHFile     = ".h";
my $descBFile    = "desc.bin";
my $descTFile    = "desctype.h";
my $imgBFile     = "image.bin";
my $imgSignFile  = "image_sign.bin";
my $bname        = "image";
my $sname        = "IMAGE";
my $falcPath     = "";
my $acl          = 0;
my @image        = ();
my @imemOverlays = ();
my @dmemOverlays = ();
my @emptyData    = ();
my $outdir       = ".";
my @extraIncs    = ();
my $tmpname      = "tmp";
my $maxOverlays  = 64;
my $toolsVersion = 4; # RM can use this version to determine which version of Ucode Descriptor is in use.
my $outputPrefix = "";
my $bVerbose     = '';
my $ucodeSegmentAlignment = 0;
my $debugSyms;
my $hsBootLoader = 0;
my %attributes =
(
    DESC_SIZE             => {NAME=>"descriptorSize"       , FMT=>"%d"  , TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    IMG_SIZE              => {NAME=>"imageSize"            , FMT=>"%d"  , TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    TOOLS_VERSION         => {NAME=>"toolsVersion"         , FMT=>"%s"  , TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_VERSION           => {NAME=>"appVersion"           , FMT=>"%s"  , TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    DATE                  => {NAME=>"date"                 , FMT=>"%s"  , TYPE=>"char[]", SIZE=>64, VAL=>0},
    HSBOOTLOADER          => {NAME=>"selwreBootLoader"     , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    LOAD_START_OFFS       => {NAME=>"bootLoaderStartOffset", FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    LOAD_SIZE             => {NAME=>"bootLoaderSize"       , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    LOAD_IMEM_OFFS        => {NAME=>"bootLoaderImemOffset" , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    LOAD_ENTRY_POINT      => {NAME=>"bootLoaderEntryPoint" , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_START_OFFS        => {NAME=>"appStartOffset"       , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_SIZE              => {NAME=>"appSize"              , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_IMEM_OFFS         => {NAME=>"appImemOffset"        , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_IMEM_ENTRY        => {NAME=>"appImemEntry"         , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_DMEM_OFFS         => {NAME=>"appDmemOffset"        , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_RES_CODE_OFFS     => {NAME=>"appResidentCodeOffset", FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_RES_CODE_SIZE     => {NAME=>"appResidentCodeSize"  , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_RES_DATA_OFFS     => {NAME=>"appResidentDataOffset", FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_RES_DATA_SIZE     => {NAME=>"appResidentDataSize"  , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_NUM_IMEM_OVERLAYS => {NAME=>"nbImemOverlays"       , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
    APP_NUM_DMEM_OVERLAYS => {NAME=>"nbDmemOverlays"       , FMT=>"0x%x", TYPE=>"LwU32" , SIZE=>4 , VAL=>0},
);

#
# All attributes that need dumped as part of the descriptor are stored in a
# hash ('attributes'). Since hashes are unordered, we can't simply iterate
# over the keys to dump each attribute.  This array is used to workaround
# that and defines the exact order in which the attributes should appear in
# the descriptor.
#
my @fields =
(
   'DESC_SIZE'             , 
   'IMG_SIZE'              ,
   'TOOLS_VERSION'         ,
   'APP_VERSION'           , 
   'DATE'                  ,
   'HSBOOTLOADER'          ,
   'LOAD_START_OFFS'       , 
   'LOAD_SIZE'             , 
   'LOAD_IMEM_OFFS'        ,
   'LOAD_ENTRY_POINT'      , 
   'APP_START_OFFS'        , 
   'APP_SIZE'              ,
   'APP_IMEM_OFFS'         ,
   'APP_IMEM_ENTRY'        ,
   'APP_DMEM_OFFS'         ,
   'APP_RES_CODE_OFFS'     ,
   'APP_RES_CODE_SIZE'     , 
   'APP_RES_DATA_OFFS'     , 
   'APP_RES_DATA_SIZE'     ,
   'APP_NUM_IMEM_OVERLAYS' ,
   'APP_NUM_DMEM_OVERLAYS' ,
);

GetOptions("bootloaderFilename=s"             => \$blFilename,
           "bootloaderImemOffset=o"           => \$blImemOffs,
           "bootloaderStackOffset=o"          => \$blStackOffs,
           "rm-desc-bfile=s"                  => \$descBFile,
           "rm-desc-tfile=s"                  => \$descTFile,
           "rm-img-hfile=s"                   => \$imgHFile,
           "rm-img-bfile=s"                   => \$imgBFile,
           "rm-img-signfile=s"                => \$imgSignFile,
           "rm-image-basename=s"              => \$bname,
           "rm-descriptor-structure-name=s"   => \$sname,
           "acl=i"                            => \$acl,
           "falcon-tools=s"                   => \$falcPath,
           "outdir=s"                         => \$outdir,
           "incs=s"                           => \@extraIncs,
           "output_prefix|output-prefix=s"    => \$outputPrefix,
           "verbose"                          => \$bVerbose,
           "ucode-segment-alignment=o"        => \$ucodeSegmentAlignment,
           "use-hs-bootloader=o"              => \$hsBootLoader
           );

# what is remaining on the command-line should just be the elf-file
die "Usage: flcnmkimg.pl [options] <elf-file>\n" unless (($#ARGV + 1) >= 1);
$elf = shift @ARGV;


#
# Prepend any include headers given on the command-line to the includes array.
# Prepend them in the same order as they are specified in case there are
# dependencies or pre-compiled headers.
#
@extraIncs = split(/,/,join(',',@extraIncs));
unshift(@includes, @extraIncs);

print_summary() if $bVerbose;

#
# The bootloader comes in as a single .bin file that is prepared before this
# script is exelwted. Simply load/extract the data from it.
#
my $copyFrom;
if ($hsBootLoader != 0)
{
    $copyFrom = $blImemOffs;
}
else
{
    $copyFrom = 0;
}
my ($loader , $loaderSize) = extract($blFilename, $copyFrom);

# Align bootloader size to specified alignment
if ($ucodeSegmentAlignment != 0)
{
    $loaderSize = alignUp($loaderSize, $ucodeSegmentAlignment);
}

#
# The ELF file provided to this script contains three sections that this script
# needs to process (CODE=0x00000000, DATA=0x10000000, and DESC=0x30000000). Run
# 'dumpexec' to create a separate file for each respective section and then
# load each file.
#
$tmpname = "$outdir/tmp";

`$falcPath/bin/dumpexec --binary --image $tmpname $elf`;
my ($code   , $codeSize)   = extract("$tmpname.0x00000000", 0);
my ($data   , $dataSize)   = extract("$tmpname.0x10000000", 0);
my ($descdat, $descSize)   = extract("$tmpname.0x30000000", 0);

#
# Join the three sections together into one large image. Keep track of the
# offset within the global image where each section starts.
#
my $loaderOffs = addBuffer($loader);
my $codeOffs   = addBuffer($code);
my $dataOffs   = addBuffer($data);

#
# Extract the debug symbols from the image (once). These will be used later to
# find the name of each overlay.
#
($debugSyms) = getDebuggingSymbols();

# process the data in the DESC section to establish the image attributes
collectAttributes($descdat);

if (@emptyData != 0)
{
    addBuffer(\@emptyData);
}

# sanity-check the attributes -- abort if anything fatal is detected
verifyAttributes();

# dump the desc & image in bin-file format
dumpBinary($descBFile, $imgBFile);

#
# Dump signing descriptor & image in bin-file format. This file is sent to the
# signing server for signing the code and data segments. The signing
# descriptor contains the code offset -- with loader (4 bytes), code size
# with loader (4 bytes), data offset (4 bytes), data size (4 bytes), and last,
# the image size (4 bytes).
#
dumpBinaryWithSigningDescriptor($imgSignFile);

# dump the desc type header
$fdes = new IO::File($descTFile, "w") or die "Cannot open file for writing: $!";
dumpHeader($fdes);
$fdes->close();

# dump the image in h-file format
$fdes = new IO::File($imgHFile, "w") or die "Cannot open file for writing: $!";
dumpImage($fdes);
$fdes->close();

# done!


################################################################################


#
# This routine accepts a reference to an array representing some glob of image
# data and adds (appends) it to the main image. Zeros are inserted at the end
# if the buffer does not entirely fill a complete IMEM/DMEM block
# (256-bytes/64-words).
#
# @param[in]  buffer  Reference to the buffer to add
#
# @return The byte-offset where the buffer was added.
#
sub addBuffer
{
    my $aref  = shift;
    my $size  = @$aref;
    my $pos   = @image;
    my $dwordsToFill;

    #
    # Make it IMEM/DMEM block aligned by default
    # If ucodeSegmentAlignment is provided by ucode, then align it accordingly
    # One usecase is with mWPR feature, where we need to make code and data part 4K aligned
    # since we apply different MMU locks (permissions) to code and data part of LS ucodes
    # These locks can be applied at 4K granularity, so these parts need to be in sizes of 4K
    #
    if ($ucodeSegmentAlignment == 0)
    {
        $dwordsToFill = 256/4;
    }
    else
    {
        $dwordsToFill = $ucodeSegmentAlignment/4;
    }

    # append the buffer to the master image
    push(@image, @$aref);

    # make sure an exact number of IMEM/DMEM blocks are consumed
    if ($size % $dwordsToFill != 0)
    {
        my $asize = (int($size / $dwordsToFill) + 1) * $dwordsToFill;
        my $fill  = $asize - $size;
        while ($fill-- > 0)
        {
            push(@image, 0);
        }
    }
    return $pos * 4;
}

#
# This routine creates a word-array from the contents of a file (specified by
# name). If the file does not contain a exact word-multiple (4-bytes) of bytes,
# the last word is zero-extended.
#
# @param[in]  filename  Full path (including the name) of the file to source.
#
# @return a reference to the word-array populated from the file contents.
#
sub extract
{
    my $filename = shift;
    my $from     = shift;
    my @data     = ();
    my @bytes    = ();
    my $in       = "";
    my $word     = 0;
    my $n        = 0;
    my $size     = 0;
    my $copiedSize = 0;
    my $fdes;

    open(FDES, $filename) or return \@data, $size;
    binmode(FDES);

    while (!eof(FDES))
    {
        $word |= (ord(getc(FDES)) << ($n * 8));
        if ((($n + 1) % 4 == 0) and ($size > $from))
        {
            push(@data, $word);
            $copiedSize += 4;
        }

        if ((++$n % 4 == 0))
        {
            $word = 0;
            $n = 0;
        }

        $size += 1;
    }
    close(FDES) or die "can't close $filename: $!";

    if ($n != 0)
    {
        push(@data, $word);
        $copiedSize += $n;
    }

    return \@data, $copiedSize;
}

#
# Takes the image and attributes and dumps them in the proper c-file format.
#
# TODO: Remove the image blob dumping in this sub, after binres colwersion
#
# @param[in]  fdes  Open file-descriptor for the stream to write/dump to
#
sub dumpImage
{
    my $fdes = shift;

    # generate the name for multiple-inclusion sentry from the output filename
    my ($fname, $dir, $suffix) = fileparse($imgHFile, qw(.h));
    my $sentry = "_" . uc($fname) . "_H_";

    print $outputPrefix . " generating $fname$suffix\n";

    # dump out the copyright information, autogen message, and #include list
    foreach $p (@header)
    {
        print $fdes "$p\n";
    }
    print $fdes "\n\n";
    print $fdes "#ifndef $sentry\n";
    print $fdes "#define $sentry\n";

    print $fdes "\n";
    foreach $p (@includes)
    {
        print $fdes "#include \"$p\"\n";
    }
    print $fdes "\n";

    #
    # Dump out all attributes except for the overlay lists (the overlay lists
    # requires special print handling).
    #
    # Normal (single-value) attributes are printed with with the following
    # form:
    #
    #     /* .<attribute_name> = */ <attribute_value>,
    #

    print $fdes "$sname $bname" . "_desc =\n";
    print $fdes "{\n";
    foreach $f (@fields)
    {
        printf $fdes "        /* .%-22s = */ " .
            $attributes{$f}{FMT} . ",\n",
            $attributes{$f}{NAME},
            $attributes{$f}{VAL};
    }

    #
    # Now print out the IMEM overlay list.  Each entry represents a single IMEM
    # overlay. Each overlay has a start offset and a size.
    #
    print $fdes "        /* .loadImemOvl = */\n";
    print $fdes "        {\n";
    my $ovlfmt = "{ /* .start = */ 0x%05x , /* .size = */ 0x%04x }, ";

    foreach my $overlay (@imemOverlays)
    {
        printf $fdes "            " . $ovlfmt, $overlay->{START}, $overlay->{SIZE};
        if ($overlay->{NAME} ne "")
        {
            printf $fdes "// $overlay->{NAME}";
            if ($overlay->{HS_OVERLAY} == 1)
            {
                printf $fdes " (HS)";
            }
            if ($overlay->{RESIDENT} == 1)
            {
                printf $fdes " (RESIDENT)";
            }
            printf $fdes "\n";
        }
        else
        {
            printf $fdes "\n";
        }
    }
    print $fdes "        },\n";

    #
    # Now print out the DMEM overlay list.  Each entry represents a single DMEM
    # overlay. Each overlay has a start offset, a current size, and a max size.
    #
    print $fdes "        /* .loadDmemOvl = */\n";
    print $fdes "        {\n";
    my $dmemOvlfmt = "{ /* .start = */ 0x%06x , /* .lwrSize = */ 0x%04x, /* .maxSize = */ 0x%04x }, ";

    foreach my $overlay (@dmemOverlays)
    {
        printf $fdes "            " . $dmemOvlfmt, $overlay->{START}, $overlay->{LWR_SIZE}, , $overlay->{MAX_SIZE};
        if ($overlay->{NAME} ne "")
        {
            printf $fdes "// $overlay->{NAME}";
            if ($overlay->{RESIDENT} == 1)
            {
                printf $fdes " (RESIDENT)";
            }
            printf $fdes "\n";
        }
        else
        {
            printf $fdes "\n";
        }
    }
    print $fdes "        },\n";

    print $fdes "};\n";

    print $fdes "\n";
    print $fdes "#endif // $sentry\n";
}

#
# Takes the image and attributes and dumps them in the proper bin-file format.
#
# @param[in]  binName  Filename of the bin-file to write
#
sub dumpBinary
{
    my $descName = shift;
    my $binName = shift;
    my $count;
    my $buffer = \@image;
    my ($fname, $dir, $suffix);

    ($fname, $dir, $suffix) = fileparse($descName, qw(.h));
    print $outputPrefix . " generating $fname$suffix\n";

    open(FDES, ">", $descName)
        or die "can't open $descName: $!";
    binmode(FDES);

    #
    # Dump out all attributes/fields except for the overlay list (the overlay
    # list requires special print handling).
    #
    foreach $f (@fields)
    {
        SWITCH:
        {
            $attributes{$f}{TYPE} eq "LwU32" && do
            {
                print FDES pack("L", $attributes{$f}{VAL});
            };

            $attributes{$f}{TYPE} eq "char[]" && do
            {
                $str   = $attributes{$f}{VAL};
                $str   =~ s/"//g;
                $count = length $str;

                print FDES $str;
                while ($count < $attributes{$f}{SIZE})
                {
                    print FDES pack("C", 0);
                    $count++;
                }
            };
        }
    }

    #
    # Now print out the overlay list.  Each entry represents a single overlay.
    # Each overlay has a start offset and a size. Zero-fill any unused overlays
    # up to the maximum number of overlays.
    #

    foreach my $overlay (@imemOverlays)
    {
        print FDES pack("L2", $overlay->{START}, $overlay->{SIZE});
    }

    $count = $#imemOverlays + 1;
    while ($count < $maxOverlays)
    {
        print FDES pack("L2", 0, 0);
        $count++;
    }

    #
    # Dump compression flag, override to 0 because the binary isn't comressed,
    # only the binary in header is
    #
    print FDES pack("L", 0);


    #
    # Finally, dump the actual code blob and close the file.
    #
    close(FDES) or die "can't close $descName: $!";

    ($fname, $dir, $suffix) = fileparse($binName, qw(.h));
    print $outputPrefix . " generating $fname$suffix\n";

    open(FDES, ">", $binName) or die "can't open $binName: $!";
    binmode(FDES);

    foreach $i (@{$buffer})
    {
        print FDES pack("L", $i);
    }
    close(FDES) or die "can't close $binName: $!";
}

#
# Dump signing descriptor & image in bin-file format. This file is sent to the
# signing server for signing the code and the data segments. The signing
# descriptor contains the code offset -- with loader (4 bytes), code size
# with loader (4 bytes), data offset (4 bytes), data size (4 bytes), and last,
# the image size (4 bytes).
#
#   @param[in] binSignFile destination file
#
sub dumpBinaryWithSigningDescriptor {
    my $binSignFile = shift;
    my $imgSize = @image * 4;

    open(FDES, ">", $binSignFile)
        or die "can't open $binSignFile: $!";
    binmode(FDES);

    # First, write descriptor
    print FDES pack("L", $loaderOffs);                  # code start
    print FDES pack("L", $dataOffs - $loaderOffs);      # code size (padded)
    print FDES pack("L", $dataOffs);                    # data start
    print FDES pack("L", $imgSize - $dataOffs);         # data size (padded)
    print FDES pack("L", $imgSize);                     # images size

    # Then, dump the image blob and close the file.
    foreach my $i (@image)
    {
        print FDES pack("L", $i);
    }
    close(FDES) or die "can't close $binSignFile: $!";
}

#
# Dump out a header file that contains the type definition for the descriptor
# dumped by @ref dumpImage.
#
# @param[in]  fdes  Open file-descriptor for the stream to write/dump to
#
sub dumpHeader
{
    my $fdes = shift;

    # generate the name for multiple-inclusion sentry from the output filename
    my ($fname, $dir, $suffix) = fileparse($descTFile, qw(.h));
    my $sentry = "_" . uc($fname) . "_H_";

    # dump out the copyright information and autogen message
    foreach $p (@header)
    {
        print $fdes "$p\n";
    }
    print $fdes "\n\n";
    print $fdes "#ifndef $sentry\n";
    print $fdes "#define $sentry\n\n";

    # dump the descriptor typedef
    print $fdes "typedef struct\n";
    print $fdes "{\n";
    foreach $f (@fields)
    {
        # field is any array
        if ($attributes{$f}{TYPE} =~ m/(.+)(\[])/)
        {
            printf $fdes "    %-5s %s[%d];\n",
                $1,
                $attributes{$f}{NAME},
                $attributes{$f}{SIZE};
        }
        # field is a normal (non-array) type
        else
        {
            printf $fdes "    %-5s %s;\n",
                $attributes{$f}{TYPE},
                $attributes{$f}{NAME};
        }
    }
    print $fdes "    struct {LwU32 start; LwU32 size;} loadImemOvl[$maxOverlays];\n";
    print $fdes "} $sname;\n\n";
    print $fdes "#endif // $sentry\n";
}

#
# This routine simply dumps a buffer of words in a nice clean output format.
#
# @param[in]  fdes        Open file-descriptor for the stream to write/dump to
# @param[in]  maxColumns  Max. words to display on a single row of output
#
sub dumpBuffer
{
    my $fdes       = shift;
    my $maxColumns = shift;
    my $c          = 0;
    my $buffer     = \@image;

    foreach $i (@{$buffer})
    {
        if ($c % $maxColumns == 0)
        {
            printf $fdes "/* 0x%04x */  ", ($c * 4);
        }

        $c += 1;
        printf $fdes "0x%08x, ", $i;

        if (($c > 0) && ($c % $maxColumns == 0))
        {
            print $fdes "\n";
        }
    }

    if ($c % $maxColumns != 0)
    {
        print $fdes "\n";
    }
}

#
# Get the name of the ELF section corresponding to given code address.
#
# @param[in]  addr  Code address of section to lookup
#
# @return
#     String-name of the section when found or an empty string when not found.
#
sub getImemSectionName
{
    my $addr = shift;
    foreach my $sym (@$debugSyms)
    {
        if ($addr == hex($sym->{ADDR}))
        {
            return $sym->{NAME};
        }
    }
    return "";
}

#
# Get the name of the ELF section corresponding to given data address.
#
# @param[in]  addr  Data address of section to lookup
#
# @return
#     String-name of the section when found or an empty string when not found.
#
sub getDmemSectionName
{
    my $addr = shift;
    foreach my $sym (@$debugSyms)
    {
        my $name = $sym->{NAME};
        if ($addr == hex($sym->{ADDR}))
        {
            return $sym->{NAME};
        }
    }
    return "";
}

#
# This routine is responsible for processing all the data extracted from the
# image's DESC section and using it to populate the image attribute hash (where
# information like the image-size, number of overlays, etc ... is stored).
#
# @param[in]  dat  A reference to the array extracted from the DESC section
#
sub collectAttributes
{
    my $dat               = shift;
    my $startResidentCode = shift(@$dat);
    my $endResidentCode   = shift(@$dat);
    my $endResidentData   = shift(@$dat);
    my $imemOverlayCount  = shift(@$dat);
    my $hsOverlayCount    = shift(@$dat);
    my $dmemOverlayCount  = shift(@$dat);
    my $descSize          = 0;
    my $time              =
        "$weekDays[$dayOfWeek] $months[$month] $dayOfMonth " .
            "$hour:$minute:$second $year";
    # fullDataSize will eventually include the entire data size, including all DMEM overlays.
    my $fullDataSize = $dataSize;
    my $lastOvlEnd = 0;

    #
    # Go through the attributes and callwlate the size of the attributes
    # descriptor.
    #
    foreach $f (keys %attributes)
    {
        $descSize += $attributes{$f}{SIZE};
    }
    $descSize += ($maxOverlays * 8);

    for (my $i = 0; $i < $imemOverlayCount; $i++)
    {
        my $start     = shift(@$dat);
        my $end       = shift(@$dat);
        my $resident  = shift(@$dat);
        my $size      = $end - $start;
        my $hsOverlay = 0;

        if ($i < $hsOverlayCount)
        {
            $hsOverlay = 1;
        }

        if ($size > 0)
        {
            push (@imemOverlays,
            {
                START      => $start,
                SIZE       => $size ,
                NAME       => getImemSectionName($start),
                HS_OVERLAY => $hsOverlay,
                RESIDENT   => $resident,
            });
        }
    }

    for (my $i = 0; $i < $dmemOverlayCount; $i++)
    {
        my $start    = shift(@$dat);
        my $lwrSize  = shift(@$dat);
        my $maxSize  = shift(@$dat);
        my $resident = shift(@$dat);
        my $name;

        #
        # OS heap section is not in the same overlay format as other
        # DMEM overlays in the ELF file. All other DMEM overlays have their own
        # load_start and load_stop sections which makes it easy for us to look
        # up their names int he ELF file. Hence, we hardcode the names for the
        # two special required overlays.
        #
        if ($i == 0)
        {
            $name = 'dmem_os_heap';
        }
        else
        {
            $name = getDmemSectionName($start);
        }

        if ($lwrSize > $maxSize)
        {
            printf "lwrSize of overlay %s (0x%x) is larger than " .
                   "maxSize (0x%x)\n",
                   $name, $lwrSize, $maxSize;
            exit 1;
        }

        #
        # for resident overlay, if the size is zero, then it means no variable
        # is compiled in that overlay, thus it shouldn't appear in the generated
        # header. Note that for non-resident overlay, it is possible to have zero
        # size if it's used as a stack surface, thus we are not doing this check
        # for non-resident overlay.
        #
        if (($name ne 'dmem_os_heap') && ($resident == 1) && ($lwrSize == 0))
        {
            next;
        }

        my $lwrOvlEnd = ($start & ~(0x10000000)) + $maxSize;

        #
        # The last overlay end may not be the end of all overlays, e.g. resident 
        # DMEM end would be less than os_heap end.
        #
        $lastOvlEnd = $lwrOvlEnd if $lwrOvlEnd > $lastOvlEnd;

        push (@dmemOverlays,
        {
            START      => ($start & ~(0x10000000)), # mask off 0x1 in the uppermost nibble for data section addresses
            LWR_SIZE   => $lwrSize,
            MAX_SIZE   => $maxSize,
            NAME       => $name,
            RESIDENT   => $resident,
        });
    }

    #
    # Check if the data section includes all the DMEM overlays.
    # If not, fill it with zeroes
    #
    if ($lastOvlEnd > $dataSize)
    {
        $fullDataSize = $lastOvlEnd;

        #
        # Push the required number of zeroes. addBuffer() pads it up to the block boundary,
        # so we need to fill from the next block.
        #
        my $emptyDataSize = $fullDataSize - (($dataSize + 0xff) & ~0xff);
        while ($emptyDataSize > 0)
        {
            push(@emptyData, 0);
            $emptyDataSize -= 4;
        }
    }

    $attributes{DESC_SIZE}{VAL}             = $descSize;
    $attributes{IMG_SIZE}{VAL}              = @image * 4;
    $attributes{TOOLS_VERSION}{VAL}         = $toolsVersion;
    $attributes{APP_VERSION}{VAL}           = $acl;
    $attributes{APP_NUM_IMEM_OVERLAYS}{VAL} = @imemOverlays;
    $attributes{APP_NUM_DMEM_OVERLAYS}{VAL} = @dmemOverlays;
    $attributes{DATE}{VAL}                  = "\"$time \"";
    $attributes{HSBOOTLOADER}{VAL}          = $hsBootLoader;
    $attributes{LOAD_START_OFFS}{VAL}       = $loaderOffs;
    $attributes{LOAD_SIZE}{VAL}             = $loaderSize;
    $attributes{LOAD_IMEM_OFFS}{VAL}        = $blImemOffs;
    $attributes{LOAD_ENTRY_POINT}{VAL}      = $blImemOffs;
    $attributes{APP_START_OFFS}{VAL}        = $codeOffs;
    $attributes{APP_SIZE}{VAL}              =
        $fullDataSize + ($dataOffs - $codeOffs);
    $attributes{APP_IMEM_OFFS}{VAL}         = 0;
    $attributes{APP_IMEM_ENTRY}{VAL}        = $startResidentCode;
    $attributes{APP_DMEM_OFFS}{VAL}         = 0;
    $attributes{APP_RES_CODE_OFFS}{VAL}     = $startResidentCode;
    $attributes{APP_RES_CODE_SIZE}{VAL}     = ($endResidentCode - $startResidentCode);
    $attributes{APP_RES_DATA_OFFS}{VAL}     = $dataOffs - $codeOffs;
    $attributes{APP_RES_DATA_SIZE}{VAL}     = $endResidentData;
}

#
# As much as possible, analyze the image attributes and look for problems.
# Exit with error status if anything fatal is detected.
#
sub verifyAttributes
{
    #
    # Ensure that the bootloader will not accidentally overwrite itself (code)
    # while loading the main application.
    #

    my $appEndResCode = $attributes{APP_RES_CODE_OFFS}{VAL} +
                        $attributes{APP_RES_CODE_SIZE}{VAL} - 1;

    if ($appEndResCode >= $blImemOffs)
    {
        printf "Bootloader (located at 0x%x) overlaps with application " .
               "(start-offset=0x%x, size=0x%x)\n",
               $attributes{LOAD_IMEM_OFFS}{VAL},
               $attributes{APP_RES_CODE_OFFS}{VAL},
               $attributes{APP_RES_CODE_SIZE}{VAL};
        exit 1;
    }

    #
    # Ensure that the bootloader will not accidentally overwrite its stack
    # while loading the main application's data segment. Since we only know the
    # starting offset of the stack and stacks grown down (toward offset zero),
    # subtract off 64-bytes from the starting stack offset to account for the
    # runtime stack depth.
    #

    my $blStackStart  = $blStackOffs - 64;
    my $appEndResData = $attributes{APP_DMEM_OFFS}{VAL} +
                        $attributes{APP_RES_DATA_SIZE}{VAL};

    if ($appEndResData >= $blStackStart)
    {
        printf "Bootloader's stack (range: 0x%x-0x%x) overlaps with " .
               "application's data-segment (range: 0x%x-0x%x)\n",
               $blStackStart,
               $blStackOffs,
               $attributes{APP_DMEM_OFFS}{VAL},
               $attributes{APP_RES_DATA_SIZE}{VAL};
        exit 1;
    }
}

#
# Print a brief summary describing the output
#
sub print_summary
{
    print "\nFalcon Image Summary\n";
    printf "%20s: %s\n", "Image"        , $imgHFile;
    printf "%20s: %s\n", "Bin"          , $imgBFile;
    printf "%20s: %d\n", "Ucode Version", $acl;
    print "\n";
}

#
# Analyze the ELF file and return an array of all local debug symbols. Each
# element in the array will be a hash containing key/values pairs for the
# symbol name and address.
#
sub getDebuggingSymbols
{
    my @syms = ();

    # Use objdump to get a list of all symbols in the ELF file.
    my @output = `$falcPath/bin/falcon-elf-objdump --syms $elf`;
    if ($? != 0)
    {
        warn "Error running objdump (err=$?). Overlay names will not be " .
             "included in generated output files.";
        return \@syms;
    }

    foreach my $line (@output)
    {
        if ($line =~ m/^\s*([0-9A-Fa-f]+)\sl.+d\s+\.(.+)\t0{8}/)
        {
            #
            # Look for lines with the following format for IMEM overlays:
            #
            #     00003d3a l    d  .ovl1  00000000 .ovl1
            #
            # These are the (l)ocal (d)ebug symbols that correspond to each
            # overlay/section (but not limited to overlays).
            #
            my $sym =
            {
                NAME => $2,
                ADDR => $1,
            };
            push @syms, $sym;
        }
        elsif ($line =~ m/^\s*([0-9A-Fa-f]+)\sg+\s+\*ABS\*+\s[0-9A-Fa-f]+\s+__load_start_+(\w+)/)
        {
            #
            # Additionally, look for lines with the following format for overlays:
            #
            #     1000c000 g       *ABS*  00000000 __load_start_dmem_dummy1
            #
            # These are the (g)lobal symbols that correspond to each overlay's
            # start address. We will use these symbols to match DMEM overlays
            # with their names since not all dmem overlays will have local
            # debug symbols corresponding to them. Empty DMEM overlays that
            # intend to be used only as a heap won't have those symbols.
            #
            my $sym =
            {
                NAME => $2,
                ADDR => $1,
            };
            push @syms, $sym;
        }
    }
    return \@syms;
}

#
# This routine aligns the value with given granularity
#
# @param[in]  origVal   Original value before alignment
# @param[in]  gran      granularity for alignment
#
# @return
#     Value after alignment
#
sub alignUp
{
    my $origVal = shift;
    my $gran    = shift;

    return ((($origVal) + (($gran) - 1)) & ~((($origVal) - ($origVal) + ($gran))-1));
}
