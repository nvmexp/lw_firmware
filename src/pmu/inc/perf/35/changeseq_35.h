/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_35.h
 * @brief   Common perf change sequence 35 related defines.
 */

#ifndef CHANGESEQ_35_H
#define CHANGESEQ_35_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseq.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CHANGE_SEQ_35 CHANGE_SEQ_35;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc perfChangeSeqInfoGet_SUPER()
 * @memberof CHANGE_SEQ_35
 * @public
 */
#define perfChangeSeqInfoGet_35 perfChangeSeqInfoGet_SUPER

/*!
 * @copydoc perfChangeSeqInfoSet_SUPER()
 * @memberof CHANGE_SEQ_35
 * @public
 */
#define perfChangeSeqInfoSet_35 perfChangeSeqInfoSet_SUPER

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Perf Change Sequence 35 Table
 *
 * @extends CHANGE_SEQ
 */
struct CHANGE_SEQ_35
{
    /*!
     * @brief Base class.
     *
     * Must be first member in structure.
     */
    CHANGE_SEQ  super;

    /*!
     * @brief Change request info to force.
     *
     * This change request shall be programmed regardless if it matches the
     * previously programmed change request.
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE          changeForce;

    /*!
     * @brief Buff to store the current change request.
     *
     * This is an aligned version of @ref LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE.
     * It is shared between the PERF and PERF_DAEMON tasks.
     */
     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ALIGNED changeLwrr;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
PerfChangeSeqConstruct     (perfChangeSeqConstruct_35)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfChangeSeqConstruct_35");

/* ------------------------ Include Derived Types --------------------------- */

#endif // CHANGESEQ_35_H
