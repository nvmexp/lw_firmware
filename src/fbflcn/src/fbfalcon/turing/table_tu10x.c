/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include <lwmisc.h>

// include manuals
#include "dev_fbfalcon_csb.h"

// project headers
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "priv.h"


void fbfalconLoadBootTrainingTable_TU102
(
        LwU32 *pdmem
)
{
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
    pVdpaEntry = findDirtIdTableEntry(FW_MEMORY_BOOT_TRAINING_TABLE_ID);
    if (pVdpaEntry == NULL)
    {
        // FIXME: we should halt here (program flow should guarantee we
        // only ilwoke boottime training if there's a valid table.
        gBiosTable.pBootTrainingTable = NULL;
        return;
    }

    LwU32 trainingTableOffset = pVdpaEntry->offset_start;
    LwU32 trainingTableSize = REF_VAL(FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE,
            pVdpaEntry->size);

    // The table size on Turing is exceeding our definition - skip check
    /*

    // check that we will read enough data for the table header
    if (trainingTableSize < sizeof(BOOT_TRAINING_TABLE))
    {
        FW_MBOX_WR32_STALL(10, *pdmem);
        FW_MBOX_WR32_STALL(11,
                FW_MEMORY_BOOT_TRAINING_TABLE_ID);
        FW_MBOX_WR32_STALL(12, trainingTableSize);
        FW_MBOX_WR32_STALL(13,
                sizeof(BOOT_TRAINING_TABLE));

        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_TABLE_SIZE_ERROR);
    }
    */

    // load table
    LwU32 trainingTableOffsetAbsolute = trainingTableOffset +
            gBiosTable.fw_descriptor->fw_Image_offset;
    LwU32 trainingTableStart = loadTableSegment(
            trainingTableOffsetAbsolute, pdmem, trainingTableSize);

    BOOT_TRAINING_TABLE* pMBTT = (BOOT_TRAINING_TABLE*) trainingTableStart;

    // check for correct table version
    if (TABLE_VAL(pMBTT->Version) != BOOT_TRAINING_TABLE_11_VERSION)
    {
        FW_MBOX_WR32_STALL(7, *pdmem);
        FW_MBOX_WR32_STALL(8, trainingTableOffset);
        FW_MBOX_WR32_STALL(9, trainingTableSize);
        FW_MBOX_WR32_STALL(10,
                TABLE_VAL(pMBTT->Version));
        FW_MBOX_WR32_STALL(11,
                BOOT_TRAINING_TABLE_1X_VERSION);
        FW_MBOX_WR32_STALL(12,
                TABLE_VAL(pMBTT->StructSize));
        FW_MBOX_WR32_STALL(13,
                TABLE_VAL(pMBTT->MemBootTrainTblFlags));

        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_TABLE_VERSION_ERROR);
    }
    gBiosTable.pBootTrainingTable = (BOOT_TRAINING_TABLE*) trainingTableStart;
}


/*
 * Find Frequency Table Entry
 *  lookup function from frequency to pmct table for that entry
 *
 */
