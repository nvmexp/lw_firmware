/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_PMUMON_H
#define LIB_PMUMON_H

/*!
 * @file lib_pmumon.h
 *
 * @brief Function and type declarations for the PMUMON lib.
 */

/* ------------------------ Public Variables -------------------------------- */
//! UStreamer data logging queue id used by PMUMon.
extern LwU8 pmumonUstreamerQueueId;

/* ------------------------ Forward Declarations ---------------------------- */
typedef struct PMUMON_QUEUE_DESCRIPTOR PMUMON_QUEUE_DESCRIPTOR;

/* ------------------------ Structure --------------------------------------- */
/*!
 * @brief   Meta-data describing the location and properties of a cirlwlar queue
 *          in the frame buffer.
 */
struct PMUMON_QUEUE_DESCRIPTOR
{
    /*!
     * Indicates if the queue has been constructed.
     */
    LwBool bConstructed;

    /*!
     * Offset from the base of the surface memory descriptor where the header
     * for the cirlwlar queue resides.
     */
    LwU32 headerOffset;

    /*!
     * Offset from the base of the surface memory descriptor where the byte
     * buffer for the cirlwlar queue resides.
     */
    LwU32 bufferOffset;

    /*!
     * The size in bytes of the cirlwlar queue entries to be written.
     */
    LwU32 entrySize;

    /*!
     * The number of elements (of size = entrySize) the FB cirlwlar queue is
     * composed of. This is required in order to wrap around headIndex
     * appropriately.
     */
    LwU32 queueSize;

    /*!
     * Header to be updated with every update of the queue.
     */
    RM_PMU_PMUMON_QUEUE_HEADER header;
};

/* ------------------------ Functions --------------------------------------- */
/*!
 * @brief      Construct a PMUMON Queue Descriptor
 *
 * @param      pQueueDesc       The queue descriptor being constructed
 * @param[in]  headerOffset     The FB super-surface offset of the cirlwlar queue header
 * @param[in]  bufferOffset     The FB super-surface offset of the cirlwlar queue byte buffer
 * @param[in]  entrySize        The size of a queue entry in bytes
 * @param[in]  queueSize        The number of entries in the queue
 *
 * @return     FLCN_OKcif successful.
 */
FLCN_STATUS pmumonQueueConstruct(PMUMON_QUEUE_DESCRIPTOR *pQueueDesc,
                                 LwU32                    headerOffset,
                                 LwU32                    bufferOffset,
                                 LwU32                    entrySize,
                                 LwU32                    queueSize)
    GCC_ATTRIB_SECTION("imem_libPmumonConstruct", "pmumonQueueConstruct");

/*!
 * @brief      Write an entry to the PMUMON Queue described by pQueueDesc.
 *
 * @param[in]  pQueueDesc   Descriptor for a PMUMON Queue.
 * @param[in]  pData        Pointer to the data to be written out.
 *
 * @return     FLCN_OK if successful. Otherwise, any non-OK code that is bubbled
 *             up.
 */
FLCN_STATUS pmumonQueueWrite(PMUMON_QUEUE_DESCRIPTOR *pQueueDesc, void *pData)
    GCC_ATTRIB_SECTION("imem_libPmumon", "pmumonQueueWrite");

/*!
 * @brief       PMUMon hook to setup uStreamer related information during uStreamer
 *              initialization.
 *
 * @return      FLCN_OK if successful. Otherwise, any non-OK code that is bubbled
 *              up.
 */
FLCN_STATUS pmumonUstreamerInitHook(void)
    GCC_ATTRIB_SECTION("imem_libPmumonConstruct", "pmumonUstreamerInitHook");

#endif // LIB_PMUMON_H
