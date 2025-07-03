/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_AVG_H
#define LIB_AVG_H

/*!
 * @file    lib_avg.h
 * @copydoc lib_avg.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Macros ------------------------------------------ */
/*!
 * @brief   List of an overlay descriptors required by the average library
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_AVG)
#define OSTASK_OVL_DESC_DEFINE_LIB_AVG                                         \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64                                            \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libAvg)                              \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, libAvg)
#else
#define OSTASK_OVL_DESC_DEFINE_LIB_AVG                                         \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * @brief List of clients that use the average library
 */
#define COUNTER_AVG_CLIENT_ID_FREQ    0
#define COUNTER_AVG_CLIENT_ID_VOLT    1
#define COUNTER_AVG_CLIENT_ID_MAX     2

/*!
 * @brief Macro to check if client id is valid
 */
#define COUNTER_AVG_CLIENT_IS_VALID(clientId)                                 \
    ((clientId) < COUNTER_AVG_CLIENT_ID_MAX)

/* ------------------------ Datatypes --------------------------------------- */
/*!
 * Structure to hold the counter sample which includes the timestamp and
 * unit product.
 */
typedef struct
{
    /*!
     * Timestamp in ns.
     */
    LwU64   timens;
    /*!
     * Unit product - represents product of the quantity being averaged with time
     */
    LwU64   unitProduct;
} COUNTER_SAMPLE;


/*!
 * Structure to hold previously requested target and previous
 * sample value computed based on that for a given index.
 */
typedef struct
{
    /*!
     * Index used to identify the type of sample
     */
    LwU8              index;
    /*!
     * Previous requested target. This is used to update the
     * previous sample values.
     */
    LwU32             prevTarget;
    /*!
     * Previous sample values (timestamp and unit product). This is increamented
     * everytime there a new target request or client's request to
     * compute the counted average against it's sample.
     */
    COUNTER_SAMPLE    prevSample;
} COUNTER;

/*!
 * Main structure to hold mask of all supported indices and their
 * counter values to compute counted average frequency.
 */
typedef struct
{
    /*!
     * Mask of all the supported indices
     */
    LwU32       indexMask;
    /*!
     * Pointer to the statically allocated array of counter state.
     */
    COUNTER    *pCounter;
} COUNTER_AVG;

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
// AVG interfaces
FLCN_STATUS swCounterAvgInit   (void)
    GCC_ATTRIB_SECTION("imem_libAvg", "swCounterAvgInit");
FLCN_STATUS swCounterAvgCompute(LwU8 clientId, LwU8 index, COUNTER_SAMPLE *pSample, LwU32 *pAvg)
    GCC_ATTRIB_SECTION("imem_libAvg", "swCounterAvgCompute");
FLCN_STATUS swCounterAvgUpdate(LwU8 clientId, LwU8 index, LwU32 target)
    GCC_ATTRIB_SECTION("imem_libAvg", "swCounterAvgUpdate");

/* ------------------------ Include Derived Types --------------------------- */

#endif // LIB_AVG_H
