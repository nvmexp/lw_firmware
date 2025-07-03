/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJACR_H
#define PMU_OBJACR_H

/*!
 * @file pmu_objacr.h
 *   Manages following tasks
 *      1. Add a new task for LS bootstrapping for falcon reset case
 */

/* ------------------------ System Includes -------------------------------- */
#include "rmlsfm.h"
#include "acr_status_codes.h"
#include "falcon.h"
#if PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)
#include "acr.h"
#else
#include "acr/pmu_acr.h"
#endif

/* ------------------------ Application Includes --------------------------- */
#include "config/g_acr_hal.h"
/* ------------------------ Forward Declartion ----------------------------- */
typedef struct FBIF_MEMORY_APERTURE FBIF_MEMORY_APERTURE;

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * OBJACR structure to store ACR reg
 */
typedef struct
{
    //
    // WPR header is associated with each Falcon. PMU needs to find out the
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
    // Signature : 128 - bit Signature for the entire Ucode blob
    // Data Size : Size of the data blob placed next to Ucode blob
    //
    // RM initializes the WPR region details after pmuStateLoad.
    // PMU needs start address, region ID, and WPR offset

    // ACR Memory descriptor
    RM_FLCN_MEM_DESC    wprRegionDesc;

    // ACR region ID of WPR region
    LwU8                wprRegionId;

    // WPR offset from startAddress
    LwU8                wprOffset;

    // Falcon's present in WPR header
    LwU8                wprFalconIndex;

    // Buffer used to read/write from/to BSI RAM
    LwU32              *pBsiRWBuf;
} OBJACR;

/*!
 * The sizes of following three variables are multiple of 256. So they are put
 * altogether in this 256 aligned buffer struct to avoid generating DMEM hole.
 * Thus please don't change the sizes of these variables to be non-multiple of
 * 256 unless you can rearrange the variables to minimize the DMEM hole.
 * Otherwise it can cause DMEM hole which can be harmful to some PMU profiles.
 * Also, don't try to explicitly set the alignment of these three variables
 * since that will cause the compiler to declare the size of this struct in
 * same alignment which can also cause DMEM hole.
 */
typedef struct
{

    // WPR header, must be aligned with WPR header for DMA access
    LwU8 pWpr[LSF_WPR_HEADERS_TOTAL_SIZE_MAX_PMU];

    // LSB header, must be aligned with LSB header for DMA access
    LwU8 lsbHeader[LSF_LSB_HEADER_TOTAL_SIZE_MAX];

    // IMEM/DMEM store, must be 256 aligned
    LwU32 xmemStore[IMEM_BLOCK_SIZE/sizeof(LwU32)];
} OBJACR_ALIGNED_256;

/* ------------------------ External Definitions --------------------------- */
extern OBJACR             Acr;
extern OBJACR_ALIGNED_256 Acr256;

/* -------------------------- Compile time checks -------------------------- */
ct_assert((LSF_WPR_HEADER_ALIGNMENT % 256) == 0);
ct_assert(LW_IS_ALIGNED(LSF_WPR_HEADERS_TOTAL_SIZE_MAX_PMU, LSF_WPR_HEADER_ALIGNMENT));
ct_assert(LW_IS_ALIGNED(LSF_LSB_HEADER_TOTAL_SIZE_MAX, LSF_LSB_HEADER_ALIGNMENT));

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */

#endif // PMU_OBJACR_H

