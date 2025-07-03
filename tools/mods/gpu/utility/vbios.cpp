/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2013,2015-2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file    vbios.cpp
 * @brief   VBIOS specific utility functions
 */

// The defines below were taken from "vbiosdcb4x.h" in RM tree - since the header
// structure is complicated there with 1000's of lines in seemed more reasonable
// just to copy the few lines and two simple STRUCTs

#include "lwmisc.h"
#include <stdio.h>
#include "core/include/tee.h"
#include "vbios.h"
#include "kepler/gk104/dev_bus.h" //LW_PBUS_IFR_*

#define DCB_PTR_OFFSET                     0x0036
#define LW_DCB4X_MAIN_VERSION_DCB_40   0x00000040
#define DCB4X_MAX_MAIN_ENTRIES                 16

#define LW_DCB4X_PATH_TYPE                                         3:0
#define LW_DCB4X_PATH_TYPE_TMDS                             0x00000002
#define LW_DCB4X_PATH_TYPE_LVDS                             0x00000003
#define LW_DCB4X_PATH_TYPE_SKIPENTRY                        0x0000000F
#define LW_DCB4X_PATH_LOCATION                                   21:20
#define LW_DCB4X_PATH_LOCATION_CHIP                         0x00000000

struct MODS_DCB4X_COMMON_HEADER_STRUCT
{
    UINT08 Version;
    UINT08 HeaderSize;
    UINT08 EntryCount;
    UINT08 EntrySize;
};

struct MODS_DCB4X_MAIN_ENTRY_STRUCT
{
    UINT32 DisplayPath;
    UINT32 DisplaySpecific;
};

// Reverse LVDS and TMDS entries in the DCB array
RC VBIOSReverseLVDSTMDS(UINT08 *BiosImage)
{
    if (BiosImage == NULL)
    {
        Printf(Tee::PriAlways,
            "Error: DCB overrides require VBIOS file - (\"-bios\" argument is missing).\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    UINT16 DCBPtrOffset = *((UINT16*)(&BiosImage[DCB_PTR_OFFSET]));
    MODS_DCB4X_COMMON_HEADER_STRUCT *Header = (MODS_DCB4X_COMMON_HEADER_STRUCT *)
        (BiosImage + DCBPtrOffset);

    if ( (Header->Version != LW_DCB4X_MAIN_VERSION_DCB_40) ||
         (Header->EntrySize != sizeof(MODS_DCB4X_MAIN_ENTRY_STRUCT)) )
    {
        Printf(Tee::PriAlways,
            "Error: DCB overrides not supported for this VBIOS DCB version\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    MODS_DCB4X_MAIN_ENTRY_STRUCT *Entry = (MODS_DCB4X_MAIN_ENTRY_STRUCT*)
        (BiosImage + DCBPtrOffset + Header->HeaderSize);

    for (UINT32 DcbIndex = 0; DcbIndex < DCB4X_MAX_MAIN_ENTRIES; DcbIndex++)
    {
        if (DRF_VAL(_DCB4X, _PATH, _TYPE, Entry[DcbIndex].DisplayPath) == LW_DCB4X_PATH_TYPE_SKIPENTRY)
              continue;

        if (DRF_VAL(_DCB4X, _PATH, _LOCATION, Entry[DcbIndex].DisplayPath) !=
            LW_DCB4X_PATH_LOCATION_CHIP)
            continue;

        switch (DRF_VAL(_DCB4X, _PATH, _TYPE, Entry[DcbIndex].DisplayPath))
        {
            case LW_DCB4X_PATH_TYPE_TMDS:
                Entry[DcbIndex].DisplayPath = FLD_SET_DRF(_DCB4X, _PATH,
                                                          _TYPE, _LVDS,
                                                          Entry[DcbIndex].DisplayPath);
                break;
            case LW_DCB4X_PATH_TYPE_LVDS:
                Entry[DcbIndex].DisplayPath = FLD_SET_DRF(_DCB4X, _PATH,
                                                          _TYPE, _TMDS,
                                                          Entry[DcbIndex].DisplayPath);
                break;
        }
    }

    return OK;
}
