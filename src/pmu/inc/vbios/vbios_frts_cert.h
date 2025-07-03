/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @brief   Interfaces for interacting with the CERT-specific portions of
 *          Firmware Runtime Security.
 */

#ifndef VBIOS_FRTS_CERT_H
#define VBIOS_FRTS_CERT_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "vbios/vbios_frts_cert30.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_CERT30)
/*!
 * Maximum number of VDPA entries supported in FRTS region.
 */
#define VBIOS_FRTS_VDPA_ENTRIES_MAX \
    (VBIOS_FRTS_CERT30_VDPA_ENTRIES_MAX)

/*!
 * Size of VDPA entries in FRTS region.
 */
#define VBIOS_FRTS_VDPA_ENTRIES_SIZE \
    (VBIOS_FRTS_CERT30_VDPA_ENTRIES_SIZE)
#else // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_CERT30)
/*!
 * Maximum number of VDPA entries supported in FRTS region.
 *
 * @note    This is a meaningless fallback value to keep the macro defined when
 *          FRTS is not enabled.
 */
#define VBIOS_FRTS_VDPA_ENTRIES_MAX \
    (1U)

/*!
 * Size of VDPA entries in FRTS region.
 *
 * @note    This is a meaningless fallback value to keep the macro defined when
 *          FRTS is not enabled.
 */
#define VBIOS_FRTS_VDPA_ENTRIES_SIZE \
    (1U)
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_CERT30)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
//
// If FRTS is enabled, we need at least one "CERT" variant supported, so assert
// that here.
//
// Lwrrently, this is only CERT30, but additional variants should be added here
// in the future.
//
ct_assert(
    !PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS) ||
    PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_CERT30));

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */

#endif // VBIOS_FRTS_CERT_H
