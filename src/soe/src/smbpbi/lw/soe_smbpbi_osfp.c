/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021  by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_smbpbi_osfp.c
 * @brief  SMBus Post-Box Interface. Handling of requests targeting
 *         OSFP optical transceivers.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsoe.h"
#include "soe_objsmbpbi.h"
#include "smbpbi_shared_lwswitch.h"

/* ------------------------- Types and Enums -------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define OSFP_MASK_ALL               0
#define OSFP_MASK_PRESENT           1
#define OSFP_XCEIVER_IDX_ILWALID    ((LwU8) -1)
#define OSFP_READ_VAR(memb, var)                                \
    smbpbiHostMemRead(LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,    \
                                    osfpData.memb),             \
                                    sizeof(var),                \
                                    (LwU8 *)&(var))

/* ------------------------- Prototypes ------------------------------------- */
static LwU8 _smbpbiOsfpGetPingPongBuff(SOE_SMBPBI_OSFP_XCEIVER_PING_PONG_BUFF *pBuff, 
                                       SOE_SMBPBI_OSFP_XCEIVER_MASKS *pXcvrMask);
static LwU8 _smbpbiOsfpCheckPresent(SOE_SMBPBI_OSFP_XCEIVER_MASKS *pXcvrMask, 
                                    LwU8 xceiverIdx, LwU8 maskType);
static LwU8 _smbpbiOsfpSendCommand(LwU8 cmd);

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Public Functions ------------------------------- */

LwU8
smbpbiOsfpGetHottest
(
    LwU32   *pCmd,
    LwU32   *pData,
    LwU32   *pExtData
)
{
    SOE_SMBPBI_OSFP_XCEIVER_PING_PONG_BUFF  pingPongBuff;
    LwU16       presentMask;
    LwU16       allMask;
    LwU16       hottestMask;
    LwS16       maxTemp;
    LwS16       *pTemperature;
    LwU32       failedXceivers = 0;
    unsigned    idx;
    LwU8        status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 4,
                        _OPTICAL_XCEIVER_CTL_GET_HOTTEST))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto smbpbiOsfpGetHottest_exit;
    }

    status = _smbpbiOsfpGetPingPongBuff(&pingPongBuff, NULL);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetHottest_exit;
    }

    status = _smbpbiOsfpCheckPresent(&pingPongBuff.xcvrMask, OSFP_XCEIVER_IDX_ILWALID, OSFP_MASK_PRESENT);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetHottest_exit;
    }

    status = _smbpbiOsfpCheckPresent(&pingPongBuff.xcvrMask, OSFP_XCEIVER_IDX_ILWALID, OSFP_MASK_ALL);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetHottest_exit;
    }

    presentMask = pingPongBuff.xcvrMask.present;
    allMask = pingPongBuff.xcvrMask.all;
    pTemperature = pingPongBuff.temperature;

    *pData = 0;
    *pExtData = 0;
    maxTemp = LW_S16_MIN;
    hottestMask = 0;

    FOR_EACH_INDEX_IN_MASK(16, idx, presentMask)
    {
        if (pTemperature[idx] > maxTemp)
        {
            hottestMask = LWBIT(idx);
            maxTemp = pTemperature[idx];
        }
        else if (pTemperature[idx] == maxTemp)
        {
            hottestMask |= LWBIT(idx);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    failedXceivers = allMask & ~presentMask;

    *pCmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _OPTICAL_XCEIVER_CTL_HOTTEST_MASK_ALL,
                            hottestMask, *pCmd);
    *pCmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _OPTICAL_XCEIVER_CTL_HOTTEST_TEMPERATURE,
                            maxTemp, *pCmd);
    *pData = FLD_SET_DRF_NUM(_MSGBOX, _DATA, _OPTICAL_XCEIVER_CTL_HOTTEST_PRESENT_ALL,
                            presentMask, *pData);
    *pData = FLD_SET_DRF_NUM(_MSGBOX, _DATA, _OPTICAL_XCEIVER_CTL_HOTTEST_MASK_ALL,
                            hottestMask >> 8, *pData);
    if (failedXceivers != 0)
    {
        *pExtData = FLD_SET_DRF_NUM(_MSGBOX, _EXT_DATA, _OPTICAL_XCEIVER_CTL_XCEIVERS_FAILED,
                                    failedXceivers, *pExtData);
        *pData = FLD_SET_DRF(_MSGBOX, _DATA, _OPTICAL_XCEIVER_CTL_HOTTEST_FAILED_PRESENT, _YES,
                                *pData);
    }

