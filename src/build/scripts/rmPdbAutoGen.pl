#!/usr/bin/elw perl

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

use strict;
use warnings "all";

use Getopt::Long;       # for parsing args
use Carp;               # for 'croak', 'cluck', etc.
use File::Copy;
use File::Compare;
use File::Spec;
use File::Basename;

BEGIN
{
    # HACK : add chip-config dir @INC path
    # for using impl::* modules without '-Ic:/long/path/to/chip-config' in command line
    my $buildDir = dirname __FILE__;
    unshift @INC, File::Spec->catfile($buildDir, '..', '..', '..','drivers', 'common', 'chip-config');
    unshift @INC, File::Spec->catfile($buildDir);
}
use impl::ProfilesImpl; # Implementations for Profiles

#
# Global variables
#
my $opt_verbose = 0;
my $opt_keepTmpFile = 1;
my $opt_target = 'pmu'; # target flcn builds : pmu, dpu, sec2...
my $opt_outdir  = '.';  # dir for output files, default to current dir
my $opt_lwroot  = '';   # path to LW_ROOT
my $opt_perl  = '';     # path to perl program
my $opt_p4  = '';       # path to p4 program
my $progname;           # 'rmPdbAutoGen';
my $devnull;            # /dev/null for multi-OS
my $errorNum = 0;

my $rmRootPath;         # full path to RM root
my $flcnCfgCmd;         # flcn's chip-config.pl command
my $rmCfgCmd;           # rmconfig's chip-config.pl command

my $ProfilesRef;        # ref for Profiles.pm

my @profiles;           # all profiles defined in flcn-config.cfg
my @features;           # all valid features defined in Features.pm
my %chipsByProfile;     # 'chip' can be a normal chip or a ip version range
                        # Ex.  $chipsByProfile{'pmu-gt21x'} = [ GT21X, ]
my %chipsByFeature;     # Ex.  $chipsByFeature{"PMUTASK_I2C"} = [ GT21X, GF10X, ... ];
my %cfgArgs;            # hash for ARGS

my $PERL;               # perl program name
my $P4;                 # p4 program name

my $binAccessFuncName = {
    'pmu'   => 'pmuGetBinArchiveUcode_',
    'sec2'  => 'sec2GetBinArchiveRtosUcode_',
    'dpu'   => 'dispGetBinArchive_',
};

#
# Helper Functioins
#
sub verbose { print(@_, "\n") if ( $opt_verbose ); }
sub message { print("[$progname] @_\n") }
sub prerr   { print STDERR "***  ", @_, "\n";  $errorNum++; }
sub fatal   { croak("\n[$progname] ERROR: @_");  }
sub errchk  { fatal("Too many errors") if ($errorNum > 1) }

# Remove duplicates from an array; does not change order.
sub utilUniquifyArray {
    my %saw;
    # were we given a ref or actual list?
    return if ! defined($_[0]);
    my $listRef = ((@_ == 1) && ref($_[0]) eq "ARRAY") ? $_[0] : \@_;

    return grep { !$saw{$_}++ } @$listRef;
}

# Is OBJ a member of ARRAY-REF list ( using 'eq' )
sub utilIsMemberOfArrayRef {
    # arg1 must be either array ref or undef
    ((@_ == 2) && ((ref($_[1]) eq "ARRAY") || !defined($_[1])))
        or croak 'usage: utilIsMemberOfArrayRef( OBJ, ARRAY-REF )';

    my ($obj, $arrayRef) = @_;

    foreach (@$arrayRef) {
        return 1 if ($_ eq $obj);
    }
    return;
}

#
# Functions
#

# usage
sub usage {

    print <<__USAGE__;

$progname:

Usage:

   perl $progname.pl [ options ]

Options:

    -? --help               This message

    --target <pmu|dpu|sec2> Set the target to pmu or dpu or sec2.  Default: pmu

    --verify                Verify if the generated file the same with the
                            target file.  Exit code is 0 if the files are equal

    --output-dir            Put output files to the directory

    --verbose               Turn on verbose message

__USAGE__

    exit 0;
}

