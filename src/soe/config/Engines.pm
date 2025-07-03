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
#
# soe-config file that specifies all engines known to SOE applications
#

package Engines;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Engines is derived from 'Groups'

@ISA = qw(Groups);

#
# The actual engine definitions.
# This list contains all engines that soe-config is aware of.
#
# IMPORTANT: New source files must be added to both Engines.pm and Sources.def.
#

# soe-config does not lwrrently manage any engines yet, so this list has a temporary member below.
my $enginesRef = [

    IC =>
    {
        DESCRIPTION         => "Interrupt Controller",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "ic/limerock/soe_iclr10.c"      ,
                                "ic/lagunaseca/soe_icls10.c"    ,
                               ],
    },

    LSF =>
    {
        DESCRIPTION         => "Light Secure Falcon",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "lsf/lw/soe_objlsf.c"           ,
                                "lsf/limerock/soe_lsflr10.c"     ,
                               ],
    },

    SOE =>
    {
        DESCRIPTION         => "SOE",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "soe/lw/soe_objsoe.c"               ,
                                "soe/limerock/soe_soelr10.c"        ,
                                "soe/limerock/soe_queuelr10.c"      ,
                                "soe/limerock/soe_prierrlr10.c"     ,
                                "soe/limerock/soe_discoverylr10.c"  ,
                                'soe/limerock/soe_positionlr10.c'   ,
                                "soe/limerock/soe_dmalr10.c"        ,
                                "soe/limerock/soe_nxbarlr10.c"      ,
                                'soe/limerock/soe_privringlr10.c'   ,
                                "soe/lagunaseca/soe_dmals10.c"      ,
                                "soe/lagunaseca/soe_soels10.c"      ,
                                "soe/lagunaseca/soe_privringls10.c" ,
                                "soe/lagunaseca/soe_discoveryls10.c",
                            ],
    },

    TIMER =>
    {
        DESCRIPTION         => "Timer Engine",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "timer/lw/soe_objtimer.c"       ,
                                "timer/limerock/soe_timerlr10.c",
                               ],
    },

    RTTIMER =>
    {
        DESCRIPTION         => "RT Timer Engine",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "rttimer/lw/soe_objrttimer.c"       ,
                                "rttimer/limerock/soe_rttimerlr10.c",
                               ],
    },

    THERM =>
    {
        DESCRIPTION         => "Themal SOE",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "therm/lw/soe_objtherm.c"      ,
                                "therm/limerock/soe_thermlr10.c"      ,
                                "therm/lagunaseca/soe_thermls10.c"   ,
                               ],
    },

    BIF =>
    {
        DESCRIPTION         => "Bus Interface via SOE",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "bif/lw/soe_objbif.c"      ,
                                "bif/limerock/soe_biflr10.c"      ,
                                "bif/lagunaseca/soe_bifls10.c"    ,
                                "bif/lagunaseca/soe_lanemarginingls10.c"    ,
                               ],
    },

    CORE =>
    {
        DESCRIPTION         => "Core functionality offered by SOE",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "core/lw/soe_objcore.c"      ,
                                "core/limerock/soe_corelr10.c"      ,
                               ],
    },

    SAW =>
    {
        DESCRIPTION         => "SAW Block",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "saw/lw/soe_objsaw.c"      ,
                                "saw/limerock/soe_sawlr10.c"      ,
                               ],
    },

    INTR =>
    {
        DESCRIPTION         => "INTR Block",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "intr/lw/soe_objintr.c"      ,
                                "intr/limerock/soe_intrlr10.c"      ,
                                "intr/lagunaseca/soe_intrls10.c"    ,
                               ],
    },

    LWLINK =>
    {
        DESCRIPTION         => "LWLink Engine",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "lwlink/lw/soe_objlwlink.c"      ,
                                "lwlink/limerock/soe_lwlinklr10.c"      ,
                                "lwlink/lagunaseca/soe_lwlinkls10.c"    ,
                               ],
    },

    PMGR =>
    {
        DESCRIPTION         => "PMGR",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "pmgr/lw/soe_objpmgr.c"      ,
                                "pmgr/limerock/soe_pmgrlr10.c"      ,
                               ],
    },

    I2C =>
    {
        DESCRIPTION         => "I2C",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "i2c/lw/soe_obji2c.c"      ,
                                "i2c/limerock/soe_i2clr10.c"      ,
                               ],
    },

    GPIO =>
    {
        DESCRIPTION         => "GPIO",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "gpio/lw/soe_objgpio.c"      ,
                                "gpio/limerock/soe_gpiolr10.c"      ,
                                "gpio/lagunaseca/soe_gpiols10.c"    ,
                               ],
    },

    SPI =>
    {
        DESCRIPTION         => "SPI",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "spi/lw/soe_objspi.c"      ,
                                "spi/lw/soe_spirom.c"      ,
                                "spi/limerock/soe_spilr10.c"      ,
                                "spi/limerock/soe_eraseledgerlr10.c" ,
                               ],
    },

    SMBPBI =>
    {
        DESCRIPTION         => "SMBPBI protocol server",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "smbpbi/lw/soe_objsmbpbi.c"   ,
                                "smbpbi/lw/soe_smbpbi.c"      ,
                                "smbpbi/lw/soe_smbpbi_async.c",
                                "smbpbi/lw/soe_smbpbi_bundle.c",
                                "smbpbi/lw/soe_smbpbi_dem.c"  ,
                                "smbpbi/lw/soe_smbpbi_osfp.c" ,
                               ],
    },

    BIOS =>
    {
        DESCRIPTION         => "BIOS SOE",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "bios/lw/soe_objbios.c"      ,
                                "bios/limerock/soe_bioslr10.c"      ,
                               ],
    },

    GIN =>
    {
        DESCRIPTION         => "GIN Block",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "gin/lw/soe_objgin.c"            ,
                                "gin/lagunaseca/soe_ginls10.c"   ,
                               ],
    },

    IFR =>
    {
        DESCRIPTION         => "IFR",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "ifr/lw/soe_objifr.c"           ,
                                "ifr/limerock/soe_ifrlr10.c"    ,
                                "inforomfs/bitmap.c"             ,
                                "inforomfs/block.c"              ,
                                "inforomfs/flash.c"              ,
                                "inforomfs/fs.c"        ,
                                "inforomfs/map.c"       ,
                               ],
    },

];

# Create the item group
sub new {

    @_ == 1 or croak 'usage: obj->new()';

    my $type = shift;

    my $self = Groups->new("engine", $enginesRef);

    return bless $self, $type;
}

# end of the module
1;
