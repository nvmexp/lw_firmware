/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJSMBPBI_H
#define PMU_OBJSMBPBI_H

/*!
 * @file    pmu_objsmbpbi.h
 * @copydoc pmu_objsmbpbi.c
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application includes --------------------------- */
#include "unit_api.h"
#include "pmgr/i2cdev.h"
#include "config/g_smbpbi_hal.h"
#include "rmpmusupersurfif.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

/* ------------------------ Types definitions ------------------------------ */
/*!
 * Structure for receiving SMBPBI I2CS MSGBOX requests from ISR.
 *
 * Beginning of structure needs to be binary-compatible with DISP2UNIT_HDR field.
 */
typedef struct
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    LwU32           msgBoxCmd;
} DISPATCH_SMBPBI_IRQ;

/*!
 * Structure for receiving latest Total GPU Power reading from PMGR
 * PWR_MONITOR.
 *
 * Beginning of structure needs to be binary-compatible with DISP2UNIT_HDR field.
 */
typedef struct
{
    DISP2UNIT_HDR       hdr;
    LwU32               totalGpuPowermW;
} DISPATCH_SMBPBI_PWR_MONITOR_TGP_UPDATE;

/*!
 * Generic dispatching structure used to send work in the form of commands and
 * signals to the smbpbi sub-task for processing.
 *
 * Beginning of structure needs to be binary-compatible with DISP2UNIT_HDR field.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    DISPATCH_SMBPBI_IRQ                     irq;
    DISPATCH_SMBPBI_PWR_MONITOR_TGP_UPDATE  pwrMonTgpUpdate;
} DISPATCH_SMBPBI;

/*!
 * There are 3 buffers combined in the same surface, that we share with the RM.
 * This enum helps to callwlate offsets to these buffers within the surface.
 */
typedef enum
{
    //! client scratch space for passing parameters to rmControls
    PMU_SMBPBI_HOST_MEM_TYPE_SCRATCH,

    //! Fermi era InfoROM object tree unpacked
    PMU_SMBPBI_HOST_MEM_TYPE_INFOROM_GF100,

    //! Kepler era InfoROM object tree unpacked
    PMU_SMBPBI_HOST_MEM_TYPE_INFOROM_GK104,

    //! register file, holding client visible/mutable state
    PMU_SMBPBI_HOST_MEM_TYPE_REGFILE,

    //! upper limit of the enum type
    PMU_SMBPBI_HOST_MEM_TYPE_LIMIT,

} PMU_SMBPBI_HOST_MEM_TYPE;

//! This enum lists states of exelwtion for an asynchronous request
enum PMU_SMBPBI_ASYNC_REQUEST_STATE
{
    //! No request filed
    PMU_SMBPBI_ASYNC_REQUEST_STATE_IDLE = 0,

    //! Request sent to the RM, waiting to hear back
    PMU_SMBPBI_ASYNC_REQUEST_STATE_IN_FLIGHT,

    //! Waiting for the client to collect status of the completed request
    PMU_SMBPBI_ASYNC_REQUEST_STATE_COMPLETED,
};

//! Scratch bank type (R/W)
typedef LwU8 SMBPBI_SCRATCH_BANK_TYPE;
#define SMBPBI_SCRATCH_BANK_READ    0
#define SMBPBI_SCRATCH_BANK_WRITE   1

//! Internal state register cache size
#define SMBPBI_REG_CACHE_SIZE       (LW_MSGBOX_CMD_ARG2_REG_INDEX_MAX + 1)

/* ------------------------ External definitions --------------------------- */
/*!
 * SMBPBI object Definition
 */