# parse command line options
sub parseArgs {

    my @getoptArgs = (      # options
        "help|?",               # --help / -?
        "target=s",             # --target <pmu|dpu|sec2>
        "verify",               # --verify
        "verbose",              # --verbose
        "output-dir=s",         # --output-dir

        # those options are added for makefiles
        "p4=s",                 # --p4 P4PATH
        "perl=s",               # --perl PERLPATH
        "lwroot=s"              # --lwroot LW_ROOT_PATH
    );

    my $result = GetOptions(\%cfgArgs, @getoptArgs);

    fatal("Arguments parsing error")    if ! $result;

    # --help
    usage()                     if $cfgArgs{help};

    # --verbose
    $opt_verbose = 1            if $cfgArgs{verbose};

    # --perl
    $opt_perl = $cfgArgs{perl}  if $cfgArgs{perl};

    # --p4
    $opt_p4 = $cfgArgs{p4}      if $cfgArgs{p4};

    # --output-dir
    $opt_outdir = $cfgArgs{'output-dir'}    if $cfgArgs{'output-dir'};

    # --lwroot
    $opt_lwroot = $cfgArgs{lwroot}          if $cfgArgs{lwroot};

    # --target <pmu|dpu|sec2>
    $opt_target = lc $cfgArgs{target}       if $cfgArgs{target};

    if (  $opt_target ne 'pmu'
          && $opt_target ne 'dpu'
          && $opt_target ne 'sec2') {
        prerr();
        prerr("Unknown target: '$opt_target'");
        prerr();
        exit 0;
    }
}

# setup
sub setup {

    # get programe name
    __FILE__ =~ m/(\w+)(.pl)?$/;
    $progname = $1;
    $progname = 'rmPdbAutoGen'     if ! $progname;

    $devnull = File::Spec->devnull();

    # parse command line arguments
    parseArgs();

    if ($opt_perl) {
        $PERL = File::Spec->catfile($opt_perl);
    } else {
        $PERL = 'perl';
    }

    if ($opt_p4) {
        $P4 = File::Spec->catfile($opt_p4);
    } else {
        $P4 = 'p4';
    }

    # skip the program path check if --perl is given
    # '> nul' under windows lwmake environment returns error
    if (! $opt_perl ) {
        # check perl
        system("$PERL -v > $devnull");
        fatal("Please add 'perl' to your PATH: $!")         if $?;
    }

    if (! $opt_p4 ) {
        # check p4
        system("$P4 info > $devnull");
        fatal("P4 server not available: $!")                if $?;
    }

    my $chipCfgPath;    # full path to chip-config.pl
    my $flcnCfgDir;     # full path to flcn's config dir
    my $flcnBuildDir;   # full path to flcn's build dir
    my $rmCfgPath;      # full path to rmconfig.pl
    my $cfgFileName;    # pmu-config.cfg or dpu-config.cfg or ...
    my $cfgFilePath;    # path to flcn config files
    my $lwRootPath;     # LW_ROOT
    my $srcRootPath;    # path to flcn src root. Ex. 'prod_app' for pmu

    $cfgFileName = $opt_target."-config.cfg";

    if ($opt_lwroot) {
        $lwRootPath = $opt_lwroot;
    }
    elsif (defined $ELW{LW_ROOT}) {
        $lwRootPath = $ELW{LW_ROOT};
    }
    else {
        # use __FILE__ path to get LW_ROOT
        $lwRootPath = File::Spec->catfile((dirname __FILE__),
                    '..', '..', '..');
    }


    #
    # path setting for different targets
    #
    my @tmp = split /\\/, $lwRootPath;
    if ($opt_target eq 'pmu') {
        $flcnCfgDir   = File::Spec->catfile($lwRootPath,
                            'pmu_sw', 'config');
        $flcnBuildDir = File::Spec->catfile($lwRootPath,
                            'pmu_sw', 'build');
        # $srcRootPath must be UNIX format
        $srcRootPath  = File::Spec::Unix->catfile( @tmp ,
                            'pmu_sw', 'prod_app');
    }elsif ($opt_target eq 'dpu') {
        $flcnCfgDir   = File::Spec->catfile($lwRootPath,
                            'uproc', 'disp', 'dpu', 'config');
        $flcnBuildDir = File::Spec->catfile($lwRootPath,
                            'uproc', 'disp', 'dpu', 'build');
        # $srcRootPath must be UNIX format
        $srcRootPath  = File::Spec::Unix->catfile( @tmp ,
                            'uproc', 'disp', 'dpu', 'src');
    }elsif ($opt_target eq 'sec2') {
        $flcnCfgDir   = File::Spec->catfile($lwRootPath,
                            'uproc', 'sec2', 'config');
        $flcnBuildDir = File::Spec->catfile($lwRootPath,
                            'uproc', 'sec2', 'build');
        # $srcRootPath must be UNIX format
        $srcRootPath  = File::Spec::Unix->catfile( @tmp ,
                            'uproc', 'sec2', 'src');
    }

    $chipCfgPath = File::Spec->catfile($lwRootPath,
                        'drivers', 'common', 'chip-config', 'chip-config.pl');
    $rmRootPath  = File::Spec->catfile($lwRootPath,
                        'drivers', 'resman');
    $rmCfgPath   = File::Spec->catfile($lwRootPath,
                        'drivers', 'resman', 'config', 'rmconfig.pl');

    $cfgFilePath = File::Spec->catfile($flcnCfgDir, $cfgFileName);


    $flcnCfgCmd  = "$PERL $chipCfgPath ".
                   '--mode '.$opt_target.'-config '.
                   "--config $cfgFilePath ".
                   "--source-root $srcRootPath ".
                   '';

    $rmCfgCmd    = "$PERL $rmCfgPath ";

    # to include build related files : Profiles.pm...
    unshift @INC, $flcnBuildDir;

    # if PWD is set, p4.exe will be very confused...
    delete $ELW{PWD};
}

