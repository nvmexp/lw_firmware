/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMUIFCMN_H
#define PMUIFCMN_H

/*!
 * @file   pmuifcmn.h
 * @brief  PMU Interface - Common Definitions
 *
 * Structures, attributes, or other properties common to ALL commands and
 * messages are defined here.
 */

#include "lwmisc.h"
#include "flcnifcmn.h"

/*!
 * Colwenience macros for determining the size of body for a command or message:
 */
#define RM_PMU_CMD_BODY_SIZE(u,t)      sizeof(RM_PMU_##u##_CMD_##t)
#define RM_PMU_MSG_BODY_SIZE(u,t)      sizeof(RM_PMU_##u##_MSG_##t)

/*!
 * Colwenience macros for determining the size of a command or message:
 */
#define RM_PMU_CMD_SIZE(u,t)  \
    (RM_FLCN_QUEUE_HDR_SIZE + RM_PMU_CMD_BODY_SIZE(u,t))

#define RM_PMU_MSG_SIZE(u,t)  \
    (RM_FLCN_QUEUE_HDR_SIZE + RM_PMU_MSG_BODY_SIZE(u,t))

/*!
 * Colwenience macros for determining the type of a command or message
 * (intended to be used symmetrically with the CMD and MSG _SIZE macros):
 */
#define RM_PMU_CMD_TYPE(u,t)  RM_PMU_##u##_CMD_ID_##t
#define RM_PMU_MSG_TYPE(u,t)  RM_PMU_##u##_MSG_ID_##t

/*!
 * Define the maximum number of command sequences that can be in flight at
 * any given time.  This is dictated by the width of the sequence number
 * id ('seqNumId') stored in each sequence packet (lwrrently 8-bits).
 *
 * NOTE: Modified this from 256 => 64 for the sake of reducing kernel memory
 *       footprint. Ideally we will never have more than 64 on-the-fly sequences
 *       from the RM to PMU. -kyleo
 */
#define RM_PMU_MAX_NUM_SEQUENCES                64U

/*!
 * Defines the logical queue IDs that must be used when submitting commands
 * to the PMU.  The identifiers must begin with zero and should increment
 * sequentially.  _COUNT must always be set to the last identifier plus one.
 *
 * RM_PMU_COMMAND_QUEUE_HPQ
 *  High Priority Queue for commands from RM to PMU. Commands can have optional
 *  MSG sent by PMU to RM on the MSG queue. Each command is acked by PMU.
 *  Commands sent to HPQ have highest priority. Commands within HPQ are served
 *  in first come first served order (FCFS).
 *
 * RM_PMU_COMMAND_QUEUE_LPQ
 *  Low Priority Queue for commands from RM to PMU. Commands can have optional
 *  MSG sent by PMU to RM on the MSG queue. Each command is acked by PMU.
 *  Commands sent to LPQ are served after HPQ. Commands within LPQ are served
 *  in first come first served order (FCFS).
 *
 * RM_PMU_COMMAND_QUEUE_FBFLCN
 *  Queue for FB falcon to PMU communication.
 *  This will be mostly used by FB falcon for acknowledging the completion of
 *  commands sent from PMU.
 *
 * RM_PMU_COMMAND_QUEUE__LAST
 *  Must always be set to the last command queue identifier.
 */
#define RM_PMU_COMMAND_QUEUE_HPQ                0U
#define RM_PMU_COMMAND_QUEUE_LPQ                1U
#define RM_PMU_COMMAND_QUEUE_FBFLCN             2U
#define RM_PMU_COMMAND_QUEUE__LAST              2U
#define RM_PMU_MESSAGE_QUEUE                    3U
#define RM_PMU_QUEUE_COUNT                      4U

/*!
 * Verifies that the given queue identifier is a valid command queue id.
 * It is expected that the id is specified as an unsigned integer.
 */
#define  RM_PMU_QUEUEID_IS_COMMAND_QUEUE(id)       \
             ((id)  < RM_PMU_MESSAGE_QUEUE)

/*!
 * Verifies that the given queue identifier is a valid RM command queue id.
 */
#define  RM_PMU_QUEUEID_IS_RM_COMMAND_QUEUE(id)    \
            (((id) == RM_PMU_COMMAND_QUEUE_HPQ) || \
             ((id) == RM_PMU_COMMAND_QUEUE_LPQ))

/*!
 * Verifies that the given queue identifier is a valid message queue id.
 * It is expected that the id is specified as an unsigned integer.
 */
#define  RM_PMU_QUEUEID_IS_MESSAGE_QUEUE(id)       \
             ((id) == RM_PMU_MESSAGE_QUEUE)

/*!
 * An enumeration containing all valid logical mutex identifiers.
 *
 * NOTE: Please avoid using the IDs here since we are not using them anymore in
 * RM and RTOS falcons. We are planning to completely remove these IDs until the
 * remaining user (lwwatch) moved away from these deprecated IDs.
 */
