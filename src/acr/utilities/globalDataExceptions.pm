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
    'g_reserved'        => 1,
    'g_desc'            => 1,
    'g_bIsDebug'        => 1,
    'g_pWprHeader'      => 1,
    'g_lsbHeader'       => 1,
    'g_dmaProp'         => 1,
    'g_signature'       => 1,
    'g_blCmdLineArgs'   => 1,
    'g_scrubState'      => 1,
    'g_kdfSalt'         => 1,
    'g_UcodeLoadBuf'    => 1,
    'g_dmHash'          => 1,
    'g_copyBufferA'     => 1,
    'g_copyBufferB'     => 1,
    'g_LsSigGrpLoadBuf' => 1,
    'g_dmHashForGrp'    => 1,
    '_tmpBuf'           => 1,
    'g_resetPLM'        => 1,
    '__stack_chk_guard' => 1,
    'g_wprHeaderWrappers' => 1,
    'g_lsbHeaderWrapper'  => 1,
    'g_lsSignature'       => 1,
    'g_hashBuffer'        => 1,
    'g_hashResult'        => 1,
    'g_newMeasurement'    => 1,
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
    'g_dummyUnused'      => 1,
    'g_hsFmcParams'      => 1,
    'g_HsFmcSignatureVerificationParams' => 1,
    'g_msrExtBuffer'     => 1,
    'g_offsetApmRts'     => 1,
    'g_apmDmaGroup'      => 1,
    'g_hashDstBuffer'    => 1,
	'cdev_PKA'           => 1,
    'cdev_SE0'           => 1,
    'cdev_RNG'           => 1,
	'setup_device'       => 1,
	'tegra_cengine'      => 1,
    'tegra_cdev'         => 1,
    'lwpka_engine_max_timeout' => 1,
    'ccc_engine'         => 1,
    'hw_cdev'            => 1,
    );
