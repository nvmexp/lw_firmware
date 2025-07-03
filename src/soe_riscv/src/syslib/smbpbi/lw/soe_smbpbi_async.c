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
 * @file   soe_smbpbi_async.c
 * @brief  This contains the SOE SMBPBI server implementation
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsmbpbi.h"
#include "soe_smbpbi_async.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define SCRATCH_MEMORY(index)                  (scratchMemory[index])
#define SCRATCH_MEMORY_START                   ((LwU8 *)scratchMemory)
#define REGISTER_FILE(index)                   (registerFile[index])

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Private Functions ------------------------------ */
sysSYSLIB_CODE static LwU8 _smbpbiDmemScratchRd(LwU16 offset, LwU32 *pData);
sysSYSLIB_CODE static LwU8 _smbpbiDmemScratchWr(LwU16 offset, LwU32 data);
sysSYSLIB_CODE static LwU8 _smbpbiDmemScratchCp(LwU32 dst, LwU32 src, LwU32 szBytes);
sysSYSLIB_CODE static LwU32 _smbpbisrvDmemScratchPageOffset(void);
sysSYSLIB_CODE static LwU8 _smbpbiAsyncRequestPoll(LwU8 arg2, LwU32 *pData);
sysSYSLIB_CODE static LwU8 _smbpbiHandleAsyncRequest(LwU8 arg1, LwU8 arg2, LwU32 *pData);
sysSYSLIB_CODE static LwU8 _smbpbiHandleDeviceDisableRequest(LwU16 flags);
sysSYSLIB_CODE static LwU8 _smbpbiAsyncReqHandleDeviceModeControlSet(LW_MSGBOX_DEVICE_MODE_CONTROL_PARAMS *pParams);

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
sysTASK_DATA LwU32 scratchMemory[LW_MSGBOX_SCRATCH_BUFFER_SIZE_D];
sysTASK_DATA LwU32 registerFile[LW_MSGBOX_REGISTER_FILE_SIZE];

/* ------------------------- Prototypes ------------------------------------- */

/*!
 * @brief Scratch Memory Read Helper
 *
 * @param[in]       offset Dword offset in the scratch memory
 * @param[in, out]  pData  Pointer to the read buffer
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      read successfully
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 *     offset exceeds buffer size
 */