typedef struct
{
    /*!
     * SMBUS message box configuration. Initialized by RM and reported over
     * SMBUS once requested. No other usecase.
     */
    RM_PMU_MSGBOX_CONFIG  config;

    /*!
     * Total GPU Power, received from power monitor
     */
    LwU32                 totalGpuPowermW;

    /*!
     * async request completion status and data registers provided
     * by the RM to be returned to the caller
     */
    LwU32                asyncRequestRegStatus;
    LwU32                asyncRequestRegData;
    LwU32                asyncRequestRegExtData;

    /*!
     * Shadow copy (local cache) of the internal state registers
     */
    LwU32                regFile[SMBPBI_REG_CACHE_SIZE];

#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_LWLINK_INFO_THROUGHPUT)
    /*!
     * Lwlink throughput sample valid masks
     * Each element is for a counter type
     * Each bit corresponds to a link ID
     */
    LwU32 lwlinkTpValidSampleMask[LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_MAX];

    /*!
     * Lwlink throughput last queried masks
     * Each element is for a counter type
     * Each bit corresponds to a link ID
     */
    LwU32 lwlinkTpLastQueriedMask[LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_MAX];
#endif

    /*!
     * row remap state
     */
#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_RRL_STATS)
    LwU16                 remappedRowsCntCor;
    LwU16                 remappedRowsCntUnc;
    LwBool                bRemappingFailed;
    LwBool                bRemappingPending;

    LW2080_CTRL_FB_GET_ROW_REMAPPER_HISTOGRAM_PARAMS rowRemapHistogram;
#endif // PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_RRL_STATS)

    /*!
     * Boolean tracking when the PMU has redirected a SMBPBI command to the RM
     * for processing and a response from the RM expected.
     */
    LwBool                bRmResponsePending;

    /*!
     * counts of dynamically retired pages
     */
    LwU8                  retiredPagesCntSbe;
    LwU8                  retiredPagesCntDbe;

    /*!
     * Total GPU Power reading available
     */
    LwBool                bTgpAvailable;

    /*!
     * async request sequence number
     */
    LwU8                  asyncRequestSeqNo;

    /*!
     * A variable, tracking progress of an async request
     * (enum PMU_SMBPBI_ASYNC_REQUEST_STATE above)
     */
    LwU8                  asyncRequestState;

#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS)
    /*! Has the accumulate util command been received? */
    LwBool               bEnableUtilAcc;
#endif // PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS

#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_DRIVER_EVENT_MSG)
    /*! DEM FIFO is active */
    LwBool               bDemActive;
#endif // PMU_SMBPBI_DRIVER_EVENT_MSG

} OBJSMBPBI;

extern OBJSMBPBI Smbpbi;

/*!
 * SMBPBI RESIDENT object Definition
 * To be used in interrupt context
 */
typedef struct
{
    /*!
     * Number of outstanding synchronous client requests. Should never
     * exceed 1. Otherwise this is a client protocol error.
     */
     LwU8                 requestCnt;
} OBJSMBPBI_RESIDENT;

extern OBJSMBPBI_RESIDENT SmbpbiResident;

/*!
 * SMBPBI_LWLINK object Definition
 */
typedef struct
{
    /*!
     * Array of samples 64-bit counter values
     */
    LwU64 lwlinkTpSnapshot[LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_MAX];

    /*!
     * Array of last queried 64-bit counter values
     * Used to compute counter delta
     */
    LwU64 lwlinkTpLastQueried[LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_MAX];

} SMBPBI_LWLINK;

extern SMBPBI_LWLINK SmbpbiLwlink[];

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/*!
 * SMBPBI Event Routing
 */
#define SMBPBI_EVT_MSGBOX_REQUEST               (DISP2UNIT_EVT__COUNT + 0U)
#define SMBPBI_EVT_PWR_MONITOR_TGP_UPDATE       (DISP2UNIT_EVT__COUNT + 1U)

/*!
 * SMBPBI Actions
 */
#define PMU_SMBPBI_GET_REQUEST                                       (LwU8)0x00
#define PMU_SMBPBI_SET_RESPONSE                                      (LwU8)0x01

/*!
 * Macros PMU_MUTEX synchronizing communication with SMBPBI on secondary GPU.
 *
 * _TIMEOUT_NS - Duration for which to attempt acquiring mutex before failing
 *     due to timeout.  Lwrrently 50ms.
 */
#define PMU_SMBPBI_MUTEX_TIMEOUT_NS                                    50000000

/*!
 * SMBPBI counters used for utilization accounting
 */
#define PMU_SMBPBI_UTIL_COUNTERS_CONTEXT                             0x00000000
#define PMU_SMBPBI_UTIL_COUNTERS_SM                                  0x00000001

/*!
 * Macro to determine if SMBPBI utilization accounting is enabled
 */
