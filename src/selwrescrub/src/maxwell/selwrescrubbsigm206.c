/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: BSI related functions are handled
 */

/* ------------------------- System Includes -------------------------------- */
#include "selwrescrubovl.h"
#include "dev_fb.h"
#include "dev_lwdec_csb.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_gc6_island.h"
#include "rmselwrescrubif.h"
#include "rmbsiscratch.h"
#include "selwrescrubreg.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define NEXT_OFFSET(offset,i)                           (offset+(i*sizeof(LwU32)))
#define VPR_ADDR_ALIGN_IN_BSI                           (20)
/*-------------------------Function Prototypes--------------------------------*/
static LwU8
_selwrescrubComputeCheckSum( LwU8*, LwU16)                      ATTR_OVLY(SELWRE_OVL);

/*
 * @brief Read / write content of BSI RAM from offset
 *
 * @param[in/out] pBuf     Depends on read or write
 * @param[in]     size    size to read from BSI ram
 * @param[in]     offset  offset from we want to read
 * @param[in]     bRead   direction of the transfer
 *
 * @return        SELWRESCRUB_RC_OK    : If read is possible from correct offset
 * @return        SELWRESCRUB_RC_ERROR : Not able to set correct offset
 */
SELWRESCRUB_RC
selwrescrubBsiRamRW_GM206
(
    LwU32  *pBuf,
    LwU32  size,
    LwU32  offset,
    LwBool bRead
)
{
    LwU32 i;
    LwU32 ramCtrl = 0;
    LwBool bValid = LW_FALSE;

    if ((offset+size) > ( DRF_VAL( _PGC6, _BSI_RAM, _SIZE, FALC_REG_RD32(BAR0,
            LW_PGC6_BSI_RAM)) << 10 ))
    {
        return SELWRESCRUB_RC_ERROR;
    }

    // Size is in bytes
    size = size >> 2;

    for (i = 0; i < size; i++)
    {
        bValid  = LW_FALSE;
        ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, NEXT_OFFSET(offset,i), ramCtrl);
        ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _DISABLE, ramCtrl);
        ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _DISABLE, ramCtrl);
        FALC_REG_WR32(BAR0, LW_PGC6_BSI_RAMCTRL, ramCtrl);

        do
        {
            if (bRead)
            {
                pBuf[i] = FALC_REG_RD32(BAR0, LW_PGC6_BSI_RAMDATA);
            }
            else
            {
                FALC_REG_WR32(BAR0, LW_PGC6_BSI_RAMDATA, pBuf[i]);
            }

            // readback the offset to ensure we are accessing correct data
            ramCtrl = FALC_REG_RD32(BAR0, LW_PGC6_BSI_RAMCTRL);
            if (FLD_TEST_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _DISABLE, ramCtrl) &&
                FLD_TEST_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _DISABLE, ramCtrl) &&
                DRF_VAL(_PGC6, _BSI_RAMCTRL, _ADDR, ramCtrl)  == NEXT_OFFSET(offset,i))
            {
                bValid = LW_TRUE;
            }
        }while(!bValid);
    }
    return SELWRESCRUB_RC_OK;
}

SELWRESCRUB_RC
selwrescrubGetRangeFromBSI_GM206(PVPR_ACCESS_CMD pVprCmd)
{
    LwU32 bsiRamSize = 0;
    LwU32 brssOffset = 0;
    LwU32 brssSize   = sizeof(BSI_RAM_SELWRE_SCRATCH_V1);
    LwU8* pBrss;
    BSI_RAM_SELWRE_SCRATCH_DATA brss;
    BSI_RAM_SELWRE_SCRATCH_HDR  brssHdr;

    pVprCmd->bVprEnabled   = LW_FALSE;

    // BRSS structure is stored at the end of BSI RAM
    bsiRamSize = DRF_VAL(_PGC6, _BSI_RAM, _SIZE,  FALC_REG_RD32(BAR0, LW_PGC6_BSI_RAM)) * 1024;
    brssSize   = sizeof(BSI_RAM_SELWRE_SCRATCH_HDR);
    brssOffset = bsiRamSize - brssSize;

    pBrss      = (LwU8*)&brssHdr;

    // Read BRSS header first
    selwrescrubBsiRamRW_GM206( (LwU32*)pBrss, brssSize, brssOffset, LW_TRUE);

    FALC_REG_WR32(BAR0, 0x1580, 0xdead);
    if (brssHdr.identifier != LW_BRSS_IDENTIFIER)
    {
    FALC_REG_WR32(BAR0, 0x1584, 0xdead1);
        return SELWRESCRUB_RC_ILWALID_BSI;
    }

    switch(brssHdr.version)
    {
        case LW_BRSS_VERSION_V1:
            brssSize   = sizeof(BSI_RAM_SELWRE_SCRATCH_V1);
            brssOffset = bsiRamSize - brssHdr.size;
            pBrss = (LwU8*)&brss.brssDataU.V1;
            selwrescrubBsiRamRW_GM206( (LwU32*)pBrss, brssSize, brssOffset, LW_TRUE);
        break;
    }

    FALC_REG_WR32(BAR0, 0x1584, 0xdead2);
    // Find checksum of BRSS and verify
    if (brss.brssDataU.V1.checksum != _selwrescrubComputeCheckSum((LwU8*)(pBrss)+1,brssSize-1))
    {
    FALC_REG_WR32(BAR0, 0x1584, 0xdead3);
        return SELWRESCRUB_RC_BRSS_FAIL;
    }

    pVprCmd->vprRangeStart  = ((LwU64)brss.brssDataU.V1.vprData[0]) << VPR_ADDR_ALIGN_IN_BSI;
    pVprCmd->vprRangeEnd    = ((LwU64)brss.brssDataU.V1.vprData[1]) << VPR_ADDR_ALIGN_IN_BSI;

    pVprCmd->vprRangeEnd    += VPR_END_ADDR_ALIGNMENT - 1;
    pVprCmd->bVprEnabled    = LW_TRUE;

    return SELWRESCRUB_RC_OK;
}

static LwU8
_selwrescrubComputeCheckSum( LwU8 *pOffsetStart, LwU16 dataSize )
{
    LwU16 index = 0;
    LwU32 checkSum = 0;

    for ( index = 0; index < dataSize; index++ )
    {
        checkSum = checkSum + (*pOffsetStart);
        checkSum &= 0xFF;

        pOffsetStart++;
    }

    checkSum = 0x100 - checkSum;
    return ( checkSum & 0xFF );
}

