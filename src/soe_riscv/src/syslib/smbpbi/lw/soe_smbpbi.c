/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_smbpbi.c
 * @brief  Common part of the SOE SMBPBI server implementation
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "lwrtos.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsmbpbi.h"
#include "soe_objsoe.h"
#include "soe_objtherm.h"
#include "soeifcmn.h"
#include "soe_smbpbi_async.h"
#include "smbpbi_fabric_state.h"
#include "soe_objbif.h"
#include "tasks.h"
#include "cmdmgmt.h"

#include <lwriscv/print.h>
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Private Functions ------------------------------ */
sysSYSLIB_CODE static void _smbpbiSetStatus(OBJSMBPBI *pSmbpbi, LwU32 cmd, LwU32 data, LwU32 extData);
sysSYSLIB_CODE static LwU8 _smbpbiGetTemp(LwU8 sensor, LwU32 *pTemp, LwBool  bExtended);
sysSYSLIB_CODE static LwU8 _smbpbiGetThermParam(LwU8 paramId, LwU32 *pData);
sysSYSLIB_CODE static void _smbpbiPrepareDataCopy(LwU32 *pCmd, LwU32 data, LwU32 extData);
sysSYSLIB_CODE static LwU8 _smbpbiGetLwlinkInfoHandler(LwU8 arg1, LwU8 arg2, LwU32 *pCmd,
                                        LwU32 *pData, LwU32 *pExtData);
sysSYSLIB_CODE static LwU8 _smbpbiGetPcieLinkInfoHandler(LwU8 arg1, LwU8 arg2, LwU32 *pData, LwU32 *pExtData);
sysSYSLIB_CODE static LwU8 _smbpbiGetMiscDeviceFlagsHandler(LwU8 arg1, LwU8 arg2, LwU32 *pData, LwU32 *pExtData);
sysSYSLIB_CODE static LwU8 _smbpbiGetDeviceDisableState(LwU32 *pData);
sysSYSLIB_CODE static void _smbpbiInforomInitCapDword(void);
sysSYSLIB_CODE static LwU8 _smbpbiGetSysId(LwU32 attr, LwU16 offset, LwU32 *pData);
sysSYSLIB_CODE static FLCN_STATUS _smbpbiSetupSharedSurface(RM_FLCN_U64 dmaHandle);
sysSYSLIB_CODE static void _smbpbiSetupDriverStateCallback(LwU32 driverPollingPeriodUs);
sysSYSLIB_CODE static FLCN_STATUS _smbpbiProcessInitCmd(RM_SOE_SMBPBI_CMD_INIT *pCmd);
sysSYSLIB_CODE static FLCN_STATUS _smbpbiProcessUnloadCmd(RM_SOE_SMBPBI_CMD_UNLOAD *pCmd);
sysSYSLIB_CODE static FLCN_STATUS _smbpbiProcessSetLinkErrorInfoCmd(RM_SOE_SMBPBI_CMD_SET_LINK_ERROR_INFO *pCmd);
sysSYSLIB_CODE static LwU8 s_smbpbiRegWrClient(LwU8 reg, LwU32 data);
sysSYSLIB_CODE static LwU8 s_smbpbiEventSet(LwMsgboxEventType event);
sysSYSLIB_CODE static LwU8 _smbpbiGetWriteProtectionMode(LwU8 arg1, LwU32 *pData);

/* ------------------------- Static Variables ------------------------------- */
static sysTASK_DATA OS_TMR_CALLBACK smbpbiTaskOsTmrCb;
sysSYSLIB_CODE static OsTmrCallbackFunc (_smbpbiMonitorDriverState);
/* ------------------------- Global Variables ------------------------------- */
sysTASK_DATA RM_SOE_SMBPBI_INFOROM_DATA *pInforomData = NULL;


// OS_TIMER            SmbpbiOsTimer;




/*!
 * Static system ID (infoROM) data type attributes to drive the exelwtion
 * of LW_MSGBOX_CMD_OPCODE_GET_SYS_ID_DATA msgbox request
 */

#define LW_SMBPBI_SYS_ID_SRCTYPE                                            1:0
#define LW_SMBPBI_SYS_ID_SRCTYPE_DMEM                                       0
#define LW_SMBPBI_SYS_ID_SRCTYPE_IFR_U8                                     1
#define LW_SMBPBI_SYS_ID_SRCTYPE_IFR_U32                                    2
#define LW_SMBPBI_SYS_ID_SRCTYPE_IFR_STRING                                 3
#define LW_SMBPBI_SYS_ID_OFFSET                                             12:2
#define LW_SMBPBI_SYS_ID_SIZE                                               21:13
#define LW_SMBPBI_SYS_ID_CAPBIT                                             26:22
#define LW_SMBPBI_SYS_ID_CAPINDEX                                           28:27

/**
 * The static info like OBD and OEM is cached in RM_SOE_SMBPBI_INFOROM_DATA and now
 * the attrs use 11 bits to store the offsets of the member in the structure.
 * The assertion is to check the offsets does not exceed 11 bits. Therefore,
 * future elements to be added should be placed closed to the head of the
 * structure to avoid exceeding.
 */
#define LW_ASSERT_EXPR(expr)                              (!sizeof(char[((expr)? 1 : -1)]))
#define LW_ASSERT_DMEM_OFFSET(name)                                                     \
    LW_ASSERT_EXPR(LW_OFFSETOF(SOE_SMBPBI_SYSID_DATA, name) <= 0x7ff)
#define LW_ASSERT_INFOROM_OFFSET(name)                                                  \
    LW_ASSERT_EXPR(LW_OFFSETOF(RM_SOE_SMBPBI_INFOROM_DATA, name) <= 0x7ff)

#define SMBPBI_SYS_ID_ATTR_INIT_DMEM(name,index,cap)                                    \
    (DRF_DEF(_SMBPBI, _SYS_ID, _SRCTYPE, _DMEM) |                                       \
    LW_ASSERT_DMEM_OFFSET(name) |                                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _OFFSET,                                                  \
            LW_OFFSETOF(SOE_SMBPBI_SYSID_DATA, name)) |                                 \
    DRF_NUM(_SMBPBI, _SYS_ID, _SIZE, sizeof Smbpbi.config.sysIdData.name) |             \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap)))

#define SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U8(obj,name,index,cap)                          \
    (DRF_DEF(_SMBPBI, _SYS_ID, _SRCTYPE, _IFR_U8) |                                     \
    LW_ASSERT_INFOROM_OFFSET(obj.name) |                                                \
    DRF_NUM(_SMBPBI, _SYS_ID, _OFFSET,                                                  \
            LW_OFFSETOF(RM_SOE_SMBPBI_INFOROM_DATA, obj.name)) |                        \
    DRF_NUM(_SMBPBI, _SYS_ID, _SIZE, sizeof(LwU8)) |                                    \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap)))

#define SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U32(obj,name,index,cap)                         \
    (DRF_DEF(_SMBPBI, _SYS_ID, _SRCTYPE, _IFR_U32) |                                    \
    LW_ASSERT_INFOROM_OFFSET(obj.name) |                                                \
    DRF_NUM(_SMBPBI, _SYS_ID, _OFFSET,                                                  \
            LW_OFFSETOF(RM_SOE_SMBPBI_INFOROM_DATA, obj.name)) |                        \
    DRF_NUM(_SMBPBI, _SYS_ID, _SIZE, sizeof(LwU32)) |                                   \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap)))

