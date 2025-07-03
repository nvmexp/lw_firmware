/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- Application Includes --------------------------- */
#include "soe_objifr.h"
#include "config/g_soe_hal.h"
#include "main.h"
#include "config/g_ifr_private.h"

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _ifrEventHandle(DISP2UNIT_CMD *pRequest);

/* ------------------------- Global Variables ------------------------------- */
LwU8 localBuffer[SOE_DMA_MAX_SIZE] __attribute__ ((aligned (SOE_DMA_MAX_SIZE)));

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Entry point for the IFR task. This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(task_ifr, pvParameters)
{
    DISP2UNIT_CMD  disp2Ifr;
    RM_FLCN_QUEUE_HDR hdr;
    FLCN_STATUS status;

    for (;;)
    {
        if (OS_QUEUE_WAIT_FOREVER(Disp2QIfrThd, &disp2Ifr))
        {
                RM_FLCN_CMD_SOE *pCmd = disp2Ifr.pCmd;
                switch (pCmd->hdr.unitId)
                {
                    case RM_SOE_UNIT_IFR:
                        status = _ifrEventHandle(&disp2Ifr);
                        osCmdmgmtCmdQSweep(&pCmd->hdr, disp2Ifr.cmdQueueId);
                        if (status == FLCN_OK)
                        {
                            //
                            // Bug 200564601: Ideally we want to be able to transmit
                            // the exact error condition here instead of just
                            // forcing a timeout by not responding.
                            //
                            hdr.unitId  = RM_SOE_UNIT_NULL;
                            hdr.ctrlFlags = 0;
                            hdr.seqNumId = pCmd->hdr.seqNumId;
                            hdr.size = RM_FLCN_QUEUE_HDR_SIZE;
                            osCmdmgmtRmQueuePostBlocking(&hdr, NULL);
                        }
                        break;

                    default:
                        break;
                }
        }
    }
}

/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS
_ifrWriteFileFromSysmem
(
    const char pName[INFOROM_FS_FILE_NAME_SIZE],
    LwU16 offset,
    LwU16 sizeInBytes,
    RM_FLCN_U64 dmaHandle
)
{
    LwU16       lwrrentOffset = offset;
    LwU32       bytesWritten  = 0;
    LwU32       writeSize;
    FLCN_STATUS status        = FLCN_ERROR;
    FS_STATUS   fsStatus      = FS_ERROR;

    if (!g_isInforomInitialized)
    {
        status = initInforomFs();

        if (status != FLCN_OK)
            return status;
        else
            g_isInforomInitialized = LW_TRUE;
    }

    while (bytesWritten < sizeInBytes)
    {
        writeSize = LW_MIN((sizeInBytes - bytesWritten), SOE_DMA_MAX_SIZE);

        memset(localBuffer, 0, SOE_DMA_MAX_SIZE);

        // offset all reads by 4 bytes to make room for status header
        status = soeDmaReadFromSysmem_HAL(&Soe, &localBuffer,
                                            dmaHandle, bytesWritten + sizeof(LwU32),
                                            writeSize);
        if (status != FLCN_OK)
        {
            return status;
        }

        fsStatus = inforomFsWrite(g_pFs, pName, lwrrentOffset, writeSize, localBuffer);
        if (fsStatus != FS_OK)
        {
            goto write_fs_status_and_exit;
        }


        bytesWritten += writeSize;
        lwrrentOffset += writeSize;
    }

write_fs_status_and_exit:
    status = soeDmaWriteToSysmem_HAL(&Soe, &fsStatus, dmaHandle, 0, sizeof(LwU32));

    return status;
}

static FLCN_STATUS
_ifrReadFileToSysmem
(
    const char pName[INFOROM_FS_FILE_NAME_SIZE],
    LwU16 offset,
    LwU16 sizeInBytes,
    RM_FLCN_U64 dmaHandle
)
{
    LwU16       lwrrentOffset = offset;
    LwU32       bytesRead     = 0;
    LwU32       readSize;
    FLCN_STATUS status        = FLCN_ERROR;
    FS_STATUS   fsStatus      = FS_ERROR;

    if (!g_isInforomInitialized)
    {
        status = initInforomFs();

        if (status != FLCN_OK)
            return status;
        else
            g_isInforomInitialized = LW_TRUE;
    }

    while (bytesRead < sizeInBytes)
    {
        readSize = LW_MIN((sizeInBytes - bytesRead), SOE_DMA_MAX_SIZE);

        memset(localBuffer, 0, SOE_DMA_MAX_SIZE);
        fsStatus = inforomFsRead(g_pFs, pName, lwrrentOffset, readSize, localBuffer);

        if (fsStatus != FS_OK)
        {
            goto write_fs_status_and_exit;
        }

        // offset all writes by 4 bytes to make room for status header
        status = soeDmaWriteToSysmem_HAL(&Soe, localBuffer, dmaHandle, bytesRead + sizeof(LwU32),
                                        readSize);

        if (status != FLCN_OK)
        {
            return status;
        }

        bytesRead += readSize;
        lwrrentOffset += readSize;
    }

write_fs_status_and_exit:
    status = soeDmaWriteToSysmem_HAL(&Soe, &fsStatus, dmaHandle, 0, sizeof(LwU32));

    return status;
}

/*!
 * @brief   Helper call handling events sent to IFR task.
 */
static FLCN_STATUS
_ifrEventHandle
(
    DISP2UNIT_CMD *pRequest
)
{
    FLCN_STATUS status = FLCN_OK;
    RM_SOE_IFR_CMD_PARAMS *pParams = &pRequest->pCmd->cmd.ifr.params;

    switch (pRequest->pCmd->cmd.ifr.cmdType)
    {
        case RM_SOE_IFR_READ:
            return _ifrReadFileToSysmem(pParams->fileName, pParams->offset,
                                        pParams->sizeInBytes, pParams->dmaHandle);
        case RM_SOE_IFR_WRITE:
            return _ifrWriteFileFromSysmem(pParams->fileName, pParams->offset,
                                            pParams->sizeInBytes, pParams->dmaHandle);
        default:
            break;
    }

    return status;
}
