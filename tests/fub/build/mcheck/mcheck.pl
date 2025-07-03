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
package FubMcheck;

use File::Temp qw/ tempfile tempdir tmpnam /;
use File::Basename;
use Carp qw(cluck croak);

BEGIN
{
    # HACK : add main mcheck dir @INC path
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
        NAME            => 'FUB',
        CONFIG_FILE     => $self->{LW_ROOT}.'/uproc/fub/build/mcheck/mcheck.config',

        RMCFG_PATH      => $self->{LW_ROOT}.'/drivers/common/chip-config/chip-config.pl',
        RMCFG_HEADER    => 'fub-config.h',
        RMCFG_PROFILE   => '',  # to be filled in each run
        RMCFG_SRC_PATH  => $self->{LW_ROOT}.'/uproc/fub/src',
        RMCFG_ARGS      => '--mode fub-config '.
                           '--config '.$self->{LW_ROOT}.'/uproc/fub/config/fub-config.cfg ',

        SCAN_CPP_IMACROS => '',     # set from build_drfheader_flcn()

        # RTOS FLCN client info
        FLCN_TARGET         => 'fub',
        UCODE_ACCESS_FUNC   => 'bspGetBinArchiveFubUcode',
    );

    $self->{CLIENT}   = \%client_info;
}

# override Mcheck::generate_headers
sub generate_headers
{
    my $self = shift;

    # fub cannot reuse rm register macros, that introduce several conflictions
    # $self->generate_headers_default();

    # build flcndrf.h and appand it to $self->{CLIENT}->{SCAN_CPP_IMACROS}
    $self->build_drfheader_flcn();

    $self->build_drfheader_fub();
}

sub build_drfheader_fub
{
    my $self = shift;

    my $drfheader_cpp_args;
    my $tmpHeaderDir = $self->get_tempfile_dir();

    print "build_drfheader_fub\n"       if $self->{opt}->{verbose};

    $drfheader_cpp_args = "-imacros $tmpHeaderDir/".$self->{CLIENT}->{RMCFG_HEADER}.' ' .
                          "-imacros ".$self->{LW_ROOT}."/sdk/lwpu/inc/lwmisc.h " .
                          "-dM ";

    # Create null temp file (/dev/null does not work)
    my $dummySrcFile;
    my $osname = $^O;
    if ($osname =~ m/cygwin/i)
    {
        $dummySrcFile =  File::Temp::tempnam('.','__tmp_').'.c';
    }
    else
    {
        $dummySrcFile = tmpnam() . '.c';
    }
    # create the empty file
    open DUMMY_FILE, ">$dummySrcFile";    close DUMMY_FILE;

    my $cmd = $self->{GCC}." -nostdinc -DMCHECK " .
              $self->{cfg}->{scan_cpp_defines}.' '.
              $drfheader_cpp_args.' '.
              "-P -E $dummySrcFile 2>/dev/null > $tmpHeaderDir/fubdrf.h";

    print "build_drfheader_fub system($cmd);\n" if $self->{opt}->{verbose};

    system($cmd) or die "drfheader system failed";

    unlink $dummySrcFile;

    $self->{CLIENT}->{SCAN_CPP_IMACROS} .= "-imacros $tmpHeaderDir/fubdrf.h ";
}

sub getProfileNameFromBinAccessFunc
{
    (@_ == 2) or croak 'usage: obj->getProfileNameFromBinAccessFunc( FUNC-NAME )';

    my ($self, $binAccessFunc) = @_;

    my $binFuncPrefix = $self->{CLIENT}->{UCODE_ACCESS_FUNC};
    
    $binAccessFunc =~ m/^($binFuncPrefix)_(.+)/;
    my $funcPostfix = $2;       # The hal provider name at function postfix, e.g. 'GF11X'

    my $profile = 'fub-lwdec_'.(lc $funcPostfix);   # fub-lwdec_tu10x

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
our $MCHECK = FubMcheck->new();

$MCHECK->init($lwroot_from_pwd,     # init hash {opt} and paths
              $srcroot_from_pwd);

$MCHECK->mcheck();
