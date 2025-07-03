#!/usr/bin/elw perl

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#


package PmuGenRmProp;

use File::Basename;
use File::Spec;
use Carp qw(cluck croak);

BEGIN
{
    my $pwdDir = dirname __FILE__;

    # main uproc script dir, for accessing shared genRmProp.pm
    unshift @INC, $pwdDir.'/../../uproc/build/scripts';

    # build dir (local dir) for this client
    unshift @INC, $pwdDir;

}

use GenRmProp;

our @ISA;
@ISA = qw( GenRmProp );

#
# Initialize client info for when running mcheck on the RM HAL
#
sub setupClientInfo
{
    (@_ == 1) or croak 'usage: obj->setupClientInfo( )';

    my $self = shift;

    my %client;

    $client{flcnCfgDir} = File::Spec->catfile($self->{LW_ROOT},
                                'pmu_sw', 'config');

    # directory path of the generated files
    $client{rmFileDir} = File::Spec->catfile($self->{LW_ROOT},
                                'drivers', 'resman', 'kernel',
                                'inc', 'pmu', 'bin');

    # $srcRootPath must be UNIX format
    my @tmp = split /\\/, $self->{LW_ROOT};
    $client{srcRootPath} = File::Spec::Unix->catfile( @tmp ,
                                'pmu_sw', 'prod_app');


    $self->{CLIENT}   = \%client;
}

# getSetPropFuncName
# e.g.
sub getSetPropFuncName {

    (@_ == 2) or croak 'usage: obj->getSetPropFuncName( PROFILE-NAME )';

    my $self    = shift;
    my $profile = shift;

    my $eng = lc $self->{NAME};    # pmu

    # Self-Init test profile
    if ($profile eq 'pmu-ga10x_selfinit_riscv') {
        return $eng.'SetProfilePropertiesSelfinit_GA10X';
    }

    # RISCV profiles
    my $patternRiscv = $eng.'\-([a-z0-9]+)_riscv';
    if ($profile =~ m/^$patternRiscv$/) {
        return $eng.'SetProfilePropertiesRiscv_'.(uc $1);
    }

    my $defuleName = GenRmProp::getSetPropFuncName_default($profile);

    # throw an error when new added profiles cannot be handled by the current code
    # user who add the new profile should update the implementation in getSetPropFuncName().
    die "getSetPropFuncName() cannot handle profile '$profile'" if ($defuleName =~ m/__failed__$/);

    return $defuleName;
}


#===========================================================
package main;

use Cwd qw(abs_path);
use File::Basename;

# get $LW_ROOT from the relative path of this file 'genRmProp.pl'
my $lwroot_from_pwd  = abs_path((dirname __FILE__).'/../..');

#
# Exelwtion starts here
#
our $GENPROP = PmuGenRmProp->new('PMU');

$GENPROP->setup($lwroot_from_pwd);

$GENPROP->run();

1;

