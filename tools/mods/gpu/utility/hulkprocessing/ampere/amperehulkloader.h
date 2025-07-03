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
#include "gpu/utility/hulkprocessing/hulkloader.h"

class AmpereGpuSubdevice;

/**
 * Implements HulkLoader template. See HulkLoader for a description of the interface HulkLoader 
 * provides to its subclasses.
 */
class AmpereHulkLoader : public HulkLoader
{
public:
    explicit AmpereHulkLoader(AmpereGpuSubdevice* pGpuSubdevice) noexcept 
        : m_pGpuSubdevice(pGpuSubdevice) 
    { 
    }
protected:
    AmpereGpuSubdevice* const m_pGpuSubdevice;
    virtual RC DoSetup() override final;
    virtual void GetLoadStatus(UINT32* pProgress, UINT32* pErrorCod) noexcept override final;
private:
    RC WaitForScrubbing();
};