smbpbiOsfpGetHottest_exit:
    return status;
}

LwU8
smbpbiOsfpGetLedState
(
    LwU32   *pData,
    LwU32   *pExtData
)
{
    SOE_SMBPBI_OSFP_XCEIVER_MASKS   xcvrMask;
    LwU16       presentMask;
    unsigned    idx;
    LwU8        ledState[SOE_SMBPBI_OSFP_NUM_XCEIVERS];
    LwU8        status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 4,
                        _OPTICAL_XCEIVER_CTL_GET_LED_STATE))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto smbpbiOsfpGetLedState_exit;
    }

    status = _smbpbiOsfpGetPingPongBuff(NULL, &xcvrMask);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetLedState_exit;
    }

    status = _smbpbiOsfpCheckPresent(&xcvrMask, OSFP_XCEIVER_IDX_ILWALID, OSFP_MASK_ALL);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetLedState_exit;
    }

    presentMask = xcvrMask.all;

    status = OSFP_READ_VAR(ledState, ledState);

    if (status == LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        FOR_EACH_INDEX_IN_MASK(16, idx, presentMask)
        {
            if (idx < LW_MSGBOX_DATA_OPTICAL_XCEIVER_CTL_LED_STATE__SIZE)
            {
                *pData = FLD_IDX_SET_DRF_NUM(_MSGBOX, _DATA_OPTICAL_XCEIVER_CTL, _LED_STATE,
                                             idx, ledState[idx], *pData);
            }
            else
            {
                *pExtData = FLD_IDX_SET_DRF_NUM(_MSGBOX, _EXT_DATA_OPTICAL_XCEIVER_CTL, _LED_STATE,
                                             idx, ledState[idx], *pExtData);
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

smbpbiOsfpGetLedState_exit:
    return status;
}

LwU8
smbpbiOsfpGetXceiverTemperature
(
    LwU8    xceiverIdx,
    LwU32   *pData
)
{
    SOE_SMBPBI_OSFP_XCEIVER_PING_PONG_BUFF  pingPongBuff;
    LwU8        status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 4,
                        _OPTICAL_XCEIVER_CTL_GET_TEMPERATURE))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto smbpbiOsfpGetXceiverTemperature_exit;
    }

    status = _smbpbiOsfpGetPingPongBuff(&pingPongBuff, NULL);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetXceiverTemperature_exit;
    }

    status = _smbpbiOsfpCheckPresent(&pingPongBuff.xcvrMask, xceiverIdx, OSFP_MASK_PRESENT);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetXceiverTemperature_exit;
    }

    *pData = pingPongBuff.temperature[xceiverIdx];

smbpbiOsfpGetXceiverTemperature_exit:
    return status;
}

LwU8
smbpbiOsfpSetLedLocate
(
    LwU8    xceiverIdx
)
{
    SOE_SMBPBI_OSFP_XCEIVER_MASKS   xcvrMask;
    LwU8    cmd;
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 4,
                        _OPTICAL_XCEIVER_CTL_SET_LED_LOCATE))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto smbpbiOsfpSetLedLocate_exit;
    }

    status = _smbpbiOsfpGetPingPongBuff(NULL, &xcvrMask);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpSetLedLocate_exit;
    }

    status = _smbpbiOsfpCheckPresent(&xcvrMask, xceiverIdx, OSFP_MASK_ALL);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpSetLedLocate_exit;
    }

    cmd = DRF_DEF(_SOE_SMBPBI_OSFP_FIFO, _CMD, _OPCODE, _LED_LOCATE_ON) |
          DRF_NUM(_SOE_SMBPBI_OSFP_FIFO, _CMD, _ARG, xceiverIdx);

    status = _smbpbiOsfpSendCommand(cmd);

smbpbiOsfpSetLedLocate_exit:
    return status;
}

LwU8
smbpbiOsfpResetLedLocate
(
    LwU8    xceiverIdx
)
{
    SOE_SMBPBI_OSFP_XCEIVER_MASKS   xcvrMask;
    LwU8    cmd;
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 4,
                        _OPTICAL_XCEIVER_CTL_RESET_LED_LOCATE))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto smbpbiOsfpResetLedLocate_exit;
    }

    status = _smbpbiOsfpGetPingPongBuff(NULL, &xcvrMask);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpResetLedLocate_exit;
    }

    status = _smbpbiOsfpCheckPresent(&xcvrMask, xceiverIdx, OSFP_MASK_ALL);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpResetLedLocate_exit;
    }

    cmd = DRF_DEF(_SOE_SMBPBI_OSFP_FIFO, _CMD, _OPCODE, _LED_LOCATE_OFF) |
          DRF_NUM(_SOE_SMBPBI_OSFP_FIFO, _CMD, _ARG, xceiverIdx);

    status = _smbpbiOsfpSendCommand(cmd);

