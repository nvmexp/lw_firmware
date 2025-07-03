#!/usr/bin/elw perl

#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#


package Sec2GenRmProp;

use File::Basename;
use File::Spec;
use Carp qw(cluck croak);

BEGIN
{
    my $pwdDir = dirname __FILE__;

    # main uproc script dir, for accessing shared genRmProp.pm
    unshift @INC, $pwdDir.'/../../build/scripts';

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
                                'uproc', 'sec2', 'config');

    # directory path of the generated files
    $client{rmFileDir} = File::Spec->catfile($self->{LW_ROOT},
                                'drivers', 'resman', 'kernel',
                                'inc', 'sec2', 'bin');

    # $srcRootPath must be UNIX format
    my @tmp = split /\\/, $self->{LW_ROOT};
    $client{srcRootPath} = File::Spec::Unix->catfile( @tmp ,
                                'uproc', 'sec2', 'src');


    $self->{CLIENT}   = \%client;
}

# getSetPropFuncName
# e.g.
sub getSetPropFuncName {

    (@_ == 2) or croak 'usage: obj->getSetPropFuncName( PROFILE-NAME )';

    my $self    = shift;
    my $profile = shift;

    my $eng = lc $self->{NAME};    # sec2

    if ($profile eq 'sec2-gp10x_nouveau'        ||
        $profile eq 'sec2-ga100_nouveau'        ||
        $profile eq 'sec2-ga10x_nouveau_boot_from_hs'        ||
        $profile eq 'sec2-tu10x_dualcore'       ||
        $profile eq 'sec2-tu116_dualcore'       ||
        $profile eq 'sec2-tu116_prmods'         ||
        $profile eq 'sec2-ga10x_prmods')
    {
        return undef;
    }

    # AUTO profiles
    if ($profile eq 'sec2-tu10a') {
        return $eng.'SetProfilePropertiesAuto_TU104';
    }

    # PR MODS profiles
    my $patternPrmods = $eng.'\-([a-z0-9]+)_prmods';
    if ($profile =~ m/^$patternPrmods$/) {
        my $hal = $1;
        $hal = 'gp102' if ($hal eq 'gp10x');
        $hal = 'tu102' if ($hal eq 'tu10x');
        return $eng.'SetProfilePropertiesPrmods_'.(uc $hal);
    }

    # RISCV_LS profiles
    my $patternRiscvLs = $eng.'\-([a-z0-9]+)_riscv_ls';
    if ($profile =~ m/^$patternRiscvLs$/) {
        return $eng.'SetProfilePropertiesRiscvLs_'.(uc $1);
    }

    # APM profiles
    my $patternApm = $eng.'\-([a-z0-9]+)_apm';
    if ($profile =~ m/^$patternApm$/) {
        return $eng.'SetProfilePropertiesApm_'.(uc $1);
    }

    # NEW_WPR_BLOB profiles
    my $patternNewWprBlob = $eng.'\-([a-z0-9]+)_new_wpr_blob';
    if ($profile =~ m/^$patternNewWprBlob$/) {
        return $eng.'SetProfilePropertiesNewWprBlob_'.(uc $1);
    }

    # BOOT_FROM_HS profiles
    my $patternBootFromHs = $eng.'\-([a-z0-9]+)_boot_from_hs';
    if ($profile =~ m/^$patternBootFromHs$/) {
        return $eng.'SetProfilePropertiesBootFromHs_'.(uc $1);
    }

    # PR44 ALT IMAGE PROFILE
    my $patternPr44AltImgBfhs = $eng.'\-([a-z0-9]+)_pr44_alt_img_boot_from_hs';
    if ($profile =~ m/^$patternPr44AltImgBfhs$/) {
        return $eng.'SetProfilePropertiesPr44AltImgBootFromHs_'.(uc $1);
    }

    # PRMODS BOOT_FROM_HS profiles
    my $patternPrmodsBootFromHs = $eng.'\-([a-z0-9]+)_prmods_boot_from_hs';
    if ($profile =~ m/^$patternPrmodsBootFromHs$/) {
        return $eng.'SetProfilePropertiesPrmodsBootFromHs_'.(uc $1);
    }

    # APM_BOOT_FROM_HS profiles
    my $patternApmBootFromHs = $eng.'\-([a-z0-9]+)_apm_boot_from_hs';
    if ($profile =~ m/^$patternApmBootFromHs$/) {
        return $eng.'SetProfilePropertiesApmBootFromHs_'.(uc $1);
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
my $lwroot_from_pwd  = abs_path((dirname __FILE__).'/../../..');

#
# Exelwtion starts here
#
our $GENPROP = Sec2GenRmProp->new('SEC2');

$GENPROP->setup($lwroot_from_pwd);

$GENPROP->run();

1;