sysSYSLIB_CODE static LwU8
_smbpbiDmemScratchRd
(
    LwU16   offset,
    LwU32  *pData
)
{
    if (offset >= LW_MSGBOX_SCRATCH_BUFFER_SIZE_D)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    *pData = SCRATCH_MEMORY(offset);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Scratch Memory Write Helper
 *
 * @param[in]       offset Dword offset in the scratch memory
 * @param[in]       data   Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      read successfully
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 *     offset exceeds buffer size
 */
sysSYSLIB_CODE static LwU8
_smbpbiDmemScratchWr
(
    LwU16   offset,
    LwU32   data
)
{
    if (offset >= LW_MSGBOX_SCRATCH_BUFFER_SIZE_D)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    SCRATCH_MEMORY(offset) = data;

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief SMBPBI Scratch Copy Helper
 *
 * @param[in] dst      Dst dword offset in scratch memory
 * @param[in] src      Src dword offset in scratch memory
 * @param[in] szBytes  Size in bytes
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 *
 * @return other errors
 */
sysSYSLIB_CODE static LwU8
_smbpbiDmemScratchCp
(
    LwU32   dst,
    LwU32   src,
    LwU32   szBytes
)
{
    // Transfer to byte address to do the copy
    src *= sizeof(LwU32);
    dst *= sizeof(LwU32);

    if ((src + szBytes) > LW_MSGBOX_SCRATCH_BUFFER_SIZE)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_DATA;
    }

    if ((dst + szBytes) > LW_MSGBOX_SCRATCH_BUFFER_SIZE)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    if (((dst >= src) && (dst < (src + szBytes))) ||
        ((src > dst) && (src < (dst + szBytes))))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    memcpy(SCRATCH_MEMORY_START + dst, SCRATCH_MEMORY_START + src, szBytes);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief SMBPBI Get current page offset
 *
 * @return Page offset in dword
 */
sysSYSLIB_CODE static LwU32
_smbpbisrvDmemScratchPageOffset
(
    void
)
{
    return (SCRATCH_MEMORY_START == NULL) ?
        0 : (LW_MSGBOX_SCRATCH_PAGE_SIZE_D * DRF_VAL(
        _MSGBOX, _DATA, _SCRATCH_PAGE_SRC,
        REGISTER_FILE(LW_MSGBOX_CMD_ARG2_REG_INDEX_SCRATCH_PAGE)));
}

/*!
 * @brief SMBPBI Colwert a scratch ptr into the byte offset
 *
 * @param[in]  ptr          scratch ptr
 * @param[in]  bank_type    unused
 * @param[out] pOffset      byte offset into scratch memory
 *
 * @return SMBPBI status
 */
sysSYSLIB_CODE LwU8
smbpbiScratchPtr2Offset
(
    LwU8                ptr,
    SCRATCH_BANK_TYPE   bank_type,
    LwU32               *pOffset
)
{
    *pOffset = ptr * sizeof(LwU32);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief SMBPBI Read/Write a block from/to the scratch memory
 *
 * @param[in]       offset  byte offset into the scratch memory
 * @param[in,out]   pBuf    DMEM buffer
 * @param[in]       size    in bytes
 * @param[in]       bWrite  TRUE: write to scratch, FALSE: read from scratch
 *
 * @return SMBPBI status
 */
sysSYSLIB_CODE LwU8
smbpbiScratchRW
(
    LwU32   offset,
    void    *pBuf,
    LwU32   size,
    LwBool  bWrite
)
{
    LwU8    *pScratch;
    if ((offset >= LW_MSGBOX_SCRATCH_BUFFER_SIZE) ||
        ((offset + size) > LW_MSGBOX_SCRATCH_BUFFER_SIZE))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    pScratch = SCRATCH_MEMORY_START + offset;

    if (bWrite)
    {
        memcpy(pScratch, pBuf, size);
    }
    else
    {
        memcpy(pBuf, pScratch, size);
    }

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Async Request Poll
 *
 * @param[in]  arg2   Async request seq number
 * @param[in] pData   Pointer to the input data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if the state is idle
 *     if arg2 != current seq number
 * @return LW_MSGBOX_CMD_STATUS_ACCEPTED
 *     if the state is in flight
 * @return LW_MSGBOX_CMD_STATUS_MISC
 *     if the state is unknown
 */
sysSYSLIB_CODE static LwU8
_smbpbiAsyncRequestPoll
(
    LwU8  arg2,
    LwU32 *pData
)
{
    LwU8 status;

    if (arg2 != Smbpbi.asyncRequestSeqNo)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    switch (Smbpbi.asyncRequestState)
    {
        case SOE_SMBPBI_ASYNC_REQUEST_STATE_IDLE:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
            break;
        }
        case SOE_SMBPBI_ASYNC_REQUEST_STATE_IN_FLIGHT:
        {
            status = LW_MSGBOX_CMD_STATUS_ACCEPTED;
            break;
        }
        case SOE_SMBPBI_ASYNC_REQUEST_STATE_COMPLETED:
        {
            *pData = Smbpbi.asyncRequestStatus;

            Smbpbi.asyncRequestState = SOE_SMBPBI_ASYNC_REQUEST_STATE_IDLE;
            Smbpbi.asyncRequestSeqNo++;

            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
            break;
        }
    }

    return status;
}

sysSYSLIB_CODE static LwU8
_smbpbiHandleDeviceDisableRequest
(
    LwU16   flags
)
{
    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 4, _SET_DEVICE_DISABLE))
    {
        return LW_MSGBOX_DATA_ASYNC_REQ_STATUS_ERROR_NOT_SUPPORTED;
    }

    //
    // Set scratch register state and unconditionally set the reload required
    // bit. We don't have a good way of reading the canonical (InfoROM) state
    // yet so assume for now that any state changes require a driver reload.
    //
    smbpbiSetDeviceDisableMode_HAL(&Smbpbi, flags);
    Smbpbi.bDriverReloadRequired = LW_TRUE;

    return LW_MSGBOX_DATA_ASYNC_REQ_STATUS_SUCCESS;
}

sysSYSLIB_CODE static LwU8
_smbpbiAsyncReqHandleDeviceModeControlSet
(
    LW_MSGBOX_DEVICE_MODE_CONTROL_PARAMS *pParams
)
{
    LwU16 modeType = pParams->modeType;
    LwU16 flags = pParams->flags;
    LwU8 status = LW_MSGBOX_DATA_ASYNC_REQ_STATUS_SUCCESS;

    switch (modeType)
    {
        case LW_MSGBOX_DEVICE_MODE_CONTROL_PARAMS_MODE_TYPE_DISABLE:
        {
            status = _smbpbiHandleDeviceDisableRequest(flags);
            break;
        }
        default:
        {
            status = LW_MSGBOX_DATA_ASYNC_REQ_STATUS_ERROR_ILWALID_ARGUMENT;
            break;
        }
    }

    return status;
}

/*!
 * @brief Async Request Helper
 *
 * @param[in]  arg1   Route to an async command handler
 * @param[in]  arg2   Poll: Async requset seq number
 *                    Async requests: Dword offset in the page
 * @param[in] pData   Pointer to the input data
 *
 * @return LW_MSGBOX_CMD_STATUS_Accepted
 *     if success to issue the command
 * @return LW_MSGBOX_CMD_STATUS_ERR_BUSY
 *     if state is not idle
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 *     if command is not found/supported
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if arg2 is incorrect @see arg2 above
 *
 * @return other errors
 */
sysSYSLIB_CODE static LwU8
_smbpbiHandleAsyncRequest
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData
)
{
    void  *pParams;
    LwU32 paramsSize  = 0;
    LwU32 paramOffset = arg2;

    if (arg1 == LW_MSGBOX_CMD_ARG1_ASYNC_REQUEST_POLL)
    {
        return _smbpbiAsyncRequestPoll(arg2, pData);
    }

    if (Smbpbi.asyncRequestState != SOE_SMBPBI_ASYNC_REQUEST_STATE_IDLE)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_BUSY;
    }

    // Check if the param is in one page
    paramOffset += _smbpbisrvDmemScratchPageOffset();
    if (paramOffset >= LW_MSGBOX_SCRATCH_PAGE_SIZE_D)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    pParams = &SCRATCH_MEMORY(paramOffset);

    switch(arg1)
    {
        case LW_MSGBOX_CMD_ARG1_ASYNC_REQUEST_DEVICE_MODE_CONTROL:
        {
            paramsSize = sizeof(LW_MSGBOX_DEVICE_MODE_CONTROL_PARAMS);
            if ((paramOffset + paramsSize) > LW_MSGBOX_SCRATCH_PAGE_SIZE_D)
            {
                return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
            }
            Smbpbi.asyncRequestStatus =
                _smbpbiAsyncReqHandleDeviceModeControlSet(pParams);
            break;
        }
        default:
        {
            return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
        }
    }

    // Fake async command:
    // Inform clients commands issued instead of being exlwted already
    Smbpbi.asyncRequestState = SOE_SMBPBI_ASYNC_REQUEST_STATE_COMPLETED;
    *pData = Smbpbi.asyncRequestSeqNo;

    return LW_MSGBOX_CMD_STATUS_ACCEPTED;
}

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief SMBPBI Register Read Helper
 *
 * @param[in]   regIdx  Register ID (index)
 * @param[out]  pData   Pointer to read value
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     If success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     No such register
 */
