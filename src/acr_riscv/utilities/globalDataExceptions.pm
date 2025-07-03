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
# This file contains exception list of global initialized data in ACR binaries.
#
# This file is used by check_global_data.pl.
#
# WARNING:
# Without proper analysis and review, exceptions should not be added to this file.
#

use warnings 'all';
package globalDataExceptions;

# Exception list of global initialized data.
# Build will not fail for these variables.
our %excp_vars = (
    'g_desc'            => 1,
    'g_bIsDebug'        => 1,
    'g_dmaProp'         => 1,
    'g_blCmdLineArgs'   => 1,
    'g_scrubState'      => 1,
    'g_UcodeLoadBuf'    => 1,
    'g_copyBufferA'     => 1,
    'g_copyBufferB'     => 1,
    'g_LsSigGrpLoadBuf' => 1,
    'g_dmHashForGrp'    => 1,
    '__stack_chk_guard' => 1,
    'g_wprHeaderWrappers' => 1,
    'g_lsbHeaderWrapper'  => 1,
    'g_lsSignature'       => 1,
    'g_hashBuffer'        => 1,
    'g_hashResult'        => 1,
    'g_rsaKeyModulusDbg_GA10X'   => 1,
    'g_rsaKeyExponentDbg_GA10X'  => 1,
    'g_rsaKeyMpDbg_GA10X'        => 1,
    'g_rsaKeyRsqrDbg_GA10X'      => 1,
    'g_rsaKeyModulusProd_GA10X'   => 1,
    'g_rsaKeyExponentProd_GA10X'  => 1,
    'g_rsaKeyMpProd_GA10X'        => 1,
    'g_rsaKeyRsqrProd_GA10X'      => 1,
    'g_lsEncryptIV'      => 1,
    'g_lsDecryptionBuf'  => 1,
    'g_tmpSaltBuffer'    => 1,
    'g_lsEncKdfSalt'     => 1,
    'g_hsFmcParams'    => 1,
    'g_HsFmcSignatureVerificationParams' => 1,
    );
