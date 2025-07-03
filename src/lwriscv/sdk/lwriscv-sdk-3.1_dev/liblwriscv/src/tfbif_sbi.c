/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    tfbif_sbi.c
 * @brief   TFBIF driver - SBI variant. Calls SK to configure TFBIF.
 */

#include <stddef.h>
#include <lwmisc.h>

#include <liblwriscv/tfbif.h>
#include <lwriscv/sbi.h>

LWRV_STATUS
tfbif_configure_aperture
(
    const tfbif_aperture_cfg_t *p_cfgs,
    uint8_t num_cfgs
)
{
    LWRV_STATUS err = LWRV_OK;
    SBI_RETURN_VALUE sbi_ret;
    uint8_t idx;

    if (p_cfgs == NULL)
    {
        err = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    for (idx = 0; idx < num_cfgs; idx++)
    {
        if (p_cfgs[idx].aperture_idx >= TFBIF_NUM_APERTURES)
        {
            err = LWRV_ERR_ILWALID_ARGUMENT;
            goto out;
        }

        sbi_ret = sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_TFBIF_TRANSCFG_SET,
            (uint64_t)p_cfgs[idx].aperture_idx , (uint64_t)p_cfgs[idx].swid);
        if (sbi_ret.error != SBI_SUCCESS)
        {
            err = LWRV_ERR_ILWALID_ARGUMENT;
            goto out;
        }

        sbi_ret = sbicall3(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_TFBIF_REGIONCFG_SET,
            (uint64_t)p_cfgs[idx].aperture_idx , (uint64_t)p_cfgs[idx].b_vpr, (uint64_t)p_cfgs[idx].aperture_id);
        if (sbi_ret.error != SBI_SUCCESS)
        {
            err = LWRV_ERR_ILWALID_ARGUMENT;
            goto out;
        }
    }

out:
    return err;
}
