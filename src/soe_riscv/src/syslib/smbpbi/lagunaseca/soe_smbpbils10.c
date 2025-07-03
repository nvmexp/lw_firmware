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
 * @file    soe_smbpbilr10.c
 * @brief   SOE HAL functions for LR10+ for thermal message box functionality
 */

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"

#include "dev_lwlsaw_ip.h"
#include "dev_lwlsaw_ip_addendum.h"

/* ------------------------ Application Includes --------------------------- */
#include "soe_objsmbpbi.h"
#include "cmdmgmt.h"
#include "soe_objsaw.h"
#include "soe_objbif.h"
#include "config/g_smbpbi_hal.h"

#include "dev_therm.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
static sysTASK_DATA RM_FLCN_CMD_SOE  smbpbiCmd;

/* ------------------------ Macros ----------------------------------------- */
//
// Firmware Version is stored as conselwtive bytes in scratch registers.
// https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/BIT#BIT_BIOSDATA_.28Version_2.29
//
#define LWSWITCH_RAW_BIOS_VERSION_BYTE_COUNT      4
#define LWSWITCH_RAW_BIOS_OEM_VERSION_BYTE_COUNT  1
#define LWSWITCH_FIRMWARE_VERSION_REG_COUNT       2

/*!
 * @brief Reverses the byte order of a word; that is, switching the endianness
 * of the word.
 *
 * @param[in]   a   A 32-bit word
 *
 * @returns The 32-bit word with its byte order reversed.
 */

#define REVERSE_BYTE_ORDER(a) \
    (((a) >> 24) | ((a) << 24) | (((a) >> 8) & 0xFF00) | (((a) << 8) & 0xFF0000))

/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * SMBPBI task initialization
 */
sysSYSLIB_CODE void
smbpbiInit_LS10
(
    void
)
{
    // Capabilities Dword 0
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_0, _TEMP_GPU_0, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_0, _THERMP_TEMP_SHUTDN, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_0, _THERMP_TEMP_GPU_SW_SLOWDOWN,
                _AVAILABLE, Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_0, _GET_FABRIC_STATE_FLAGS,
                _AVAILABLE, Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_0]);

    // Capabilities Dword 1
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_1, _ECC_V6, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_1, _LWLINK_INFO_STATE_V1, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_1, _LWLINK_ERROR_COUNTERS, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_1, _ACCESS_WP_MODE, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1]);

    // Capabilities Dword 2
    // Indicate, that the RM/SOE image is running (vs. IFR/SOE)
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF_NUM(_MSGBOX, _DATA_CAP_2, _NUM_SCRATCH_BANKS,
                LW_MSGBOX_SCRATCH_NUM_PAGES_CAP_CODE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _LWSWITCH_DRIVER, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _GET_PCIE_LINK_INFO, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _LWLINK_INFO_LINK_STATE_V2, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _LWLINK_INFO_SUBLINK_WIDTH, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _LWLINK_INFO_THROUGHPUT, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _LWLINK_INFO_TRAINING_ERROR_STATE, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _LWLINK_INFO_RUNTIME_ERROR_STATE, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_2, _SCRATCH_PAGE_SIZE, _256B,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_2]);

    // Capabilities Dword 4
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_4] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_4, _SET_DEVICE_DISABLE, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_4]);
    Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_4] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_4, _REQUEST_BUNDLING, _AVAILABLE,
                Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_4]);

    Smbpbi.config.capCount   = LW_MSGBOX_DATA_CAP_COUNT;
    Smbpbi.driverStateSeqNum = SMBPBI_DRIVER_SEQ_NUM_INIT;

    // Initialize static system identification data.
    smbpbiInitSysIdInfo_HAL();
}

/*!
 * Specific handler-routine for dealing with thermal message box interrupts.
 */
