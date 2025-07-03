/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lr10.c
 * @brief  HS Routines for SOE Bootloader
 */

#include "lwuproc.h"
#include "boot_platform_interface.h"
#include "soe_bar0_hs_inlines.h"
#include "soe_prierr.h"
#include "dev_soe_csb.h"


// ALL FUNCTIONS IN THIS FILE MUST BE HS-SECURE.
// THEY WILL BE CALLED BY THE SOE'S HS BOOTLOADER.


//
// defines to CLEAR/SET DISABLE_EXCEPTIONS in SEC SPR
// For more info please check Falcon_6_arch doc (Section 2.3.1)
//
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS                                  19:19
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_CLEAR                            0x00000000
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_SET                              0x00000001

static void hsExceptionHandler(void);

GCC_ATTRIB_STACK_PROTECT()
void soeBar0InitTmout_PlatformHS(void)
{
  _soeBar0InitTmout_LR10();
}

GCC_ATTRIB_STACK_PROTECT()
LwU32 soeBar0RegRd32_PlatformHS
(
    LwU32  addr
)
{
    LwU32 data = 0;

    _soeBar0ErrorChkRegRd32_LR10(addr, &data);
    return data;
}

#if !defined(LW_PSMC_BOOT_42_CHIP_ID_LS10)
#define LW_PSMC_BOOT_42_CHIP_ID_LS10    0x7
#endif
GCC_ATTRIB_STACK_PROTECT()
LwBool soeVerifyChipId_PlatformHS(void)
{
    LwU32 chipID;

    chipID = soeBar0RegRd32_PlatformHS( LW_PSMC_BOOT_42 );
    chipID = DRF_VAL(_PSMC, _BOOT_42, _CHIP_ID, chipID);
    if ((chipID != LW_PSMC_BOOT_42_CHIP_ID_LR10) &&
        (chipID != LW_PSMC_BOOT_42_CHIP_ID_LS10))
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

// Revocation support for HS.

/*!
* For SW Revocation
* eg: this should be >= the version fused in the HW (Both Fuse and FPF versions)
* @see hsUcodeRevocation, soeGetRevocationUcodeVersionSW_PlatformHS
*/
#define LW_SOE_HS_UCODE_VERSION_SW 1

GCC_ATTRIB_STACK_PROTECT()
LwU32 
soeGetRevocationUcodeVersionSW_PlatformHS()
{
    return LW_SOE_HS_UCODE_VERSION_SW;
}

/*!
 * Exception Vector Handler
 */
void hsExceptionHandler()
{
    // coverity[no_escape]
    while(1);
}

/*!
 * This installs a minimum exception handler which just spins out.
 * This subroutine sets up the Exception vector and other required registers for each exceptions. 
 */
void hsInstallExceptionHandler(void)
{
    LwU32 secSpr = 0;

    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IBRKPT1, 0x00000000);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IBRKPT2, 0x00000000);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IBRKPT3, 0x00000000);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IBRKPT4, 0x00000000);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IBRKPT5, 0x00000000);

    // Clear IE0 bit in CSW.
    falc_sclrb( 16 );

    // Clear IE1 bit in CSW.
    falc_sclrb( 17 );

    // Clear IE2 bit in CSW.
    falc_sclrb( 18 );

    // Clear Exception bit in CSW.
    falc_sclrb( 24 );

    falc_wspr( EV, hsExceptionHandler);

    falc_rspr( secSpr, SEC );
    // Clear DISABLE_EXCEPTIONS
    secSpr = FLD_SET_DRF( _FALC, _SEC_SPR, _DISABLE_EXCEPTIONS, _CLEAR, secSpr );
    falc_wspr( SEC, secSpr );
}

