/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_OBJACR_H
#define SEC2_OBJACR_H

/*!
 * @file sec2_objacr.h
 *   Manages following tasks
 *      1. Add a new task for LS bootstrapping for falcon reset case
 */

/* ------------------------ System Includes -------------------------------- */
#include "rmlsfm.h"
#include "rmlsfm_new_wpr_blob.h"

/* ------------------------ Application Includes --------------------------- */
#include "sec2_acr.h"
#include "acrtypes.h"
#include "config/g_acr_hal.h"
/* ------------------------ Forward Declaration ---------------------------- */

/* ------------------------ Types Definitions ------------------------------ */

/*!
 * OBJACR structure to store ACR reg
 */
typedef struct
{
#ifdef NEW_WPR_BLOBS
    // WPR header, must be aligned to LSF_WPR_HEADER_ALIGNMENT for DMA access
    LwU8 pWpr[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
#else
    // WPR header, must be aligned to LSF_WPR_HEADER_ALIGNMENT for DMA access
    LwU8 pWpr[LSF_WPR_HEADERS_TOTAL_SIZE_MAX];
#endif

    //
    // WPR header is associated with each Falcon. SEC2 needs to find out the
    // LSB offset which from WPR base to bootstrap falcon
    // ---------------------------------------------------------
    // |  LS Falcon ID | LSB offset | Bootstrap Owner | Status |
    // ---------------------------------------------------------
    //
    // LSB header Information is gathered from the LS falcon ID
    // --------------------------------------------------------
    // | UO | US | BLCO | BLCS| BLDO | BLDS| Sign | Data size |
    // --------------------------------------------------------
    // Ucode offset : Ucode offset for this LS falcon (Offset from the base of WPR)
    // Ucode size : Size of this ucode blob
    // BL-code offset : BL offset that will be used by secure falcon while bootstrapping
    // BL-code size : Size of the bootloader code
    // BL-data offset : BL data blob that needs to be loaded while bootstrapping
    // BL-data size : Size of the BL-data blob
    // Signature : 128  bit Signature for the entire Ucode blob
    // Data Size : Size of the data blob placed next to Ucode blob
    //
    // RM initializes the WPR region details after sec2StateLoad.
    // SEC2 assumes start address and WPR offset to be same and get it from MMU registers
    // programmed by ACR load binary.
    // wprRegionId is hardcoded to be region '1' i.e. LSF_WPR_EXPECTED_REGION_ID.
    //

    // ACR Memory descriptor
    RM_FLCN_MEM_DESC wprRegionAddress;

    // ACR region ID of WPR region
    LwU8  wprRegionId;

    // WPR offset from startAddress
    LwU8  wprOffset;
} OBJACR, *POBJACR;

/* ------------------------ External Definitions --------------------------- */
extern OBJACR Acr GCC_ATTRIB_ALIGN(LSF_WPR_HEADER_ALIGNMENT);
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#endif // SEC2_OBJACR_H