sysSYSLIB_CODE void
smbpbiService_LS10(void)
{
    DISP2UNIT_CMD   disp2Smbpbi = { 0 };
    LwU32           regVal;

    // Verify _MSGBOX_INTR bit and clear it.
    regVal = BAR0_REG_RD32(LW_THERM_MSGBOX_COMMAND);
    if (FLD_TEST_DRF(_THERM, _MSGBOX_COMMAND, _INTR, _PENDING, regVal))
    {
        //
        // It's indeed thermal MSGBOX IRQ, clear its root cause.
        // Then read back in order to ensure, that the write has really
        // completed in the HW before we proceed.
        //
        regVal = FLD_SET_DRF(_THERM, _MSGBOX_COMMAND, _INTR, _NOT_PENDING, regVal);
        BAR0_REG_WR32(LW_THERM_MSGBOX_COMMAND, regVal);
        (void) BAR0_REG_RD32(LW_THERM_MSGBOX_COMMAND);

        // Send message to the SMBPBI task to process msgbox request.
        if (SOECFG_FEATURE_ENABLED(SOETASK_SMBPBI))
        {
            // Prevent overflow - increment only if not MAX
            if(Smbpbi.requestCnt != LW_U8_MAX)
            {
                // We only allow a single active request
                if (0 == Smbpbi.requestCnt++)
                {
                    disp2Smbpbi.eventType                    = SMBPBI_EVT_MSGBOX_IRQ;
                    disp2Smbpbi.cmdQueueId                   = SOE_CMDMGMT_CMD_QUEUE_RM;
                    disp2Smbpbi.pCmd                         = &smbpbiCmd;
                    smbpbiCmd.hdr.unitId                     = RM_SOE_UNIT_SMBPBI;
                    smbpbiCmd.hdr.ctrlFlags                  = 0;
                    smbpbiCmd.hdr.seqNumId                   = 0;
                    smbpbiCmd.hdr.size                       = RM_FLCN_QUEUE_HDR_SIZE;
                    smbpbiCmd.cmd.smbpbiCmd.msgbox.msgboxCmd = regVal;

                    lwrtosQueueSendFromISR(Disp2QSmbpbiThd, &disp2Smbpbi,
                                       sizeof(disp2Smbpbi));
                }
            }
        }
    }
    else
    {
        // A spurious interrupt. The MSGBOX trigger bit is not set.
        OS_BREAKPOINT();
    }
}

/*!
 * Provides read/write access to message box interface by exelwting requested
 * msgbox action:
 *  SOE_SMBPBI_GET_REQUEST    - retrieve msgbox request
 *  SOE_SMBPBI_SET_RESPONSE   - send out msgbox response
 *
 * @param[in,out]   pCmd        buffer that holds msgbox command or NULL
 * @param[in,out]   pData       buffer that holds msgbox command specific data or NULL
 * @param[in,out]   pExtData    buffer that holds msgbox command specific ext-data or NULL
 * @param[in]       action      requested action:
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if invalid action
 * @return FLCN_OK                  otherwise
 */
sysSYSLIB_CODE FLCN_STATUS
smbpbiExelwte_LS10
(
    LwU32  *pCmd,
    LwU32  *pData,
    LwU32  *pExtData,
    LwU8    action
)
{
    FLCN_STATUS status  = FLCN_ERR_ILWALID_ARGUMENT;

    if (action == SOE_SMBPBI_GET_REQUEST)
    {
        if (pCmd != NULL)
        {
            *pCmd  = BAR0_REG_RD32(LW_THERM_MSGBOX_COMMAND);
        }

        if (pData != NULL)
        {
            *pData  = BAR0_REG_RD32(LW_THERM_MSGBOX_DATA_IN);
        }

        if (pExtData != NULL)
        {
            *pExtData  = BAR0_REG_RD32(LW_THERM_MSGBOX_DATA_OUT);
        }

        status = FLCN_OK;
    }
    else if (action == SOE_SMBPBI_SET_RESPONSE)
    {
        //
        // Write data first since cmd carries acknowledgment.
        //
        // GF11X message box implementation brought several changes:
        //  - register addresses => that is answered by this HAL call.
        //  - two data registers(IN/OUT) => since their behavior is fully
        //    equivalent we keep using DATA_IN for both in/out operations
        //    and we may separate that in the future if needed.
        //  - MUTEX => since PMU acts as a client there is no PMU action
        //    related to mutex that is used to synchronize requests.
        //
        if (pData != NULL)
        {
            BAR0_REG_WR32(LW_THERM_MSGBOX_DATA_IN, *pData);
        }

        if (pExtData != NULL)
        {
            BAR0_REG_WR32(LW_THERM_MSGBOX_DATA_OUT, *pExtData);
        }

        if (pCmd != NULL)
        {
            smbpbiCheckEvents(pCmd);
            BAR0_REG_WR32(LW_THERM_MSGBOX_COMMAND, *pCmd);
        }

        status = FLCN_OK;
    }

    return status;
}

/*!
 * @brief       Get PCIe Speed and Width
 *
 * @param[out]  pSpeed   Pointer to returned PCIe link speed value
 * @param[out]  pWidth   Pointer to returned PCIe link width value
 */
