/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_lpwr.h
 * @brief   Common perf change sequence LPWR related defines.
 */

#ifndef CHANGESEQ_LPWR_H
#define CHANGESEQ_LPWR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseq.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Accessor to retrieve a pointer to the global CHANGE_SEQ_LPWR object.
 * @memberof CHANGE_SEQ_LPWR
 * @public
 *
 * @param[in]   pChangeSeq  CHANGE_SEQ pointer.
 *
 * @return CHANGE_SEQ_LPWR object pointer
 * @return NULL if the CHANGE_SEQ_LPWR object does not exist
 */
#define PERF_CHANGE_SEQ_LPWR_GET(pChangeSeq)                                  \
            (((pChangeSeq) == NULL) ? NULL : (pChangeSeq)->pChangeSeqLpwr)

/*!
 * @brief Public interface to get the LPWR script pointer.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @note Not used in automotive builds, as the LPWR feature is disabled.
 *
 * @param[in] pChangeSeq    Change Sequencer Pointer.
 * @param[in] lpwrScriptId  LPWR script id.
 *
 * @return LPWR script pointer if valid
 * @return NULL otherwise
 */
#define PERF_CHANGE_SEQ_LPWR_SCRIPT_GET(pChangeSeqLpwr, lpwrScriptId)          \
    ((((pChangeSeqLpwr) != NULL) &&                                            \
        ((lpwrScriptId) < LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_MAX)) ?   \
     (&(pChangeSeqLpwr)->pChangeSeqLpwrScripts->lpwrScripts[lpwrScriptId]) :   \
     NULL)

/*!
 * @brief Helper macro to provide more info on perf state with respect to the
 * requested LPWR feature.
 *
 * @note  This macro is not perfectly thread safe but it is good enough for the
 *        current use cases as it will be set to TRUE by LPWR task running at
 *        a higher priority than other tasks. It will be set to false by the
 *        perf daemon task which is running at the default priority.
 *        PP-TODO : Add synchronization with other tasks if new use needs it.
 *
 * @memberof CHANG_SEQ
 * @public
 *
 * @param[in]   _featureId  LPWR feature id.
 *                      @ref LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPTS_ID_<XYZ>
 *
 * @return
 *              LW_TRUE     If Perf state is engaged
 *              LW_FALSE    If Perf state is disengaged.
 *
 */
#define perfChangeSeqIsLpwrFeatureEngaged(_featureId)                         \
    ((PERF_CHANGE_SEQ_LPWR_GET(pChangeSeq))->pChangeSeqLpwrScripts->bEngaged)

/*!
 * @brief Helper macro to set / clear CHANGE_SEQ_LPWR::bPerfRestoreDisable.
 *
 * @memberof CHANG_SEQ
 * @public
 *
 * @param[in] pChangeSeq    Change Sequencer Pointer.
 * @param[in] _bDisable     Boolean to disable Perf restore.
 *
 */
#define perfChangeSeqLpwrPerfRestoreDisableSet(pChangeSeq, _bDisable)         \
    ((PERF_CHANGE_SEQ_LPWR_GET(pChangeSeq))->bPerfRestoreDisable = (_bDisable))

/*!
 * @brief Helper macro to set / clear CHANGE_SEQ_LPWR::bPerfRestoreDisable.
 *
 * @memberof CHANG_SEQ
 * @public
 *
 * @param[in] pChangeSeq    Change Sequencer Pointer.
 *
 * @return
 *              LW_TRUE     If Perf restore is disabled
 *              LW_FALSE    If Perf restore is enabled
 */
#define perfChangeSeqLpwrPerfRestoreDisableGet(pChangeSeq)                    \
    ((PERF_CHANGE_SEQ_LPWR_GET(pChangeSeq))->bPerfRestoreDisable)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief PMU component responsible for programming the clocks and volt rails to
 * the desired values during entry and/or exit of low power states.
 *
 * The Change Sequencer Low Power is responsible for building and exelwting
 * scripts to enter or exit various low power features.
 */
struct CHANGE_SEQ_LPWR
{
    /*!
     * @brief Low Power perf change request info.
     *
     * This field is used to restore the perf on Low Power feature exit.
     *
     * @note It is the responsibility of the child class to allocate the memory
     * for this structure and assign this pointer to the allocated memory.
     *
     * @protected
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE         *pChangeLpwr;

    /*!
     * @brief Buffer containing all supported LPWR scripts.
     *
     * @note Change Sequencer will allocate the memory only if all required
     * PMU features are enabled. @ref PMU_PERF_CHANGE_SEQ_LPWR
     *
     * @ref LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPTS
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPTS   *pChangeSeqLpwrScripts;

    /*!
     * @brief Change Sequencer script buffer to DMA in / out the perf
     * change sequencer header and steps. We need new script for LPWR
     * as the current VF switch script may is being used by perf task
     * while the perf daemon is running LPWR post processing perf restore.
     *
     * @ref CHANGE_SEQ_SCRIPT
     *
     * @protected
     */
    CHANGE_SEQ_SCRIPT                           script;

    /*!
     * Boolean tracking whether perf restore is disabled.
     * This boolean will be used by LPWR team for local debugging
     * by disabling the perf restore at run time if they are skipping
     * voltage gate / ungate step.
     */
    LwBool                                      bPerfRestoreDisable;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS perfChangeSeqConstruct_LPWR(CHANGE_SEQ *pChangeSeq)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfChangeSeqConstruct_LPWR");

FLCN_STATUS perfChangeSeqLpwrScriptBuild(CHANGE_SEQ *pChangeSeq)
    GCC_ATTRIB_SECTION("imem_changeSeqLoad", "perfChangeSeqLpwrScriptBuild");

/* ------------------------ Include Derived Types --------------------------- */

#endif // CHANGESEQ_LPWR_H
