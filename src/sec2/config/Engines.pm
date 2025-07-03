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
# sec2-config file that specifies all engines known to SEC2 applications
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
# This list contains all engines that sec2-config is aware of.
#
# IMPORTANT: New source files must be added to both Engines.pm and Sources.def.
#

# sec2-config does not lwrrently manage any engines yet, so this list has a temporary member below.
my $enginesRef = [

    IC =>
    {
        DESCRIPTION         => "Interrupt Controller",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "ic/maxwell/sec2_icgm20x.c"      ,
                                "ic/pascal/sec2_icgp10x.c"       ,
                                "ic/ampere/sec2_icga100.c"       ,
                                "ic/ampere/sec2_icga10x.c"       ,
                                "ic/hopper/sec2_icgh100.c"       ,
                               ],
    },

    LSF =>
    {
        DESCRIPTION         => "Light Secure Falcon",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "lsf/lw/sec2_objlsf.c"           ,
                                "lsf/maxwell/sec2_lsfgm20x.c"    ,
                                "lsf/pascal/sec2_lsfgp10x.c"     ,
                                "lsf/turing/lsftu10x.c"          ,
                               ],
    },

    LSR =>
    {
        DESCRIPTION         => "Light secure Riscv related operations ",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "lsr/ampere/lsrga100.c"          ,
                                "lsr/ampere/lsrga102.c"          ,
                                "lsr/hopper/lsrgh100.c"          ,
                               ],
    },

    SEC2 =>
    {
        DESCRIPTION         => "SEC2",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "sec2/lw/sec2_objsec2.c"                 ,
                                "sec2/maxwell/sec2_sec2gm20x.c"          ,
                                "sec2/pascal/sec2_sec2gp10x.c"           ,
                                "sec2/pascal/sec2_sec2gp10xgvxxx.c"      ,
                                "sec2/pascal/sec2_sec2gp10xga10x.c"      ,
                                "sec2/pascal/sec2_queuegp10x.c"          ,
                                "sec2/pascal/sec2_mutexgp10x.c"          ,
                                "sec2/pascal/sec2_prierrgp10x.c"         ,
                                "sec2/volta/sec2gv10x.c"                 ,
                                "sec2/volta/sec2_mutexgv10x.c"           ,
                                "sec2/turing/sec2tu10x.c"                ,
                                "sec2/turing/sec2tu10xonly.c"            ,
                                "sec2/turing/sec2tu116.c"                ,
                                "sec2/turing/sec2tu10xga10x.c",          ,
                                "sec2/ampere/sec2ga100.c"                ,
                                "sec2/ampere/sec2ga100only.c"            ,
                                "sec2/ampere/sec2ga10x.c"                ,
                                "sec2/ada/sec2ad10x.c"                   ,
                                "sec2/hopper/sec2gh100.c"                ,
                               ],
    },

    RTTIMER =>
    {
        DESCRIPTION         => "RT Timer Engine",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "rttimer/lw/sec2_objrttimer.c"       ,
                                "rttimer/pascal/sec2_rttimergp10x.c",
                               ],
    },

    GFE =>
    {
        DESCRIPTION         => "GFE",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "gfe/lw/gfe_ecid.c",
                                "gfe/lw/gfe_sha2.c",
                                "gfe/lw/gfe_bigint.c",
                                "gfe/lw/gfe_hs.c",
                                "gfe/pascal/sec2_gfegp10xga10x.c",
                                "gfe/hopper/sec2_gfegh100.c",
                               ],
    },

    HWV =>
    {
        DESCRIPTION         => "HW verification task",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "hwv/lw/hwv_perf.c",
                               ],
    },

    PR =>
    {
        DESCRIPTION         => "Pr Task",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "pr/lw/sec2_objpr.c",
                                "pr/lw/pr_lassahs.c",
                                "pr/lw/pr_lassahs_hs.c",
                                "pr/pascal/sec2_prgp10x.c",
                                "pr/pascal/sec2_prgp10xgv10x.c",
                                "pr/pascal/sec2_prmpkandcertgp10x.c",
                                "pr/volta/sec2_prgv10x.c",
                                "pr/volta/sec2_prmpkandcertgv10x.c",
                                "pr/turing/sec2_prtu10x.c",
                                "pr/turing/sec2_prmpkandcerttu10x.c",
                                "pr/turing/sec2_prmpkandcerttu116.c",
                                "pr/ampere/sec2_prmpkandcertga10x.c",
                                "pr/ada/sec2_prmpkandcertad10x.c",
                                "pr/ada/sec2_prad10x.c",
                                "pr/ampere/sec2_prga10x.c",
                               ],
    },

    VPR =>
    {
        DESCRIPTION         => "VPR task",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "vpr/lw/vpr_mthds.c",
                                "vpr/pascal/sec2_vprgp10x.c",
                                "vpr/pascal/sec2_vprgp10xonly.c",
                                "vpr/pascal/sec2_vprgp10xgv10xonly.c",
                                "vpr/pascal/sec2_vprgp107.c",
                                "vpr/volta/sec2_vprgv100.c",
                                "vpr/volta/sec2_vprgv100only.c",
                                "vpr/turing/sec2_vprtu10x.c",
                                "vpr/turing/sec2_vprtu10xonly.c",
                                "vpr/ampere/sec2_vprga100.c",
                                "vpr/ampere/sec2_vprga10x.c",
                                "vpr/ada/sec2_vprad10x.c",
                                "vpr/hopper/sec2_vprgh20x.c",
                                "sec2/pascal/sec2_vprpoliciesgp10x.c",
                                "sec2/volta/sec2_vprpoliciesgv10x.c",
                                "sec2/volta/sec2_vprpoliciesgv10xonly.c",
                                "sec2/turing/sec2_vprpoliciestu10x.c",
                               ],
    },

    ACR =>
    {
        DESCRIPTION        => "ACR Engine",
        DEFAULT_STATE      => DISABLED,
        SRCFILES           => [
                               "acr/pascal/sec2_acrgp10x.c",
                               "acr/turing/sec2_acrtu10x.c",
                               "acr/pascal/sec2_acrgp10xgv100only.c",
                               "acr/ampere/sec2_acrga100.c",
                               "acr/ampere/sec2_acrga10x.c",
                               "acr/hopper/sec2_acrgh100.c",
                              ],
    },

    APM =>
    {
        DESCRIPTION         => "Ampere Protected Memory Engine",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "apm/lw/apm_keys.c",
                                "apm/lw/sec2_apmencryptdma.c",
                                "apm/lw/sec2_apmscpcrypt.c",
                                "apm/lw/apm_rts.c",
                                "apm/ampere/sec2_apmga100.c",
                                "apm/lw/sec2_apmrtm.c",
                                "apm/ampere/sec2_apmrtmga100.c",
                               ],
    },

    SPDM =>
    {
        DESCRIPTION         => "SPDM Task Engine",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "signcert/lw/sign_cert.c",
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