sysSYSLIB_CODE LwU8
smbpbiRegRd
(
    LwU8    regIdx,
    LwU32  *pData
)
{
    if (regIdx >= LW_MSGBOX_REGISTER_FILE_SIZE)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    *pData = REGISTER_FILE(regIdx);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief SMBPBI Register Write Helper
 *
 * @param[in] regIdx    Register ID (index)
 * @param[in] data      value to be written
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     If success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     No such register
 */
sysSYSLIB_CODE LwU8
smbpbiRegWr
(
    LwU8    regIdx,
    LwU32   data
)
{
    if (regIdx >= LW_MSGBOX_REGISTER_FILE_SIZE)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    REGISTER_FILE(regIdx) = data;

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief SMBPBI Scratch Register Handler
 *
 * @param[in] cmd    Arg1 dst dword offset in scratch memeory
 *                   Arg2 size in bytes
 * @param[in] pData  Src dword offset in scratch memory
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 *
 * @return other errors
 */
sysSYSLIB_CODE LwU8
smbpbiHandleScratchRegAccess
(
    LwU32 cmd,
    LwU32 *pData
)
{
    LwU8   status  = LW_MSGBOX_CMD_STATUS_SUCCESS;;
    LwU8   arg1    = DRF_VAL(_MSGBOX, _CMD, _ARG1, cmd);
    LwU8   arg2    = DRF_VAL(_MSGBOX, _CMD, _ARG2, cmd);
    LwU32  src     = DRF_VAL(_MSGBOX, _DATA, _COPY_SRC_OFFSET, *pData);
    LwU32  dst     = arg1;
    LwU32  szBytes = (arg2 + 1) * sizeof(LwU32);
    LwU32  pageReg;
    LwU32  pageOffsetRead;
    LwU32  pageOffsetWrite;

    if (dst >= LW_MSGBOX_SCRATCH_PAGE_SIZE_D)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    // Only check when it is copy
    if ((DRF_VAL(_MSGBOX, _CMD, _OPCODE, cmd) ==
        LW_MSGBOX_CMD_OPCODE_SCRATCH_COPY) &&
        (src >= LW_MSGBOX_SCRATCH_PAGE_SIZE_D))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_DATA;
    }

    if (smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_SCRATCH_PAGE, &pageReg) !=
        LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        pageReg = 0;
    }

    pageOffsetRead  = DRF_VAL(_MSGBOX, _DATA, _SCRATCH_PAGE_SRC, pageReg) *
                              LW_MSGBOX_SCRATCH_PAGE_SIZE_D;
    pageOffsetWrite = DRF_VAL(_MSGBOX, _DATA, _SCRATCH_PAGE_DST, pageReg) *
                              LW_MSGBOX_SCRATCH_PAGE_SIZE_D;

    src += pageOffsetRead;

    switch (DRF_VAL(_MSGBOX, _CMD, _OPCODE, cmd))
    {
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_READ:
        {
            status = _smbpbiDmemScratchRd(dst + pageOffsetRead, pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_WRITE:
        {
            status = _smbpbiDmemScratchWr(dst + pageOffsetWrite, *pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_COPY:
        {
            status = _smbpbiDmemScratchCp(dst + pageOffsetWrite, src, szBytes);
            break;
        }
    }

    return status;
}

/*!
 * @brief Async Request Handler
 *
 * @param[in] cmd   Command
 * @param[in] pData Pointer to the input data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 *
 * @return other errors
 */
sysSYSLIB_CODE LwU8
smbpbiHandleAsyncRequest
(
    LwU32 cmd,
    LwU32 *pData
)
{
    LwU8 arg1 = DRF_VAL(_MSGBOX, _CMD, _ARG1, cmd);
    LwU8 arg2 = DRF_VAL(_MSGBOX, _CMD, _ARG2, cmd);

    return _smbpbiHandleAsyncRequest(arg1, arg2, pData);
}
