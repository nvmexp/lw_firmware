/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021  by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_smbpbi_dem.c
 * @brief  SMBus Post-Box Interface. Handling of Driver Event Messages
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsoe.h"
#include "soe_objsmbpbi.h"
#include "smbpbi_shared_lwswitch.h"

/* ------------------------- Types and Enums -------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define DEM_FIFO_PTR(x) ((x) % INFOROM_DEM_OBJECT_V1_00_FIFO_SIZE)
#define DEM_RECORD_SIZE_MAX (sizeof(LW_MSGBOX_DEM_RECORD)   \
                            + LW_MSGBOX_MAX_DRIVER_EVENT_MSG_TXT_SIZE)
#define DEM_READ_VAR(memb, var)                                         \
    smbpbiInforomRead(LW_OFFSETOF(RM_SOE_SMBPBI_INFOROM_DATA,           \
                                    DEM.memb),                          \
                                    sizeof(var),                        \
                                    (LwU8 *)&(var))

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Initialize the DEM mechanism
 *
 * @param[in]   pSmbpbi SMBPBI object
 *
 * @return      void
 */
void
smbpbiInitDem
(
    OBJSMBPBI   *pSmbpbi
)
{
    pSmbpbi->config.cap[LW_MSGBOX_DATA_CAP_4] =
        FLD_SET_DRF(_MSGBOX, _DATA_CAP_4, _GET_DRIVER_EVENT_MSG, _AVAILABLE,
                    pSmbpbi->config.cap[LW_MSGBOX_DATA_CAP_4]);
    DEM_READ_VAR(bValid, pSmbpbi->bDemActive);
}

/*!
 * Check if the DEM event is pending
 *
 * @return  LW_TRUE     if the event is pending
 *          LW_FALSE    otherwise
 */
LwBool
smbpbiDemEventSource(void)
{
    inforom_U016    readOffset;
    inforom_U016    writeOffset;
    LwU8            status;
    LwBool          bRv = LW_FALSE;

    status = DEM_READ_VAR(object.v1.readOffset, readOffset);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiEventSource_exit;
    }

    status = DEM_READ_VAR(object.v1.writeOffset, writeOffset);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiEventSource_exit;
    }

    bRv = readOffset != writeOffset;

smbpbiEventSource_exit:
    return bRv;
}

/*!
 * Move the oldest logged DEM to the scratch memory buffer
 *
 * @param[in]   arg1    unused, MBZ
 * @param[in]   arg2    scratch memory ptr
 *
 * @return      LW_MSGBOX_CMD_STATUS_ERR_ARG1           arg1 not zero
 *              LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE  DEM buffering
 *                                                      inactive or
 *                                                      the FIFO is empty
 *              LW_MSGBOX_CMD_STATUS_ERR_MISC           DMA error
 *              other                                   percolated from
 *                                                      inferiors
 */

LwU8
smbpbiGetDem
(
    LwU8    arg1,
    LwU8    arg2
)
{
    LwU32           dstOffset;
    inforom_U016    readOffset;
    inforom_U016    writeOffset;
    LwU16           rem;
    LwU8            recSize;
    LwU8            buffer[DEM_RECORD_SIZE_MAX];
    LwU8            *bufPtr;
    LwU8            status = LW_MSGBOX_CMD_STATUS_ERR_MISC;

    if (arg1 != 0)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
        goto smbpbiGetDem_exit;
    }

    if (!Smbpbi.bDemActive)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto smbpbiGetDem_exit;
    }

    status = smbpbiScratchPtr2Offset(arg2, SCRATCH_BANK_READ, &dstOffset);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetDem_exit;
    }

    status = DEM_READ_VAR(object.v1.readOffset, readOffset);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetDem_exit;
    }

    status = DEM_READ_VAR(object.v1.writeOffset, writeOffset);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetDem_exit;
    }

    if (readOffset == writeOffset)
    {
        // The FIFO is empty
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto smbpbiGetDem_exit;
    }

    // readOffset points to the oldest message in the FIFO.
    status = DEM_READ_VAR(object.v1.fifoBuffer[readOffset +
                                               LW_OFFSETOF(LW_MSGBOX_DEM_RECORD, recordSize)],
                          recSize);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetDem_exit;
    }

    //
    // Copy the message from the FIFO into the local buffer variable,
    // while handling a possible wrap-around.
    //
    rem = recSize * sizeof(LwU32);
    bufPtr = buffer;

    while (rem > 0)
    {
        LwU16   copySz = LW_MIN(rem, INFOROM_DEM_OBJECT_V1_00_FIFO_SIZE - readOffset);

        status = smbpbiInforomRead(
                    LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,
                                inforomObjects.DEM.object.v1.fifoBuffer[readOffset]),
                    copySz,
                    bufPtr);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetDem_exit;
        }

        rem -= copySz;
        bufPtr += copySz;
        readOffset = DEM_FIFO_PTR(readOffset + copySz);
    }

    // Copy the message from the local buffer into the scratch memory
    status = smbpbiScratchWrite(dstOffset, buffer, recSize * sizeof(LwU32));
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetDem_exit;
    }

    // Write back the read pointer
    status = smbpbiInforomWrite(LW_OFFSETOF(SOE_SMBPBI_SHARED_SURFACE,
                                           inforomObjects.DEM.object.v1.readOffset),
                               sizeof(readOffset), (LwU8 *)&readOffset);

smbpbiGetDem_exit:
    return status;
}
