#!/usr/bin/elw perl

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

package GenRmProp;

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
my $dbg_keepTmpFile = 1;
my $progname;           # 'genRmProp';
my $devnull;            # /dev/null for multi-OS
my $errorNum = 0;

my $g_flcnCfgCmd;       # flcn's chip-config.pl command

my $ProfilesRef;        # ref for Profiles.pm

my @profiles;           # all profiles defined in flcn-config.cfg
my %validFeature_H;     # all valid features defined in Features.pm
my %featuresByProfile;  # Ex. $featuresByProfile{'pmu_gp104'} = [ PMUTASK_I2C, PMUTASK_ACR, ... ];

my %cfgArgs;            # hash for ARGS

my $PERL;               # perl program name
my $P4;                 # p4 program name

my $eng;                # 'pmu'
my $ENG;                # 'PMU'

#
# Helper Functioins
#
sub verbose { print(@_, "\n") if ( $cfgArgs{verbose} ); }
sub message { print("[$progname] @_\n") }
sub prerr   { print STDERR "***  ", @_, "\n";  $errorNum++; }
sub fatal   { croak("\n[$progname] ERROR: @_");  }
sub errchk  { fatal("Too many errors") if ($errorNum > 1) }

#
# Functions
#

# usage
sub usage {

    print <<__USAGE__;

Usage:

   perl $0 [ options ]

Options:

    -? --help               This message

    --output-dir            Put output files to the directory

    --verbose               Turn on verbose message

__USAGE__

    exit 0;
}

sub setupClientInfo
{
    die "client must implement setupClientInfo()";
}

sub getSetPropFuncName
{
    die "client must implement getSetPropFuncName()";
}


# parse command line options
sub parseArgs {

    my @getoptArgs = (      # options
        "help|?",               # --help / -?
        "verbose",              # --verbose
        "output-dir=s",         # --output-dir

        # those options are added for makefiles
        "p4=s",                 # --p4 P4PATH
        "perl=s",               # --perl PERLPATH
        "lwroot=s",             # --lwroot LW_ROOT_PATH

        # developer only
        'quick',                # quick mode to loop only target profiles

    );

    my $result = GetOptions(\%cfgArgs, @getoptArgs);

    fatal("Arguments parsing error")    if ! $result;

    # --help
    usage()                             if $cfgArgs{help};

    # --output-dir
    $cfgArgs{'output-dir'} = '.'        unless $cfgArgs{'output-dir'};

    if ($cfgArgs{perl}) {
        $PERL = File::Spec->catfile($cfgArgs{perl});
    } else {
        $PERL = 'perl';
    }

    if ($cfgArgs{p4}) {
        $P4 = File::Spec->catfile($cfgArgs{p4});
    } else {
        $P4 = 'p4';
    }

    # skip the program path check if --perl is given
    # '> nul' under windows lwmake environment returns error
    if (! $cfgArgs{perl} ) {
        # check perl
        system("$PERL -v > $devnull");
        fatal("Please add 'perl' to your PATH: $!")         if $?;
    }

    if (! $cfgArgs{p4} ) {
        # check p4
        system("$P4 info > $devnull");
        fatal("P4 server not available: $!")                if $?;
    }

}

# setup
sub setup {

    (@_ == 2) or croak 'usage: obj->setup( LWROOT-FROM-PWD )';
    
    my $self = shift;
    my $lwRootFromPwd = shift;

    $ENG = $self->{NAME};
    $eng = lc $ENG;

    # get programe name
    $0 =~ m/(\w+)(.pl)?$/;
    $progname = $1;
    $progname = 'genRmProp'     if ! $progname;

    $devnull = File::Spec->devnull();

    # parse command line arguments
    parseArgs();

    my $chipCfgPath;    # full path to chip-config.pl
    my $cfgFileName;    # pmu-config.cfg or dpu-config.cfg or ...
    my $cfgFilePath;    # path to flcn config files
    my $lwRootPath;     # LW_ROOT

    # find LW_ROOT
    if ($cfgArgs{lwroot}) {
        $lwRootPath = $cfgArgs{lwroot};
    }
    elsif (defined $ELW{LW_ROOT}) {
        $lwRootPath = $ELW{LW_ROOT};
    }
    else {
        # from client info
        $lwRootPath = $lwRootFromPwd;
    }

    $self->{LW_ROOT} = $lwRootPath;

    # setupClientInfo() requires $self->{LW_ROOT}
    $self->setupClientInfo();

    $cfgFileName = $eng."-config.cfg";

    $chipCfgPath  = File::Spec->catfile($lwRootPath,
                        'drivers', 'common', 'chip-config', 'chip-config.pl');

    $cfgFilePath  = File::Spec->catfile($self->{CLIENT}->{flcnCfgDir}, $cfgFileName);


    $g_flcnCfgCmd  = "$PERL $chipCfgPath ".
                   '--mode '.$eng.'-config '.
                   "--config $cfgFilePath --source-root ".
                   $self->{CLIENT}->{srcRootPath}.
                   ' ';

    # if PWD is set, p4.exe will be very confused...
    delete $ELW{PWD};
}