#define SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING(obj,name,index,cap)                      \
    (DRF_DEF(_SMBPBI, _SYS_ID, _SRCTYPE, _IFR_STRING) |                                 \
    LW_ASSERT_INFOROM_OFFSET(obj.name) |                                                \
    DRF_NUM(_SMBPBI, _SYS_ID, _OFFSET,                                                  \
            LW_OFFSETOF(RM_SOE_SMBPBI_INFOROM_DATA, obj.name)) |                        \
    DRF_NUM(_SMBPBI, _SYS_ID, _SIZE,                                                    \
            LW_ARRAY_ELEMENTS(((PRM_SOE_SMBPBI_INFOROM_DATA)0)->obj.name)) |            \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap)))

#define SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(index,cap)                             \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPINDEX, index) |                                       \
    DRF_NUM(_SMBPBI, _SYS_ID, _CAPBIT, DRF_BASE(LW_MSGBOX_DATA_CAP_##index##cap))

sysTASK_DATA LwU32   SysIdDataTypeAttr[LW_MSGBOX_CMD_ARG1_SYS_ID_DATA_TYPE_MAX + 1] =
{
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(1, _BOARD_PART_NUM_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(1, _OEM_INFO_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(1, _SERIAL_NUM_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING(OBD, marketingName, 1, _MARKETING_NAME_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(1, _GPU_PART_NUM_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(1, _MEMORY_VENDOR_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(1, _MEMORY_PART_NUM_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(1, _BUILD_DATE_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(firmwareVer, 1, _FIRMWARE_VER_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(vendorId, 1, _VENDOR_ID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(devId, 1, _DEV_ID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(subVendorId, 1, _SUB_VENDOR_ID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(subId, 1, _SUB_ID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_DMEM(guid, 1, _GPU_GUID_V1),
    SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING(IMG, inforomVer, 1, _INFOROM_VER_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PRODUCT_LENGTH_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PRODUCT_WIDTH_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PRODUCT_HEIGHT_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PCIE_SPEED_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _PCIE_WIDTH_V1),
    SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED(2, _TGP_LIMIT_V1),
};

#undef SMBPBI_SYS_ID_ATTR_INIT_DMEM
#undef SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U8
#undef SMBPBI_SYS_ID_ATTR_INIT_INFOROM_U32
#undef SMBPBI_SYS_ID_ATTR_INIT_INFOROM_STRING
#undef SMBPBI_SYS_ID_ATTR_INIT_UNSUPPORTED

/* ------------------------- Prototypes ------------------------------------- */
sysSYSLIB_CODE FLCN_STATUS      smbpbiDispatch(DISP2UNIT_CMD *pDispSmbpbi);

/* ------------------------- Constants -------------------------------------- */
sysTASK_DATA static const LwU32 smbpbiEventsEdgeTriggered =
                BIT(LW_MSGBOX_EVENT_TYPE_SERVER_RESTART);

/*!
 * Processes and acknowledges all SMBPBI-related commands sent to the SOE
 * (either externally or internally).
 *
 * @param[in] pCmd  Pointer to the DISP2UNIT_CMD command to process
 */
sysSYSLIB_CODE FLCN_STATUS
smbpbiDispatch
(
    DISP2UNIT_CMD *pDispSmbpbi
)
{
    RM_FLCN_CMD_SOE  *pCmd = pDispSmbpbi->pCmd;
    RM_FLCN_QUEUE_HDR hdr;
    LwU32             cmd;
    LwU32             data;
    LwU32             extData;
    LwU8              status = FLCN_ERR_QUEUE_TASK_ILWALID_CMD_TYPE;

    switch (pDispSmbpbi->eventType)
    {
        case DISP2UNIT_EVT_COMMAND:
        {
            if (RM_SOE_SMBPBI_CMD_ID_INIT == pCmd->cmd.smbpbiCmd.cmdType)
            {
                status = _smbpbiProcessInitCmd(&pCmd->cmd.smbpbiCmd.init);
            }
            else if (RM_SOE_SMBPBI_CMD_ID_UNLOAD == pCmd->cmd.smbpbiCmd.cmdType)
            {
                status = _smbpbiProcessUnloadCmd(&pCmd->cmd.smbpbiCmd.unload);
            }
            else if (RM_SOE_SMBPBI_CMD_ID_SET_LINK_ERROR_INFO ==
                     pCmd->cmd.smbpbiCmd.cmdType)
            {
                status = _smbpbiProcessSetLinkErrorInfoCmd(&pCmd->cmd.smbpbiCmd.linkErrorInfo);
            }
            if (status == FLCN_OK)
            {
                cmdQueueSweep(&pCmd->hdr, pDispSmbpbi->cmdQueueId);
                hdr.unitId  = RM_SOE_UNIT_NULL;
                hdr.ctrlFlags = 0;
                hdr.seqNumId = pCmd->hdr.seqNumId;
                hdr.size = RM_FLCN_QUEUE_HDR_SIZE;
                if (!msgQueuePostBlocking(&hdr, NULL))
                {
                    dbgPrintf(LEVEL_ALWAYS, "smbpbiDispatch: Failed sending response back to driver\n");
                }
            }
            break;
        }
        case SMBPBI_EVT_MSGBOX_IRQ:
        {
            // Retrieve pending command and optional input data.
            smbpbiExelwte_HAL(&Smbpbi, &cmd, &data, &extData,
                              SOE_SMBPBI_GET_REQUEST);

            if (cmd == pCmd->cmd.smbpbiCmd.msgbox.msgboxCmd)
            {
                static LwBool   bDoOnce = LW_TRUE;

                // Two conselwtive reads are identical -> process cmd.
                if (bDoOnce)
                {
                    bDoOnce = LW_FALSE;
                    // Instruct the client to re-read the caps
                    status = LW_MSGBOX_CMD_STATUS_READY;
                }
                else
                {

                    //
                    // Special case the Bundle Launch request
                    // in order to avoid relwrsion.
                    //
                    if (FLD_TEST_DRF(_MSGBOX, _CMD, _ENCODING, _STANDARD, cmd) &&
                        FLD_TEST_DRF(_MSGBOX, _CMD, _OPCODE, _BUNDLE_LAUNCH, cmd))
                    {
                        status = smbpbiBundleLaunch(&cmd, &data, &extData);
                    }
                    else
                    {
                        status = smbpbiHandleMsgBoxRequest(&cmd, &data, &extData);
                    }
                }
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_REQUEST;
            }

            // Update status and send out response
            cmd = FLD_SET_DRF_NUM(_MSGBOX,_CMD,_STATUS, status, cmd);

            // Copy data[23:0] into cmd[23:0] if requested and status is good
            if (LW_MSGBOX_CMD_IS_COPY_DATA_SET(cmd) &&
               (status == LW_MSGBOX_CMD_STATUS_SUCCESS))
            {
                _smbpbiPrepareDataCopy(&cmd, data, extData);
            }

            _smbpbiSetStatus(&Smbpbi, cmd, data, extData);

            status = FLCN_OK;
            break;
        }
    }

    return status;
}

/* !
 * API to read arbitrary offset from the shared surface
 *
 * @params[in] pDmemAddr    Pointer to where to store the data which was read
 * @params[in] offset       Offset into the shared surface to start reading
 * @params[in] size         Size in bytes to read starting from offset
 */
sysSYSLIB_CODE FLCN_STATUS
smbpbiReadSharedSurface
(
    void *pDmemAddr,
    LwU32 offset,
    LwU32 size
)
{
    if (pDmemAddr == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Fail if the requested offset is outside of the size of the shared surface
    if (offset >= sizeof(SOE_SMBPBI_SHARED_SURFACE))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Fail if the requested size goes beyond the shared surface
    if (size > (sizeof(SOE_SMBPBI_SHARED_SURFACE) - offset))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (!(Smbpbi.config.bDmaAvailable))
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    return soeDmaReadFromSysmem_HAL(&Soe, pDmemAddr, Smbpbi.config.dmaHandle,
                                    offset, size);
}

/*!
 * API to report if SMBPBI shared surface can be used
 */
sysSYSLIB_CODE LwBool
smbpbiIsSharedSurfaceAvailable(void)
{
    return Smbpbi.config.bDmaAvailable;
}

/*!
 * Processes SMBPBI Unload Command.
 *
 * @param[in] pCmd  Pointer to the RM_SOE_SMBPBI_CMD_UNLOAD command to process
 */
sysSYSLIB_CODE FLCN_STATUS
_smbpbiProcessUnloadCmd
(
    RM_SOE_SMBPBI_CMD_UNLOAD   *pCmd
)
{
    FLCN_STATUS status = FLCN_OK;

    // Check to see initial _smbpbiProcessInitCmd completed successfully
    if (pInforomData == NULL)
    {
        return FLCN_OK;
    }

    Smbpbi.config.bDmaAvailable = LW_FALSE;

    status = soeDmaReadFromSysmem_HAL(&Soe,
                                      pInforomData,
                                      Smbpbi.config.dmaHandle,
                                      LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE, inforomObjects),
                                      sizeof(RM_SOE_SMBPBI_INFOROM_DATA));
    if (status != FLCN_OK)
    {
        return status;
    }

    return status;
}

/*!
 * Sets up the shared surface for SMBPBI
 *
 * @param[in] dmaHandle  Handle to the DMA address
 */
sysSYSLIB_CODE FLCN_STATUS
_smbpbiSetupSharedSurface
(
    RM_FLCN_U64 dmaHandle
)
{
    FLCN_STATUS status = FLCN_OK;

    pInforomData = lwosCallocAligned(0, sizeof(RM_SOE_SMBPBI_INFOROM_DATA),
                                     SOE_DMA_MAX_SIZE);
    if (!pInforomData)
    {
        SOE_HALT();
        return FLCN_ERR_NO_FREE_MEM;
    }

    Smbpbi.config.bDmaAvailable = LW_TRUE;
    Smbpbi.config.dmaHandle     = dmaHandle;

    status = soeDmaReadFromSysmem_HAL(&Soe,
                                      pInforomData,
                                      dmaHandle,
                                      LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE, inforomObjects),
                                      sizeof(RM_SOE_SMBPBI_INFOROM_DATA));
    return status;
}

/*!
 * @ref OsTimerCallback
 *
 * OS_TIMER callback that monitors the driver's timeout status
 *
 */
sysSYSLIB_CODE static FLCN_STATUS
_smbpbiMonitorDriverState
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwU32 seqNum;

    lwrtosTaskCriticalEnter();

    smbpbiGetDriverSeqNum_HAL(Smbpbi, &seqNum);

    Smbpbi.bDriverTimeout = (Smbpbi.driverStateSeqNum == seqNum) ?
                                LW_TRUE :
                                LW_FALSE;

    Smbpbi.driverStateSeqNum = seqNum;

    lwrtosTaskCriticalExit();

    return FLCN_OK;
}

/*!
 * Schedules the callback to monitor driver timeout status
 */
sysSYSLIB_CODE void
_smbpbiSetupDriverStateCallback
(
    LwU32 driverPollingPeriodUs
)
{
    // Initialize smbpbi task callback
    memset(&smbpbiTaskOsTmrCb, 0, sizeof(OS_TMR_CALLBACK));

    osTmrCallbackCreate(&smbpbiTaskOsTmrCb,                             // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                0,                                                      // ovlImem
                _smbpbiMonitorDriverState,                              // pTmrCallbackFunc
                Disp2QSmbpbiThd,                                        // queueHandle
                driverPollingPeriodUs,                                  // periodNormalus
                driverPollingPeriodUs,                                  // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                RM_SOE_UNIT_SMBPBI);                                    // taskId

    // Schedule callback to monitor Driver timeout state
    osTmrCallbackSchedule(&smbpbiTaskOsTmrCb);
}

/*!
 * Processes SMBPBI Init Command.
 *
 * @param[in] pCmd  Pointer to the RM_SOE_SMBPBI_CMD_INIT command to process
 */
sysSYSLIB_CODE FLCN_STATUS
_smbpbiProcessInitCmd
(
    RM_SOE_SMBPBI_CMD_INIT *pCmd
)
{
    LwU32       cmd;
    FLCN_STATUS status;

    status = _smbpbiSetupSharedSurface(pCmd->dmaHandle);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Initialize message box capability dword from InfoROM shared surface.
    _smbpbiInforomInitCapDword();

    smbpbiOsfpSetCaps(Smbpbi.config.cap);

    _smbpbiSetupDriverStateCallback(pCmd->driverPollingPeriodUs);

    Smbpbi.bDriverReloadRequired = LW_FALSE;

    cmd = DRF_DEF(_MSGBOX, _CMD, _STATUS, _READY);

    (void) s_smbpbiEventSet(LW_MSGBOX_EVENT_TYPE_SERVER_RESTART);

    // Unmask all events
    (void) smbpbiRegWr(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENT_MASK, 0);

    smbpbiInitDem(&Smbpbi);

    smbpbiExelwte_HAL(&Smbpbi, &cmd, NULL, NULL, SOE_SMBPBI_SET_RESPONSE);

    return FLCN_OK;
}

/*
 * Processes SMBPBI Set Link Error Info Command
 *
 * @param[in] pCmd  Pointer to the RM_SOE_SMBPBI_CMD_SET_LINK_ERROR_INFO command
 *                  to process
 */
sysSYSLIB_CODE static FLCN_STATUS
_smbpbiProcessSetLinkErrorInfoCmd
(
    RM_SOE_SMBPBI_CMD_SET_LINK_ERROR_INFO *pCmd
)
{
    lwrtosTaskCriticalEnter();

    if (pCmd->trainingErrorInfo.isValid)
    {
        Smbpbi.linkTrainingErrorInfo.attemptedTrainingMask0 =
            pCmd->trainingErrorInfo.attemptedTrainingMask0;
        Smbpbi.linkTrainingErrorInfo.trainingErrorMask0 =
            pCmd->trainingErrorInfo.trainingErrorMask0;
    }

    //
    // OR the runtime mask to keep all errors since boot.
    //
    if (pCmd->runtimeErrorInfo.isValid)
    {
        Smbpbi.linkRuntimeErrorInfo.mask0.lo |= pCmd->runtimeErrorInfo.mask0.lo;
        Smbpbi.linkRuntimeErrorInfo.mask0.hi |= pCmd->runtimeErrorInfo.mask0.hi;
    }

    lwrtosTaskCriticalExit();

    return FLCN_OK;
}

/*!
 * Complete the SMBPBI request processing by posting status and
 * returning data.
 *
 * @param[in]   pSmbpbi     Pointer to the OBJSMBPBI object
 * @param[in]   cmd         command/status register
 * @param[in]   data        data register
 * @param[in]   extData     ext-data register
 */
sysSYSLIB_CODE static void
_smbpbiSetStatus
(
    OBJSMBPBI  *pSmbpbi,
    LwU32       cmd,
    LwU32       data,
    LwU32       extData
)
{
    lwrtosENTER_CRITICAL();
    {
        if (pSmbpbi->requestCnt == 0)
        {
            // No request is in flight. Internal error.
            SOE_HALT();
        }

        if (pSmbpbi->requestCnt != 1)
        {
            //
            // More than one request was submitted.
            // Protocol violation by the client.
            //
            cmd = FLD_SET_DRF(_MSGBOX, _CMD, _STATUS, _ERR_REQUEST, cmd);
        }

        pSmbpbi->requestCnt = 0;
        smbpbiExelwte_HAL(pSmbpbi, &cmd, &data, &extData,
                          SOE_SMBPBI_SET_RESPONSE);
    }
    lwrtosEXIT_CRITICAL();
}

/*!
 * @brief This function process MSGBOX request
 *
 * @param[in]       cmd         requested command
 * @param[in,out]   pData       input/output data buffer
 * @param[in,out]   pExtData    input/output ext-data buffer
 *
 * @return      MSGBOX status
 */
sysSYSLIB_CODE LwU8
smbpbiHandleMsgBoxRequest
(
    LwU32  *pCmd,
    LwU32  *pData,
    LwU32  *pExtData
)
{
    LwU8            opcode;
    LwU8            arg1;
    LwU8            arg2;
    LwU8            type;
    LwU8            status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
    LwU32           cmd = *pCmd;

//    DBG_PRINTF_SMBPBI(("MBs: c: %08x, d: %08x\n", cmd, *pData, 0, 0));

    switch (DRF_VAL(_MSGBOX, _CMD, _ENCODING, cmd))
    {
        case LW_MSGBOX_CMD_ENCODING_STANDARD:
        {
            // standard SMBPBI request
            break;
        }

//TODO(my): add case LW_MSGBOX_CMD_ENCODING_DEBUG: in order to handle debug requests

        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_OPCODE;
            goto done;
        }
    }

    opcode = DRF_VAL(_MSGBOX, _CMD, _OPCODE, cmd);
    arg1 = DRF_VAL(_MSGBOX, _CMD, _ARG1, cmd);
    arg2 = DRF_VAL(_MSGBOX, _CMD, _ARG2, cmd);

    switch (opcode)
    {
        case LW_MSGBOX_CMD_OPCODE_NULL_CMD:
        {
            // NULL command always succeeds (for testing SMBPBI).
            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_GET_CAP_DWORD:
        {
            if (arg1 < Smbpbi.config.capCount)
            {
                *pData = Smbpbi.config.cap[arg1];
            }
            else
            {
                *pData = 0;
            }
            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_GET_TEMP:
        case LW_MSGBOX_CMD_OPCODE_GET_EXT_TEMP:
        {
            status = _smbpbiGetTemp(arg1, pData,
                        opcode == LW_MSGBOX_CMD_OPCODE_GET_EXT_TEMP);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_GET_THERM_PARAM:
        {
            status = _smbpbiGetThermParam(arg1, pData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_GET_ECC_V6:
        {
            status = smbpbiGetEccV6_HAL(&Smbpbi, arg1, arg2, pData, pExtData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_GET_LWLINK_INFO:
        {
            status = _smbpbiGetLwlinkInfoHandler(arg1, arg2, pCmd, pData, pExtData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_GET_PCIE_LINK_INFO:
        {
            status = _smbpbiGetPcieLinkInfoHandler(arg1, arg2, pData, pExtData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_SCRATCH_READ:
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_WRITE:
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_COPY:
        {
            status = smbpbiHandleScratchRegAccess(cmd, pData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_REGISTER_ACCESS:
        {
            switch (arg1)
            {
                case LW_MSGBOX_CMD_ARG1_REGISTER_ACCESS_WRITE:
                {
                    status = s_smbpbiRegWrClient(arg2, *pData);
                    break;
                }
                case LW_MSGBOX_CMD_ARG1_REGISTER_ACCESS_READ:
                {
                    status = smbpbiRegRd(arg2, pData);
                    break;
                }
                default:
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                    break;
                }
            }
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_ASYNC_REQUEST:
        {
            status = smbpbiHandleAsyncRequest(cmd, pData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_GET_MISC_DEVICE_FLAGS:
        {
            status = _smbpbiGetMiscDeviceFlagsHandler(arg1, arg2, pData, pExtData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_GET_SYS_ID_DATA:
        {
            type = arg1;

            if (type > LW_MSGBOX_CMD_ARG1_SYS_ID_DATA_TYPE_MAX)
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                break;
            }

            status = _smbpbiGetSysId(SysIdDataTypeAttr[type],
                                    arg2 * sizeof(*pData), pData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_BUNDLE_LAUNCH:
        {
            //
            // Disallow launching a bundle from inside a bundle,
            // as that is the only way to hit this case.
            //
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;

            break;
        }
        case LW_MSGBOX_CMD_OPCODE_GET_DRIVER_EVENT_MSG:
        {
            status = smbpbiGetDem(arg1, arg2);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_ACCESS_WP_MODE:
        {
            status = _smbpbiGetWriteProtectionMode(arg1, pData);
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_OPTICAL_XCEIVER_CTL:
        {
            if (smbpbiIsSharedSurfaceAvailable())
            {
                switch (arg1)
                {
                    case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_CTL_GET_HOTTEST:
                    {
                        status = smbpbiOsfpGetHottest(pCmd, pData, pExtData);
                        break;
                    }

                    case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_CTL_GET_LED_STATE:
                    {
                        status = smbpbiOsfpGetLedState(pData, pExtData);
                        break;
                    }

                    case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_CTL_GET_TEMPERATURE:
                    {
                        status = smbpbiOsfpGetXceiverTemperature(arg2, pData);
                        break;
                    }

                    case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_CTL_SET_LED_LOCATE:
                    {
                        status = smbpbiOsfpSetLedLocate(arg2);
                        break;
                    }

                    case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_CTL_RESET_LED_LOCATE:
                    {
                        status = smbpbiOsfpResetLedLocate(arg2);
                        break;
                    }

                    default:
                    {
                        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                        break;
                    }
                }
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
            }
            break;
        }

        case LW_MSGBOX_CMD_OPCODE_OPTICAL_XCEIVER_INFO:
        {
            if (smbpbiIsSharedSurfaceAvailable())
            {
                status = smbpbiOsfpGetInfo(
                        DRF_VAL(_MSGBOX, _CMD, _ARG1_OPTICAL_XCEIVER_INFO_XCEIVER_IDX, cmd),
                        DRF_VAL(_MSGBOX, _CMD, _ARG1_OPTICAL_XCEIVER_INFO_TYPE, cmd),
                        arg2,
                        pData,
                        pExtData);
            }
            else
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
            }
            break;
        }

        default:
        {
            // Unknown command
            status = LW_MSGBOX_CMD_STATUS_ERR_OPCODE;
            break;
        }
    }

done:
//    DBG_PRINTF_SMBPBI(("MBe: c: %08x, d: %08x, e: %02x\n", cmd, *pData, status, 0));
    return status;
}

/*!
 * Colwert FLCN_STATUS into MSGBOX status.
 *
 * @param[in]  status     SOE status code
 *
 * @return corresponding MSGBOX status value
 */
sysSYSLIB_CODE LwU8
smbpbiMapSoeStatusToMsgBoxStatus
(
    FLCN_STATUS status
)
{
    switch (status)
    {
        case FLCN_OK:
        {
            return LW_MSGBOX_CMD_STATUS_SUCCESS;
        }
        case FLCN_ERR_I2C_BUSY:
        case FLCN_ERR_I2C_NACK_ADDRESS:
        case FLCN_ERR_I2C_NACK_BYTE:
        case FLCN_ERR_I2C_SIZE:
        case FLCN_ERR_I2C_BUS_ILWALID:
        {
            return LW_MSGBOX_CMD_STATUS_ERR_I2C_ACCESS;
        }
        case FLCN_ERR_NOT_SUPPORTED:
        {
            return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        }
        default:
        {
            break;
        }
    }
    return LW_MSGBOX_CMD_STATUS_ERR_MISC;
}

/*!
 * @brief Internal state registers' logic that is in addition
 * to simple storage
 *
 * @param[in] reg
 *      register ID (index)
 * @param[in] data
 *      the value to be written
 *
 * @return whatever inferiors return
 */
sysSYSLIB_CODE static LwU8
s_smbpbiRegWrClient
(
    LwU8    reg,
    LwU32   data
)
{
    LwU32   pending;
    LwU8    status;

    switch (reg)
    {
        case LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING:
        {
            status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING, &pending);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                return status;
            }

            pending &= data | ~smbpbiEventsEdgeTriggered;
            data = pending;
            break;
        }

        case LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENT_MASK:
        {
            data &= LW_MSGBOX_EVENT_TYPE__ALL;
            break;
        }
    }

    return smbpbiRegWr(reg, data);
}

sysSYSLIB_CODE static LwU8
s_smbpbiEventSet
(
    LwMsgboxEventType   event
)
{
    LwU32   pending;
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if ((status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING, &pending))
                                    != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        OS_BREAKPOINT();
        goto s_smbpbiEventSet_exit;
    }

    if ((pending & BIT(event)) != 0)
    {
        goto s_smbpbiEventSet_exit;
    }

    pending |= BIT(event);

    if ((status = smbpbiRegWr(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING,
                    pending)) != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        OS_BREAKPOINT();
        goto s_smbpbiEventSet_exit;
    }

s_smbpbiEventSet_exit:
    return status;
}

/*!
 * @brief Query event sources and fill the event mask
 *
 * @param[in/out]  pStatus  STATUS register value
 *
 * @return status percolated from inferiors
 */
sysSYSLIB_CODE LwU8
smbpbiCheckEvents
(
    LwU32   *pStatus
)
{
    unsigned    event;
    LwU32       pending;
    LwU32       mask;
    LwU32       pendingNew;
    unsigned    eventBit;
    LwU8        status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (FLD_TEST_DRF(_MSGBOX, _CMD, _STATUS, _INACTIVE, *pStatus) ||
        !LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _NUM_SCRATCH_BANKS))
    {
        //
        // Don't scan/post events unless the server is open for business
        // or the internal state registers are unavailable.
        //
        goto smbpbiCheckEvents_exit;
    }

    status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING, &pending);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        OS_BREAKPOINT();
        goto smbpbiCheckEvents_exit;
    }

    status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENT_MASK, &mask);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        OS_BREAKPOINT();
        goto smbpbiCheckEvents_exit;
    }

    pendingNew = pending & smbpbiEventsEdgeTriggered;

    for (event = 0; event < (unsigned)LW_MSGBOX_NUM_EVENTS; ++event)
    {
        if ((BIT(event) & smbpbiEventsEdgeTriggered) == 0)
        {
            // Level triggered
            LwBool  bPending;

            switch (event)
            {
                case LW_MSGBOX_EVENT_TYPE_DRIVER_ERROR_MESSAGE:
                {
                    bPending = smbpbiDemEventSource();
                    break;
                }
                default:
                {
                    bPending = LW_FALSE;
                    break;
                }
            }
            if (bPending)
            {
                pendingNew |= BIT(event);
            }
        }
    }

    if (pendingNew != pending)
    {
        if ((status = smbpbiRegWr(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING,
                        pendingNew)) != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            OS_BREAKPOINT();
            goto smbpbiCheckEvents_exit;
        }
    }

    eventBit = (pendingNew & ~mask) != 0;
    *pStatus = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _EVENT_PENDING, eventBit, *pStatus);

smbpbiCheckEvents_exit:
    return status;
}

/*!
 * @brief Handles DMA read from SMBPBI host side buffers
 *
 * @param[in]   offset
 *      byte offset into the surface
 * @param[in]   size
 *      transaction size
 * @param[in, out] pDst
 *      destination buffer pointer
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      read successfully
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *      DMA malfunction
 */
sysSYSLIB_CODE LwU8
smbpbiHostMemRead(LwU32 offset, LwU32 size, LwU8 *pDst)
{
    LwU8       dmaBufSpace[SMBPBI_IFR_XFER_SIZE + DMA_MIN_READ_ALIGNMENT - 1];
    LwU8       *dmaBuf = SOE_ALIGN_UP_PTR(dmaBufSpace, DMA_MIN_READ_ALIGNMENT);
    LwU8       retVal  = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU8       lwrSz;
    LwU8       off;
    LwU32      base;
    LwU32      limit;

    limit = sizeof(SOE_SMBPBI_SHARED_SURFACE);
    if ((offset > limit) || (size > limit) || ((offset + size) > limit))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_MISC;
    }

    // Its recommended to use DMA section when task is doing back to back DMA.
    lwosDmaLockAcquire();
    {
        off  = offset % DMA_MIN_READ_ALIGNMENT;
        base = offset - off;

        while (size > 0)
        {
            lwrSz = LW_MIN(size, (LwU32)(SMBPBI_IFR_XFER_SIZE - off));
            if (FLCN_OK == soeDmaReadFromSysmem_HAL(&Soe, dmaBuf,
                                        Smbpbi.config.dmaHandle,
                                        base, SMBPBI_IFR_XFER_SIZE))

            {
                memcpy(pDst, dmaBuf + off, lwrSz);
                pDst += lwrSz;
                base += SMBPBI_IFR_XFER_SIZE;
                size -= lwrSz;
                off = 0;
            }
            else
            {
                retVal = LW_MSGBOX_CMD_STATUS_ERR_MISC;
                break;
            }
        }
    }
    lwosDmaLockRelease();

    return retVal;
}

/*!
 * @brief DMA Inforom data from the shared surface,
 * if DMA is available. Otherwise read from the local copy.
 *
 * @param[in]   offset
 *      byte offset into the RM_SOE_SMBPBI_INFOROM_DATA struct
 * @param[in]   size
 *      transaction size
 * @param[in, out] pDst
 *      destination buffer pointer
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      read successfully
 *
 * @return others
 *      Whatever percolates from the inferiors.
 */
sysSYSLIB_CODE LwU8
smbpbiInforomRead
(
    LwU32   offset,
    LwU32   size,
    LwU8    *pDst
)
{
    LwU8    status;

    if (smbpbiIsSharedSurfaceAvailable())
    {
        status = smbpbiHostMemRead(
                 LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE, inforomObjects) + offset,
                 size,
                 pDst);
    }
    else
    {
        memcpy(pDst, (char *)pInforomData + offset, size);
        status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    }

    return status;
}

/*!
 * @brief DMA Inforom data into the shared surface,
 * if DMA is available. Otherwise write to the local copy.
 *
 * @param[in]   offset
 *      byte offset into the RM_SOE_SMBPBI_INFOROM_DATA struct
 * @param[in]   size
 *      transaction size
 * @param[in, out] pSrc
 *      source buffer pointer
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      read successfully
 *
 * @return others
 *      Whatever percolates from the inferiors.
 */
sysSYSLIB_CODE LwU8
smbpbiInforomWrite
(
    LwU32   offset,
    LwU32   size,
    LwU8    *pSrc
)
{
    LwU8        status;
    FLCN_STATUS flcnStatus;

    if (smbpbiIsSharedSurfaceAvailable())
    {
        flcnStatus = soeDmaWriteToSysmem_HAL(&Soe, pSrc,
                        Smbpbi.config.dmaHandle,
                        LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE, inforomObjects) + offset,
                        size);
        status = smbpbiMapSoeStatusToMsgBoxStatus(flcnStatus);
    }
    else
    {
        memcpy((char *)pInforomData + offset, pSrc, size);
        status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    }

    return status;
}

sysSYSLIB_CODE static LwU8
_smbpbiGetTemp
(
    LwU8    sensor,
    LwU32   *pTemp,
    LwBool  bExtended
)
{
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwTemp  temp;
    switch (sensor)
    {
        case LW_MSGBOX_CMD_ARG1_TEMP_LWSWITCH_0:
        {
            temp = thermGetTsenseMax_HAL();
            break;
        }

        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status == LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        if (!bExtended)
        {
            temp &= ~0xff;
        }

        *pTemp = temp;
    }

    return status;
}

sysSYSLIB_CODE static LwU8
_smbpbiGetThermParam
(
    LwU8   paramId,
    LwU32 *pData
)
{
    LwU8        status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    FLCN_STATUS soeStatus;
    LwTemp      temp;

    if (paramId < DRF_SIZE(LW_MSGBOX_DATA_CAP_0_THERMP_BITS))
    {
        if ((BIT(paramId) & DRF_VAL(_MSGBOX_DATA_CAP_0, _THERMP, _BITS,
                                   Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0])) != 0)
        {
            switch (paramId)
            {
                case LW_MSGBOX_CMD_ARG1_THERM_PARAM_TEMP_SHUTDN:
                {
                    soeStatus = thermGetTsenseThresholds_HAL(&Therm,
                                    TSENSE_OVERT_THRESHOLD, &temp);
                    break;
                }
                case LW_MSGBOX_CMD_ARG1_THERM_PARAM_TEMP_SW_SLOWDN:
                {
                    soeStatus = thermGetTsenseThresholds_HAL(&Therm,
                                    TSENSE_WARN_THRESHOLD, &temp);
                    break;
                }
                default:
                {
                    soeStatus = FLCN_ERR_NOT_SUPPORTED;
                    break;
                }
            }

            status = smbpbiMapSoeStatusToMsgBoxStatus(soeStatus);
            if (LW_MSGBOX_CMD_STATUS_SUCCESS == status)
            {
                *pData = RM_SOE_LW_TEMP_TO_CELSIUS_ROUNDED(temp);
            }
        }
        else
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        }
    }
    else
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    return status;
}

sysSYSLIB_CODE static void
_smbpbiPrepareDataCopy
(
    LwU32 *pCmd,
    LwU32  data,
    LwU32  extData
)
{
    // Only perform data-copy result size encoding if the command supports it
    LwU32 opcode = DRF_VAL(_MSGBOX, _CMD, _OPCODE, *pCmd);
    switch (opcode)
    {
        case LW_MSGBOX_CMD_OPCODE_GET_ECC_V6:
        {
            *pCmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_DATA, data, *pCmd);
            //
            // If extData is non-zero then client needs to read both data-out and
            // ext-data registers for accurate result
            //
            if (extData)
            {
                *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_DATA_OUT, _SET, *pCmd);
                *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_EXT_DATA_OUT, _SET, *pCmd);
            }
            else
            {
                *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_EXT_DATA_OUT, _NOT_SET, *pCmd);
                // Indicate if entire DATA_OUT needs to be read
                if ((data >> DRF_SIZE(LW_MSGBOX_CMD_COPY_SIZE_ENCODING_DATA)) != 0)
                {
                    *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_DATA_OUT, _SET, *pCmd);
                }
                else
                {
                    *pCmd = FLD_SET_DRF(_MSGBOX, _CMD, _COPY_SIZE_ENCODING_READ_DATA_OUT, _NOT_SET, *pCmd);
                }
            }
            break;
        }

        //
        // If the command doesn't support data-copy result size encoding then copy
        // the data-out register as ususal
        //
        default:
        {
            *pCmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _DATA_COPY, data, *pCmd);
            break;
        }
    }
}

/*!
 * Get the requested Lwlink info
 *
 * @param[in]   arg1      Requested LwLink Info query (arg1)
 * @param[in]   arg2      Requested Link number or Info Page
 * @param[out]  pData     Pointer to output Data
 * @param[out]  pExtData  Pointer to output ExtData
 *
 * @return      status    SMBPBI status
 */
sysSYSLIB_CODE static LwU8
_smbpbiGetLwlinkInfoHandler
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pCmd,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_COUNT:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V1:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_BANDWIDTH:
            if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1,
                                            _LWLINK_INFO_STATE_V1))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                goto _smbpbiGetLwlinkInfoHandler_exit;
            }
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_REPLAY:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_RECOVERY:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_FLIT_CRC:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_DATA_CRC:
            if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1,
                                            _LWLINK_ERROR_COUNTERS))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                goto _smbpbiGetLwlinkInfoHandler_exit;
            }
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V2:
            if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                            _LWLINK_INFO_LINK_STATE_V2))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                goto _smbpbiGetLwlinkInfoHandler_exit;
            }
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_SUBLINK_WIDTH:
            if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                            _LWLINK_INFO_SUBLINK_WIDTH))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                goto _smbpbiGetLwlinkInfoHandler_exit;
            }
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_TX:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_RX:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_TX:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_RX:
            if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                            _LWLINK_INFO_THROUGHPUT))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                goto _smbpbiGetLwlinkInfoHandler_exit;
            }
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_TRAINING_ERROR_STATE:
            if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                            _LWLINK_INFO_TRAINING_ERROR_STATE))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                goto _smbpbiGetLwlinkInfoHandler_exit;
            }
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_RUNTIME_ERROR_STATE:
            if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                            _LWLINK_INFO_RUNTIME_ERROR_STATE))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
                goto _smbpbiGetLwlinkInfoHandler_exit;
            }
            break;
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            goto _smbpbiGetLwlinkInfoHandler_exit;
        }
    }

    // Execute command
    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_COUNT:
            status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, pData);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V1:
            status = smbpbiGetLwlinkStateV1_HAL(&Smbpbi, arg1, arg2, pData, pExtData);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_BANDWIDTH:
            status = smbpbiGetLwlinkBandwidth_HAL(&Smbpbi, arg1, arg2, pData, pExtData);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_REPLAY:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_RECOVERY:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_FLIT_CRC:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_DATA_CRC:
            status = smbpbiGetLwlinkErrorCount_HAL(&Smbpbi, arg1, arg2, pData);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V2:
            status = smbpbiGetLwlinkStateV2_HAL(&Smbpbi, arg1, arg2, pData, pExtData);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_SUBLINK_WIDTH:
            status = smbpbiGetLwlinkSublinkWidth_HAL(&Smbpbi, arg1, arg2, pData, pExtData);
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_TX:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_RX:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_TX:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_RX:
            status = smbpbiGetLwlinkThroughput_HAL(&Smbpbi, arg1, arg2, pCmd, pData, pExtData);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_TRAINING_ERROR_STATE:
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_RUNTIME_ERROR_STATE:
            status = smbpbiGetLwlinkErrorState_HAL(&Smbpbi, arg1, arg2, pCmd,  pData, pExtData);
            break;
        //
        // The default case can never be hit, since the argument check is done
        // in the previous switch statement.
        //
    }

