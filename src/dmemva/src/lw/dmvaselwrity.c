/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dmvaselwrity.h"
#include "dmvautils.h"
#include "dmvadma.h"
#include "dmvainstrs.h"
#include "dmvaregs.h"
#include "dmvacommon.h"
#include "dmvavars.h"
#include "dmvainterrupts.h"
#include "dmvamem.h"
#include <lwmisc.h>

LwU32 g_signature[4] ATTR_OVLY(".data") ATTR_ALIGNED(16);
LwU32 *g_pSignature = &g_signature[0];

// Ucode with security level x can add permissions from ucode with security
// level y iff (addPermissions[x][y] == 1) and (x == 3 or blockMask[x] == 1).
const LwBool addPermissions[4][3] =
{
    /* ucode lvl   block lvl */
    /*             0 1 2     */
    /* lvl0 */    {0,0,0},
    /* lvl1 */    {1,0,0},
    /* lvl2 */    {1,0,0},
    /* lvl3 */    {1,1,1},
};

// Ucode with security level x can remove permissions from ucode with security
// level y iff (removePermissions[x][y] == 1) and (x == 3 or blockMask[x] == 1).
const LwBool removePermissions[4][3] =
{
    /* ucode lvl   block lvl */
    /*             0 1 2     */
    /* lvl0 */    {1,0,0},
    /* lvl1 */    {1,1,0},
    /* lvl2 */    {1,0,1},
    /* lvl3 */    {1,1,1},
};

LwU32 getUcodeLevel(void)
{
    // LW_PFALCON_FALCON_SCTL_UCODE_LEVEL is not implemented yet
    LwU32 sctl = DMVAREGRD(SCTL);
    if (REF_VAL(LW_PFALCON_FALCON_SCTL_HSMODE, sctl))
    {
        return 3;
    }
    else if (!REF_VAL(LW_PFALCON_FALCON_SCTL_LSMODE, sctl))
    {
        return 0;
    }
    else
    {
        return REF_VAL(LW_PFALCON_FALCON_SCTL_LSMODE_LEVEL, sctl);
    }
}

LwU32 hsEntry(HsFuncId fId, va_list args) ATTR_OVLY(".ovl1Entry");
LwU32 hsEntry(HsFuncId fId, va_list args)
{
    LwU32 res = 0;
    UASSERT(fId < N_HS_FUNCS);
    switch (fId)
    {
        case HS_ALIVE:
            res = HS_ALIVE_MAGIC;
            break;
        case HS_SET_BLK_LVL:
            dmva_dmlvl(va_arg(args, LwU32), va_arg(args, LevelMask));
            break;
        case HS_SET_BLK_LVL_IMPL:
            ;
            LwU32 physBlk = va_arg(args, LwU32);
            LevelMask blockMask = va_arg(args, LevelMask);
            switch (va_arg(args, DmInstrImpl))
            {
                case DM_IMPL_INSTRUCTION:
                    dmva_dmlvl(physBlk, blockMask);
                    break;
                case DM_IMPL_DMCTL:
                    DMVAREGWR(DMSTAT, blockMask);
                    LwU32 dmctl = DRF_VAL(_PFALCON, _FALCON_DMCTL, _ADDR_BLK, physBlk);
                    dmctl = FLD_SET_DRF(_PFALCON, _FALCON_DMCTL, _CMD, _DMLVL, dmctl);
                    DMVAREGWR(DMCTL, dmctl);
                    break;
                default:
                    halt();
            }
            break;
        case HS_SET_LVL:
            ;
            LwU32 level = va_arg(args, LwU32);
            UASSERT(level < 3);

            DMVAREGWR(SCTL, DRF_DEF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE));

            LwU32 sctl = DMVAREGRD(SCTL);
            sctl = FLD_SET_DRF_NUM(_PFALCON, _FALCON_SCTL, _LSMODE, level != 0, sctl);
            sctl = FLD_SET_DRF_NUM(_PFALCON, _FALCON_SCTL, _LSMODE_LEVEL, level, sctl);
            DMVAREGWR(SCTL, sctl);

            break;
        case HS_ISSUE_DMA:
            dmvaIssueDma(va_arg(args, LwU32), va_arg(args, LwU32), va_arg(args, LwU32),
                         va_arg(args, LwU32), va_arg(args, LwU32), va_arg(args, LwU32),
                         va_arg(args, LwU32), va_arg(args, DmaDir), va_arg(args, DmaImpl));
            break;
        case HS_CSB_WR:
            DMVACSBWR(va_arg(args, LwU32 *), va_arg(args, LwU32));
            break;
        case HS_CSB_RD:
            res = DMVACSBRD(va_arg(args, LwU32 *));
            break;
        default:
            halt();
    }

