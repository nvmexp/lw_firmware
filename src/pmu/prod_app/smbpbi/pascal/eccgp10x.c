/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2018  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    eccgf10x.c
 * @brief   PMU HAL functions for GF10X+, handling SMBPBI queries for
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
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Handler for the LW_MSGBOX_CMD_OPCODE_GET_ECC_V4 command.
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
smbpbiEccV4Query_GP10X
(
    LwU32   cmd,
    LwU32  *pData
)
{
    LwU8    part;
    LwU8    slice;
    LwU8    subpart;
    LwU8    gpc;
    LwU8    tpc;
    LwU8    tex;
    LwU8    ent;
    LwU16   offset;
    struct  {
        inforom_U008 sbe;
        inforom_U008 dbe;
    }       cnt8;
    LwU8    status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1, _ECC_V4))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_SEL, cmd))
    {
        case LW_MSGBOX_CMD_ARG1_ECC_V4_SEL_CNT:
        {
            switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_CNT_DEV, cmd))
            {
                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_DEV_FB:
                {
                    part = DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V4_FB_PARTITION, cmd);
                    if (part >= INFOROM_ECC_OBJECT_V4_10_FB_PARTITONS)
                    {
                        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                        break;
                    }
                    switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_CNT_FB_SRC, cmd))
                    {
                        case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_FB_SRC_LTC:
                        {
                            slice = DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V4_FB_SLICE, cmd);
                            if (slice >= INFOROM_ECC_OBJECT_V4_10_FB_SLICES)
                            {
                                status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                                break;
                            }
                            switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_CNT_TYPE, cmd))
                            {
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_SBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.fbEcc[part].ltc[slice].sbe);
                                    break;
                                }
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_DBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.fbEcc[part].ltc[slice].dbe);
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
                        case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_FB_SRC_FB:
                        {
                            subpart = DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V4_FB_SUBPARTITION, cmd);
                            if (subpart >= INFOROM_ECC_OBJECT_V4_10_FB_SUBPARTITIONS)
                            {
                                status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                                break;
                            }
                            switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_CNT_TYPE, cmd))
                            {
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_SBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.fbEcc[part].fb[subpart].sbe);
                                    break;
                                }
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_DBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.fbEcc[part].fb[subpart].dbe);
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
                        default:
                        {
                            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                            break;
                        }
                    }
                    break;
                }
                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_DEV_GR:
                {
                    gpc = DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V4_GR_GPC, cmd);
                    tpc = DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V4_GR_TPC, cmd);
                    if ((gpc >= INFOROM_ECC_OBJECT_V4_10_GR_GPCS) ||
                        (tpc >= INFOROM_ECC_OBJECT_V4_10_GR_TPCS))
                    {
                        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                        break;
                    }
                    switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_CNT_GR_SRC, cmd))
                    {
                        case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_GR_SRC_SHM:
                        {
                            switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_CNT_TYPE, cmd))
                            {
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_SBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.grEcc[gpc][tpc].shm.sbe);
                                    break;
                                }
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_DBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.grEcc[gpc][tpc].shm.dbe);
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
                        case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_GR_SRC_RF:
                        {
                            switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_CNT_TYPE, cmd))
                            {
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_SBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.grEcc[gpc][tpc].rf.sbe);
                                    break;
                                }
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_DBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.grEcc[gpc][tpc].rf.dbe);
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
                        case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_GR_SRC_TEX:
                        {
                            tex = DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V4_GR_TEX, cmd);
                            //
                            // Following check is dead code.
                            //
                            // if (tex >= INFOROM_ECC_OBJECT_V4_10_GR_TEXS)
                            // {
                            //     status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                            //    break;
                            // }
                            //
                            switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_CNT_TYPE, cmd))
                            {
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_SBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.grEcc[gpc][tpc].tex[tex].sbe);
                                    break;
                                }
                                case LW_MSGBOX_CMD_ARG1_ECC_V4_CNT_TYPE_DBE:
                                {
                                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                                        ECC.object.v4.grEcc[gpc][tpc].tex[tex].dbe);
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
                        default:
                        {
                            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                            break;
                        }
                    }
                    break;
                }
                default:
                {
                    status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                    break;
                }
            }

            if (status == LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                status = smbpbiHostMemRead_INFOROM_GF100(offset,
                                                   sizeof(*pData), (LwU8 *)pData);
            }

            break;
        }
        case LW_MSGBOX_CMD_ARG1_ECC_V4_SEL_FBA:
        {
            part = DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V4_FB_PARTITION, cmd);
            subpart = DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V4_FB_SUBPARTITION, cmd);
            if ((part >= INFOROM_ECC_OBJECT_V4_10_FB_PARTITONS) ||
                (subpart >= INFOROM_ECC_OBJECT_V4_10_FB_SUBPARTITIONS))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
                break;
            }

            if (FLD_TEST_DRF(_MSGBOX, _CMD, _ARG1_ECC_V4_FBA_SC, _IND, cmd))
            {
                // individual addresses
                ent = DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V4_FBA_IND_INDEX, cmd);
                //
                // Following check is dead code.
                //
                // if (ent >= INFOROM_ECC_OBJECT_V4_10_ADDRESS_ENTRIES)
                // {
                //     status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
                //     break;
                // }
                //
                if (FLD_TEST_DRF(_MSGBOX, _CMD, _ARG1_ECC_V4_FBA_IND_FLD, _ADDR, cmd))
                {
                    // the address itself
                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                             ECC.object.v4.fbAddresses[part][subpart].addresses[ent].fbAddress);
                    status = smbpbiHostMemRead_INFOROM_GF100(offset,
                                                       sizeof(*pData), (LwU8 *)pData);
                }
                else // FLD_TEST_DRF(_MSGBOX, _CMD, _ARG1_ECC_V4_FBA_IND_FLD, _CTRS, cmd)
                {
                    // individual per address counters
                    offset = LW_OFFSETOF(INFOROMHALINFO_FERMI,
                             ECC.object.v4.fbAddresses[part][subpart].addresses[ent].sbe);
                    status = smbpbiHostMemRead_INFOROM_GF100(offset,
                                                       sizeof(cnt8), (LwU8 *)&cnt8);
                    if (status == LW_MSGBOX_CMD_STATUS_SUCCESS)
                    {
                        *pData = DRF_NUM(_MSGBOX, _DATA, _ECC_CNT_8BIT_DBE, cnt8.dbe) |
                                 DRF_NUM(_MSGBOX, _DATA, _ECC_CNT_8BIT_SBE, cnt8.sbe);
                    }
                }
            }
            else // (FLD_TEST_DRF(_MSGBOX, _CMD, _ARG1_ECC_V4_FBA_SC, _LWMUL, cmd))
            {
                status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
            }
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
