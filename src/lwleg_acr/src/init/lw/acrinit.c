/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acrinit.c
 */

//
// Includes
//
#include "acr.h"
#include "dev_fb.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_csb.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

//
// Global Variables
//

//
// Extern Variables
//

/*!
 * @brief Cleanup the falcon SCP/GPR registers(state) to zero before exiting.
 */
void acrCleanup(void)
{
    //TODO: Cleanup the temporary memory
    // Clearing all falcon GPRs
    falc_reset_gprs();

    // Reset SCP registers
    falc_scp_xor(SCP_R0, SCP_R0);
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_xor(SCP_R6, SCP_R6);
    falc_scp_xor(SCP_R7, SCP_R7);
}

#ifndef ACR_SAFE_BUILD
/*!
 * @brief A trusted block of code which will be running as NS block.\n
 *        \note
 *        THIS FUNCTION IS NEVER CALLED. BUT WE USE ITS OPCODES DIRECTLY
 *        IN acrForceHalt. ANY CHANGE HERE SHOULD BE REFLECTED IN acrForceHalt.
 */
void acrTrustedNSBlock(void)
{
    // Enable interrupts
    falc_ssetb_i(16);
    falc_ssetb_i(17);
    // Falcon core v > 4
    falc_ssetb_i(18);

    falc_wspr(SEC, 0);
    falc_halt();
    return;
}

/*!
 * @brief WAR to force HALT when HS code is done
 */
#define ACR_IMTAG_BLK_INDEX_MASK   0x00FFFFFFU
#define ACR_IMTAG_HIT_OR_MISS      31

/*!
 * @brief Assembly instruction to cause falcon exelwtion PC jump.
 * @param[in] imm destination of the jump in IMEM(Instruction Memory).
 *
 */
static inline
void falc_jmp( unsigned int imm ){ asm volatile ( " jmp %0 ;":: "r"(imm) );}

#ifdef ACR_LOAD_PATH
/*!
 * @brief We need to HALT in HS code with lockdown disabled to make sure\n
 *        NS code doesnt get LS access. But LOCKDOWN can be cleared only\n
 *        while exiting HS mode into NS parts which is a problem. So this\n
 *        function overwrites the NS block with HALT codes and jump straight\n
 *        into it. It makes sure NS code doesnt have any unwanted code segments.
 */
void acrForceHalt(void)
{
    LwU32 dstblk         = 0;
    LwU32 data           = 0;
    LwU32 index          = 0;
    LwU32 CRegAddrIMEMC  = 0;
    LwU32 CRegAddrIMEMD  = 0;
    LwU32 CRegAddrIMEMT  = 0;
    LwU32 CRegAddrScpCtl = 0;

    LwU32 dstPC = ((LwU32)acr_code_start) - 0x100U;

    falc_imtag(&dstblk, dstPC);

    if ((dstblk & BIT32(ACR_IMTAG_HIT_OR_MISS)) != 0U)
    {
        // If MISS, something wrong. Write error code and HALT
        acrWriteStatusToFalconMailbox(ACR_ERROR_WAR_NOT_POSSIBLE);
        falc_halt();
    }

#ifdef ACR_FALCON_PMU
    CRegAddrIMEMC = LW_CPWR_FALCON_IMEMC(0U);
    CRegAddrIMEMD = LW_CPWR_FALCON_IMEMD(0U);
    CRegAddrIMEMT = LW_CPWR_FALCON_IMEMT(0U);
    CRegAddrScpCtl = LW_CPWR_PMU_SCP_CTL_CFG;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(ACR_FALCON_PMU || ACR_FALCON_SEC2);
#endif

    //
    // Use IMEMC port 0 for destination write
    //
    dstblk = dstblk & ACR_IMTAG_BLK_INDEX_MASK;
    data = FLD_SET_DRF_NUM(_CPWR, _FALCON_IMEMC, _BLK, dstblk, 0);
    data = FLD_SET_DRF(_CPWR, _FALCON_IMEMC, _AINCW, _TRUE, data);
    ACR_REG_WR32_STALL(CSB, CRegAddrIMEMC, data);
    ACR_REG_WR32_STALL(CSB, CRegAddrIMEMT, (dstPC>>8));

    //
    // Write 5 DWORDS of opcodes from acrTrustedNSBlock directly into dstBlk
    // Please look at acrTrustedNSBlock to find what these opcodes are
    //
    ACR_REG_WR32_STALL(CSB, CRegAddrIMEMD, 0xf41031f4);
    ACR_REG_WR32_STALL(CSB, CRegAddrIMEMD, 0x31f41131);
    ACR_REG_WR32_STALL(CSB, CRegAddrIMEMD, 0xfe94bd12);
    ACR_REG_WR32_STALL(CSB, CRegAddrIMEMD, 0x02f8009a);
    ACR_REG_WR32_STALL(CSB, CRegAddrIMEMD, 0x000000f8);

    // Fill a block with HALT instructions
    while (index < ((256U - 20U)/4U))
    { 
        ACR_REG_WR32_STALL(CSB, CRegAddrIMEMD, 0x0);
        ++index;
    }

    // De-assert big hammer
    ACR_REG_WR32_STALL(CSB, CRegAddrScpCtl, 0x0);
    //falc_halt();
    // Jump into dstPC pointing to the dstblk
    falc_jmp(dstPC);
}
#endif
#endif

/*!
 * @brief ACR routine to check if falcon is in DEBUG mode or not\n
 *        Check by reading DEBUG_MODE bit of LW_CPWR_PMU_SCP_CTL_STAT.
 *
 * @return TRUE if debug mode
 * @return FALSE if production mode
 *
 */
LwBool
acrIsDebugModeEnabled_GM200(void)
{
    LwU32   scpCtl = 0;

#ifdef ACR_FALCON_PMU
    scpCtl = ACR_REG_RD32_STALL(CSB, LW_CPWR_PMU_SCP_CTL_STAT);
#elif ACR_FALCON_SEC2
    scpCtl = ACR_REG_RD32_STALL(CSB, LW_CSEC_SCP_CTL_STAT);
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(ACR_FALCON_PMU || ACR_FALCON_SEC2);
#endif

    // Using CPWR define since we dont have in falcon manual, Filed hw bug 200179864
    return !FLD_TEST_DRF( _CPWR, _PMU_SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, scpCtl);
}

