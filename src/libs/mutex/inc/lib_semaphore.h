/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_SEMAPHORE_H
#define LIB_SEMAPHORE_H

/*!
 * @file     lib_semaphore.h
 *
 * @brief    OS based writer biased read-write (RW) semaphore to synchronize
 *           access to DMEM structures.
 *
 * @details  This module implements a set of abstraction routines of RW
 *           semaphore on top of lwrtos semaphore functions. Here are a few key
 *           features:
 *           1. RW semaphore:
 *              - Multiple readers are allowed to read at the same time.
 *              - Write-write exclusion: only one writer can write at any given
 *                time.
 *              - Read-write exclusion: readers can not read while writer is
 *                writing and vise-versa.
 *           2. Writer biased:
 *              - When there is a writer waiting, all existing readers will
 *                finish but new readers will be blocked until the writer or
 *                any new writers finishes.
 *              - If the frequency of writer is high, there is a possible reader
 *                starvation.
 *           3. Functionality exposure through macro wrappers:
 *              - Overlay operation routines are added to the library functions.
 *              - Please use the provided macros instead of the functions.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwuproc.h"
#include "lwrtos.h"

/* ------------------------ Application Includes ---------------------------- */
#include "flcntypes.h"
#include "flcncmn.h"
#include "lwostask.h"
#include "dmemovl.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief    Macro for creating a RW semaphore.
 *
 * @details  Please see @ref osSemaphoreCreateRW for details.
 *
 * @param[in] _ovlIdx  Please see @ref osSemaphoreCreateRW.
 *
 * @return    PSEMAPHORE_RW  Please see @ref osSemaphoreCreateRW.
 */
#define OS_SEMAPHORE_CREATE_RW(_ovlIdx)                                        \
    osSemaphoreCreateRW((_ovlIdx));

/*!
 * @brief    Macro for a reader to acquire the RW semaphore to read data.
 *
 * @details  Please see @ref osSemaphoreRWTakeRD for details.
 *
 * @param[in]  _pSem  Please see @ref osSemaphoreRWTakeRD.
 */
#define OS_SEMAPHORE_RW_TAKE_RD(_pSem)                                         \
    do                                                                         \
    {                                                                          \
        OSTASK_OVL_DESC _ovlDescList[] = {                                     \
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)              \
        };                                                                     \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_ovlDescList);                  \
        osSemaphoreRWTakeRD((_pSem));                                          \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_ovlDescList);                   \
    } while (LW_FALSE)

/*!
 * @brief    Macro for a reader to release the RW semaphore.
 *
 * @details  Please see @ref osSemaphoreRWGiveRD for details.
 *
 * @param[in]  _pSem  Please see @ref osSemaphoreRWGiveRD.
 */
#define OS_SEMAPHORE_RW_GIVE_RD(_pSem)                                         \
    do                                                                         \
    {                                                                          \
        OSTASK_OVL_DESC _ovlDescList[] = {                                     \
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)              \
        };                                                                     \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_ovlDescList);                  \
        osSemaphoreRWGiveRD((_pSem));                                          \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_ovlDescList);                   \
    } while (LW_FALSE)

/*!
 * @brief    Macro for a writer to acquire the RW semaphore to write data.
 *
 * @details  Please see @ref osSemaphoreRWTakeWR for details.
 *
 * @param[in]  _pSem  Please see @ref osSemaphoreRWTakeWR.
 */
#define OS_SEMAPHORE_RW_TAKE_WR(_pSem)                                         \
    do                                                                         \
    {                                                                          \
        OSTASK_OVL_DESC _ovlDescList[] = {                                     \
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)              \
        };                                                                     \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_ovlDescList);                  \
        osSemaphoreRWTakeWR((_pSem));                                          \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_ovlDescList);                   \
    } while (LW_FALSE)

/*!
 * @brief    Macro for a writer to try acquiring the RW semaphore to write data.
 *
 * @details  Please see @ref osSemaphoreRWTakeWRCond for details.
 *
 * @param[in]  _pSem      Please see @ref osSemaphoreRWTakeWRCond.
 * @param[out] pBSuccess  Please see @ref osSemaphoreRWTakeWRCond.
 */
#define OS_SEMAPHORE_RW_TAKE_WR_COND(_pSem, pBSuccess)                         \
    do                                                                         \
    {                                                                          \
        OSTASK_OVL_DESC _ovlDescList[] = {                                     \
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)              \
        };                                                                     \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_ovlDescList);                  \
        osSemaphoreRWTakeWRCond((_pSem), (pBSuccess));                         \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_ovlDescList);                   \
    } while (LW_FALSE)

/*!
 * @brief    Macro for a writer to release the RW semaphore.
 *
 * @details  Please see @ref osSemaphoreRWGiveWR for details.
 *
 * @param[in]  _pSem  Please see @ref osSemaphoreRWGiveWR.
 */
#define OS_SEMAPHORE_RW_GIVE_WR(_pSem)                                         \
    do                                                                         \
    {                                                                          \
        OSTASK_OVL_DESC _ovlDescList[] = {                                     \
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)              \
        };                                                                     \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_ovlDescList);                  \
        osSemaphoreRWGiveWR((_pSem));                                          \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_ovlDescList);                   \
    } while (LW_FALSE)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief    RW semaphore structure.
 *
 * @details  This is the key structure for RW semaphore operations. The fields
 *           of the the structure are exposed but should always be managed by
 *           the provided APIs. Direct modifications may lead to undefined
 *           behavior.
 */