_smbpbiGetLwlinkInfoHandler_exit:
    return status;
}

/*!
 * @brief Gets PCIe status and error counts
 *
 * @param[in]  arg1     Page index
 * @param[in]  arg2     Must be zero
 * @param[out] pData    Pointer to info:
 *                      - link speed
 *                      - link width
 *                      - non fatal error count
 *                      - fatal error count
 *                      - unsupported req count
 *                      - L0 to recovery count
 *                      - replay rollover count
 *                      - NAKS received count
 * @param[out] pExtData Pointer to info:
 *                      - correctable error count
 *                      - replay count
 *                      - NAKS sent count
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 *      if arg1 indicates invalid page index
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *      if arg2 is not 0
 */
sysSYSLIB_CODE LwU8
_smbpbiGetPcieLinkInfoHandler
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8  status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU32 pcieSpeed = 0;
    LwU32 pcieWidth = 0;
    LwU32 fatalErrCount = 0;
    LwU32 nonFatalErrCount = 0;
    LwU32 unsuppReqErrCount = 0;
    LwU32 val = 0;
    LwU32 dataVal = 0;
    LwU32 extDataVal = 0;

    if (arg2 != 0)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto _smbpbiGetPcieLinkInfoHandler_exit;
    }

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2,
                                    _GET_PCIE_LINK_INFO))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto _smbpbiGetPcieLinkInfoHandler_exit;
    }

    switch(arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_PCIE_LINK_INFO_PAGE_0:
        {
            smbpbiGetPcieSpeedWidth_HAL(&Smbpbi, &pcieSpeed, &pcieWidth);

            dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                      _PCIE_LINK_INFO_PAGE_0_LINK_SPEED,
                                      pcieSpeed, dataVal);

            dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                      _PCIE_LINK_INFO_PAGE_0_LINK_WIDTH,
                                      pcieWidth, dataVal);

            bifGetPcieFatalNonFatalUnsuppReqCount_HAL(&Bif, &fatalErrCount,
                                                         &nonFatalErrCount,
                                                         &unsuppReqErrCount);

            dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                      _PCIE_LINK_INFO_PAGE_0_NONFATAL_ERROR_COUNT,
                                      nonFatalErrCount, dataVal);

            dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                      _PCIE_LINK_INFO_PAGE_0_FATAL_ERROR_COUNT,
                                      fatalErrCount, dataVal);

            dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                      _PCIE_LINK_INFO_PAGE_0_UNSUPP_REQ_COUNT,
                                      unsuppReqErrCount, dataVal);

            bifGetPcieCorrectableErrorCount_HAL(&Bif, &val);

            extDataVal = FLD_SET_DRF_NUM(_MSGBOX, _EXT_DATA,
                                         _PCIE_LINK_INFO_PAGE_0_CORRECTABLE_ERROR_COUNT,
                                         val, extDataVal);

            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_PCIE_LINK_INFO_PAGE_1:
        {
            bifGetPcieL0ToRecoveryCount_HAL(&Bif, &val);
            dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                      _PCIE_LINK_INFO_PAGE_1_L0_TO_RECOVERY_COUNT,
                                      val, dataVal);

            bifGetPcieReplayCount_HAL(&Bif, &val);
            extDataVal = FLD_SET_DRF_NUM(_MSGBOX, _EXT_DATA,
                                         _PCIE_LINK_INFO_PAGE_1_REPLAY_COUNT,
                                         val, extDataVal);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_PCIE_LINK_INFO_PAGE_2:
        {
            bifGetPcieReplayRolloverCount_HAL(&Bif, &val);
            dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                      _PCIE_LINK_INFO_PAGE_2_REPLAY_ROLLOVER_COUNT,
                                      val, dataVal);

            bifGetPcieNaksRcvdCount_HAL(&Bif, &val);
            dataVal = FLD_SET_DRF_NUM(_MSGBOX, _DATA,
                                      _PCIE_LINK_INFO_PAGE_2_NAKS_RCVD_COUNT,
                                      val, dataVal);

            bifGetPcieNaksSentCount_HAL(&Bif, &val);
            extDataVal = FLD_SET_DRF_NUM(_MSGBOX, _EXT_DATA,
                                         _PCIE_LINK_INFO_PAGE_2_NAKS_SENT_COUNT,
                                         val, extDataVal);

            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            goto _smbpbiGetPcieLinkInfoHandler_exit;
        }
    }

    *pData = dataVal;
    *pExtData = extDataVal;