smbpbiOsfpResetLedLocate_exit:
    return status;
}

LwU8
smbpbiOsfpGetInfo
(
    LwU8    xceiverIdx,
    LwU8    infoType,
    LwU8    offset,
    LwU32   *pData,
    LwU32   *pExtData
)
{
    SOE_SMBPBI_OSFP_XCEIVER_MASKS xcvrMask;
    unsigned    capbit;
    unsigned    limit = 0;
    unsigned    readOffset = 0;
    unsigned    size;
    LwU32       buffer[2] = {0};
    LwU8        status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (infoType > LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_INFO_TYPE_MAX)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
        goto smbpbiOsfpGetInfo_exit;
    }

    capbit = DRF_BASE(LW_MSGBOX_DATA_CAP_4_OPTICAL_XCEIVER_INFO_TYPE_SERIAL_NUMBER) + infoType;

    if (REF_VAL(capbit:capbit, Smbpbi.config.cap[LW_MSGBOX_DATA_CAP_4]) !=
                LW_MSGBOX_DATA_CAP_4_OPTICAL_XCEIVER_INFO_TYPE_SERIAL_NUMBER_AVAILABLE)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto smbpbiOsfpGetInfo_exit;
    }

    status = _smbpbiOsfpGetPingPongBuff(NULL, &xcvrMask);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetInfo_exit;
    }

    status = _smbpbiOsfpCheckPresent(&xcvrMask, xceiverIdx, OSFP_MASK_PRESENT);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiOsfpGetInfo_exit;
    }

    switch (infoType)
    {
        SOE_SMBPBI_OSFP_XCEIVER_INFO    *pInfo;

        case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_INFO_TYPE_SERIAL_NUMBER:
        {
            ct_assert(sizeof(pInfo->serialNumber) ==
                      LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_SERIAL_NUMBER);
            limit = LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_SERIAL_NUMBER;
            readOffset = LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,
                                    osfpData.info[xceiverIdx].serialNumber);
            break;
        }

        case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_INFO_TYPE_PART_NUMBER:
        {
            ct_assert(sizeof(pInfo->partNumber) ==
                      LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_PART_NUMBER);
            limit = LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_PART_NUMBER;
            readOffset = LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,
                                    osfpData.info[xceiverIdx].partNumber);
            break;
        }

        case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_INFO_TYPE_FW_VERSION:
        {
            ct_assert(sizeof(pInfo->firmwareVersion) ==
                      LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_FW_VERSION);
            limit = LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_FW_VERSION;
            readOffset = LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,
                                    osfpData.info[xceiverIdx].firmwareVersion);
            break;
        }

        case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_INFO_TYPE_HW_REVISION:
        {
            ct_assert(sizeof(pInfo->hardwareRevision) ==
                      LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_HW_REVISION);
            limit = LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_HW_REVISION;
            readOffset = LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,
                                    osfpData.info[xceiverIdx].hardwareRevision);
            break;
        }

        case LW_MSGBOX_CMD_ARG1_OPTICAL_XCEIVER_INFO_TYPE_FRU_EEPROM:
        {
            ct_assert(sizeof(pInfo->fruEeprom) ==
                      LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_FRU_EEPROM);
            limit = LW_MSGBOX_OPTICAL_XCEIVER_INFO_DATA_SIZE_FRU_EEPROM;
            readOffset = LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,
                                    osfpData.info[xceiverIdx].fruEeprom);
            break;
        }

        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            goto smbpbiOsfpGetInfo_exit;
        }
    }

    size = sizeof(*pData) + sizeof(*pExtData);
    offset *= size;

    if (offset >= limit)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto smbpbiOsfpGetInfo_exit;
    }

    if ((offset + size) > limit)
    {
        size = limit - offset;
    }

    readOffset += offset;

    status = smbpbiHostMemRead(readOffset, size, (LwU8 *)buffer);

    if (status == LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        *pData = buffer[0];
        *pExtData = buffer[1];
    }

smbpbiOsfpGetInfo_exit:
    return status;
}

