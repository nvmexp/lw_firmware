/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    tfbif.c
 * @brief   TFBIF driver
 *
 * Used to configure TFBIF apertures
 */

#include <stddef.h>
#include <lwmisc.h>

#include <liblwriscv/tfbif.h>
#include <liblwriscv/io.h>

#include <liblwriscv/print.h>

//The register manuals lwrrently don't define these as indexed fields.
//Because of this, we need to define our own. We have asked the hardware
//Folks to provide indexed field definitions in the manuals, so we don't
//need to do this here.
#define LW_PRGNLCL_TFBIF_TRANSCFG_ATT_SWID(i)     ((i)*4U+1U):((i)*4U)
#define LW_PRGNLCL_TFBIF_REGIONCFG_T_VPR(i)       ((i)*4U+3U):((i)*4U+3U)
#define LW_PRGNLCL_TFBIF_REGIONCFG1_T_APERT_ID(i) ((i)*8U+4U):((i)*8U)
#define LW_PRGNLCL_TFBIF_REGIONCFG2_T_APERT_ID(i) (((i)-4U)*8U+4U):(((i)-4U)*8U)

LWRV_STATUS
tfbif_configure_aperture
(
    const tfbif_aperture_cfg_t *p_cfgs,
    uint8_t num_cfgs
)
{
    LWRV_STATUS err = LWRV_OK;
    uint8_t idx;
    uint32_t transcfg, regioncfg, regioncfg1, regioncfg2;

    if (p_cfgs == NULL)
    {
        err = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    transcfg = localRead(LW_PRGNLCL_TFBIF_TRANSCFG);
    regioncfg = localRead(LW_PRGNLCL_TFBIF_REGIONCFG);
    regioncfg1 = localRead(LW_PRGNLCL_TFBIF_REGIONCFG1);
    regioncfg2 = localRead(LW_PRGNLCL_TFBIF_REGIONCFG2);

    for (idx = 0; idx < num_cfgs; idx++)
    {
        if (p_cfgs[idx].aperture_idx >= TFBIF_NUM_APERTURES ||
            p_cfgs[idx].swid >= (1 << DRF_SIZE(LW_PRGNLCL_TFBIF_TRANSCFG_ATT0_SWID)) ||
            p_cfgs[idx].aperture_id >= (1 << DRF_SIZE(LW_PRGNLCL_TFBIF_REGIONCFG1_T0_APERT_ID))
        )
        {
            err = LWRV_ERR_ILWALID_ARGUMENT;
            goto out;
        }

        transcfg = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _TFBIF_TRANSCFG, _ATT_SWID, p_cfgs[idx].aperture_idx, p_cfgs[idx].swid, transcfg);
        regioncfg = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T_VPR, p_cfgs[idx].aperture_idx, p_cfgs[idx].b_vpr, regioncfg);
        if (p_cfgs[idx].aperture_idx <= 3)
        {
            regioncfg1 = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG1, _T_APERT_ID, p_cfgs[idx].aperture_idx, p_cfgs[idx].aperture_id, regioncfg1);
        }
        else
        {
            regioncfg2 = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG2, _T_APERT_ID, p_cfgs[idx].aperture_idx - 4, p_cfgs[idx].aperture_id, regioncfg2);
        }
    }

    localWrite(LW_PRGNLCL_TFBIF_TRANSCFG, transcfg);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG, regioncfg);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG1, regioncfg1);
    localWrite(LW_PRGNLCL_TFBIF_REGIONCFG2, regioncfg2);

out:
    return err;
}