enum
{
    RM_PMU_MUTEX_ID_RSVD1 = 0,
    RM_PMU_MUTEX_ID_RSVD2    ,
    RM_PMU_MUTEX_ID_RSVD3    ,
    RM_PMU_MUTEX_ID_RSVD4    ,
    RM_PMU_MUTEX_ID_RSVD5    ,
    RM_PMU_MUTEX_ID_RSVD6    ,
    RM_PMU_MUTEX_ID_RSVD7    ,
    RM_PMU_MUTEX_ID_MSGBOX   ,  // only used by lwwatch and needs to be removed
    RM_PMU_MUTEX_ID_RSVD8    ,
    RM_PMU_MUTEX_ID_RSVD9    ,
    RM_PMU_MUTEX_ID_RSVD10   ,
    RM_PMU_MUTEX_ID_CLK      ,  // only used by lwwatch and needs to be removed
    RM_PMU_MUTEX_ID_RSVD11   ,
    RM_PMU_MUTEX_ID_RSVD12   ,
    RM_PMU_MUTEX_ID_RSVD13   ,
    RM_PMU_MUTEX_ID_RSVD14   ,
    RM_PMU_MUTEX_ID_ILWALID
};

/*!
 * Compares an unit id against the values in the unit_id enumeration and
 * verifies that the id is valid.  It is expected that the id is specified
 * as an unsigned integer.
 */
#define  RM_PMU_UNITID_IS_VALID(id)              \
             ((id) < RM_PMU_UNIT_END)

/*!
 * Defines the size of the surface/buffer that will be allocated to store
 * debug spew from the PMU ucode application when falcon-trace is enabled.
 */
#define RM_PMU_DEBUG_SURFACE_SIZE                               (32U * 1024U)

/*!
 * The PMU's frame-buffer interface block has several slots/indices which can
 * be bound to support DMA to various surfaces in memory. This is an
 * enumeration that gives name to each index based on type of memory-aperture
 * the index is used to access.
 *
 * XXX: The _PELPG dmaslot needs to be removed. One of the general-purpose
 *      dmaslots (_VIRT/_PHYS_???) must be used instead.
 */
enum
{
    RM_PMU_DMAIDX_UCODE         = 0,
    RM_PMU_DMAIDX_VIRT          = 1,
    RM_PMU_DMAIDX_PHYS_VID      = 2,
    RM_PMU_DMAIDX_PHYS_SYS_COH  = 3,
    RM_PMU_DMAIDX_PHYS_SYS_NCOH = 4,
    RM_PMU_DMAIDX_RSVD          = 5,
    RM_PMU_DMAIDX_PELPG         = 6,
    RM_PMU_DMAIDX_END           = 7
};

/*!
 * Falcon PMU DMA's minimum size in bytes.
 *
 * After GF10X (falcon3) removal minimal value required here is 4, but we keep
 * this at 16 to assure more efficient DMA transfers in PMU.
 */
#define RM_PMU_FB_COPY_RW_ALIGNMENT                                          16U

/*!
 * Macros to make aligned versions of RM_PMU_XXX structures. PMU needs aligned
 * data structures to issue DMA read/write operations.
 */
#define RM_PMU_MAKE_ALIGNED_STRUCT_HELPER(type, size)                          \
    typedef union                                                              \
    {                                                                          \
        type    data;                                                          \
        LwU8    pad[LW_ALIGN_UP(sizeof(type), (size))];                   \
    } type##_ALIGNED, *P##type##_ALIGNED

#define RM_PMU_MAKE_ALIGNED_STRUCT(type)                                       \
    RM_PMU_MAKE_ALIGNED_STRUCT_HELPER(type, RM_PMU_FB_COPY_RW_ALIGNMENT)

/*!
 * Mailbox register index used by PMU to report the falcon being booted inselwre
 * mode. @ref LSF_FALCON_MODE_TOKEN_FLCN_INSELWRE
 */
#define LSF_PMU_FALCON_MODE_MAILBOX_ID           (0x6U)

/*!
 * @brief   RM to PMU RPC (Remote Procedure Call) header structure.
 */
typedef struct
{
    /*!
     * Identifies the unit servicing requested RPC.
     */
    LwU8        unitId;
    /*!
     * Identifies the requested RPC (within the unit).
     */
    LwU8        function;
    /*!
     * RPC call flags (@see RM_PMU_RPC_FLAGS).
     */
    LwU8        flags;
    /*!
     * Falcon's status code to describe failures.
     */
    FLCN_STATUS flcnStatus;
    /*!
     * RPC's total exec. time (measured on RM side).
     */
    LwU32       execTimeRmns;
    /*!
     * RPC's actual exec. time (measured on PMU side).
     */
    LwU32       execTimePmuns;
} RM_PMU_RPC_HEADER,
*PRM_PMU_RPC_HEADER;

/*!
 * @brief   PMU to RM RPC (Remote Procedure Call) header structure.
 */
typedef struct
{
    /*!
     * Identifies the unit servicing requested RPC.
     */
    LwU8        unitId;
    /*!
     * Identifies the requested RPC (within the unit).
     */
    LwU8        function;
    /*!
     * Time of RPC to transfer from PMU, to dispatch in the RM.
     */
    RM_FLCN_U64 rpcTransferTime;
} PMU_RM_RPC_HEADER,
*PPMU_RM_RPC_HEADER;

#endif  // PMUIFCMN_H