void
smbpbiOsfpSetCaps
(
    LwU32   *pCaps
)
{
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_CTL_GET_HOTTEST);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_CTL_GET_LED_STATE);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_CTL_GET_TEMPERATURE);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_CTL_SET_LED_LOCATE);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_CTL_RESET_LED_LOCATE);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_INFO_TYPE_SERIAL_NUMBER);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_INFO_TYPE_PART_NUMBER);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_INFO_TYPE_FW_VERSION);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_INFO_TYPE_HW_REVISION);
    LW_MSGBOX_CAP_SET_AVAILABLE(pCaps, 4, _OPTICAL_XCEIVER_INFO_TYPE_FRU_EEPROM);
}

/* ------------------------- Private functions ------------------------------ */
static LwU8
_smbpbiOsfpGetPingPongBuff
(
    SOE_SMBPBI_OSFP_XCEIVER_PING_PONG_BUFF *pBuff,
    SOE_SMBPBI_OSFP_XCEIVER_MASKS *pXcvrMask
)
{
    LwU8        buffIdx;
    LwU8        buffIdxTemp;
    LwU8        status;

    do 
    {
        status = OSFP_READ_VAR(pingPongBuffIdx, buffIdx);

        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto _smbpbiOsfpGetPingPongBuff_exit;
        }

        // Only 2 entries in ping pong buffer
        if (buffIdx > 1)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_DATA;
            goto _smbpbiOsfpGetPingPongBuff_exit;
        }

        // Shouldn't need to get both in same call
        if (pBuff != NULL)
        {
            status = OSFP_READ_VAR(pingPongBuff[buffIdx], *pBuff);

            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                goto _smbpbiOsfpGetPingPongBuff_exit;
            }
        }

        if (pXcvrMask != NULL)
        {
            status = OSFP_READ_VAR(pingPongBuff[buffIdx].xcvrMask, *pXcvrMask);

            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                goto _smbpbiOsfpGetPingPongBuff_exit;
            }
        }

        status = OSFP_READ_VAR(pingPongBuffIdx, buffIdxTemp);

        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto _smbpbiOsfpGetPingPongBuff_exit;
        }

        if (buffIdxTemp > 1)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_DATA;
            goto _smbpbiOsfpGetPingPongBuff_exit;
        }
    } while (buffIdx != buffIdxTemp);

_smbpbiOsfpGetPingPongBuff_exit:
    return status;
}

static LwU8
_smbpbiOsfpCheckPresent
(
    SOE_SMBPBI_OSFP_XCEIVER_MASKS   *pXcvrMask,
    LwU8                            xceiverIdx,
    LwU8                            maskType
)
{
    LwU16                           mask;
    LwU8                            status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if ((xceiverIdx != OSFP_XCEIVER_IDX_ILWALID) &&
        (xceiverIdx >= SOE_SMBPBI_OSFP_NUM_XCEIVERS))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto _smbpbiOsfpCheckPresent_exit;
    }

    mask = maskType == OSFP_MASK_ALL ? pXcvrMask->all : pXcvrMask->present;

    if (mask == 0)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto _smbpbiOsfpCheckPresent_exit;
    }

    if ((xceiverIdx != OSFP_XCEIVER_IDX_ILWALID) &&
        ((LWBIT(xceiverIdx) & mask) == 0))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto _smbpbiOsfpCheckPresent_exit;
    }

_smbpbiOsfpCheckPresent_exit:
    return status;
}

static LwU8
_smbpbiOsfpSendCommand
(
    LwU8    cmd
)
{
    SOE_SMBPBI_OSFP_CMD_FIFO    fifo;
    LwU8                        status;
    FLCN_STATUS                 flcnStatus;

    status = OSFP_READ_VAR(cmdFifo, fifo);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiOsfpSendCommand_exit;
    }

    if (OSFP_FIFO_BYTES_AVAILABLE(&fifo) >= sizeof(cmd))
    {
        osfp_fifo_write_element(&fifo, cmd);

        flcnStatus = soeDmaWriteToSysmem_HAL(&Soe, (LwU8 *)&fifo.prod,
                        Smbpbi.config.dmaHandle,
                        LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,
                                    osfpData.cmdFifo.prod),
                        sizeof(fifo.prod));
        status = smbpbiMapSoeStatusToMsgBoxStatus(flcnStatus);
    }
    else
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_AGAIN;
    }

_smbpbiOsfpSendCommand_exit:
    return status;
}
