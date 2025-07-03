/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
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
#include "priv.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"

#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"

#include "lwuproc.h"



void fbfalconLoadBootTrainingTable_GA102
(
        LwU32 *pdmem
)
{
    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(FW_MEMORY_BOOT_TRAINING_TABLE_ID);
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

    // FIXME !!!
    // for gh100 the table structure has been updated but the fields have not been added to the bios in use
    // line below needs to be removed when that happens (see BUG 3236636 )
    // pls also update //hw/.../system_table.cpp which has the same fix
    trainingTableSize = sizeof(BOOT_TRAINING_TABLE);

    // check that we will read enough data for the table header
    if (trainingTableSize < sizeof(BOOT_TRAINING_TABLE))
    {
        //FW_MBOX_WR32_STALL(10, *pdmem);
        //FW_MBOX_WR32_STALL(11, FW_MEMORY_BOOT_TRAINING_TABLE_ID);
        //FW_MBOX_WR32_STALL(12, trainingTableSize);
        //FW_MBOX_WR32_STALL(13, sizeof(BOOT_TRAINING_TABLE));

        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_TABLE_SIZE_ERROR);
    }

    // load table
    LwU32 trainingTableOffsetAbsolute = trainingTableOffset;
    LwU32 trainingTableStart = loadTableSegment(
            trainingTableOffsetAbsolute, pdmem, trainingTableSize);

    BOOT_TRAINING_TABLE* pMBTT = (BOOT_TRAINING_TABLE*) trainingTableStart;

    // check for correct table version
    if (TABLE_VAL(pMBTT->Version) != BOOT_TRAINING_TABLE_11_VERSION)
    {
        //FW_MBOX_WR32_STALL(7, *pdmem);
        //FW_MBOX_WR32_STALL(8, trainingTableOffset);
        //FW_MBOX_WR32_STALL(9, trainingTableSize);
        //FW_MBOX_WR32_STALL(10, TABLE_VAL(pMBTT->Version));
        //FW_MBOX_WR32_STALL(11, BOOT_TRAINING_TABLE_1X_VERSION);
        //FW_MBOX_WR32_STALL(12, TABLE_VAL(pMBTT->StructSize));
        //FW_MBOX_WR32_STALL(13, TABLE_VAL(pMBTT->MemBootTrainTblFlags));

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
fbfalconFindFrequencyTableEntry_GA102
(
        LwU16 targetFrequencyMHz
)
{
    // get performance table information
    //Perf6xHeader* pPTH = gBiosTable.pPTH;
    //LwU8 pTBaseEntryCount = TABLE_VAL(pPTH->BaseEntryCount);
    //Perf6xBaseEntry* ppte = (Perf6xBaseEntry*) ((LwU8*)pPTH + TABLE_VAL(pPTH->HeaderSize));
    //LwU16 pTBaseEntryStepSize = TABLE_VAL(pPTH->BaseEntrySize) +
    //        (TABLE_VAL(pPTH->ClockEntryCount) * TABLE_VAL(pPTH->ClockEntrySize));

    // get memory clock performance table information

    PerfMemClk11Header* ppmh = gBiosTable.pMCHp;
    LwU8 pmctBaseEntryCount = TABLE_VAL(ppmh->EntryCount);
    PerfMemClk11BaseEntry* ppme = gBiosTable.pMCBEp;
    LwU16 pMemClkEntryStepSize = TABLE_VAL(ppmh->BaseEntrySize) +
            (TABLE_VAL(ppmh->StrapEntryCount) * TABLE_VAL(ppmh->StrapEntrySize));


    LwU8 i;
    LwU8 index = 0xff;

    for (i=0; i<pmctBaseEntryCount; i++)
    {
        LwU16 minFreqMHz = TABLE_VAL(ppme->Minimum);
        LwU16 maxFreqMHz = TABLE_VAL(ppme->Maximum);

        if ((targetFrequencyMHz >= minFreqMHz) && (targetFrequencyMHz <= maxFreqMHz))
        {
            index = i;
            break;
        }

        //ppte = (Perf6xBaseEntry*)((LwU8*)ppte + pTBaseEntryStepSize);
        ppme = (PerfMemClk11BaseEntry*)((LwU8*)ppme + pMemClkEntryStepSize);

    }

    return index;
}