# initGeneralData
#   init some shared global data used in later functions :
#       @profiles
#       @features
sub initGeneralData {

    # all profiles defined in Profiles.pm
    $ProfilesRef = Profiles->new($progname);
    foreach my $buildName (@{$ProfilesRef->grpItemsListRef()}) {
        my $profileName = $opt_target.'-'. lc $buildName;   # dpu-v0200
        push @profiles, $profileName;
    }

    # all valid features defined in Features.pm
    @features = grep { chomp; $_ !~ m/^(PLATFORM|ARCH)_/} `$flcnCfgCmd --print all-features`;

    # filter out exceptions
    my $pattern = (uc $opt_target).'DBG_';       # ex. PMUDBG_
    @features = grep { $_ !~ m/$pattern/} @features;
    $pattern = '^'.(uc $opt_target).'CORE_';     # ex. '^PMUCORE_'
    @features = grep { $_ !~ m/$pattern/} @features;
    $pattern = '^DBG_';                          # '^DBG_'
    @features = grep { $_ !~ m/$pattern/} @features;
}


# parseProfileChipInfo
#   init hash %chipsByProfile
#
#   ex.  $chipsByProfile{'pmu-gt21x'} = [ GT21X, ]
sub parseProfileChipInfo {

    my $queryCmd = "$rmCfgCmd --profile fullrm --print hal-functions:ip2chip:chip2family";

    my @results = grep { chomp; } `$queryCmd`;

    # check if any error
    if ($?) {
        prerr('Fails at Rmconfig query.  This might be caused by Chip removal.');
        prerr("Please edit 'Chips.pm' and mark the deleted chip with flag ':OBSOLETE' to run $progname.");
        prerr("For more infomation, please check $progname Wiki");
        prerr('    https://wiki.lwpu.com/engwiki/index.php/Resman/RM_Foundations/Lwrrent_Projects/RTOS-FLCN-Scripts#rmPdbAutoGen_-_To_Handle_Chip_Delete');

        fatal("Query to Rmconfig failed : '$queryCmd'");
    }

    #  Output format:
    #   < Access function name >                   < CHIP Family List >
    #   pmuGetBinArchiveUcode_GF10X: GF100 GF100B GF104 GF104B GF106 GF106B GF108
    #   sec2GetBinArchiveRtosUcode_GP10X: GP102 GP104 GP105 GP106 GP107 GP108 GP107F
    #   dispGetBinArchive_v02_01: GK104 GK106 GK107 GK110 GK110B GK110C GK208 GK208S GM107
    foreach my $line (@results) {
        my @items           = split ' ', $line;
        my $accessFuncName  = $items[0];            # sec2GetBinArchiveRtosUcode_GP10X:
        my @chips           = @items[1..$#items];   # chip family list : GM10X, GK20A GM20B GM20D

        next    unless ( $accessFuncName =~ /$binAccessFuncName->{$opt_target}([a-zA-Z0-9_]+):/);
        my $buildName = lc $1;
        $buildName =~ s/_//g;
        # HACK for GSP-Lite
        $buildName = 'gv100' if ( ( $buildName eq 'v0300' ) && ( $opt_target eq 'dpu' ) );
        # HACK for GSP
        $buildName = 'tu10x' if ( ( $buildName eq 'v0400' ) && ( $opt_target eq 'dpu' ) );
        $buildName = 'ga10x' if ( ( $buildName eq 'v0401' ) && ( $opt_target eq 'dpu' ) );
        $buildName = 'ad10x' if ( ( $buildName eq 'v0404' ) && ( $opt_target eq 'dpu' ) );

        # HACK for GA10X SEC2 to define features based on boot_from_hs profile
        $buildName = 'ga10x_boot_from_hs' if ( ( $buildName eq 'ga10x' ) && ( $opt_target eq 'sec2' ) );
        #TODO: need to add for dpu-ga10x_boot_from_hs here to update feature 
        #attribute in g_dpu.def correctly. Dislwssed. Will be done later with help from Lucia
        #after the profile changes are submitted to TOT. 
        # HACK for superchip

        #TODO: need to add for dpu-ad10x_boot_from_hs here to update feature 
        #attribute in g_dpu.def correctly. Dislwssed. Will be done later with help from Lucia
        #after the profile changes are submitted to TOT. 
        # HACK for superchip        
        $buildName = 'gh10x' if ( ( $buildName eq 'g000' ) && ( $opt_target eq 'pmu' ) );

        # skip the entry if it is not supported by any enabled chip
        next    unless @chips;

        # $chipsByProfile{'pmu-gm10x'} = [ GM10X, ];
        $chipsByProfile{$opt_target.'-'.$buildName} = \@chips;
    }

    # Error Checks
    foreach my $profile (@profiles) {
        # check if the the profile have have valid chip list from --bindata query
        if ( ! exists $chipsByProfile{$profile} ||
             ! scalar @{$chipsByProfile{$profile}} ) {
            # At the begining of creating new profiles, it's possible the bindata
            # files are not added to RM yet.  Give warning here instead of fatal error.
            print("WARNING: Cannot get chip information for profile '$profile' from bindata query\n");
        }
    }
}

# parseFeatureChipInfo
#   init hash %chipsByFeature
#    Ex.  $chipsByFeature{"PMUTASK_I2C"} = [ GT21X, GF10X, ... ];
#         $chipsByFeature{"DPUTASK_I2C"} = [ v02_01_thru_v02_04, v02_05, ... ];  (IP-Version)
sub parseFeatureChipInfo {

    foreach my $profile (@profiles) {
        my $chipsListRef = $chipsByProfile{$profile};
        my $chip;
        my $provider;

        my $cmd = "$flcnCfgCmd --profile $profile ".
                  '--print enabled-features'.
                  '';

        #  Example Output format:
        #     PLATFORM_PMU
        #     PMUCORE_GT21X
        #     PMUTASK_PERFMON
        #     PMU_PG_CPU_ONLY
        #     PMU_I2C_INT
        my @results = grep { chomp; } `$cmd`;

        # save each feature into data hash
        foreach my $feature (@results) {
            foreach my $chip (@{$chipsListRef}) {
                push @{$chipsByFeature{$feature}}, $chip;
            }
        }
    }
}


# outputFeaturePDB
#   * generate new g_xxx.def (ex. g_pmu.def)
#   * if any change, copy and replace g_xxx.def in RM path
sub outputFeaturePDB {
                     # .../kernel/inc/pmu/bin/g_pmu.def
    my $rmDefPath  = File::Spec->catfile($rmRootPath, 'kernel', 'inc', $opt_target,
                                         'bin', 'g_'.$opt_target.'.def');
                     # .../_out/g_tmp_pmu.def
    my $tmpDefPath = File::Spec->catfile($opt_outdir, 'g_tmp_'.$opt_target.'.def');

    open TMP_OUT_DEF, '>', $tmpDefPath    or fatal ("Cannot open output file : $tmpDefPath");

    # output the headers
    print TMP_OUT_DEF <<__CODE__;
# -*- mode: perl; -*-

##
##  !! DO NOT EDIT !! DO NOT EDIT !! DO NOT EDIT !! DO NOT EDIT !!
##
##  The File was automatically generated by '$progname' script
##
##  [ NOTE ] On Chip Removal :
##    If this file contians some deleted chip and cause chip-config fail to run,
##    please follow the instructions on PMU-Config Wiki and use ':OBSOLETE' flag
##    on deleted chip to re-gen this file.
##

use Rmconfig;

__CODE__

    print TMP_OUT_DEF "my \$".$opt_target."Hal = [\n\n";

    print TMP_OUT_DEF "        PDB_PROPERTIES => [\n\n";

    # message for source of the mapping
    if ($opt_target eq 'pmu') {
        print TMP_OUT_DEF "        #   The mapping is from PMU Binary Resource - UCODE_IMAGE\n";
        print TMP_OUT_DEF "        #\n";
    }
    elsif ($opt_target eq 'dpu') {
        print TMP_OUT_DEF "        #   The mapping is from built-in table in rmPdbAutoGen.pl\n";
        print TMP_OUT_DEF "        #\n";
    }

    foreach my $profile (@profiles) {
        my $chipsStr = ($chipsByProfile{$profile})? (join ", ", @{$chipsByProfile{$profile}}) : '';

        #     pmu-gk10x    => [ GF110F2, GK104, GK106, GK107 ],
        # or  dpu-v0207    => [ v02_07_and_later ],
        printf TMP_OUT_DEF ("        #   %-12s => [ %s ],\n", $profile, $chipsStr);
    }
    print TMP_OUT_DEF "\n";

    # output CFG_FEATURE_*
    foreach my $feature (sort @features) {
        my $featureStr = $feature;
        $featureStr =~ s/^PMU_?//;   # remove the heading PMU or PMU_
        $featureStr = 'FEATURE_'.$featureStr    if ($featureStr !~ m/^TASK_/);

        if ($chipsByFeature{$feature}) {
            my $chipStr = join ", ", @{$chipsByFeature{$feature}};

            print TMP_OUT_DEF "        GEN_$featureStr => [\n";
            print TMP_OUT_DEF "            AUTO_SET => [ $chipStr, ],\n";
            print TMP_OUT_DEF "        ],\n";
            print TMP_OUT_DEF "\n";

        }
        else {
            print TMP_OUT_DEF "        GEN_$featureStr => [ ],\n";
            print TMP_OUT_DEF "\n";
        }
    }

    # output the end of the file
    print TMP_OUT_DEF <<__CODE__;
    ],
];

