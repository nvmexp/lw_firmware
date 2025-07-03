/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _SOE_OBJSMBPBI_H_
#define _SOE_OBJSMBPBI_H_

/*!
 * @file    soe_objsmbpbi.h
 * @copydoc soe_objsmbpbi.c
 */

/* ------------------------ System includes -------------------------------- */
#include "lwuproc.h"
#include "soesw.h"
#include "smbpbi_shared_lwswitch.h"
/* ------------------------ Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "config/soe-config.h"
#include "config/g_smbpbi_hal.h"
#include "oob/smbpbi_priv.h"

/* ------------------------ Types definitions ------------------------------ */
#if 0   //TODOmy
/*!
 * Structure for receiving SMBPBI I2CS MSGBOX requests from ISR.
 *
 * Beginning of structure needs to be binary-compatible with DISP2UNIT_CMD
 * eventType field.
 */
typedef struct
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    LwU32           msgBoxCmd;
} DISPATCH_SMBPBI_IRQ;

/*!
 * Generic dispatching structure used to send work in the form of commands and
 * signals to the smbpbi sub-task for processing.
 *
 * Beginning of structure needs to be binary-compatible with DISP2UNIT_CMD
 * eventType field.
 */
typedef union
{
    LwU8                                        eventType;
    DISP2UNIT_HDR                               hdr;

    DISP2UNIT_CMD                               cmd;
    DISPATCH_SMBPBI_IRQ                         irq;
} DISPATCH_SMBPBI;
#endif

/*!
 * Enumeration of SMBPBI task's OS_TIMER callback entries
 */
typedef enum
{
    /*!
     * Entry to poll on Driver timeout state
     */
    SMBPBI_OS_TIMER_ENTRY_DRIVER_STATE,
    /*!
     * Must always be the last entry.
     */
    SMBPBI_OS_TIMER_ENTRY_NUM_ENTRIES
} SMBPBI_OS_TIMER_ENTRIES;

//! Scratch bank type (R/W)
typedef enum
{
    SCRATCH_BANK_READ,
    SCRATCH_BANK_WRITE,
} SCRATCH_BANK_TYPE;

/* ------------------------ External definitions --------------------------- */
/*!
 * SMBPBI object Definition
 */

typedef struct
{
    LwU8  firmwareVer[14];
    LwU16 vendorId;
    LwU16 devId;
    LwU16 subVendorId;
    LwU16 subId;
    LwU8  guid[16];
} SOE_SMBPBI_SYSID_DATA;

typedef struct
{
    LwU32   cap[LW_MSGBOX_DATA_CAP_COUNT];  //! Capability data
    LwU8    capCount;                       //! Capability dword counter
    LwBool  bDmaAvailable;                  //! Whether shared surface is available or not
    RM_FLCN_U64 dmaHandle;                  //! DMA handle for the inforom shared surface
    SOE_SMBPBI_SYSID_DATA sysIdData;        //! Static system identification data
} SOE_SMBPBI_CONFIG;

typedef struct
{
    /*!
     * SMBUS message box configuration. Reported over
     * SMBUS once requested.
     */
    SOE_SMBPBI_CONFIG  config;

    /*!
     * Number of outstanding synchronous client requests. Should never
     * exceed 1. Otherwise this is a client protocol error.
     */
    LwU8               requestCnt;

    /*!
     * async request sequence number
     */
    LwU8               asyncRequestSeqNo;

    /*!
     * A variable, tracking progress of an async request
     * (enum SOE_SMBPBI_ASYNC_REQUEST_STATE)
     */
    LwU8               asyncRequestState;

    /*!
     * async request completion status and data registers provided
     * by the RM to be returned to the caller
     */
    LwU32              asyncRequestStatus;

    /*!
     * Boolean signifying driver reload is required
     */
    LwBool             bDriverReloadRequired;

    /*!
     * Driver sequence number used to determine driver timeout state
     */
    LwU32              driverStateSeqNum;

    /*!
     * Boolean signifying driver timeout state
     */
    LwBool             bDriverTimeout;

    /*! DEM FIFO is active */
    LwBool             bDemActive;

    /*!
     * Lwlink throughput sample valid masks
     * Each element is for a counter type
     * Each bit corresponds to a link ID
     */
    LwU64 lwlinkTpValidSampleMask[LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_MAX];

    /*!
     * Lwlink throughput last queried masks
     * Each element is for a counter type
     * Each bit corresponds to a link ID
     */
    LwU64 lwlinkTpLastQueriedMask[LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_MAX];

    /*!
     * Bitmask of link training errors
     */
    RM_SOE_SMBPBI_TRAINING_ERROR_INFO     linkTrainingErrorInfo;

    /*!
     * Bitmask of link runtime errors
     */
    RM_SOE_SMBPBI_RUNTIME_ERROR_INFO      linkRuntimeErrorInfo;

    SMBPBI_HAL_IFACES  hal;

} OBJSMBPBI;

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

