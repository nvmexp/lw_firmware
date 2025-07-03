/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_MUTEX_H
#define LIB_MUTEX_H

/*!
 * @file lib_mutex.h
 *
 * @brief This is the common library for the HW mutex related functions.
 *
 * NOTE NOTE NOTE NOTE
 *
 * This lib is not FI (Fault Injection) aware. So, usage of this lib in use cases
 * like establishment of chain of trust is not advisable.
 *
 * NOTE NOTE NOTE NOTE
 *
 *
 * Public interfaces lwrrently exposed by this library:
 * mutexAcquire - when acquire mutex from LS callers.
 * mutexRelease - when release mutex from LS callers.
 * mutexAcquire_hs - when acquire mutex from HS callers.
 * mutexRelease_hs - when release mutex from HS callers.
 *
 *
 * Public MACRO lwrrently exposed by this library:
 * MUTEX_LOG_TO_PHY_ID
 *
 * For other interface or MACRO that you want to call outside this library,
 * please contact with Nick Kuo first to ensure they will not expose any security
 * info to external.
 *
 *
 * Following are what need to be implemented in the container ucode before using
 * this library:
 *
 * osPTimerTimeNsLwrrentGet
 *   Needed by LS callers. This is already implemented in RTOS lib (uproc/libs/rtos/...).
 *   So just need to link that library before calling into mutex library.
 *
 * osPTimerTimeNsElapsedGet
 *   Needed by LS callers. This is already implemented in RTOS lib (uproc/libs/rtos/...).
 *   So just need to link that library before calling into mutex library.
 *
 * osPTimerTimeNsLwrrentGet_hs
 *   Needed by HS callers. This is already implemented in RTOS lib (uproc/libs/rtos/...).
 *   So just need to link that library before calling into mutex library.
 *
 * osPTimerTimeNsElapsedGet_hs
 *   Needed by HS callers. This is already implemented in RTOS lib (uproc/libs/rtos/...).
 *   So just need to link that library before calling into mutex library.
 *
 * vApplicationIsLsOrHsModeSet_hs
 *   Needed by HS callers. We need to implement this function to ensure SCTL_LSMODE
 *   or SCTL_HSMODE of the client engine is set before acquiring/releasing the mutex.
 *   This function needs to be placed in HS overlay and that HS overlay needs to be
 *   loaded and validated before calling into the mutex library.
 *
 * CSB_REG_RD32_HS_ERRCHK/CSB_REG_WR32_HS_ERRCHK
 *   Needed by HS callers. These APIs are needed when mutex lib wants to read/write
 *   registers via CSB bus which will check if the operation succeeds or not so
 *   that we can do error handling if the operation fails. Check @ref sec2CsbErrChkRegRd32Hs_GP10X
 *   and @ref sec2CsbErrChkRegWr32Hs_GP10X for sample implentation. This function
 *   needs to be placed in HS overlay and that HS overlay needs to be loaded and
 *   validated before calling into the mutex library.
 *
 * BAR0_REG_RD32_HS_ERRCHK/BAR0_REG_WR32_HS_ERRCHK
 *   Needed by HS callers. These APIs are needed when mutex lib wants to read/write
 *   registers via BAR0 bus which will check if the operation succeeds or not so
 *   that we can do error handling if the operation fails. Check @ref sec2Bar0ErrChkRegRd32Hs_GP10X
 *   and @ref sec2Bar0ErrChkRegWr32Hs_GP10X for sample implentation. This function
 *   needs to be placed in HS overlay and that HS overlay needs to be loaded and
 *   validated before calling into the mutex library.
 *
 * SE_REG_RD32_HS_ERRCHK/SE_REG_WR32_HS_ERRCHK
 *   Needed by HS callers. These APIs are needed when mutex lib wants to read/write
 *   registers via secure bus which will check if the operation succeeds or not so
 *   that we can do error handling if the operation fails. Check @ref seSelwreBusReadRegisterErrChk
 *   and @ref seSelwreBusWriteRegisterErrChk for sample implentation. This function
 *   needs to be placed in HS overlay and that HS overlay needs to be loaded and
 *   validated before calling into the mutex library.
 *
 *
 * Please ensure the mutex mapping establishment function (e.g. sec2MutexEstablishMapping_GP10X
 * or sec2MutexEstablishMappingHS_GP10X in SEC2) is called at the earliest point at booting
 * time to ensure the mapping is established before calling into mutex lib.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwuproc.h"

/* ------------------------ Application Includes ---------------------------- */
#include "flcntypes.h"
#include "flcnretval.h"
#include "lw_mutex.h"

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
extern MUTEX_LOG2PHY_MAPPING mutexLog2PhyTable[LW_MUTEX_ID__COUNT];
extern MUTEX_ENGINE_INFO     mutexEngineInfo[MUTEX_ENGINE_COUNT];

