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
                                "syslib/ic/lagunaseca/soe_icls10.c",
                               ],
    },

    LSF =>
    {
        DESCRIPTION         => "Light Secure Falcon",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/lsf/lw/soe_objlsf.c",
                                "syslib/lsf/lagunaseca/lsfls10.c",
                               ],
    },

    SOE =>
    {
        DESCRIPTION         => "SOE RISCV Engine ",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/soe/lagunaseca/soe_soels10.c",
                                "syslib/soe/lagunaseca/soe_discoveryls10.c",
                                "syslib/soe/lagunaseca/soe_positionls10.c",
                                "syslib/soe/lagunaseca/soe_privringls10.c",
                                "syslib/soe/lagunaseca/soe_dmals10.c",
                               ],
    },

    THERM =>
    {
        DESCRIPTION         => "SOE RISCV Engine ",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/therm/lagunaseca/soe_thermls10.c",
                               ],
    },

    INTR =>
    {
        DESCRIPTION         => "SOE RISCV Engine ",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/intr/lagunaseca/soe_intrls10.c",
                               ],
    },

    CORE =>
    {
        DESCRIPTION         => "SOE RISCV Engine ",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/core/lagunaseca/soe_corels10.c",
                               ],
    },

    SPI =>
    {
        DESCRIPTION         => "SOE RISCV Engine ",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/spi/lw/soe_spirom.c",
                                "syslib/spi/lagunaseca/soe_eraseledgerls10.c",
                                "syslib/spi/lagunaseca/soe_spils10.c",
                               ],
    },

    LWLINK =>
    {
        DESCRIPTION         => "SOE RISCV Engine ",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/lwlink/lagunaseca/soe_lwlinkls10.c",
                               ],
    },

    SMBPBI =>
    {
        DESCRIPTION         => "SMBPBI protocol server",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/smbpbi/lw/soe_objsmbpbi.c"  ,
                                "syslib/smbpbi/lw/soe_smbpbi.c"      ,
                                "syslib/smbpbi/lw/soe_smbpbi_async.c",
                                "syslib/smbpbi/lw/soe_smbpbi_bundle.c",
                                "syslib/smbpbi/lw/soe_smbpbi_dem.c"  ,
                                "syslib/smbpbi/lw/soe_smbpbi_osfp.c" ,
                                "syslib/smbpbi/lagunaseca/soe_smbpbils10.c",
                                "syslib/smbpbi/lagunaseca/soe_smbpbieccls10.c",
                                "syslib/smbpbi/lagunaseca/soe_smbpbilwlinkls10.c",
                               ],
    },

    BIF =>
    {
        DESCRIPTION         => "SOE RISCV Engine ",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/bif/lagunaseca/soe_bifls10.c",
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
