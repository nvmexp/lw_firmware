/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    eccgv10x.c
 * @brief   PMU HAL functions for GV10X+, handling SMBPBI queries for
 *          InfoROM ECC structures.
 */

/* ------------------------ System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"
#include "oob/smbpbi_shared.h"

/* ------------------------ Application Includes --------------------------- */
#include "task_therm.h"
#include "dbgprintf.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
static LwU8 s_readEccV5Counts_GV10X(LwU32 cmd, LwU32 *pData);
static LwU8 s_readEccV5ErrBuffer_GV10X(LwU32 cmd, LwU32 *pData);

/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Handler for the LW_MSGBOX_CMD_OPCODE_GET_ECC_V5 command.
 * Reads the contents of ECC counters, as specified in ARG.
 *
 * @param[in]     cmd
 *     MSGBOX command, as received from the requestor.
 *
 * @param[in,out] pData
 *     Pointer to DATA dword supplied by requester.  Will populate this dword
 *     with the requested information.
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     Successfully serviced request.
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED
 *     ECC data not available
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     Error in request arguments
 */
LwU8
smbpbiEccV5Query_GV10X
(
    LwU32   cmd,
    LwU32  *pData
)
{
    LwU8 status;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1, _ECC_V5))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V5_SEL, cmd))
    {
        case LW_MSGBOX_CMD_ARG1_ECC_V5_SEL_COUNTS:
        {
            status = s_readEccV5Counts_GV10X(cmd, pData);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_ECC_V5_SEL_ERR_BUFFER:
        {
            status = s_readEccV5ErrBuffer_GV10X(cmd, pData);
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

/* ------------------------ Static Functions  ------------------------------ */

static LwU8
s_readEccV5Counts_GV10X
(
    LwU32   cmd,
    LwU32  *pData
)
{
    unsigned                                        index =
                (DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V5_COUNTS_INDEX_HI, cmd) << 7) |
                DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V5_COUNTS_INDEX_LO, cmd);
    unsigned                                        elementType =
                DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V5_COUNTS_TYPE, cmd);
    struct INFOROM_ECC_OBJECT_V5_00_ERROR_COUNTER   counter;
    LwU8                                            status =
                LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (index >= INFOROM_ECC_OBJECT_V5_00_ERROR_COUNTER_MAX_COUNT)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto s_readEccV5Counts_GV10X_exit;
    }

    status = smbpbiHostMemRead_INFOROM_GF100(
                LW_OFFSETOF(INFOROMHALINFO_FERMI, ECC.object.v5.errorEntry[index]),
                sizeof(counter), (LwU8 *)&counter);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_readEccV5Counts_GV10X_exit;
    }

    if (elementType == LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_HDR_META)
    {
        *pData = DRF_NUM(_MSGBOX, _DATA, _ECC_V5_COUNT_HEADER, counter.header) |
                 DRF_NUM(_MSGBOX, _DATA, _ECC_V5_ERR_BUF_METADATA, counter.metadata);
    }
    else
    {
        switch (DRF_VAL(_INFOROM_ECC_OBJECT_V5_00, _ERROR, _COUNTER_HEADER_IDENTIFIER,
                        counter.header))
        {
            case LW_INFOROM_ECC_OBJECT_V5_00_ERROR_COUNTER_HEADER_IDENTIFIER_ADDRESS_ENTRY:
            {
                switch (elementType)
                {
                    case LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_ADDR_ADDR:
                    {
                        *pData = counter.counts.addressCount.address;
                        break;
                    }
                    case LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_ADDR_UNCORRECTED_COUNTS:
                    {
                        *pData = counter.counts.addressCount.uncorrectedCounts;
                        break;
                    }
                    case LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_ADDR_CORRECTED_TOTAL:
                    {
                        *pData = counter.counts.addressCount.correctedTotal;
                        break;
                    }
                    case LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_ADDR_CORRECTED_UNIQUE:
                    {
                        *pData = counter.counts.addressCount.correctedUnique;
                        break;
                    }
                    default:
                    {
                        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                        break;
                    }
                }
                break;
            }
            case LW_INFOROM_ECC_OBJECT_V5_00_ERROR_COUNTER_HEADER_IDENTIFIER_REGION_ENTRY:
            {
                switch (elementType)
                {
                    case LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_REGN_CORRECTED_TOTAL:
                    {
                        *pData = counter.counts.regionCount.correctedTotal;
                        break;
                    }
                    case LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_REGN_CORRECTED_UNIQUE:
                    {
                        *pData = counter.counts.regionCount.correctedUnique;
                        break;
                    }
                    case LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_REGN_UNCORRECTED_TOTAL:
                    {
                        *pData = counter.counts.regionCount.uncorrectedTotal;
                        break;
                    }
                    case LW_MSGBOX_CMD_ARG2_ECC_V5_COUNTS_TYPE_REGN_UNCORRECTED_UNIQUE:
                    {
                        *pData = counter.counts.regionCount.uncorrectedUnique;
                        break;
                    }
                    default:
                    {
                        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                        break;
                    }
                }
                break;
            }
            default:
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
                break;
            }
        }
    }

s_readEccV5Counts_GV10X_exit:
    return status;
}

static LwU8
s_readEccV5ErrBuffer_GV10X
(
    LwU32   cmd,
    LwU32  *pData
)
{
    unsigned                                            index =
                DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V5_ERR_BUF_INDEX, cmd);
    unsigned                                            elementType =
                DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V5_ERR_BUF_TYPE, cmd);
    struct INFOROM_ECC_OBJECT_V5_00_SRAM_ERROR_BUFFER   entry;
    LwU8                                                status =
                LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (index >= INFOROM_ECC_OBJECT_V5_00_SRAM_ERROR_BUFFER_COUNT)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
        goto s_readEccV5ErrBuffer_GV10X_exit;
    }

    status = smbpbiHostMemRead_INFOROM_GF100(
                LW_OFFSETOF(INFOROMHALINFO_FERMI, ECC.object.v5.sramErrors[index]),
                sizeof(entry), (LwU8 *)&entry);

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_readEccV5ErrBuffer_GV10X_exit;
    }

    switch (elementType)
    {
        case LW_MSGBOX_CMD_ARG2_ECC_V5_ERR_BUF_TYPE_ERR_TYPE_META:
        {
            *pData = DRF_NUM(_MSGBOX, _DATA, _ECC_V5_ERR_BUF_ERR_TYPE, entry.errorType) |
                     DRF_NUM(_MSGBOX, _DATA, _ECC_V5_ERR_BUF_METADATA, entry.metadata);
            break;
        }
        case LW_MSGBOX_CMD_ARG2_ECC_V5_ERR_BUF_TYPE_TIME_STAMP:
        {
            *pData = entry.timestamp;
            break;
        }
        case LW_MSGBOX_CMD_ARG2_ECC_V5_ERR_BUF_TYPE_ADDR:
        {
            *pData = entry.address;
            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
            break;
        }
    }

s_readEccV5ErrBuffer_GV10X_exit:
    return status;
}

