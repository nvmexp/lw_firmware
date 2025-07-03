#pragma once

/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "amperehulkloader.h"
#include "gpu/ga100gpu.h"

/**
 * Implements HulkLoader template. See HulkLoader for a description of the interface HulkLoader 
 * provides to its subclasses.
 */
class GA100HulkLoader final : public AmpereHulkLoader
{
public:
    explicit GA100HulkLoader(GA100GpuSubdevice* gpuSubdevice) noexcept 
        : AmpereHulkLoader(gpuSubdevice) 
    { 
    }
protected:
    virtual void GetHulkFalconBinaryArray(unsigned char const** pArray, size_t* pArrayLength) noexcept override final;
    virtual unique_ptr<FalconImpl> GetFalcon() noexcept override;
};