# initGeneralData
#   init some shared global data used in later functions :
#       @profiles
#       %validFeature_H
sub initGeneralData {

    # get all profiles defined in Profiles.pm
    $ProfilesRef = Profiles->new($progname);
    foreach my $buildName (@{$ProfilesRef->grpItemsListRef()}) {
        my $profileName = $eng.'-'. lc $buildName;   # dpu-v0200
        push @profiles, $profileName;
    }

    # all valid features defined in Features.pm
    my @features = grep { chomp; $_ !~ m/^(PLATFORM|ARCH)_/} `$g_flcnCfgCmd --print all-features`;

    # TODO -- wrap this to a function.  So client can override this part
    #         grepValidFeatures()
    # filter out exceptions
    my $pattern = $ENG.'DBG_';       # ex. PMUDBG_
    @features = grep { $_ !~ m/$pattern/} @features;
    $pattern = '^'.$ENG.'CORE_';     # ex. '^PMUCORE_'
    @features = grep { $_ !~ m/$pattern/} @features;
    $pattern = '^DBG_';                          # '^DBG_'
    @features = grep { $_ !~ m/$pattern/} @features;

    $validFeature_H{$_} = 1     foreach (@features);
}



# parseFeatureInfo
#   init hash %featuresByProfile
sub parseFeatureInfo {

    # profile list for quick run -- debug only
    my %quickProfile_H = (
        'pmu-gm20x' => 1,
        'pmu-gm20b' => 1,
        'pmu-gp100' => 1,
    );

    foreach my $profile (@profiles) {

        # in quick (debug) mode, skips profiles not listed in %quickProfile_H
        next if $cfgArgs{quick} and !$quickProfile_H{$profile};

        my $cmd = "$g_flcnCfgCmd --profile $profile ".
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
            push @{$featuresByProfile{$profile}}, $feature          if $validFeature_H{$feature};
        }
    }
}


# getSetPropFuncName_default
#   getSetPropFuncName('pmu-ga100') returns 'pmuSetProfileProperties_GA100'
sub getSetPropFuncName_default {

    (@_ == 1) or croak 'usage: getSetPropFuncName_default( PROFILE-NAME )';

    my $profile = shift;    # pmu-ga100

    # default profile naming pattern :
    #   pmu-gk104 => pmuSetProfileProperties_GK104
    my $pattern = $eng.'\-([a-z0-9]+)';
    if ($profile =~ m/^$pattern$/) {
        return $eng.'SetProfileProperties_'.(uc $1);
    }

    # there must be a croak to capture this case
    return '__getSetPropFuncName()__failed__';
}


# genSetPropertyFuncs
#   * generate new g_engSetUcodeProp.c (ex. g_engSetUcodeProp.c)
#   * if any change, copy and replace g_engSetUcodeProp.c in RM path
sub genSetPropertyFuncs {

    (@_ == 1) or croak 'usage: obj->genSetPropertyFuncs( )';

    my $self = shift;

                     # .../kernel/inc/pmu/bin/g_pmuSetUcodeProp.c
    my $rmFilePath  = File::Spec->catfile($self->{CLIENT}->{rmFileDir}, 'g_'.$eng.'SetUcodeProp.c');
                     # .../_out/g_tmp_pmuSetUcodeProp.c
    my $tmpFilePath = File::Spec->catfile($cfgArgs{'output-dir'}, 'g_tmp_'.$eng.'SetUcodeProp.c');

    my $pEng        = 'p'.(ucfirst $eng);   # pPmu
    my $OBJENG;

    if ($ENG eq 'PMU') {
        $OBJENG = (ucfirst $eng);           # Pmu
    } else {
        $OBJENG = 'OBJ'.$ENG;               # OBJPMU
    }

    open TMP_OUT_DEF, '>', $tmpFilePath    or fatal ("Cannot open output file : $tmpFilePath");

    # output the headers
    print TMP_OUT_DEF <<__CODE__;
/*!
 *  !! DO NOT EDIT !! DO NOT EDIT !! DO NOT EDIT !! DO NOT EDIT !!
 *
 *  The File was automatically generated by '$progname' script
 */

#include "rmprecomp_$eng.h"
#include "noprecomp.h"

/*!
 * Utility macros those match ${ENG}_TASK_IS_ENABLED and ${ENG}_FEATURE_IS_ENABLED
 * to facilitate code searching
 */
#define ${ENG}_PDB_SET_FEATURE(feature) $pEng->setProperty($pEng, PDB_PROP_${ENG}_GEN_FEATURE_##feature, LW_TRUE)
#define ${ENG}_PDB_SET_TASK(task) $pEng->setProperty($pEng, PDB_PROP_${ENG}_GEN_TASK_##task, LW_TRUE)


__CODE__

    foreach my $profile (@profiles) {

        my $funcName = $self->getSetPropFuncName($profile);
        next unless $funcName;

        my ($rmcfgChip) = $funcName =~ /_([A-Z0-9]+)$/;

        print TMP_OUT_DEF <<__CODE__;
#if RMCFG_CHIP_ENABLED($rmcfgChip)
/*!
 * Set RM property for profile '$profile'
 */
LW_STATUS
$funcName
(
    POBJGPU pGpu,
    $OBJENG *$pEng
)
{
__CODE__
        foreach my $feature (sort (@{$featuresByProfile{$profile} || [] })) {
            my $prop = $feature;

            $prop =~ s/^${ENG}_?//;   # remove the heading PMU or PMU_

            if ($prop !~ m/^TASK_/) {
                printf TMP_OUT_DEF ("    %-60s // %s \n", "${ENG}_PDB_SET_FEATURE($prop);", $feature);
            } else {
                $prop =~ s/^TASK_//;   # remove the heading TASK
                printf TMP_OUT_DEF ("    %-60s // %s \n", "${ENG}_PDB_SET_TASK($prop);", $feature);
            }
        }
        print TMP_OUT_DEF <<__CODE__;

    return LW_OK;
}
#endif

__CODE__
    }

    close TMP_OUT_DEF;

    diffAndP4Update($rmFilePath, $tmpFilePath);

    # delete the tmp file
    unlink $tmpFilePath                 unless $dbg_keepTmpFile;
}


