/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dmvainstrs.h"
#include "dmvaassert.h"
#include "dmvautils.h"
#include "dmvaregs.h"
#include <lwmisc.h>
#include <falcon-intrinsics.h>

// dmlvl takes an immediate operand,
// so this complication is needed if we want to store the mask in a register.
void dmva_dmlvl(LwU32 blk, LevelMask blockMask)
{
    switch (blockMask)
    {
        case 0:
            falc_dmlvl(blk, 0);
            return;
        case 1:
            falc_dmlvl(blk, 1);
            return;
        case 2:
            falc_dmlvl(blk, 2);
            return;
        case 3:
            falc_dmlvl(blk, 3);
            return;
        case 4:
            falc_dmlvl(blk, 4);
            return;
        case 5:
            falc_dmlvl(blk, 5);
            return;
        case 6:
            falc_dmlvl(blk, 6);
            return;
        case 7:
            falc_dmlvl(blk, 7);
            return;
        default:
            // Mask must not have any bits not set in SECMASK_ALL
            halt();
    }
}

void dmlvlAtUcodeLevelImpl(LwU32 physBlk, LevelMask blockMask, SecMode ucodeLevel, DmInstrImpl impl)
{
    UASSERT(ucodeLevel <= 3);
    if (ucodeLevel == SEC_MODE_HS)
    {
        callHS(HS_SET_BLK_LVL_IMPL, physBlk, blockMask, impl);
    }
    else
    {
        LwU32 oldLevel = getUcodeLevel();
        callHS(HS_SET_LVL, ucodeLevel);

        switch (impl)
        {
            case DM_IMPL_INSTRUCTION:
                ;
                dmva_dmlvl(physBlk, blockMask);
                break;
            case DM_IMPL_DMCTL:
                ;
                DMVAREGWR(DMSTAT, blockMask);
                LwU32 dmctl = DRF_VAL(_PFALCON, _FALCON_DMCTL, _ADDR_BLK, physBlk);
                dmctl = FLD_SET_DRF(_PFALCON, _FALCON_DMCTL, _CMD, _DMLVL, dmctl);
                DMVAREGWR(DMCTL, dmctl);
                break;
            default:
                halt();
        }

        callHS(HS_SET_LVL, oldLevel);
    }
}

void dmilwImpl(LwU32 physBlk, DmInstrImpl impl)
{
    switch (impl)
    {
        case DM_IMPL_INSTRUCTION:
            falc_dmilw(physBlk);
            return;
        case DM_IMPL_DMCTL:
            ;
            LwU32 dmctl = DRF_VAL(_PFALCON, _FALCON_DMCTL, _ADDR_BLK, physBlk);
            dmctl = FLD_SET_DRF(_PFALCON, _FALCON_DMCTL, _CMD, _DMILW, dmctl);
            DMVAREGWR(DMCTL, dmctl);
            return;
        default:
            halt();
    }
}

void dmcleanImpl(LwU32 physBlk, DmInstrImpl impl)
{
    switch (impl)
    {
        case DM_IMPL_INSTRUCTION:
            falc_dmclean(physBlk);
            return;
        case DM_IMPL_DMCTL:
            ;
            LwU32 dmctl = DRF_VAL(_PFALCON, _FALCON_DMCTL, _ADDR_BLK, physBlk);
            dmctl = FLD_SET_DRF(_PFALCON, _FALCON_DMCTL, _CMD, _DMCLEAN, dmctl);
            DMVAREGWR(DMCTL, dmctl);
            return;
        default:
            halt();
    }
}

dmblk_t dmblkImpl(LwU32 physBlk, DmInstrImpl impl)
{
    switch (impl)
    {
        case DM_IMPL_INSTRUCTION:
            return dmva_dmblk(physBlk);
        case DM_IMPL_DMCTL:
            ;
            LwU32 dmctl = DRF_VAL(_PFALCON, _FALCON_DMCTL, _ADDR_BLK, physBlk);
            dmctl = FLD_SET_DRF(_PFALCON, _FALCON_DMCTL, _CMD, _DMBLK, dmctl);
            DMVAREGWR(DMCTL, dmctl);
            union
            {
                LwU32 integer;
                dmblk_t structure;
            } d;
            d.integer = DMVAREGRD(DMSTAT);
            return d.structure;
        default:
            halt();
    }
}

dmtag_t dmtagImpl(LwU32 va, DmInstrImpl impl)
{
    switch (impl)
    {
        case DM_IMPL_INSTRUCTION:
            return dmva_dmtag(va);
        case DM_IMPL_DMCTL:
            ;
            LwU32 dmctl = DRF_VAL(_PFALCON, _FALCON_DMCTL, _ADDR_BLK, va);
            dmctl = FLD_SET_DRF(_PFALCON, _FALCON_DMCTL, _CMD, _DMTAG, dmctl);
            DMVAREGWR(DMCTL, dmctl);
            union
            {
                LwU32 integer;
                dmtag_t structure;
            } d;
            d.integer = DMVAREGRD(DMSTAT);
            return d.structure;
        default:
            halt();
    }
}

void setdtagImpl(LwU32 va, LwU32 physBlk, DmInstrImpl impl)
{
    switch (impl)
    {
        case DM_IMPL_INSTRUCTION:
            falc_setdtag(va, physBlk);
            return;
        case DM_IMPL_DMCTL:
            ;
            DMVAREGWR(DMSTAT, va);
            LwU32 dmctl = DRF_VAL(_PFALCON, _FALCON_DMCTL, _ADDR_BLK, physBlk);
            dmctl = FLD_SET_DRF(_PFALCON, _FALCON_DMCTL, _CMD, _DMTAG_SETVLD, dmctl);
            DMVAREGWR(DMCTL, dmctl);
            return;
        default:
            halt();
    }
}
