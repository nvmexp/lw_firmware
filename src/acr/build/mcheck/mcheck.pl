#!/usr/bin/perl
#
#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#
#
# File Name:   mcheck.pl
#
# Mcheck validates that the HAL is not sharing manual defines among shared code.
#
# See the wiki for full documentation
#    https://wiki.lwpu.com/engwiki/index.php/Mcheck
#

#===========================================================
package AcrMcheck;

use File::Basename;
use Carp qw(cluck croak);

BEGIN
{
    # HACK : add rm mcheck dir @INC path
    # for using mcheck moduels without '-Ic:/long/path/to/mcheck' in command line
    my $pwdDir = dirname __FILE__;

    # main mcheck files
    unshift @INC, $pwdDir.'/../../../../drivers/common/shared/mcheck';

    # rtos-flcn mcheck files
    unshift @INC, $pwdDir.'/../../../build/mcheck';

    # chip-config files
    unshift @INC, $pwdDir.'/../../../../drivers/common/chip-config';
}

use FlcnMcheck;
use comntools;

our @ISA;
@ISA = qw( FlcnMcheck );


#
# Initialize client info for when running mcheck on the RM HAL
#
sub setup_client_info
{
    my $self = shift;

    my %client_info = (
        # General client Info
        NAME            => 'ACR',
        CONFIG_FILE     => $self->{LW_ROOT}.'/uproc/acr/build/mcheck/mcheck.config',

        RMCFG_PATH      => $self->{LW_ROOT}.'/drivers/common/chip-config/chip-config.pl',
        RMCFG_HEADER    => 'acr-config.h',
        RMCFG_PROFILE   => '',  # to be filled in each run
        RMCFG_SRC_PATH  => $self->{LW_ROOT}.'/uproc/acr/src',
        RMCFG_ARGS      => '--mode acr-config '.
                           '--config '.$self->{LW_ROOT}.'/uproc/acr/config/acr-config.cfg ',

        SCAN_CPP_IMACROS => '',     # set from build_drfheader_flcn()

        # RTOS FLCN client info
        FLCN_TARGET         => 'acr',
        UCODE_ACCESS_FUNC   => 'acrGetBinArchive\w+',
    );

    $self->{CLIENT}   = \%client_info;
}

sub getProfileNameFromBinAccessFunc
{
    (@_ == 2) or croak 'usage: obj->getProfileNameFromBinAccessFunc( FUNC-NAME )';

    my ($self, $binAccessFunc) = @_;

    my $binFuncPrefix = $self->{CLIENT}->{UCODE_ACCESS_FUNC};

    $binAccessFunc =~ m/^($binFuncPrefix)_(.+)/;
    my $funcPostfix = $2;       # The hal provider name at function postfix, e.g. 'GF11X'

    if ($binAccessFunc =~ m/acrGetBinArchive(AcrLoad|GspFmodel|Sec2Fmodel)(Ahesasc|Asb)(\w*)_$funcPostfix/)
    {
        return if $binAccessFunc =~ m/(Sig|Alt|Old)/;

        # get profile information from Bindata access function
        my $acrBinName = lc $2;                  # Ahesasc/Asb
        my $profileType = "";
        my $acrFalcon = "";


        if ($acrBinName eq "ahesasc")
        {
            $acrFalcon = "sec2";
        }
        elsif ($acrBinName eq "asb")
        {
            $acrFalcon = "gsp";
        }

        if(length($3)!=0 && $3 ne "NonProductionBindata"){
                $profileType = lcfirst $3 || "";
                # colwert camel case to tags connected with '_'
                $profileType =~ s/([A-Z])/_$1/g;        # bsiBoot => bsi_boot
                $profileType = lc $profileType;
                $profileType = $acrBinName."_$profileType";
        }
        else {
                $profileType = $acrBinName;
        }

        if ($1 eq "Sec2Fmodel" || $1 eq "GspFmodel")
        {
            $profileType = "fmodel"."_$profileType";

        }

        $profile = "acr_${acrFalcon}-".(lc $funcPostfix)."_$profileType";
    }
    else
    {
        # only checks ACR ucode on PMU and SEC2
        return unless $binAccessFunc =~ m/acrGetBinArchive(Pmu|Sec2)(\w*)Ucode(\w*)_$funcPostfix/;

        # skips Signature, Alt and Old ucode files
        return if $binAccessFunc =~ m/(Sig|Alt|Old)/;

        # get profile information from Bindata access function
        my $acrFalcon = lc $1;                  # pmu/sec2
        my $profileType = lcfirst $2 || '';     # ucode/bsiBoot
        my $profilePrefix = $3;

        #For BsiLockdown -> bsi_lockdown
        if ($profileType ne "")
        {
            # colwert camel case to tags connected with '_'
            $profileType =~ s/([A-Z])/_$1/g;        # bsiBoot => bsi_boot
            $profileType = lc $profileType;
        }

        #For NewWprBlobs -> new_wpr_blobs
        if ($profilePrefix ne "" && $profilePrefix ne "NonProductionBindata")
        {
            # colwert camel case to tags connected with '_'
            $profilePrefix =~ s/([A-Z])/_$1/g;        # bsiBoot => bsi_boot
            $profilePrefix = lc $profilePrefix;
            $profileType .= $profilePrefix;
        }

        $profile = "acr_${acrFalcon}-".(lc $funcPostfix)."_$profileType";    # acr_pmu-gm20x_bsi_boot

        # CheetAh profiles removed from ACR.
        return if $profile eq 'acr_pmu-t210_load';
    }

    return $profile;
}

#===========================================================
package main;

use Cwd qw(abs_path);
use File::Basename;

# get $LW_ROOT from the relative path of this file 'mcheck.pl'
my $lwroot_from_pwd  = abs_path((dirname __FILE__).'/../../../..');
my $srcroot_from_pwd = (dirname __FILE__).'/../../src';

#
# Exelwtion starts here
#
our $MCHECK = AcrMcheck->new();

$MCHECK->init($lwroot_from_pwd,     # init hash {opt} and paths
              $srcroot_from_pwd);

$MCHECK->mcheck();

