/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ga100hulkloader.h"
#include "gpu/utility/gm20xfalcon.h"

 // HULK ucodes
#include "gpu/ucode/hulk_ga100.enc_dbg.h"

void GA100HulkLoader::GetHulkFalconBinaryArray(
    unsigned char const** pArray, 
    size_t* pArrayLength) noexcept
{
    //TODO: Check debug flag and return release binary if appropriate
    *pArray = bin_hs_ga100_hulk_enc_dbg_bin;
    *pArrayLength = bin_hs_ga100_hulk_enc_dbg_bin_len;
}

unique_ptr<FalconImpl> GA100HulkLoader::GetFalcon() noexcept
{
    constexpr GM20xFalcon::FalconType falconType = GM20xFalcon::FalconType::SEC;    
    return make_unique<GM20xFalcon>(m_pGpuSubdevice, falconType); //GA100 uses Maxwell falcons
}