LwU8
fbfalconFindFrequencyTableEntry_TU10X
(
        LwU16 targetFrequencyMHz
)
{
    // get performance table information
    Perf6xHeader* pPTH = gBiosTable.pPTH;
    LwU8 pTBaseEntryCount = TABLE_VAL(pPTH->BaseEntryCount);
    Perf6xBaseEntry* ppte = (Perf6xBaseEntry*) ((LwU8*)pPTH + TABLE_VAL(pPTH->HeaderSize));
    LwU16 pTBaseEntryStepSize = TABLE_VAL(pPTH->BaseEntrySize) +
            (TABLE_VAL(pPTH->ClockEntryCount) * TABLE_VAL(pPTH->ClockEntrySize));

    // get memory clock performance table information
    PerfMemClk11Header* ppmh = gBiosTable.pMCHp;
    PerfMemClk11BaseEntry* ppme = gBiosTable.pMCBEp;
    LwU16 pMemClkEntryStepSize = TABLE_VAL(ppmh->BaseEntrySize) +
            (TABLE_VAL(ppmh->StrapEntryCount) * TABLE_VAL(ppmh->StrapEntrySize));


    // TODO: resolve to final solution
    //       For bringup we have more of the performance memory tables deployed then used for P-States
    //       and they typically form a continuous band across the whole frequency range. I can't disable this here.
    //

    LwBool bUseValidPStatesOnly = LW_FALSE;   // set to true if only valid p-state tale entries should be considered

    LwU8 i;
    LwU8 index = 0xff;

    for (i=0; i<pTBaseEntryCount; i++)
    {

        LwU8 state = TABLE_VAL(ppte->State);
        if (!bUseValidPStatesOnly || (state != PSTATE_MCLK_FREQ_ENTRY_STATE_SKIP))
        {
            // minimum always comes from the performance memory clock table
            // check that min on the p8 table does not hit zero
            LwU16 minFreqMHz = TABLE_VAL(ppme->Minimum);
            if (state == PSTATE_MCLK_FREQ_ENTRY_STATE_P8)
            {
                // TODO:  this should be enabled once we propogate min value to be
                //        bigger than 0 on P8 clk table
                /*
                if (min < 30)
                {
                    FBFLCN_HALT(FBFLCN_ERROR_CODE_P8_MIN_FREQUENCY_NOT_BOUND);
                }
                 */
            }

            // look for max
            // maximum is from the performance memory clock table but extended
            // to vco max on P0 to allow for overclocking.
            LwU16 maxFreqMHz = 0;
            if (state == PSTATE_MCLK_FREQ_ENTRY_STATE_P0)
            {
                if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
                {
                    PLLInfo5xEntry* pIEpMpll = gBiosTable.pIEp_refmpll;
                    maxFreqMHz = TABLE_VAL(pIEpMpll->VCOMax);
                }
                if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))
                {
                    PLLInfo5xEntry* pIEp = gBiosTable.pIEp;
                    maxFreqMHz = TABLE_VAL(pIEp->VCOMax);
                }
                maxFreqMHz = max(maxFreqMHz,TABLE_VAL(ppme->Maximum));
            }
            else
            {
                maxFreqMHz = TABLE_VAL(ppme->Maximum);
            }
            if ((targetFrequencyMHz >= minFreqMHz) && (targetFrequencyMHz <= maxFreqMHz))
            {
                index = i;
                break;
            }
            //FW_MBOX_WR32(ct++, (minFreqMHz << 16 | maxFreqMHz));
        }
        ppte = (Perf6xBaseEntry*)((LwU8*)ppte + pTBaseEntryStepSize);
        ppme = (PerfMemClk11BaseEntry*)((LwU8*)ppme + pMemClkEntryStepSize);

    }

    return index;
}


/*
 * selectTable
 *  select the proper table for a given state (P-state & frequency)
 *
 */

void
fbfalconSelectTable_TU10X
(
        LwU16 targetFrequencyMHz
)
{
    LwU8 memPerfClkTableInx = fbfalconFindFrequencyTableEntry_HAL(&Fbflcn,targetFrequencyMHz);
    // check if index is within range of available tables
    if (memPerfClkTableInx >= gBiosTable.pMCBEentryCount)
    {
        FW_MBOX_WR32(11, memPerfClkTableInx);
        FW_MBOX_WR32(12, gBiosTable.pMCBEentryCount);
        FW_MBOX_WR32(13, targetFrequencyMHz);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_PMCT_ENTRY_NOT_FOUND);
    }

    gTT.pIEp = gBiosTable.pIEp;
    gTT.pMIEp = gBiosTable.mIEp;
    gTT.pIUO = gBiosTable.pIUO;

    // find the performance memory clk table base enty
    PerfMemClk11BaseEntry* p = gBiosTable.pMCBEp;
    p = (PerfMemClk11BaseEntry*) ((LwU8*)p + memPerfClkTableInx * (
            gBiosTable.pMCBEentrySize +
            (gBiosTable.pMCBEstrapCount * gBiosTable.pMCBEstrapSize)
    ));
    EXTRACT_TABLE(p,gTT.perfMemClkBaseEntry);

    // startin with the base entry find the valid strap entry
    PerfMemClk11StrapEntry* pStrap = (PerfMemClk11StrapEntry*) ((LwU8*)p +
            gBiosTable.pMCBEentrySize + gBiosTable.strap *
            gBiosTable.pMCBEstrapSize);
    EXTRACT_TABLE(pStrap,gTT.perfMemClkStrapEntry);

    LwU8 memTweakIdx = TABLE_VAL(gTT.perfMemClkStrapEntry.MemTweakIdx);
    if (memTweakIdx >= gBiosTable.pMTBEentryCount)
    {
        FW_MBOX_WR32(12, gBiosTable.pMTBEentryCount);
        FW_MBOX_WR32(13, memTweakIdx);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_ILLEGAL_MEMTWEAK_INDEX);
    }

    // get the MemTweakBaseEntry with the same offset
    PerfMemTweak2xBaseEntry* mp = gBiosTable.pMTBEp;
    mp = (PerfMemTweak2xBaseEntry*) ((LwU8*)mp +
            memTweakIdx*(gBiosTable.pMTBEentrySize +
                    (gBiosTable.pMTBEextEntryCount * gBiosTable.pMTBEextEntrySize)));
    EXTRACT_TABLE(mp,gTT.perfMemTweakBaseEntry);

}
