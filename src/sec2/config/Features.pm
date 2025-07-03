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
# sec2-config file that specifies all known sec2-config features
#

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

use chipGroups;                 # imports CHIPS from Chips.pm and chip functions

@ISA = qw(Groups);

#
# This list contains all sec2-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and SEC2
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_SEC2 =>
    {
       DESCRIPTION         => "Running on the SEC2",
       DEFAULT_STATE       => DISABLED,
    },

    # target architecture
    ARCH_UNKNOWN =>
    {
       DESCRIPTION         => "unknown arch",
       DEFAULT_STATE       => DISABLED,
    },
    ARCH_FALCON =>
    {
       DESCRIPTION         => "FALCON architecture",
       DEFAULT_STATE       => DISABLED,
    },

    # This feature is special.  It and the SEC2CORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    SEC2CORE_BASE =>
    {
       DESCRIPTION         => "SEC2CORE Base",
       SRCFILES            => [
                               "ic/lw/sec2_objic.c"   ,
                               "sec2/lw/sec2_common_hs.c",
                               "os/lw/sec2_os.c"      ,
                               "main.c"               ,
                              ],
    },

    SEC2CORE_GP10X =>
    {
       DESCRIPTION         => "SEC2CORE for GP10X",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2CORE_GV10X =>
    {
       DESCRIPTION         => "SEC2CORE for GV10X",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2CORE_TU10X =>
    {
       DESCRIPTION         => "SEC2CORE for TU10X",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2CORE_GA10X =>
    {
       DESCRIPTION         => "SEC2CORE for GA10X",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2CORE_AD10X =>
    {
       DESCRIPTION         => "SEC2CORE for AD10X",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2CORE_GH10X =>
    {
       DESCRIPTION         => "SEC2CORE for GH10X",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2CORE_GH20X =>
    {
       DESCRIPTION         => "SEC2CORE for GH20X",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2CORE_G00X =>
    {
       DESCRIPTION         => "SEC2CORE for G00X",
       DEFAULT_STATE       => DISABLED,
    },

    DUAL_CORE =>
    {
       DESCRIPTION         => "Whether the build belongs a Dual core design",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2TASK_CMDMGMT =>
    {
       DESCRIPTION         => "SEC2 Command Management Task (CORE)",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_cmdmgmt.c" ],
    },

    SEC2TASK_CHNMGMT =>
    {
       DESCRIPTION         => "SEC2 Channel Management Task (CORE)",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_chnmgmt.c" ],
    },

    SEC2TASK_WKRTHD =>
    {
       DESCRIPTION         => "SEC2 Worker Thread Task (CORE)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "task_wkrthd.c" ],
    },

    SEC2TASK_HDCPMC =>
    {
       DESCRIPTION         => "Miracast HDCP Task",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "task_hdcpmc.c",
                               "hdcpmc/lw/hdcpmc_crypt.c",
                               "hdcpmc/lw/hdcpmc_data.c",
                               "hdcpmc/lw/hdcpmc_mem.c",
                               "hdcpmc/lw/hdcpmc_mthds.c",
                               "hdcpmc/lw/hdcpmc_pvtbus.c",
                               "hdcpmc/lw/hdcpmc_rsa.c",
                               "hdcpmc/lw/hdcpmc_scp.c",
                               "hdcpmc/lw/hdcpmc_session.c",
                               "hdcpmc/lw/hdcpmc_srm.c",
                              ],
    },

    SEC2_HDCPMC_VERIF =>
    {
        DESCRIPTION        => "Enables verification mode of the Miracast HDCP task",
        DEFAULT_STATE      => DISABLED,
        REQUIRES           => SEC2TASK_HDCPMC,
    },

    SEC2TASK_GFE =>
    {
       DESCRIPTION         => "GFE Task",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "task_gfe.c" ],
    },

    SEC2TASK_VPR =>
    {
       DESCRIPTION         => "VPR Task",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "task_vpr.c",
                               "vpr/lw/vpr_mthds.c",
                              ],
    },

    SEC2TASK_LWSR =>
    {
       DESCRIPTION         => "LWSR Task",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "task_lwsr.c",
                               "lwsr/lw/lwsr_mutex.c",
                               "lwsr/lw/lwsr_hs.c",
                              ],
    },

    SEC2TASK_PR =>
    {
       DESCRIPTION         => "PlayReady Feature",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "task_pr.c",
                               "pr/v0300/constants/drmconstants.c",
                               "pr/v0300/constants/drmversionconstants.c",
                               "pr/v0300/modules/bcertformat/common/drmbcertformatcommon.c",
                               "pr/v0300/modules/bcertformat/builder/drmbcertformatbuilder.c",
                               "pr/v0300/modules/bcertformat/parser/drmbcertformatparser.c",
                               "pr/v0300/modules/crt/real/crtimplreal.c",
                               "pr/v0300/modules/hashcache/drmhashcache.c",
                               "pr/v0300/modules/mathsafe/real/drmmathsafe.c",
                               "pr/v0300/modules/nonceverify/real/drmnonceverify.c",
                               "pr/v0300/modules/stkalloc/real/drmstkallocimplreal.c",
                               "pr/v0300/modules/utf/real/drmutf.c",
                               "pr/v0300/modules/utilities/id/drmutilitiesid.c",
                               "pr/v0300/modules/xbinary/real/drmxb.c",
                               "pr/v0300/modules/xbinary/real/drmxbbuilder.c",
                               "pr/v0300/modules/xbinary/real/drmxbparser.c",
                               "pr/v0300/modules/xmrformat/common/drmxmrformat.c",
                               "pr/v0300/modules/xmrformat/parser/drmxmrformatparser.c",
                               "pr/v0300/oem/common/AES/aes128.c",
                               "pr/v0300/oem/common/AES/aesbox.c",
                               "pr/v0300/oem/common/AES/oemaes.c",
                               "pr/v0300/oem/common/aeskeywrap/oemaeskeywrap.c",
                               "pr/v0300/oem/common/aesmulti/oemaesmulti.c",
                               "pr/v0300/oem/common/bignum/bignum.c",
                               "pr/v0300/oem/common/bignum/bignumnoinline.c",
                               "pr/v0300/oem/common/bignum/divide.c",
                               "pr/v0300/oem/common/bignum/ecex2001.c",
                               "pr/v0300/oem/common/bignum/ecppq.c",
                               "pr/v0300/oem/common/bignum/elwrve.c",
                               "pr/v0300/oem/common/bignum/field.c",
                               "pr/v0300/oem/common/bignum/kinitpr.c",
                               "pr/v0300/oem/common/bignum/kilwert.c",
                               "pr/v0300/oem/common/bignum/kmuladd.c",
                               "pr/v0300/oem/common/bignum/ksqrtpr.c",
                               "pr/v0300/oem/common/bignum/modexp.c",
                               "pr/v0300/oem/common/bignum/modmulch1.c",
                               "pr/v0300/oem/common/bignum/modsqrt.c",
                               "pr/v0300/oem/common/bignum/modular.c",
                               "pr/v0300/oem/common/bignum/mpaddsubcmp.c",
                               "pr/v0300/oem/common/bignum/mpalloc.c",
                               "pr/v0300/oem/common/bignum/mpgcdex.c",
                               "pr/v0300/oem/common/bignum/mpmul22.c",
                               "pr/v0300/oem/common/bignum/mprand.c",
                               "pr/v0300/oem/common/ecc/base/oemeccp256.c",
                               "pr/v0300/oem/common/ecc/base/oemeccp256verify.c",
                               "pr/v0300/oem/common/ecc/baseimpl/oemeccinitimpl.c",
                               "pr/v0300/oem/common/ecc/baseimpl/oemeccp256impl.c",
                               "pr/v0300/oem/common/ecc/baseimpl/oemeccp256verifyimpl.c",
                               "pr/v0300/oem/common/parsers/oemparsers_bcrl.c",
                               "pr/v0300/oem/common/parsers/oemparsers_revinfo.c",
                               "pr/v0300/oem/common/sha/sha256/oemsha256.c",
                               "pr/v0300/oem/common/trustedexec/oembroker.c",
                               "pr/v0300/oem/common/trustedexec/oembrokerlprov.c",
                               "pr/v0300/oem/common/trustedexec/oembrokeraes.c",
                               "pr/v0300/oem/common/trustedexec/oembrokertype.c",
                               "pr/v0300/oem/common/trustedexec/oemcommonmem_lw.c",
                               "pr/v0300/oem/common/trustedexec/oemcommonlw.c",
                               "pr/v0300/oem/common/trustedexec/oemtee.c",
                               "pr/v0300/oem/common/trustedexec/oemteeaes128ctr.c",
                               "pr/v0300/oem/common/trustedexec/oemteeaes128ctrpolicy.c",
                               "pr/v0300/oem/common/trustedexec/oemteeclock.c",
                               "pr/v0300/oem/common/trustedexec/oemteecritsec.c",
                               "pr/v0300/oem/common/trustedexec/oemteecrypto.c",
                               "pr/v0300/oem/common/trustedexec/oemteecryptoaes128block.c",
                               "pr/v0300/oem/common/trustedexec/oemteecryptoaes128keywrap.c",
                               "pr/v0300/oem/common/trustedexec/oemteecryptoaes128multi.c",
                               "pr/v0300/oem/common/trustedexec/oemteecryptoecc256.c",
                               "pr/v0300/oem/common/trustedexec/oemteecryptorand.c",
                               "pr/v0300/oem/common/trustedexec/oemteecryptosha256.c",
                               "pr/v0300/oem/common/trustedexec/oemteedom.c",
                               "pr/v0300/oem/common/trustedexec/oemteekeywrap.c",
                               "pr/v0300/oem/common/trustedexec/oemteelprov.c",
                               "pr/v0300/oem/common/trustedexec/oemteelprovcert.c",
                               "pr/v0300/oem/common/trustedexec/oemteelprovcerttemplate.c",
                               "pr/v0300/oem/common/trustedexec/oemteeversion.c",
                               "pr/v0300/oem/linux/oemimplrand.c",
                               "pr/v0300/oem/linux/oemmodel.c",
                               "pr/v0300/oem/linux/oemtime.c",
                               "pr/v0300/trustedexec/aes128ctr/drmteeaes128ctr.c",
                               "pr/v0300/trustedexec/base/drmteebase.c",
                               "pr/v0300/trustedexec/base/drmteekbcryptodata.c",
                               "pr/v0300/trustedexec/base/drmteexb.c",
                               "pr/v0300/trustedexec/debug/drmteedebug.c",
                               "pr/v0300/trustedexec/dom/drmteedom.c",
                               "pr/v0300/trustedexec/h264/drmteeh264.c",
                               "pr/v0300/trustedexec/licprep/drmteelicprep.c",
                               "pr/v0300/trustedexec/lprov/drmteelprov.c",
                               "pr/v0300/trustedexec/lprovcerttemplate/drmteelprovcerttemplate.c",
                               "pr/v0300/trustedexec/revocation/drmteerevocation.c",
                               "pr/v0300/trustedexec/sampleprot/drmteesampleprot.c",
                               "pr/v0300/trustedexec/sign/drmteesign.c",
                               "pr/v0300/trustedexec/teeproxystub/common/drmteeproxystubcommon.c",
                               "pr/v0300/trustedexec/teeproxystub/stub/common/drmteecache.c",
                               "pr/v0300/trustedexec/teeproxystub/stub/common/drmteestubcommon_generated.c",
                               "pr/v0300/trustedexec/teeproxystub/stub/common/drmteestubcommon.c",
                               "pr/v0300/trustedexec/teeproxystub/stub/licgen/stub/drmteestublicgenunsupported.c",
                               "pr/v0300/trustedexec/teeproxystub/stub/prndrx/stub/drmteeprndrxunsupported.c",
                               "pr/v0300/trustedexec/teeproxystub/stub/prndtx/stub/drmteeprndtxunsupported.c",
                               "pr/v0300/trustedexec/teeproxystub/stub/rprov/stub/drmteerprovunsupported.c",
                               "pr/v0300/trustedexec/teeproxystub/stub/selwrestop/stub/drmteeselwrestopunsupported.c",
                              ],
    },

    SEC2JOB_HS_SWITCH =>
    {
       DESCRIPTION         => "SEC2 HS Switch Job (dummy job to test LS/HS switching)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "job_hs_switch.c" ],
    },

    SEC2JOB_FAKEIDLE_TEST =>
    {
       DESCRIPTION         => "SEC2 fake idle test Job (dummy job to test SEC2 fake idle)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "job_fakeidle_test.c" ],
    },

    SEC2JOB_RTTIMER_TEST =>
    {
       DESCRIPTION         => "SEC2 RTTIMER test Job (dummy job to test SEC2 RTTimer)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "job_rttimer_test.c" ],
    },

    SEC2JOB_MSCG_TEST    =>
    {
       DESCRIPTION         => "SEC2 MSCG wake-up test (core support). Includes priv blocker subtest.",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    SEC2JOB_MSCG_FBBLOCKER_TEST =>
    {
       DESCRIPTION         => "SEC2 MSCG wake-up fb blocker subtest.",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    SEC2JOB_TIMER    =>
    {
       DESCRIPTION         => "Job to update the GPTMR when the SEC2 clock freq changes",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    DYNAMIC_FLCN_PRIV_LEVEL =>
    {
       DESCRIPTION         => "Dynamic Flcn privilege level",
       DEFAULT_STATE       => ENABLED,
    },

    DBG_PRINTF_FALCDEBUG =>
    {
       DESCRIPTION         => "Redirect SEC2 Debug Spew to falc_debug",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2TASK_HWV =>
    {
       DESCRIPTION         => "HWVerif Task",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "task_hwv.c" ],
    },

    SEC2RTTIMER_FOR_OS_TICKS =>
    {
       DESCRIPTION         => "Use RT-Timer for OS scheduler",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SEC2TASK_ACR =>
    {
        DESCRIPTION        => "ACR Task",
        DEFAULT_STATE      => DISABLED,
        SRCFILES           => [ "task_acr.c" ]
    },

    SEC2TASK_APM =>
    {
       DESCRIPTION         => "Ampere Protected Memory Task",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "task_apm.c" ]
    },

    SEC2TASK_SPDM =>
    {
       DESCRIPTION         => "SPDM Task",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "task_spdm.c",
                                "spdm/ampere/sec2_spdmga100.c" ]
    },

    BUG_200272812_HALT_INTR_MASK_WAR =>
    {
        DESCRIPTION        => "To track Halt interrupt mask WAR added for bug 200272812",
        DEFAULT_STATE      => DISABLED,
    },

    SEC2_INIT_HS =>
    {
        DESCRIPTION        => "To enable/disable initHs and cleanupHs overlays",
        DEFAULT_STATE      => ENABLED,
    },

    SEC2_ENCRYPT_DEVID =>
    {
        DESCRIPTION        => "Encrypt DEVID and derive DEVID based key",
        DEFAULT_STATE      => DISABLED,
    },

    SEC2_USE_MUTEX =>
    {
        DESCRIPTION        => "MUTEX Support",
        DEFAULT_STATE      => ENABLED,
    },

    SEC2_AUTO_PROFILE =>
    {
        DESCRIPTION        => "Used to do AUTO profile specific Settings",
        DEFAULT_STATE      => DISABLED,
    },

    SEC2_NOTIFICATION_INTR_GFID_PROGRAMMING =>
    {
        DESCRIPTION        => "The GFID to which a semaphore release notification is to be sent is programmed by the ucode",
        DEFAULT_STATE      => DISABLED,
    },

    SEC2_GPC_RG_SUPPORT =>
    {
        DESCRIPTION        => "This controls the GPC-RG support from SEC2 side to bootstrap GPCCS falcon",
        DEFAULT_STATE      => DISABLED,
    },

    SEC2_POSTED_WRITE_SUPPORT =>
    {
        DESCRIPTION        => "Controls SEC2 Posted Write Support",
        DEFAULT_STATE      => DISABLED,
        SRCFILES           => [ "sec2/ampere/sec2_posted_write_ga10x.c" ],
    },

    SEC2_USE_PR44 =>
    {
        DESCRIPTION        => "Playready 4.4",
        DEFAULT_STATE      => DISABLED,
        SRCFILES            => [
                               "pr/v0404/constants/drmconstants.c",
                               "pr/v0404/constants/drmversionconstants.c",
                               "pr/v0404/modules/base64/real/drmbase64implreal.c",
                               "pr/v0404/modules/bcertformat/common/drmbcertformatcommon.c",
                               "pr/v0404/modules/bcertformat/parser/drmbcertformatparser.c",
                               "pr/v0404/modules/crt/real/crtimplreal.c",
                               "pr/v0404/modules/hashcache/drmhashcache.c",
                               "pr/v0404/modules/mathsafe/real/drmmathsafe.c",
                               "pr/v0404/modules/nonceverify/real/drmnonceverify.c",
                               "pr/v0404/modules/rivcrlparser/real/drmcrlparser.c",
                               "pr/v0404/modules/rivcrlparser/real/drmrivparser.c",
                               "pr/v0404/modules/stkalloc/real/drmstkalloc.c",
                               "pr/v0404/modules/strsafe/real/drmstrsafeimplreal.c",
                               "pr/v0404/modules/utf/real/drmutf.c",
                               "pr/v0404/modules/utilities/id/drmutilitiesid.c",
                               "pr/v0404/modules/xbinary/real/drmxb.c",
                               "pr/v0404/modules/xbinary/real/drmxbbuilder.c",
                               "pr/v0404/modules/xbinary/real/drmxbparser.c",
                               "pr/v0404/modules/xmrformat/common/drmxmrformat.c",
                               "pr/v0404/modules/xmrformat/parser/drmxmrformatparser.c",
                               "pr/v0404/oem/common/broker/oembrokeraes.c",
                               "pr/v0404/oem/common/broker/oembrokerbase.c",
                               "pr/v0404/oem/common/broker/oembrokersha.c",
                               "pr/v0404/oem/common/broker/oembrokertype.c",
                               "pr/v0404/oem/common/broker/pkImpl/oembrokereccverify_pk.c",
                               "pr/v0404/oem/common/crypto/aes/aes128.c",
                               "pr/v0404/oem/common/crypto/aes/aesbox.c",
                               "pr/v0404/oem/common/crypto/aes/oemaes.c",
                               "pr/v0404/oem/common/crypto/aeskeywrap/oemaeskeywrap.c",
                               "pr/v0404/oem/common/crypto/aesmulti/oemaesecb.c",
                               "pr/v0404/oem/common/crypto/aesmulti/oemaeskdf.c",
                               "pr/v0404/oem/common/crypto/aesmulti/oemaesomac1.c",
                               "pr/v0404/oem/common/crypto/bignum/bignum.c",
                               "pr/v0404/oem/common/crypto/bignum/bignumnoinline.c",
                               "pr/v0404/oem/common/crypto/bignum/divide.c",
                               "pr/v0404/oem/common/crypto/bignum/ecex2001.c",
                               "pr/v0404/oem/common/crypto/bignum/ecppq.c",
                               "pr/v0404/oem/common/crypto/bignum/elwrve.c",
                               "pr/v0404/oem/common/crypto/bignum/field.c",
                               "pr/v0404/oem/common/crypto/bignum/kinitpr.c",
                               "pr/v0404/oem/common/crypto/bignum/kilwert.c",
                               "pr/v0404/oem/common/crypto/bignum/kmuladd.c",
                               "pr/v0404/oem/common/crypto/bignum/ksqrtpr.c",
                               "pr/v0404/oem/common/crypto/bignum/modexp.c",
                               "pr/v0404/oem/common/crypto/bignum/modmulch1.c",
                               "pr/v0404/oem/common/crypto/bignum/modsqrt.c",
                               "pr/v0404/oem/common/crypto/bignum/modular.c",
                               "pr/v0404/oem/common/crypto/bignum/mpaddsubcmp.c",
                               "pr/v0404/oem/common/crypto/bignum/mpalloc.c",
                               "pr/v0404/oem/common/crypto/bignum/mpgcdex.c",
                               "pr/v0404/oem/common/crypto/bignum/mpmul22.c",
                               "pr/v0404/oem/common/crypto/bignum/mprand.c",
                               "pr/v0404/oem/common/crypto/ecc/base/oemeccp256.c",
                               "pr/v0404/oem/common/crypto/ecc/base/oemeccp256verify.c",
                               "pr/v0404/oem/common/crypto/ecc/baseimpl/oemeccinitimpl.c",
                               "pr/v0404/oem/common/crypto/ecc/baseimpl/oemeccp256impl.c",
                               "pr/v0404/oem/common/crypto/ecc/baseimpl/oemeccp256verifyimpl.c",
                               "pr/v0404/oem/common/crypto/sha/sha256/oemsha256.c",
                               "pr/v0404/oem/common/trustedexec/oemcommonmem_lw.c",
                               "pr/v0404/oem/common/trustedexec/oemcommonlw.c",
                               "pr/v0404/oem/common/trustedexec/oemtee.c",
                               "pr/v0404/oem/common/trustedexec/oemteeaes128cbc.c",
                               "pr/v0404/oem/common/trustedexec/oemteeclock.c",
                               "pr/v0404/oem/common/trustedexec/oemteecritsec.c",
                               "pr/v0404/oem/common/trustedexec/oemteecrypto.c",
                               "pr/v0404/oem/common/trustedexec/oemteecryptoaes128block.c",
                               "pr/v0404/oem/common/trustedexec/oemteecryptoaes128ecb.c",
                               "pr/v0404/oem/common/trustedexec/oemteecryptoaes128kdf.c",
                               "pr/v0404/oem/common/trustedexec/oemteecryptoaes128keywrap.c",
                               "pr/v0404/oem/common/trustedexec/oemteecryptoaes128omac1.c",
                               "pr/v0404/oem/common/trustedexec/oemteecryptoecc256.c",
                               "pr/v0404/oem/common/trustedexec/oemteecryptorand.c",
                               "pr/v0404/oem/common/trustedexec/oemteecryptosha256.c",
                               "pr/v0404/oem/common/trustedexec/oemteedecrypt.c",
                               "pr/v0404/oem/common/trustedexec/oemteedecryptpolicy.c",
                               "pr/v0404/oem/common/trustedexec/oemteedom.c",
                               "pr/v0404/oem/common/trustedexec/oemteekeywrap.c",
                               "pr/v0404/oem/common/trustedexec/oemteelprovcert.c",
                               "pr/v0404/oem/common/trustedexec/oemteelprovcerttemplate.c",
                               "pr/v0404/oem/common/trustedexec/oemteemem.c",
                               "pr/v0404/oem/common/trustedexec/oemteememhandle.c",
                               "pr/v0404/oem/common/trustedexec/oemteeversion.c",
                               "pr/v0404/trustedexec/aes128cbc/drmteeaes128cbc.c",
                               "pr/v0404/trustedexec/aes128ctr/drmteeaes128ctr.c",
                               "pr/v0404/trustedexec/base/drmteebase.c",
                               "pr/v0404/trustedexec/base/drmteebasemem.c",
                               "pr/v0404/trustedexec/base/drmteekbcryptodata.c",
                               "pr/v0404/trustedexec/base/drmteexb.c",
                               "pr/v0404/trustedexec/debug/drmteedebug.c",
                               "pr/v0404/trustedexec/decrypt/drmteedecrypt.c",
                               "pr/v0404/trustedexec/dom/drmteedom.c",
                               "pr/v0404/trustedexec/h264/drmteeh264.c",
                               "pr/v0404/trustedexec/licprep/drmteelicprep.c",
                               "pr/v0404/trustedexec/lprovcerttemplate/drmteelprovcerttemplate.c",
                               "pr/v0404/trustedexec/revocation/drmteerevocation.c",
                               "pr/v0404/trustedexec/sampleprot/drmteesampleprot.c",
                               "pr/v0404/trustedexec/sign/drmteesign.c",
                               "pr/v0404/trustedexec/teeproxystub/common/drmteeproxystubcommon.c",
                               "pr/v0404/trustedexec/teeproxystub/stub/common/drmteecache.c",
                               "pr/v0404/trustedexec/teeproxystub/stub/common/drmteestubcommon.c",
                               "pr/v0404/trustedexec/teeproxystub/stub/common/drmteestubcommon_generated.c",
                               "pr/v0404/trustedexec/teeproxystub/stub/licgen/stub/drmteestublicgenunsupported.c",
                               "pr/v0404/trustedexec/teeproxystub/stub/rprov/stub/drmteerprovunsupported.c",
                               "pr/v0404/trustedexec/teeproxystub/stub/selwrestop/stub/drmteeselwrestopunsupported.c",
                               "pr/v0404/trustedexec/teeproxystub/stub/selwrestop2/stub/drmteeselwrestop2unsupported.c",
                               "pr/v0404/trustedexec/teeproxystub/stub/selwretime/stub/drmteeselwretimeunsupported.c",
                              ],
    },
];

# Create the Features group
sub new {

    @_ == 1 or croak 'usage: obj->new()';

    my $type = shift;

    my $self = Groups->new("feature", $featuresRef);

    $self->{GROUP_PROPERTY_INHERITS} = 1;   # FEATUREs inherit based on name

    return bless $self, $type;
}

# end of the module
1;