/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/*!
 * Get the engine info of the mutex that we are going to acquire/release
 */
#define MUTEX_GET_ENG_INFO(logId)                                              \
    (((logId) >= LW_MUTEX_ID__COUNT) ?                                         \
    NULL : &mutexEngineInfo[mutexLog2PhyTable[(logId)].engine])

/*!
 * Colwert the logical mutex ID to physical mutex ID of the corresponding engine
 */
#define MUTEX_LOG_TO_PHY_ID(logId)                                             \
    (((logId) >= LW_MUTEX_ID__COUNT) ?                                         \
    LW_MUTEX_PHYSICAL_ID_ILWALID : mutexLog2PhyTable[(logId)].physId)

/*!
 * Colwert the physical mutex ID to the address of LW_XYZ_MUTEX(mutexPhyId)
 * register (XYZ = the engine the mutex belongs to)
 */
#define MUTEX_REG_ADDR(pEI, phyId)                                             \
    ((pEI)->regAddrMutex + ((phyId) * (pEI)->regMutexIndexStride))

/*!
 * Check if the mutex physicial ID is within the mutex count of the engine
 */
#define MUTEX_INDEX_IS_VALID(pEI, phyId)                                       \
    ((phyId) < (pEI)->mutexCount)

/*!
 * Get the owner of the physical mutex ID
 */
#define MUTEX_GET_OWNER(pEI, phyId)                                            \
    DRF_VAL(_FLCN, _MUTEX, _VALUE, osRegRd32((pEI)->accessBus,                 \
            MUTEX_REG_ADDR((pEI), (phyId))))

/*!
 * HS Wrapper to call into the exact register read accessor in each falcon
 *
 * (TODO - This is replicated from LS code for quickly upgrading mutex infra to
 * support HS.  It needs to be removed once the LS/HS code sharing mechanism is
 * implemented)
 */
#define MUTEX_REG_RD32_HS(pEI, addr, pData) mutexRegRd32_hs((pEI)->accessBus, (addr), (pData))

/*!
 * HS Wrapper to call into the exact register write accessor in each falcon
 *
 * (TODO - This is replicated from LS code for quickly upgrading mutex infra to
 * support HS.  It needs to be removed once the LS/HS code sharing mechanism is
 * implemented)
 */
#define MUTEX_REG_WR32_HS(pEI, addr, val)   mutexRegWr32_hs((pEI)->accessBus, (addr), (val))

/* ------------------------ Function Prototypes ----------------------------- */
FLCN_STATUS mutexAcquire(LwU8 mutexLogId, LwU32 timeoutNs, LwU8 *pToken)
    GCC_ATTRIB_SECTION("imem_resident", "mutexAcquire");
FLCN_STATUS mutexRelease(LwU8 mutexLogId, LwU8 token)
    GCC_ATTRIB_SECTION("imem_resident", "mutexRelease");

FLCN_STATUS mutexAcquire_hs(LwU8 mutexLogId, LwU32 timeoutNs, LwU8 *pToken)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "mutexAcquire_hs");
FLCN_STATUS mutexRelease_hs(LwU8 mutexLogId, LwU8 token)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "mutexRelease_hs");

#endif // LIB_MUTEX_H