typedef struct
{
    /*!
     * @brief    Tracks the number of active readers.
     *
     * @note     Being active means actively waiting or holding the resource.
     */
    LwU8 readCount;
    /*!
     * @brief    Tracks the number of active writers.
     *
     * @note     Being active means actively waiting or holding the resource.
     */
    LwU8 writeCount;
    /*!
     * @brief    Semaphore of read access.
     * @details  All readers will test this semaphore first to operate further.
     *           Whenever there is a writer, this semaphore will be set so that
     *           existing readers will finish but new readers will be blocked.
     *           This is the key to enforce writer bias.
     */
    LwrtosSemaphoreHandle pSemRd;
    /*!
     * @brief    Semaphore of write operation.
     * @details  The first reader and each writer will set this semaphore to
     *           enforce read-write exclusion and write-write exclusion.
     */
    LwrtosSemaphoreHandle pSemWr;
    /*!
     * @brief    Semaphore that protects @ref SEMAPHORE_RW::readCount.
     */
    LwrtosSemaphoreHandle pSemResourceRd;
    /*!
     * @brief    Semaphore that protects @ref SEMAPHORE_RW::writeCount.
     */
    LwrtosSemaphoreHandle pSemResourceWr;
} SEMAPHORE_RW,
*PSEMAPHORE_RW;

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief      Initialize semaphore objects.
 *
 * @details    It allocates and initializes structure @ref SEMAPHORE_RW in the
 *             specified DMEM overlay, with counters set to 0 and sub-semaphores
 *             allocated and initialized. Please use the provided wrapper
 *             @ref OS_SEMAPHORE_CREATE_RW to ilwoke this function.
 *
 * @param[in] _ovlIdx  Index of the overlay in which we need to allocate the RW
 *                     semaphore.
 *
 * @return    PSEMAPHORE_RW  Pointer to the allocated SEMAPHORE_RW structure.
 *
 */
#define osSemaphoreCreateRW MOCKABLE(osSemaphoreCreateRW)
PSEMAPHORE_RW osSemaphoreCreateRW(LwU8 ovlIdx)
    GCC_ATTRIB_SECTION("imem_libInit", "osSemaphoreCreateRW");

/*!
 * @brief    Reader taking the RW semaphore.
 *
 * @details  This function is blocking. It will wait until the resource being
 *           available for readers. Please use the provided wrapper
 *           @ref OS_SEMAPHORE_RW_TAKE_RD to ilwoke this function.
 *
 * @param[in]  pSem  Pointer to the resource RW semaphore.
 */
void osSemaphoreRWTakeRD(PSEMAPHORE_RW pSem)
    GCC_ATTRIB_SECTION("imem_libSemaphoreRW", "osSemaphoreRWTakeRD");

/*!
 * @brief    Reader releasing the RW semaphore.
 *
 * @details  Please use the provided wrapper @ref OS_SEMAPHORE_RW_GIVE_RD
 *           to ilwoke this function.
 *
 * @param[in]  pSem  Pointer to the resource RW semaphore.
 */
void osSemaphoreRWGiveRD(PSEMAPHORE_RW pSem)
    GCC_ATTRIB_SECTION("imem_libSemaphoreRW", "osSemaphoreRWGiveRD");

/*!
 * @brief    Writer taking the RW semaphore.
 *
 * @details  This function is blocking. It will wait until the resource being
 *           available for writers. Please use the provided wrapper
 *           @ref OS_SEMAPHORE_RW_TAKE_WR to ilwoke this function.
 *
 * @param[in]  pSem  Pointer to the resource RW semaphore.
 */
void osSemaphoreRWTakeWR(PSEMAPHORE_RW pSem)
    GCC_ATTRIB_SECTION("imem_libSemaphoreRW", "osSemaphoreRWTakeWR");

/*!
 * @brief    Writer trying to take the RW semaphore.
 *
 * @details  This function is non-blocking. It will exit if the resource is
 *           unavailable to write. Please use the provided wrapper
 *           @ref OS_SEMAPHORE_RW_TAKE_WR_COND to ilwoke this function.
 *
 * @param[in]  pSem       Pointer to the resource RW semaphore.
 * @param[out] pBSuccess  LW_TRUE      If the RW semaphore is successfully set.
 *                        LW_FALSE     Otherwise.
 */
void osSemaphoreRWTakeWRCond(PSEMAPHORE_RW pSem, LwBool *pBSuccess)
    GCC_ATTRIB_SECTION("imem_libSemaphoreRW", "osSemaphoreRWTakeWRCond");

/*!
 * @brief    Release a write semaphore.
 *
 * @details  Please use the provided wrapper @ref OS_SEMAPHORE_RW_GIVE_WR to
 *           ilwoke this function.
 *
 * @param[in]  pSem  Pointer to the resource RW semaphore.
 */
void osSemaphoreRWGiveWR(PSEMAPHORE_RW pSem)
    GCC_ATTRIB_SECTION("imem_libSemaphoreRW", "osSemaphoreRWGiveWR");

#endif // LIB_SEMAPHORE_H