# return a reference to the Hal definition
__CODE__

    print TMP_OUT_DEF "return \$".$opt_target."Hal;\n";

    close TMP_OUT_DEF;

    if ( compare($rmDefPath, $tmpDefPath) ) {

        # check --verify
        if ($cfgArgs{verify}) {
            message("The generated file doesn't match the current file");
            exit 1;
        }

        if ( $opt_verbose ) {
            # output file difference if diff is available
            my $diffResult = `diff $rmDefPath $tmpDefPath 2> $devnull`;

            # if diff returns valid result
            if ( $? != -1 && $diffResult ) {
                message("diff ".$opt_target.".def $tmpDefPath");
                print $diffResult;
            }
        }

        message("Feature attributes changed, updating g_$opt_target.def");

        if ( ! -w $rmDefPath) {
            message("p4 edit $rmDefPath");

            if ($opt_p4 eq $P4) {
                # assume the script is called by makefile when --P4 is assigned
                # then we ignores the 'p4 edit' output
                system "$P4 edit $rmDefPath > $devnull";
                # note redirect to null device on win+lwmake always returns failure
                # so we don't check the return value '$?' here
            }
            else {
                system "$P4 edit $rmDefPath";
                fatal("'p4 edit $opt_target.def' failed: $!")      if $?;
            }
        }

        copy($tmpDefPath, $rmDefPath)     or fatal("file copy failed: $!");
    }
    else {
        # check --verify
        if ($cfgArgs{verify}) {
            message("The generated file is the same as the current file");
        }
        else {
            message("No update needed");
        }
    }

    # delete the tmp file
    unlink $tmpDefPath           if ( ! $opt_keepTmpFile );
}


#
#   starts here!
#

setup();

initGeneralData();

parseProfileChipInfo();

parseFeatureChipInfo();

outputFeaturePDB();

1;