_smbpbiGetPcieLinkInfoHandler_exit:
    return status;
}

/*!
 * @brief Gets the Device Disable state, Driver state, and FM state
 *
 * @param[out] pData    Pointer to output data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      if success
 */
sysSYSLIB_CODE static LwU8
_smbpbiGetDeviceDisableState
(
    LwU32 *pData
)
{
    LwU32 stateFlags = 0;
    LwU16 driverState, deviceState, disableReason;
    LwBool bDriverTimedOut;

    smbpbiGetDeviceDisableModeStatus_HAL(&Smbpbi, &driverState, &deviceState, &disableReason);

    if (deviceState == LWSWITCH_DEVICE_FABRIC_STATE_BLACKLISTED)
    {
        stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                 _DEVICE_STATUS, _DISABLED, stateFlags);
    }
    else
    {
        stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                 _DEVICE_STATUS, _ENABLED, stateFlags);
    }

    lwrtosTaskCriticalEnter();
    bDriverTimedOut = Smbpbi.bDriverTimeout;
    lwrtosTaskCriticalExit();

    if (bDriverTimedOut)
    {
        stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                         _DRIVER_STATUS, _NOT_RUNNING, stateFlags);
        stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                         _FABRIC_MANAGER_STATUS, _NOT_RUNNING, stateFlags);
    }
    else
    {
        if (driverState == LWSWITCH_DRIVER_FABRIC_STATE_OFFLINE)
        {
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DRIVER_STATUS, _NOT_RUNNING, stateFlags);
        }
        else
        {
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DRIVER_STATUS, _RUNNING, stateFlags);
        }

        switch(driverState)
        {
            case LWSWITCH_DRIVER_FABRIC_STATE_CONFIGURED:
            {
                stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                         _FABRIC_MANAGER_STATUS, _RUNNING, stateFlags);
                break;
            }
            case LWSWITCH_DRIVER_FABRIC_STATE_MANAGER_TIMEOUT:
            {
                stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                         _FABRIC_MANAGER_STATUS, _TIMEOUT, stateFlags);
                break;
            }
            case LWSWITCH_DRIVER_FABRIC_STATE_MANAGER_ERROR:
            {
                stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                         _FABRIC_MANAGER_STATUS, _ERROR, stateFlags);
                break;
            }
            case LWSWITCH_DRIVER_FABRIC_STATE_OFFLINE:  // Fallthrough
            case LWSWITCH_DRIVER_FABRIC_STATE_STANDBY:  // Fallthrough
            default:
            {
                stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                         _FABRIC_MANAGER_STATUS, _NOT_RUNNING, stateFlags);
                break;
            }
        }
    }

    switch(disableReason)
    {
        case LWSWITCH_DEVICE_BLACKLIST_REASON_NONE:
        {
            // Device not disabled
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _NONE, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _NONE, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_MANUAL_OUT_OF_BAND:
        {
            // Manual, out of band
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _OUT_OF_BAND, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _CLIENT_OVERRIDE, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_MANUAL_IN_BAND:
        {
            // Manual, in band
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _IN_BAND, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _CLIENT_OVERRIDE, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_MANUAL_PEER:
        {
            // Manual, peer device
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _PEER_DEVICE, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _CLIENT_OVERRIDE, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_TRUNK_LINK_FAILURE:
        {
            // Trunk link failure, local device
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _LOCAL_DEVICE, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _TRUNK_LINK_FAILED, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_TRUNK_LINK_FAILURE_PEER:
        {
            // Trunk link failure, peer device
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _PEER_DEVICE, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _TRUNK_LINK_FAILED, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_ACCESS_LINK_FAILURE:
        {
            // Access link failure, local device
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _LOCAL_DEVICE, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _ACCESS_LINK_FAILED, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_ACCESS_LINK_FAILURE_PEER:
        {
            // Access link failure, peer device
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _PEER_DEVICE, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _ACCESS_LINK_FAILED, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_UNSPEC_DEVICE_FAILURE:
        {
            // Unspecified failure, local device
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _LOCAL_DEVICE, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _UNSPECIFIED_FAILURE, stateFlags);
            break;
        }
        case LWSWITCH_DEVICE_BLACKLIST_REASON_UNSPEC_DEVICE_FAILURE_PEER:
        {
            // Unspecified failure, peer device
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_SOURCE, _PEER_DEVICE, stateFlags);
            stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                     _DISABLE_REASON, _UNSPECIFIED_FAILURE, stateFlags);
            break;
        }
    }

    if (Smbpbi.bDriverReloadRequired)
    {
        stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                 _DRIVER_RELOAD, _PENDING, stateFlags);
    }
    else
    {
        stateFlags = FLD_SET_DRF(_MSGBOX, _DATA_DEVICE_DISABLE_STATE,
                                 _DRIVER_RELOAD, _NOT_PENDING, stateFlags);
    }

    *pData = stateFlags;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

sysSYSLIB_CODE static LwU8
_smbpbiGetMiscDeviceFlagsHandler
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 0, _GET_FABRIC_STATE_FLAGS))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    switch(arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_MISC_DEVICE_FLAGS_PAGE_0:
        {
            status = _smbpbiGetDeviceDisableState(pData);
            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            break;
        }
    }

    return status;
}

sysSYSLIB_CODE static LwU8
_smbpbiGetSysId
(
    LwU32 attr,
    LwU16 offset,
    LwU32 *pData
)
{
    LwU32   cnt;
    LwU16   orig;
    LwU16   size;
    LwU8    srctype;
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if ((BIT(DRF_VAL(_SMBPBI, _SYS_ID, _CAPBIT, attr)) &
        Smbpbi.config.cap[DRF_VAL(_SMBPBI, _SYS_ID, _CAPINDEX, attr)]) == 0)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto _smbpbiGetSysId_exit;
    }

    srctype = DRF_VAL(_SMBPBI, _SYS_ID, _SRCTYPE, attr);
    orig = DRF_VAL(_SMBPBI, _SYS_ID, _OFFSET, attr);
    size = DRF_VAL(_SMBPBI, _SYS_ID, _SIZE, attr);

    if (offset >= size)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto _smbpbiGetSysId_exit;
    }

    cnt = size - offset;
    if (cnt > sizeof(*pData))
    {
        cnt = sizeof(*pData);
    }
    else
    {
        *pData = 0;
    }

    if (srctype == LW_SMBPBI_SYS_ID_SRCTYPE_IFR_STRING)
    {
        offset *= sizeof(LwU8);
    }

    if (srctype == LW_SMBPBI_SYS_ID_SRCTYPE_DMEM)
    {
        memcpy(pData, (LwU8 *)&Smbpbi.config.sysIdData + orig + offset, cnt);
    }
    else
    {
        memcpy(pData, (LwU8 *)pInforomData + orig + offset, cnt);
    }