sysSYSLIB_CODE void
smbpbiGetPcieSpeedWidth_LS10
(
    LwU32   *pSpeed,
    LwU32   *pWidth
)
{
    bifGetPcieSpeedWidth_HAL(&Bif, pSpeed, pWidth);

    // Get PCIe Speed
    switch (*pSpeed)
    {
        case RM_SOE_BIF_LINK_SPEED_GEN1PCIE:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_2500_MTPS;
            break;
        }
        case RM_SOE_BIF_LINK_SPEED_GEN2PCIE:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_5000_MTPS;
            break;
        }
        case RM_SOE_BIF_LINK_SPEED_GEN3PCIE:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_8000_MTPS;
            break;
        }
        case RM_SOE_BIF_LINK_SPEED_GEN4PCIE:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_16000_MTPS;
            break;
        }
        default:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_UNKNOWN;
            break;
        }
    }

    // Get PCIe Width
    switch (*pWidth)
    {
        case RM_SOE_BIF_LINK_WIDTH_X1:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X1;
            break;
        }
        case RM_SOE_BIF_LINK_WIDTH_X2:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X2;
            break;
        }
        case RM_SOE_BIF_LINK_WIDTH_X4:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X4;
            break;
        }
        case RM_SOE_BIF_LINK_WIDTH_X8:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X8;
            break;
        }
        case RM_SOE_BIF_LINK_WIDTH_X16:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X16;
            break;
        }
        default:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_UNKNOWN;
        }
    }
}

/*!
 * @brief       Set the requested device disable mode to a shared scratch register
 *
 * @param[in]   flags   Set/clear flags to write to register
 */
sysSYSLIB_CODE void
smbpbiSetDeviceDisableMode_LS10
(
    LwU16 flags
)
{
    LwU32 reg = 0;

    if (FLD_TEST_DRF(_MSGBOX_DEVICE_MODE_CONTROL_PARAMS, _FLAGS, _CONTROL, _SET, flags))
    {
        reg = FLD_SET_DRF(_LWLSAW, _SCRATCH_COLD, _OOB_BLACKLIST_DEVICE_REQUESTED,
                          _DISABLE, reg);
    }
    else
    {
        reg = FLD_SET_DRF(_LWLSAW, _SCRATCH_COLD, _OOB_BLACKLIST_DEVICE_REQUESTED,
                          _ENABLE, reg);
    }

    REG_WR32(SAW, LW_LWLSAW_SCRATCH_COLD, reg);
}

/*!
 * @brief Read the current device disable mode status from the driver through
 *        the allocated scratch register.
 *
 * @param[out] pDriverState     Driver fabric state
 * @param[out] pDeviceState     Device fabric state
 * @param[out] pDisableReason   Disable reason and source
 */
sysSYSLIB_CODE void
smbpbiGetDeviceDisableModeStatus_LS10
(
    LwU16 *pDriverState,
    LwU16 *pDeviceState,
    LwU16 *pDisableReason
)
{
    LwU32 reg = 0;

    reg = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_12);

    *pDriverState   = DRF_VAL(_LWLSAW, _SW_SCRATCH_12, _DRIVER_FABRIC_STATE, reg);
    *pDeviceState   = DRF_VAL(_LWLSAW, _SW_SCRATCH_12, _DEVICE_FABRIC_STATE, reg);
    *pDisableReason = DRF_VAL(_LWLSAW, _SW_SCRATCH_12, _DEVICE_BLACKLIST_REASON, reg);
}

/*!
 * @brief       Fill in Static system identification data from different 
 *              source like PCI Config space and secure scratch registers.
 */
sysSYSLIB_CODE void
smbpbiInitSysIdInfo_LS10()
{
    SOE_SMBPBI_SYSID_DATA  *pSysId = &Smbpbi.config.sysIdData;
    LwU32                  *pCaps  = &Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_1];

    // PCI config space IDs
    bifGetPcieConfigSpaceIds_HAL(&Bif, &pSysId->vendorId, 
        &pSysId->devId, &pSysId->subVendorId, &pSysId->subId);

    *pCaps = FLD_SET_DRF(_MSGBOX_DATA, _CAP_1, _VENDOR_ID_V1, _AVAILABLE, *pCaps);
    *pCaps = FLD_SET_DRF(_MSGBOX_DATA, _CAP_1, _DEV_ID_V1, _AVAILABLE, *pCaps);
    *pCaps = FLD_SET_DRF(_MSGBOX_DATA, _CAP_1, _SUB_VENDOR_ID_V1, _AVAILABLE, *pCaps);
    *pCaps = FLD_SET_DRF(_MSGBOX_DATA, _CAP_1, _SUB_ID_V1, _AVAILABLE, *pCaps);

    // Get Firmware Version
    smbpbiGetFirmwareVersion_HAL(&Smbpbi, pSysId->firmwareVer);
    *pCaps = FLD_SET_DRF(_MSGBOX_DATA, _CAP_1, _FIRMWARE_VER_V1, _AVAILABLE, *pCaps);

    // Get LR10 UUID    
    smbpbiGetUuid_HAL(&Smbpbi, pSysId->guid);
    *pCaps = FLD_SET_DRF(_MSGBOX_DATA, _CAP_1, _GPU_GUID_V1, _AVAILABLE, *pCaps);
}

