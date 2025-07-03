/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_QUEUE_FB_H
#define CMDMGMT_QUEUE_FB_H

/*!
 * @file    cmdmgmt_queue_fb.h
 * @brief   Interfaces shared between heap and PTCB implementation of the FBQ.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "unit_api.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Determines whether a command is targetted for LPWR
 *
 * @note    If LPWR task is not enabled, this macro always returns false.
 *
 * @param[in]   unitId  ID of target unit
 *
 * @return  LW_TRUE     LPWR is enabled and command is destined for LPWR
 * @return  LW_FALSE    Otherwise
 */
#define QUEUE_FB_IS_RPC_TARGET_LPWR(unitId)                                    \
    (OSTASK_ENABLED(LPWR) &&                                                   \
        (((unitId) == RM_PMU_UNIT_LPWR) ||                                     \
         ((unitId) == RM_PMU_UNIT_LPWR_LOADIN)))

/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Initialize head/tail pointers of queues between the RM and the PMU.
 *          Additionally setup the PMU->RM queue descriptor.
 */
void queuefbQueuesInit(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "queuefbQueuesInit");

/*!
 * @brief   Callwlate a simple 16 bit checksum of the data passed in.
 *
 * @param[in]   pBuffer     A pointer to the data to add to checksum.
 * @param[in]   size        The number of bytes in the passed in buffer, must
 *                          be a multiple of 2, will be rounded down if needed.
 *
 * @return  checksum    The sum off all bytes.
 */
LwU16 queueFbChecksum16(const void *pBuffer, LwU32 size)
    GCC_ATTRIB_SECTION("imem_resident", "queueFbChecksum16");

#endif // CMDMGMT_QUEUE_FB_H
