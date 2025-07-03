/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "turingfalcon.h"
#include "mods_reg_hal.h"

#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_gsp_tu10x.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_gsp_tu10x_gp102_sig.h"

// This hasn't been defined for GV100, which is taking the value from
// TU10x headers
#define LW_FALCON_GSP_BASE 0x00110000

// ------------------------------------ GSP ----------------------------------------

UINT32 TuringGspFalcon::GetEngineBase()
{
    if (m_pSubdev->DeviceId() >= Gpu::LwDeviceId::TU102)
        return m_pSubdev->Regs().LookupAddress(MODS_FALCON_GSP_BASE);
    else
        return LW_FALCON_GSP_BASE;
}

UINT32* TuringGspFalcon::GetUCodeData() const
{
    return dmva_ucode_data_gsp_tu10x;
}

UINT32* TuringGspFalcon::GetUCodeHeader() const
{
    return dmva_ucode_header_gsp_tu10x;
}

UINT32 TuringGspFalcon::GetPatchLocation() const
{
    return dmva_gsp_sig_patch_location[0];
}

UINT32 TuringGspFalcon::GetPatchSignature() const
{
    return dmva_gsp_sig_patch_signature[0];
}

UINT32* TuringGspFalcon::GetSignature(bool debug) const
{
    if (debug)
        return dmva_gsp_sig_dbg;
    else
        return dmva_gsp_sig_prod;
}

UINT32 TuringGspFalcon::SetPMC(UINT32 pmc, UINT32 state)
{
    return pmc;
}

void TuringGspFalcon::EngineReset()
{
    RegHal &regs = m_pSubdev->Regs();
    regs.Write32(MODS_PGSP_FALCON_ENGINE_RESET_TRUE);
    regs.Write32(MODS_PGSP_FALCON_ENGINE_RESET_FALSE);
}