#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS)
#define PMU_SMBPBI_UTIL_ACC_IS_ENABLED()                      Smbpbi.bEnableUtilAcc
#else
#define PMU_SMBPBI_UTIL_ACC_IS_ENABLED()                      LW_FALSE
#endif

/*!
 * Callwlate offset into shared surface by memory buffer type
 */
#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_HOSTMEM_SUPER_SURFACE)

#define PMU_SMBPBI_HOST_MEM_OFFSET(mem_type)                                        \
    ({                                                                              \
        unsigned    _mem_type = (mem_type) % PMU_SMBPBI_HOST_MEM_TYPE_LIMIT;        \
        unsigned    _offset = 0;                                                    \
                                                                                    \
        switch (_mem_type)                                                          \
        {                                                                           \
            case PMU_SMBPBI_HOST_MEM_TYPE_SCRATCH:                                  \
                _offset =                                                           \
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(pascalAndLater.smbpbi.smbpbiHostMemSurface.data.scratchMem);           \
                break;                                                              \
            case PMU_SMBPBI_HOST_MEM_TYPE_INFOROM_GF100:                            \
                _offset =                                                           \
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(pascalAndLater.smbpbi.smbpbiHostMemSurface.data.inforomHalInfo_GF100); \
                break;                                                              \
            case PMU_SMBPBI_HOST_MEM_TYPE_INFOROM_GK104:                            \
                _offset =                                                           \
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(pascalAndLater.smbpbi.smbpbiHostMemSurface.data.inforomHalInfo_GK104);  \
                break;                                                              \
            case PMU_SMBPBI_HOST_MEM_TYPE_REGFILE:                                  \
                _offset =                                                           \
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(pascalAndLater.smbpbi.smbpbiHostMemSurface.data.registerFile);          \
                break;                                                              \
        }                                                                           \
        _offset;                                                                    \
    })

#define PMU_SMBPBI_HOST_MEM_SURFACE()    (PmuInitArgs.superSurface)

#else                   /* PMU_SMBPBI_HOSTMEM_SUPER_SURFACE */

#define PMU_SMBPBI_HOST_MEM_OFFSET(mem_type)                                        \
    ({                                                                              \
        unsigned    _mem_type = (mem_type) % PMU_SMBPBI_HOST_MEM_TYPE_LIMIT;        \
        unsigned    _offset = 0;                                                    \
                                                                                    \
        switch (_mem_type)                                                          \
        {                                                                           \
            case PMU_SMBPBI_HOST_MEM_TYPE_SCRATCH:                                  \
                _offset = LW_OFFSETOF(RM_PMU_SMBPBI_SHARED_SURFACE_ALIGNED, data.scratchMem);           \
                break;                                                              \
            case PMU_SMBPBI_HOST_MEM_TYPE_INFOROM_GF100:                            \
                _offset = LW_OFFSETOF(RM_PMU_SMBPBI_SHARED_SURFACE_ALIGNED, data.inforomHalInfo_GF100); \
                break;                                                              \
            case PMU_SMBPBI_HOST_MEM_TYPE_INFOROM_GK104:                            \
                _offset = LW_OFFSETOF(RM_PMU_SMBPBI_SHARED_SURFACE_ALIGNED, data.inforomHalInfo_GK104); \
                break;                                                              \
            case PMU_SMBPBI_HOST_MEM_TYPE_REGFILE:                                  \
                _offset = LW_OFFSETOF(RM_PMU_SMBPBI_SHARED_SURFACE_ALIGNED, data.registerFile);         \
                break;                                                              \
        }                                                                           \
        _offset;                                                                    \
    })

#define PMU_SMBPBI_HOST_MEM_SURFACE()    (Smbpbi.config.memdescInforom)

#endif                  /* PMU_SMBPBI_HOSTMEM_SUPER_SURFACE */

/*!
 * Colwenience macros for reading host memory buffers by their type
 */
#define smbpbiHostMemRead_SCRATCH(offset, size, pDst)   \
    smbpbiHostMemRead(PMU_SMBPBI_HOST_MEM_OFFSET(PMU_SMBPBI_HOST_MEM_TYPE_SCRATCH) + \
                    (offset), (size), (pDst))