#ifdef DMVA_FALCON_SEC2
    DMVACSBWR(LW_CSEC_SCP_CTL_CFG, 0);
#elif defined DMVA_FALCON_GSP
    DMVACSBWR(LW_CGSP_SCP_CTL_CFG, 0);
#elif defined DMVA_FALCON_PMU
    DMVACSBWR(LW_CPWR_PMU_SCP_CTL_CFG, 0);
#elif defined DMVA_FALCON_LWDEC
    DMVACSBWR(LW_CLWDEC_SCP_CTL_CFG, 0);
#endif
    falc_wspr(SEC, SVEC_REG(0, 0, 0, 0));
    return res;
}

LwU32 callHS(HsFuncId fId, ...)
{
    // Make sure we have enough stack space reserved for the HS bootrom to run.
    LwU32 sp;
    falc_rspr(sp, SP);

    // make sure we have enough stack space to run bootrom
    UASSERT(sp > dataSize + BLOCK_SIZE);

    // Load the signature
    asm volatile("ccr <mt:DMEM,sc:0,sup:0,tc:2> ;");
    falc_dmwrite(0, ((6 << 16) | (LwU32)(g_pSignature)));
    falc_dmwait();

    // All the required code is already loaded into IMEM
    // Just need to program the SEC spr and jump into code
    falc_wspr(SEC, SVEC_REG(selwreCodeStart, (selwreCodeEnd - selwreCodeStart), 0, 0));

    // Mark shared HS code secure
    loadImem(selwreSharedStart, selwreSharedStart, selwreSharedEnd - selwreSharedStart, LW_TRUE);

    // Enter HS code
    va_list args;
    va_start(args, fId);
    LwU32 val = hsEntry(fId, args);

    // Mark shared HS code inselwre
    loadImem(selwreSharedStart, selwreSharedStart, selwreSharedEnd - selwreSharedStart, LW_FALSE);

    // Clear SEC after returning to NS/LS mode
    falc_wspr(SEC, SVEC_REG(0, 0, 0, 0));

    // Restore handlers
    installInterruptHandler(defaultInterruptHandler);

    return val;
}

LwU32 getExpectedMask(LwU32 oldMask, LwU32 attemptedMask, SecMode ucodeLevel, LwBool bUsingDmread)
{
    if (ucodeLevel < 3 && !(BIT(ucodeLevel) & oldMask))
    {
        return oldMask;  // Ucode will get an exception if it tries to change the mask.
    }
    /*
    // This commented-out behavior is what I believe should be correct (or just
    // treating dmread with mask 000 as "ucode wants to remove all permissions").
    // With the current implementation:
    //     - Level 0 ucode can add permissions to level 1 and 2.
    //     - Level 1 ucode can remove permissions from level 2.
    //     - Level 2 ucode can remove permissions from level 1.
    // The uncommented implementation is what's specified in the arch doc, and
    // for some reason is not considered a vulnerability, even though this
    // problem oclwrs with partial dmreads as well.
    if(bUsingDmread && attemptedMask == 0) {
        switch(ucodeLevel) {
        case 0:
            attemptedMask = LEVEL_MASK_ALL;
            break;
        case 1:
            attemptedMask = 0b010;
            break;
        case 2:
            attemptedMask = 0b100;
            break;
        case 3:
            attemptedMask = LEVEL_MASK_NONE;
            break;
        }
    }
    */
    if (bUsingDmread && attemptedMask == 0)
    {
        switch (ucodeLevel)
        {
            case 0:
                return LEVEL_MASK_ALL;
            case 1:
                return 0b010;
            case 2:
                return 0b100;
            case 3:
                return LEVEL_MASK_NONE;
            default:
                halt();
        }
    }
    LwU32 newMask = oldMask;
    LwU32 maskBitIndex;
    for (maskBitIndex = 0; maskBitIndex < 3; maskBitIndex++)
    {
        if (attemptedMask & BIT(maskBitIndex))
        {
            // ucode wants to add permissions
            if (addPermissions[ucodeLevel][maskBitIndex])
            {
                newMask |= BIT(maskBitIndex);
            }
        }
        else
        {
            // ucode wants to remove permissions
            if (removePermissions[ucodeLevel][maskBitIndex])
            {
                newMask &= ~BIT(maskBitIndex);
            }
        }
    }
    return newMask;
}

void setDmemSelwrityExceptions(LwBool bEnable)
{
    LwU32 sec;
    falc_rspr(sec, SEC);
    if (bEnable)
    {
        sec &= ~(1 << 19);
    }
    else
    {
        sec |= (1 << 19);
    }
    falc_wspr(SEC, sec);
}
