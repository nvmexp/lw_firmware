/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _PMU_FBFLCN_IF_H_
#define _PMU_FBFLCN_IF_H_

/*!
 * @file    pmu_fbflcn_if.h
 *
 * @brief   Communication interfaces between PMU and FB falcon.
 *
 * Assumptions:
 *  - PMU is a master and FBFLCN is a slave.
 *  - There can be at most one PMU request pending at any moment of time.
 *  - FBFLCN must respond to every issued request.
 *
 * Implementation:
 *  - PMU writes request into a scratch space and raises interrupt to FBFLCN.
 *    FBFLCN's queue head/tail registers are utilized both as a scratch space,
 *    and as an interrupt cause.
 *  - PMU's task issuing request blocks itself on a semaphore.
 *  - In response to an interrupt FBFLCN collects request from a scratch space
 *    and continues processing it.
 *  - Once processing is completed FBFLCN writes response into a scratch space
 *    and raises interrupt to PMU. PMU's queue head/tail registers are utilized
 *    both as a scratch space, and as an interrupt cause.
 *  - In response to an interrupt PMU notifies CMDMGMT task that subsequently
 *    fetches the FBFLCN's response, validates that it matches previously issued
 *    request and releases semaphore unblocking task that issued the request.
 */

/*!
 * The FBFLCN's physical queue ID used for incoming traffic from the RM.
 *
 * Does not belong here but keeping it until FBFLCN reorganize it's defines.
 */
#define FBFLCN_CMD_QUEUE_IDX_RM                 0U

/*!
 * The FBFLCN's physical queue ID used for incoming traffic from the PMU.
 */
#define FBFLCN_CMD_QUEUE_IDX_PMU                1U

/*!
 * The FBFLCN's physical queue ID used for communicating PMU HALT form the PMU.
 */
#define FBFLCN_CMD_QUEUE_IDX_PMU_HALT_NOTIFY    2U

/*!
 * The PMU's physical queue ID used for incoming traffic from the FBFLCN.
 */
#define PMU_CMD_QUEUE_IDX_FBFLCN                2U

/*!
 * @brief   Enumeration of command IDs exchanged between the PMU and the FBFLCN.
 *
 *  PMU_FBFLCN_CMD_ID_MCLK_SWITCH
 *      A memory clock switch request/response ID.
 *
 *  PMU_FBFLCN_CMD_ID_PMU_HALT_NOTIFY
 *      A PMU HALT notification command to FBFLCN.
 *
 * @{
 */
#define PMU_FBFLCN_CMD_ID_MCLK_SWITCH       0U
#define PMU_FBFLCN_CMD_ID_PMU_HALT_NOTIFY   1U
#define PMU_FBFLCN_CMD_ID__COUNT            2U
/*!@}*/

/*!
 * @brief   Scratch register format of the PMU to FBFLCN requests.
 *
 * @note    We pack all arguments to utilize only two 32-bit scratch registers.
 *
 *  _HEAD_SEQ
 *      Unique request identifier (auto-incremented by sender).
 *
 *  _HEAD_CMD
 *      ID of the issued request (@see PMU_FBFLCN_CMD_ID_<xyz>).
 *
 *  _HEAD_CYA
 *      Bit should be set such that HEAD and TAIL register values never match.
 *
 *  _HEAD_DATA16
 *      Request specific payload part 1.
 *
 *  _TAIL_DATA32
 *      Request specific payload part 2.
 *
 * @{
 */
#define LW_PMU_FBFLCN_HEAD_SEQ                                               7:0
#define LW_PMU_FBFLCN_HEAD_CMD                                              14:8
#define LW_PMU_FBFLCN_HEAD_CYA                                             15:15
#define LW_PMU_FBFLCN_HEAD_CYA_NO                                           0x00
#define LW_PMU_FBFLCN_HEAD_CYA_YES                                          0x01
#define LW_PMU_FBFLCN_HEAD_DATA16                                          31:16

#define LW_PMU_FBFLCN_TAIL_DATA32                                           31:0
/*!@}*/

/*!
 * @brief   Scratch register format of the FBFLCN to PMU responses.
 *
 * @note    We pack all arguments to utilize only two 32-bit scratch registers.
 *
 *  _HEAD_SEQ
 *      Unique response identifier (must match the original request).
 *
 *  _HEAD_CMD
 *      ID of the issued response (@see PMU_FBFLCN_CMD_ID_<xyz>).
 *
 *  _HEAD_CYA
 *      Bit should be set such that HEAD and TAIL register values never match.
 *
 *  _HEAD_DATA16
 *      Response specific payload part 1.
 *
 *  _TAIL_DATA32
 *      Response specific payload part 2.
 *
 * @{
 */
#define LW_FBFLCN_PMU_HEAD_SEQ                                               7:0
#define LW_FBFLCN_PMU_HEAD_CMD                                              14:8
#define LW_FBFLCN_PMU_HEAD_CYA                                             15:15
#define LW_FBFLCN_PMU_HEAD_CYA_NO                                           0x00
#define LW_FBFLCN_PMU_HEAD_CYA_YES                                          0x01
#define LW_FBFLCN_PMU_HEAD_DATA16                                          31:16

#define LW_FBFLCN_PMU_TAIL_DATA32                                           31:0
/*!@}*/

/*!
 * List of statuses that the FB falcon can send as an ack to any MCLK switch
 * request.
 *
 * _STATUS_OK                 MCLK switch was successful
 * _STATUS_ERR_GENERIC        MCLK switch failed
 * _STATUS_FREQ_NOT_SUPPORTED Unsupported frequency requested
 * _STATUS_ILWALID_PATH       Invalid path specified
 * _FBSTOP_TIME_OVERRUN       During the MCLK switch, FB was stopped for a longer
 *                            time than expected.
 *
 * @{
 */
#define FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_OK                    (0x0)
#define FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_ERR_GENERIC           (0x1)
#define FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_FREQ_NOT_SUPPORTED    (0x2)
#define FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_ILWALID_PATH          (0x3)
#define FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_FBSTOP_TIME_OVERRUN   (0x4)
/*!@}*/

#endif // _PMU_FBFLCN_IF_H_