# genFeaturePDBLwoc
#   * generate new g_engUcodePdb.h (ex. g_pmuUcodePdb.h)
#   * if any change, copy and replace g_pmuUcodePdb.h in RM path
sub genFeaturePDBLwoc {

    (@_ == 1) or croak 'usage: obj->genFeaturePDBLwoc( )';

    my $self = shift;
                      # .../kernel/inc/pmu/bin/g_pmuUcodePdb.h
    my $rmFilePath  = File::Spec->catfile($self->{CLIENT}->{rmFileDir}, 'g_'.$eng.'UcodePdb.h');
                      # .../_out/g_tmp_pmuUcodePdb.h
    my $tmpFilePath = File::Spec->catfile($cfgArgs{'output-dir'}, 'g_tmp_'.$eng.'UcodePdb.h');

    open TMP_OUT_DEF, '>', $tmpFilePath    or fatal ("Cannot open output file : $tmpFilePath");

    # output the headers
    print TMP_OUT_DEF <<__CODE__;
/*!
 *  !! DO NOT EDIT !! DO NOT EDIT !! DO NOT EDIT !! DO NOT EDIT !!
 *
 *  The File was automatically generated by '$progname' script
 */

/*!
 *  Properties mapped to $ENG ucode Features/Tasks.
 */

__CODE__

    # output GEN_FEATURE_*
    foreach my $feature (sort keys %validFeature_H) {
        my $prop = $feature;

        $prop =~ s/^${ENG}_?//;   # remove the heading PMU or PMU_
        $prop = 'FEATURE_'.$prop    if ($prop !~ m/^TASK_/);
        $prop = 'PDB_PROP_'.$ENG.'_GEN_'.$prop;     # add 'PDB_PROP_PMU_GEN_' prefix

        print TMP_OUT_DEF "LWOC_PROPERTY LwBool $prop;\n";
    }

    print TMP_OUT_DEF "\n";
    close TMP_OUT_DEF;

    diffAndP4Update($rmFilePath, $tmpFilePath);

    # delete the tmp file
    unlink $tmpFilePath                 unless $dbg_keepTmpFile;
}


sub diffAndP4Update {
    my $rmFilePath  = shift;
    my $tmpFilePath = shift;

    my $rmFileName = basename $rmFilePath;

    if ( compare($rmFilePath, $tmpFilePath) ) {

        if ( $cfgArgs{verbose} ) {
            # output file difference if diff is available
            my $diffResult = `diff $rmFilePath $tmpFilePath 2> $devnull`;

            # if diff returns valid result
            if ( $? != -1 && $diffResult ) {
                message("diff ".$eng.".def $tmpFilePath");
                print $diffResult;
            }
        }

        message("File content changed, updating $rmFileName");

        if ( ! -w $rmFilePath) {
            message("p4 edit $rmFilePath");

            if ($cfgArgs{p4} and $cfgArgs{p4} eq $P4) {
                # assume the script is called by makefile when --P4 is assigned
                # then we ignores the 'p4 edit' output
                system "$P4 edit $rmFilePath > $devnull";
                # note redirect to null device on win+lwmake always returns failure
                # so we don't check the return value '$?' here
            }
            else {
                system "$P4 edit $rmFilePath";
                fatal("'p4 edit $rmFileName' failed: $!")      if $?;
            }
        }

        copy($tmpFilePath, $rmFilePath)     or fatal("file copy failed: $!");
    }
    else {
        message("No update needed for $rmFileName");
    }

}

sub new
{
    (@_ == 2) or croak 'usage: CLASS->new( NAME )';

    my $class = shift;
    my $name  = shift;
    return bless { NAME => $name }, $class;
}

#
#   starts here!
#
sub run {
    my $self = shift;

    initGeneralData();

    parseFeatureInfo();

    $self->genSetPropertyFuncs();  # g_engSetUcodeProp.c

    # gen for LWOC migration
    $self->genFeaturePDBLwoc();    # g_engUcodePdb.h
}

1;
