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
# Version:     1.00
#
# Mcheck validates that the HAL is not sharing manual defines among shared code.
#
# See the wiki for full documentation
#    https://wiki.lwpu.com/engwiki/index.php/Mcheck
#

#===========================================================
package PmuMcheck;

use File::Basename;
use Carp qw(cluck croak);

BEGIN
{
    # HACK : add rm mcheck dir @INC path
    # for using mcheck moduels without '-Ic:/long/path/to/mcheck' in command line
    my $pwdDir = dirname __FILE__;

    # main mcheck files
    unshift @INC, $pwdDir.'/../../../drivers/common/shared/mcheck';

    # rtos-flcn mcheck files
    unshift @INC, $pwdDir.'/../../../uproc/build/mcheck';

    # chip-config files
    unshift @INC, $pwdDir.'/../../../drivers/common/chip-config';
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
        NAME            => 'PMU',
        CONFIG_FILE     => $self->{LW_ROOT}.'/pmu_sw/build/mcheck/mcheck.config',

        RMCFG_PATH      => $self->{LW_ROOT}.'/drivers/common/chip-config/chip-config.pl',
        RMCFG_HEADER    => 'pmu-config.h',
        RMCFG_PROFILE   => '',  # to be filled in each run
        RMCFG_SRC_PATH  => $self->{LW_ROOT}.'/pmu_sw/prod_app',
        RMCFG_ARGS      => '--mode pmu-config '.
                           '--config '.$self->{LW_ROOT}.'/pmu_sw/config/pmu-config.cfg ',

        SCAN_CPP_IMACROS => '',     # set from build_drfheader_flcn()

        # RTOS FLCN client info
        FLCN_TARGET         => 'pmu',
        UCODE_ACCESS_FUNC   => 'pmuGetBinArchiveUcodeFalcon|pmuGetBinArchiveUcodeDescRiscv',
    );

    $self->{CLIENT}   = \%client_info;
}

sub getProfileNameFromBinAccessFunc
{
    my ($self, $binAccessFunc) = @_;
    # calling default implemantation of getProfileNameFromBinAccessFunc()
    return $self->getProfileNameFromBinAccessFunc_default($binAccessFunc);
}

#===========================================================
package main;

use Cwd qw(abs_path);
use File::Basename;

# get $LW_ROOT from the relative path of this file 'mcheck.pl'
my $lwroot_from_pwd  = abs_path((dirname __FILE__).'/../../..');
my $srcroot_from_pwd = (dirname __FILE__).'/../../prod_app';

#
# Exelwtion starts here
#
our $MCHECK = PmuMcheck->new();

$MCHECK->init($lwroot_from_pwd,     # init hash {opt} and paths
              $srcroot_from_pwd);

$MCHECK->mcheck();

