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


LwU8
fbfalconFindGpioPin_GH100
(
        LwU32 gpioFunction
)
{
    LwU32 gpioEntryCount = gBiosTable.pGPIOHeader->entry_count;
    LIMIT(gBiosTable.pGPIOHeader->entry_count,36,FBFLCN_ERROR_CODE_GPIO_COUNT_OVERFLOW);

    GPIO41Entry* pGpioEntry = gBiosTable.pFirstGpioEntry;

    LwBool bLoop = LW_TRUE;
    LwBool bFoundEntry = LW_FALSE;
    LwU32 i = 0;
    while (bLoop) {
        if (pGpioEntry->Function == gpioFunction)
        {
            bLoop = LW_FALSE;
            bFoundEntry = LW_TRUE;
        }
        pGpioEntry++;
        i++;
        if (i >= gpioEntryCount)
        {
            bLoop = LW_FALSE;
        }
    }
    if (bFoundEntry)
    {
        return pGpioEntry->GPIONum;
    }
    return LW_GPIO_NUM_UNDEFINED;
}


//
// Lookup of the GPIO pin is lwrrently not straight forward, going throug the DCB table reveals a
// gpio table pointer relative to PCI offset, which is not accessible straight forward and/or subject
// to change for hopper time frame.
// Bios team agreed to add a direct DIRT id for the gpio table which will give us direct access through
// the vdpa lookup
//
// Bug 3183843 - Add DIRD ID for DCB GPIO Table

void
fbfalconLoadGpioTable_GH100
(
        LwU32 dmem
)
{
#if !(FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    This feature can only be used with the memory reduction achieved through preparsing the VPDA table
    as there is not enough dmem space to keep all vbios data resident.
#endif

    LwU32* pdmem = &dmem;

    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(LW_GFW_DIRT_GPIO_TABLE);
    if (pVdpaEntry == NULL)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_GPIO_TABLE_NOT_FOUND);
        return;
    }

    // the dcb table includes multiple segements with the GPIO tables only beeing a small subsection
    // to avoid excessive loading and potentially overflowing the allocated buffer space the ucode
    // only loads the GPIO sub - table:
    // 1) read dcb header (size of struct DCB40Header)
    // 2) extract GPIO_ptr
    // 3) read gpio header (size of struct DCB40_GPIO_Header)
    // 4) extract entries and size per entry
    // 5) read gpio entries (size entries * size of entry) (struct GPIO41Entry)

    LwU32 gpioTableOffset = pVdpaEntry->offset_start;

    // signature at 0x7139
    // start at  0x7133:
    //  41 23 00 08 D6 13 CB BD DC 4E EC 0B 00 00 00 00 00 00 18 14 9D 14 C1 00 00 00 00 00 00
    // version: 41
    // GPIO_ptr = 0x0BEC

    // start gpio header: 0x69EC: 41 06 24
    // start of bios seems to be at 0x5E00

    // lwrrently reading from 0x4684
    // with ptr: 0x0bec
    // desc offset: 0x13dc

    LwU32 gpioTableStart = loadTableSegment(
            gpioTableOffset, pdmem, sizeof(DCB40_GPIO_Header));

    gBiosTable.pGPIOHeader = (DCB40_GPIO_Header*) gpioTableStart;

    // check for correct table version and signature
    LwBool bError = ((gBiosTable.pGPIOHeader->version != DCB40_GPIO_TABLE_VERSION) ||
                     (gBiosTable.pGPIOHeader->header_size != sizeof(DCB40_GPIO_Header)));
    if (bError)
    {
        LwU32* pp = (LwU32*)gpioTableStart;
        LwU8 ii;
        for (ii=0; ii<8; ii++) {
            FW_MBOX_WR32_STALL(ii,(*pp++));
        }
        //FW_MBOX_WR32_STALL(10, gBiosTable.pGPIOHeader->header_size);
        //FW_MBOX_WR32_STALL(11, gBiosTable.pGPIOHeader->entry_count);
        //FW_MBOX_WR32_STALL(12, gBiosTable.pGPIOHeader->entry_size);
        //FW_MBOX_WR32_STALL(13, gBiosTable.pGPIOHeader->version);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_GPIO_TABLE_HEADER_CHECK_FAILURE);
    }

    LwU32 gpioEntryOffset = gpioTableOffset + gBiosTable.pGPIOHeader->header_size;

    LwU32 gpioEntriesStart = loadTableSegment(
            gpioEntryOffset, pdmem, gBiosTable.pGPIOHeader->entry_count * gBiosTable.pGPIOHeader->entry_size);

    gBiosTable.pFirstGpioEntry = (GPIO41Entry*)gpioEntriesStart;

}