#define smbpbiHostMemRead_INFOROM_GF100(offset, size, pDst)   \
    smbpbiHostMemRead(PMU_SMBPBI_HOST_MEM_OFFSET(PMU_SMBPBI_HOST_MEM_TYPE_INFOROM_GF100) + \
                    (offset), (size), (pDst))
#define smbpbiHostMemRead_INFOROM_GK104(offset, size, pDst)   \
    smbpbiHostMemRead(PMU_SMBPBI_HOST_MEM_OFFSET(PMU_SMBPBI_HOST_MEM_TYPE_INFOROM_GK104) + \
                    (offset), (size), (pDst))
#define smbpbiHostMemRead_REGFILE(offset, size, pDst)   \
    smbpbiHostMemRead(PMU_SMBPBI_HOST_MEM_OFFSET(PMU_SMBPBI_HOST_MEM_TYPE_REGFILE) + \
                    (offset), (size), (pDst))

#define smbpbiScratchRead(offset, pBuf, size)   \
            smbpbiScratchRW((offset), (pBuf), (size), LW_FALSE)
#define smbpbiScratchWrite(offset, pBuf, size)  \
            smbpbiScratchRW((offset), (pBuf), (size), LW_TRUE)
#define smbpbiScratchRead32(offset, pBuf)       \
            smbpbiScratchRead((offset), (pBuf), sizeof(LwU32))
#define smbpbiScratchWrite32(offset, pBuf)      \
            smbpbiScratchWrite((offset), (pBuf), sizeof(LwU32))

// DMA transfer size that SMBPBI uses for reading from the shared InfoROM buffer
#define SMBPBI_IFR_XFER_SIZE                                                  32

/* ------------------------- Public Functions ------------------------------ */
void sysMonInit(void);
FLCN_STATUS smbpbiDispatch(DISPATCH_SMBPBI *pDispSmbpbi);
#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_BUNDLED_REQUESTS))
LwU8 smbpbiBundleLaunch(LwU32 *pCmd, LwU32 *pData, LwU32 *pExtData);
#endif // PMU_SMBPBI_BUNDLED_REQUESTS
void smbpbiInitDem(OBJSMBPBI *pSmbpbi);
LwBool smbpbiDemEventSource(void);
LwU8 smbpbiGetDem(LwU8 arg1, LwU8 arg2);
LwU8 smbpbiHostMemRead(LwU32 offset, LwU32 size, LwU8 *pDst);
LwU8 smbpbiHostMemOp(LwU32 cmd, LwU32 *pData);
LwU8 smbpbiRegWr(LwU8 reg, LwU32 data);
LwU8 smbpbiRegRd(LwU8 reg, LwU32 *pData);
LwU8 smbpbiScratchPtr2Offset(LwU8 ptr, SMBPBI_SCRATCH_BANK_TYPE bank_type, LwU32 *pOffset);
LwU8 smbpbiScratchRW(LwU32 offset, void *pBuf, LwU32 size, LwBool bWrite);
void smbpbiCheckEvents(LwU32 *pStatus);
LwU8 smbpbiHandleMsgboxRequest(LwU32 *pCmd, LwU32 *pData, LwU32 *pExtData);

void smbpbiInit(void)       GCC_ATTRIB_SECTION("imem_init", "smbpbiInit");
FLCN_STATUS smbpbiAclwtilCounterHelper(LwU32 delayUs, RM_PMU_GPUMON_PERFMON_SAMPLE *pSample)
    GCC_ATTRIB_SECTION("imem_libEngUtil", "_smbpbiAclwtilCounterHelper");
FLCN_STATUS smbpbiAclwtilCounter(LwU32 delayUs, LwU32 utilCounter)
    GCC_ATTRIB_SECTION("imem_libEngUtil", "_smbpbiAclwtilCounter");
FLCN_STATUS smbpbiFspRpcSendUcodeLoad(void)
    GCC_ATTRIB_SECTION("imem_smbpbi", "smbpbiFspRpcSendUcodeLoad");
FLCN_STATUS smbpbiFspRpcSendUcodeUnload(void)
    GCC_ATTRIB_SECTION("imem_smbpbi", "smbpbiFspRpcSendUcodeUnload");

#endif // PMU_OBJSMBPBI_H

