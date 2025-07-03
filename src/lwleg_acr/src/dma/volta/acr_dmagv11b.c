/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_dmagv11b.c
 */

//
// Includes
//
#include "acr.h"
#include "dev_fbif_v4.h"
#include "dev_graphics_nobundle.h"
#include "dev_pwr_csb.h"
#include "acr_objacrlib.h"

/*!
 * @brief Program FBIF to allow physical transactions. Incase of GR falcons, make appropriate writes.
 *        Programs TRANSCFG_TARGET to specify where memory is located, if address mode is physical or not.\n
 *        Programs TRANSCFG_MEM_TYPE to indicate FBIF access address mode (virtual or physical).\n
 *        For FECS falcon, program FECS_ARB_CMD_OVERRIDE. This is for overriding memory access command.  Can be any fb_op_t type.  When the command is overriden, all access will be of that command type.\n
 *        Physical writes are not allowed, and will be blocked unless ARB_WPR_CMD_OVERRIDE_PHYSICAL_WRITES is set.\n
 *        Program OVERRIDE CMD to PHYS_SYS_MEM_NONCOHERENT for FECS for gv11b.\n
 *
 * @param[in] pFlcnCfg     Falcon configuration.
 * @param[in] ctxDma       CTXDMA configuration that needs to be setup.
 * @param[in] bIsPhysical  Boolean used to enable physical writes to FB/MEMORY for FECS.
 *
 * @return ACR_OK If CTX DMA setup is successful.
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW If there is a size overflow. 
 */
ACR_STATUS
acrlibSetupCtxDma_GV11B
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32            ctxDma,
    LwBool           bIsPhysical
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;

    CHECK_WRAP_AND_ERROR(ctxDma > LW_U32_MAX / 4U);
#ifndef ACR_SAFE_BUILD
    if (pFlcnCfg->bFbifPresent && (pFlcnCfg->falconId == LSF_FALCON_ID_PMU))
    {
        data = ACR_REG_RD32(CSB, LW_CPWR_FBIF_TRANSCFG(ctxDma));
        data = FLD_SET_DRF(_CPWR, _FBIF_TRANSCFG, _TARGET, _NONCOHERENT_SYSMEM, data);

        if (bIsPhysical)
        {
            data = FLD_SET_DRF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, data);
        }
        else
        {
            data = FLD_SET_DRF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL, data);
        }
        ACR_REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(ctxDma), data);
    }
    else
#endif
    {
        if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS) 
        {
            if (bIsPhysical)
            {
                data = ACR_REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_ARB_WPR);
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_WPR, _CMD_OVERRIDE_PHYSICAL_WRITES, _ALLOWED, data);
                ACR_REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_ARB_WPR, data);
            }

            data = ACR_REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE);
            data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _CMD, _PHYS_SYS_MEM_NONCOHERENT, data);

            if (bIsPhysical) {
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _ON, data);
            } else {
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _OFF, data);
            }
            ACR_REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE, data);
        }
    }

    return status;
}