_smbpbiGetSysId_exit:
    return status;
}

sysSYSLIB_CODE static void
_smbpbiInforomInitCapDword()
{
    LwU32 *pCaps1 = &Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1];

    if (pInforomData->OBD.bValid)
    {
        *pCaps1 = FLD_SET_DRF(_MSGBOX_DATA, _CAP_1, _MARKETING_NAME_V1, _AVAILABLE, *pCaps1);
    }

    if (pInforomData->IMG.bValid)
    {
        *pCaps1 = FLD_SET_DRF(_MSGBOX_DATA, _CAP_1, _INFOROM_VER_V1, _AVAILABLE, *pCaps1);
    }
}

sysSYSLIB_CODE static LwU8
_smbpbiGetWriteProtectionMode
(
    LwU8    arg1,
    LwU32   *pData
)
{
    LwU8    status;
    LwBool  bWpEnabled;

    //
    // RM/PMU SMBPBI server only supports the GET action. Switch
    // on all valid values to detect if the reserved bits in
    // arg1[7:1] were zeroed by the SMBPBI client correctly.
    //
    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_ACCESS_WP_MODE_ACTION_GET:
            smbpbiGetFwWriteProtMode_HAL(&Smbpbi, &bWpEnabled);
            *pData = bWpEnabled ? LW_MSGBOX_DATA_ACCESS_WP_MODE_GET_STATE_ENABLED:
                                  LW_MSGBOX_DATA_ACCESS_WP_MODE_GET_STATE_DISABLED;
            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            break;

        case LW_MSGBOX_CMD_ARG1_ACCESS_WP_MODE_ACTION_SET:
            return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;

        default:
            return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    return status;
}