extern OBJSMBPBI                  Smbpbi;
extern LwrtosQueueHandle          Disp2QSmbpbiThd;
extern LwrtosTaskHandle           OsTaskSmbpbi;
extern OS_TIMER                   SmbpbiOsTimer;
extern RM_SOE_SMBPBI_INFOROM_DATA *pInforomData;
extern SMBPBI_LWLINK              SmbpbiLwlink[];

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/*!
 * SMBPBI Event Routing
 */
#define SMBPBI_EVT_MSGBOX_IRQ                    (MAX_EVT_PER_UNIT * RM_SOE_UNIT_SMBPBI + 0)

/*!
 * SMBPBI Actions
 */
#define SOE_SMBPBI_GET_REQUEST                                       (LwU8)0x00
#define SOE_SMBPBI_SET_RESPONSE                                      (LwU8)0x01

/*
 * SMBPBI init values
 */
#define SMBPBI_DRIVER_SEQ_NUM_INIT                                   0xFFFFFFFF

#define smbpbiScratchRead(offset, pBuf, size)   \
            smbpbiScratchRW((offset), (pBuf), (size), LW_FALSE)
#define smbpbiScratchWrite(offset, pBuf, size)  \
            smbpbiScratchRW((offset), (pBuf), (size), LW_TRUE)
#define smbpbiScratchRead32(offset, pBuf)       \
            smbpbiScratchRead((offset), (pBuf), sizeof(LwU32))
#define smbpbiScratchWrite32(offset, pBuf)      \
            smbpbiScratchWrite((offset), (pBuf), sizeof(LwU32))

// DMA transfer size that SMBPBI uses for reading from the host shared buffer
#define SMBPBI_IFR_XFER_SIZE                                                  32

/* ------------------------- Public Functions ------------------------------ */
void sysMonInit(void);
FLCN_STATUS smbpbiDispatch(DISP2UNIT_CMD *pDispSmbpbi);
LwU8 smbpbiHandleMsgBoxRequest(LwU32 *pCmd, LwU32 *pData, LwU32 *pExtData);
LwU8 smbpbiBundleLaunch(LwU32 *pCmd, LwU32 *pData, LwU32 *pExtData);
void smbpbiInitDem(OBJSMBPBI *pSmbpbi);
LwBool smbpbiDemEventSource(void);
LwU8 smbpbiGetDem(LwU8 arg1, LwU8 arg2);
LwU8 smbpbiHostMemRead(LwU32 offset, LwU32 size, LwU8 *pDst);
LwU8 smbpbiHostMemOp(LwU32 cmd, LwU32 *pData);
LwU8 smbpbiRegWr(LwU8 reg, LwU32 data);
LwU8 smbpbiRegRd(LwU8 reg, LwU32 *pData);
LwU8 smbpbiScratchPtr2Offset(LwU8 ptr, SCRATCH_BANK_TYPE bank_type, LwU32 *pOffset);
LwU8 smbpbiScratchRW(LwU32 offset, void *pBuf, LwU32 size, LwBool bWrite);
LwU8 smbpbiCheckEvents(LwU32 *pStatus);
FLCN_STATUS smbpbiReadSharedSurface(void *pDmemAddr, LwU32 offset, LwU32 size);
LwBool smbpbiIsSharedSurfaceAvailable(void);
LwU8 smbpbiInforomRead(LwU32 offset, LwU32 size, LwU8 *pDst);
LwU8 smbpbiInforomWrite(LwU32 offset, LwU32 size, LwU8 *pSrc);
LwU8 smbpbiMapSoeStatusToMsgBoxStatus(FLCN_STATUS status);
LwU8 smbpbiOsfpGetHottest(LwU32 *pCmd, LwU32 *pData, LwU32 *pExtData);
LwU8 smbpbiOsfpGetLedState(LwU32 *pData, LwU32 *pExtData);
LwU8 smbpbiOsfpGetXceiverTemperature(LwU8 xceiverIdx, LwU32 *pData);
LwU8 smbpbiOsfpSetLedLocate(LwU8 xceiverIdx);
LwU8 smbpbiOsfpResetLedLocate(LwU8 xceiverIdx);
LwU8 smbpbiOsfpGetInfo(LwU8 xceiverIdx, LwU8 infoType, LwU8 offset, LwU32 *pData, LwU32 *pExtData);
void smbpbiOsfpSetCaps(LwU32 *pCaps);

FLCN_STATUS constructSmbpbi(void)  GCC_ATTRIB_SECTION("imem_init", "constructSmbpbi");
void smbpbiPreInit(void)           GCC_ATTRIB_SECTION("imem_init", "smbpbiPreInit");
void smbpbiInit(void)              GCC_ATTRIB_SECTION("imem_init", "smbpbiInit");

#endif // _SOE_OBJSMBPBI_H_
