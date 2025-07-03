/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOSCMDMGMT_H
#define LWOSCMDMGMT_H

/*!
 * @file    lwoscmdmgmt.h
 *
 * Falcons communicate with the RM (and possibly other falcons) using queues to
 * exchange messages. These queues are cirlwlar buffers located within falcon's
 * DMEM and each of them is defined by it's start address (S), end address (E),
 * and by the current position of the head (H) and tail (T) pointers that are
 * stored within falcon's registers. Message sender is writing data using head
 * pointer while message receiver is accessing data using tail pointer. Race
 * conditions are avoided since only sender/receiver can update H/T pointers
 * respectively. Condition (H == T) uniquely identify an empty queue, meaning
 * SW will never fill queue till the last byte. S, E, H, &T are DWORD aligned.
 * Messages always occupy continuous DMEM locations (not wrapped around the
 * queue's end) so that the receiver does not require additional memory to
 * restore message into the appropriate data structure. Unused space at the
 * end of the queue is explicitly skipped by insertng the REWIND message.
 * Such design imposes a restriction that no message can exceed one half of
 * the queue's size (or message writing routine can deadlock). Problem with
 * this restriction is fact that the queue can be empty (H == T) while in worst
 * case H&T can point to the middle of the queue (H == T == ((S + E) / 2))
 * meaning both before and after we have free only half of the queue. To fix
 * this sender would need to handle special case (H == T) and it would have to
 * force (H == T == S) while it does not have permisson to access tail pointer.
 * Updating the head pointer by the sender raises an interrupt to the receiver
 * notifying it that the queue is not empty.
 *
 * Example:
 *
 *       S               H                     T               E
 * ------+---------------+---------------------+---------------+----------------
 *       | 7 7 7 8 9 9 9 |                     | 5 5 6 6 R ? ? |
 * ------+---------------+---------------------+---------------+----------------
 * DMEM  | <--------------------- queue ---------------------> |
 *
 * In above example queue oclwpies falcon's DMEM address range S->E. T points
 * into DMEM where receiver can retrieve next message #5. That one is followed
 * by #6 and REWIND message indicates that the remaining space in the queue is
 * not utilized. Subsequently from the beginning queue contains messages #7-9.
 * Finally H points to the free space within the queue where sender can write
 * message that does not exceed (T - H) bytes.
 *
 * Actual implementation details should be retrieved directly from the code.
 *
 * @note
 *
 * 1)
 * Since RM does not have DMEM and falcons cannot write directly into sysmem
 * a special case was introduced where falcon allocates queue within it's own
 * DMEM, writes message into it, and manually triggers interrupt to HOST.
 * 2)
 * Since SW support for first falcon was added messages sent by RM to falcons
 * were referred as CMDs (Commands) and falcons' responses to RM were referred
 * as MSGs (Messages) and respective H/T registers were named accordingly.
 * 3)
 * Since falcon to falcon communication always writes messages to the other
 * falcon's DMEM and uses command H/T registers (that automatically raise IRQ)
 * new naming will have to be introduced to avoid CMD/MSG naming issues.
 */

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"
#include "lwrtos.h"

/* ------------------------ Application includes ---------------------------- */
#include "flcnifcmn.h"

/* ------------------------ Types definitions ------------------------------- */
/*!
 * @brief   Retrieves the value of the queue's head pointer
 *
 * @return  Queue's head pointer
 */
typedef LwU32   OsCmdmgmtQueueHeadGet(void);

/*!
 * @brief   Updates the value of the queue's head pointer
 *
 * @param[in]   head    New value of the queue's head pointer
 */
typedef void    OsCmdmgmtQueueHeadSet(LwU32 head);

/*!
 * @brief   Retrieves the value of the queue's tail pointer
 *
 * @return  Queue's tail pointer
 */
typedef LwU32   OsCmdmgmtQueueTailGet(void);

/*!
 * @brief   Writes message (header + body) to the queue at @ref head position
 *
 * @note    Caller MUST obtain the @ref mutex before calling this function
 *
 * @param[in,out]   pHead   Target location (updated after call)
 * @param[in]       pHdr    A pointer to the header of the message
 * @param[in]       pBody   A pointer to the body of the message
 * @param[in]       size    The number of bytes to write (header included)
 *
 * @return  FLCN_OK                     Success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   If invalid size, pHde, or pBody
 * @return  FLCN_ERR_TIMEOUT            Timeout on FB flush
 * @return  FLCN_ERR_xxx                returned from dmaWrite or fbFlush
 */
