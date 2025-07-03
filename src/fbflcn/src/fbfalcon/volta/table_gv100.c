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

#include "fbflcn_access.h"
#include "fbflcn_table.h"
#include "fbflcn_helpers.h"
#include "priv.h"


// find gpio pin is delayed starting with Hopper, this function is
// simply a place holder as the current flow does not seem to support
// stubs when a return value is ilwolved.
LwU8
fbfalconFindGpioPin_GV100
(
        LwU32 gpioFunction
)
{
   return 0;
}

void
fbfalconLoadBootTrainingTable_GV100
(
        LwU32 *pdmem
)
{
    // dummy function for pre turing implementation and HBM setups
    // the compiler will entirely remove this
    return;
}


/*
 * selectTable
 *  select the proper table for a given state (P-state & frequency)
 *
 */

void
fbfalconSelectTable_GV100
(
        LwU16 targetFrequency
)
{
    gTT.pIEp = gBiosTable.pIEp;
    gTT.pMIEp = gBiosTable.mIEp;
    gTT.pIUO = gBiosTable.pIUO;

    // parse the PerfMemClkEntries to find the correct clock range

    PerfMemClk11BaseEntry* p = gBiosTable.pMCBEp;
    PerfMemClk11BaseEntry* targetTable = p;
    PerfMemClk11BaseEntry* closestTargetTable = p;

    // setup to log existent frequency ranges
    LwU8 maxRegSpace = 10;    // Mailbox(2) to Mailbox(11)
    LwU32 log[maxRegSpace];
    LwU8 j;
    for (j = 0; j < maxRegSpace; j++)
    {
        log[j] = 0;
    }

    LwU8 foundEntry = 0;
    LwU16 deltaFreqMin = 0xffff;
    LwU16 deltaFreq2Max;
    LwU8 foundEntryClosest = 0;

    LwU8 i;
    for (i=0; i<gBiosTable.pMCBEentryCount; i++) {
        LwU32 min = TABLE_VAL(p->Minimum) & 0x3FFF;
        LwU32 max = TABLE_VAL(p->Maximum) & 0x3FFF;

        if (i<maxRegSpace)
        {
            log[i] = (max << 16) | min;
        }

        if ((targetFrequency >= min) && (targetFrequency <= max))
        {
            targetTable = p;
            foundEntry = 1;
        }
        // looking for the closest table below
        if (targetFrequency > max)
        {
            deltaFreq2Max = targetFrequency - max;
            if (deltaFreq2Max < deltaFreqMin)
            {
                deltaFreqMin = deltaFreq2Max;
                closestTargetTable = p;
                foundEntryClosest = 1;
            }
        }
        p = (PerfMemClk11BaseEntry*) ((LwU8*)p + gBiosTable.pMCBEentrySize +
                (gBiosTable.pMCBEstrapCount * gBiosTable.pMCBEstrapSize));
    }

    if (foundEntry == 0)
    {
        // if we don't find a table try the one below this will extend the
        // top table for OC
        if (foundEntryClosest == 1)
        {
            targetTable = closestTargetTable;
        }
        else
        {
            for (i = 0; i < maxRegSpace; i++)
            {
                FW_MBOX_WR32_STALL(i+2, log[i]);
            }
            FW_MBOX_WR32_STALL(12,
                    gBiosTable.pMCBEentryCount);
            FW_MBOX_WR32_STALL(13, targetFrequency);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_PMCT_ENTRY_NOT_FOUND);
        }
    }

    gTT.pMCBEp = targetTable;
    gTT.pMCSEp = (PerfMemClk11StrapEntry*) ((LwU8*)targetTable +
            gBiosTable.pMCBEentrySize + gBiosTable.strap *
            gBiosTable.pMCBEstrapSize);

    LwU8 memTweakIdx = TABLE_VAL(gTT.pMCSEp->MemTweakIdx);

    // get the MemTweakBaseEntry with the same offset
    PerfMemTweak2xBaseEntry* mp = gBiosTable.pMTBEp;
    gTT.pMTBEp = (PerfMemTweak2xBaseEntry*) ((LwU8*)mp +
            memTweakIdx*(gBiosTable.pMTBEentrySize +
                    (gBiosTable.pMTBEextEntryCount * gBiosTable.pMTBEextEntrySize)));

}