/*!
 * @brief       Get Firmware Version from secure scratch registers.
 *
 * @param[out]  pVersion  Pointer to Firmware version in hex.
 * 
 */
sysSYSLIB_CODE void
smbpbiGetFirmwareVersion_LS10
(
    LwU8 *pVersion
)
{
    LwU32             regData[LWSWITCH_FIRMWARE_VERSION_REG_COUNT];
    LwU8              biosVersion[LWSWITCH_RAW_BIOS_VERSION_BYTE_COUNT];
    LwU8              biosOEMVersion;
    LwU32             i;
    static const char hexDigits[] = "0123456789ABCDEF";

#define SOE_NIBBLE_TO_HEX(nibble)    (hexDigits[(nibble) & 0xf])

    // Read 4 byte firmware version from secure scratch register SW_SCRATCH_6.
    regData[0] = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_6);
    regData[0] = REVERSE_BYTE_ORDER(regData[0]);
    memcpy(biosVersion, (LwU8 *)regData, LWSWITCH_RAW_BIOS_VERSION_BYTE_COUNT);

    // Read one byte firmware OEM version from secure scratch register SW_SCRATCH_7.
    regData[1] = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_7);
    biosOEMVersion = (LwU8) DRF_VAL(_LWLSAW_SW, _SCRATCH_7, _BIOS_OEM_VERSION, regData[1]);

    //
    // Colwert the BIOS Version and OEM Version to 14 Byte Hex format as per OOB spec.
    // There is no room for the terminating null character.
    //  
    for (i = 0; i < LWSWITCH_RAW_BIOS_VERSION_BYTE_COUNT; i++)
    {
        *pVersion++ = SOE_NIBBLE_TO_HEX(biosVersion[i] >> 4);
        *pVersion++ = SOE_NIBBLE_TO_HEX(biosVersion[i]);
        *pVersion++ = '.';
    }

    *pVersion++ = SOE_NIBBLE_TO_HEX(biosOEMVersion >> 4);
    *pVersion++ = SOE_NIBBLE_TO_HEX(biosOEMVersion);
        
#undef SOE_NIBBLE_TO_HEX
}

/*!
 * @brief       Get UUID from secure scratch registers.
 *
 * @param[out]  pGuid  Pointer to UUID.
 * 
 */
sysSYSLIB_CODE void
smbpbiGetUuid_LS10
(
    LwU8 *pGuid
)
{
    LwU32 regData[4];

    //
    // Read 128-bit UUID from secure scratch registers which must be
    // populated by firmware.
    //
    regData[0] = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_8);
    regData[1] = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_9);
    regData[2] = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_10);
    regData[3] = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_11);

    memcpy(pGuid, (LwU8 *)regData, 16);
}

/*!
 * @brief Get the sequence number passed down by the driver
 *
 * @param[out]  pSeqNum   pointer to the returned sequence number
 */
sysSYSLIB_CODE void
smbpbiGetDriverSeqNum_LS10
(
    LwU32 *pSeqNum
)
{
    LwU32 reg = 0;

    reg = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_12);

    *pSeqNum = DRF_VAL(_LWLSAW, _SW_SCRATCH_12, _EVENT_MESSAGE_COUNT, reg);
}

/*!
 * @brief   Read status of firmware write protection mode
 *
 * @param[in]   pMode   Status of write protection (enabled/disabled)
 */
sysSYSLIB_CODE void
smbpbiGetFwWriteProtMode_LS10
(
    LwBool *pMode
)
{
    //
    // Check both the legacy L3 bit in GROUP_05_1 and the new L1 bit in
    // GROUP_17 since the legacy bit is still defined in the scratch spec
    // for Turing and later. Checking both provides an option to switch
    // between L3 and L1 without requiring any changes.
    //
    LwU32 scratchGroup05 = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_2);
    LwU32 scratchGroup17 = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_0);

    *pMode = FLD_TEST_REF(LW_LWLSAW_SW_SCRATCH_2_INFOROM_WRITE_PROTECT_MODE, _ENABLED, scratchGroup05) ||
             FLD_TEST_REF(LW_LWLSAW_SW_SCRATCH_0_WRITE_PROTECT_MODE, _ENABLED, scratchGroup17);
}