typedef void    OsCmdmgmtQueueDataPost(LwU32 *pHead, RM_FLCN_QUEUE_HDR *pHdr, const void *pBody, LwU32 size);

/*!
 * @brief   Structure encapsulating data describing outgoing queue.
 *
 * @note    Use-cases must update ALL relevant fields.
 *
 * NJ-TODO ... (document individual fields)
 */
typedef struct
{
    LwrtosSemaphoreHandle   mutex;
    LwU32                   start;
    LwU32                   end;
    OsCmdmgmtQueueHeadGet  *headGet;
    OsCmdmgmtQueueHeadSet  *headSet;
    OsCmdmgmtQueueTailGet  *tailGet;
    OsCmdmgmtQueueDataPost *dataPost;
} OS_CMDMGMT_QUEUE_DESCRIPTOR;

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief   Macro helper for acquiring message queue mutex.
 */
#define osCmdmgmtRmQueueMutexAcquire()                                         \
    lwrtosSemaphoreTake(OsCmdmgmtQueueDescriptorRM.mutex, lwrtosMAX_DELAY)

/*!
 * @brief   Macro helper for queueing messages while holding queue mutex.
 */
#define osCmdmgmtRmQueueMutexReleaseAndPost(pHdr, pBody)                       \
    do {                                                                       \
        lwrtosENTER_CRITICAL();                                                \
        {                                                                      \
            lwrtosSemaphoreGive(OsCmdmgmtQueueDescriptorRM.mutex);             \
            (void)osCmdmgmtRmQueuePostBlocking((pHdr), (pBody));               \
        }                                                                      \
        lwrtosEXIT_CRITICAL();                                                 \
    } while (LW_FALSE)

/*!
 * Gets pointer to FB Queue HDR.  Is right before the FLCN Queue HDR,
 * so back up by size.
 */
#define FBQ_GET_FBQ_HDR_FROM_QUEUE_HDR(p)                                      \
            ((RM_FLCN_FBQ_HDR *)(void *)((LwU8 *)(p) - sizeof(RM_FLCN_FBQ_HDR)))

/*!
 * Data alignment required by osCmdmgmtQueuePost() and all sub-interfaces.
 */
#define GCC_ATTRIB_ALIGN_QUEUE()                                               \
    GCC_ATTRIB_ALIGN(RM_FLCN_DMEM_ACCESS_ALIGNMENT)

/* ------------------------ External definitions ---------------------------- */
extern OS_CMDMGMT_QUEUE_DESCRIPTOR  OsCmdmgmtQueueDescriptorRM;

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
#define osCmdmgmtPreInit MOCKABLE(osCmdmgmtPreInit)
FLCN_STATUS osCmdmgmtPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "osCmdmgmtPreInit");

void    osCmdmgmtCmdQSweep(RM_FLCN_QUEUE_HDR *pHdr, LwU8 queueId);

#define osCmdmgmtQueuePost MOCKABLE(osCmdmgmtQueuePost)
LwBool  osCmdmgmtQueuePost(OS_CMDMGMT_QUEUE_DESCRIPTOR *pQD, RM_FLCN_QUEUE_HDR *pHdr, const void *pBody, LwBool bBlock);
void    osCmdmgmtQueueWrite_DMEM(LwU32 *pHead, RM_FLCN_QUEUE_HDR *pHdr, const void *pBody, LwU32 size);

#define osCmdmgmtRmQueuePostBlocking    MOCKABLE(osCmdmgmtRmQueuePostBlocking)
LwBool  osCmdmgmtRmQueuePostBlocking   (RM_FLCN_QUEUE_HDR *pHdr, const void *pBody);

#define osCmdmgmtRmQueuePostNonBlocking MOCKABLE(osCmdmgmtRmQueuePostNonBlocking)
LwBool  osCmdmgmtRmQueuePostNonBlocking(RM_FLCN_QUEUE_HDR *pHdr, const void *pBody);

FLCN_STATUS osCmdmgmtQueueWaitForEmpty(OS_CMDMGMT_QUEUE_DESCRIPTOR *pQD);

/* ------------------------ Inline Functions -------------------------------- */
#endif // LWOSCMDMGMT_H
