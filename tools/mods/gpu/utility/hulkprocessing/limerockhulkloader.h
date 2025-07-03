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
#include "hulkloader.h"

namespace LwLinkDevIf
{
    class LimerockDev;
}

/**
 * Implements HulkLoader template. See HulkLoader for a description of the interface HulkLoader provides to its subclasses.
 */
class LimerockHulkLoader final :
    public HulkLoader
{
    LwLinkDevIf::LimerockDev *m_limerockDev;
public:
    explicit LimerockHulkLoader(LwLinkDevIf::LimerockDev* limerockDev) noexcept 
        : m_limerockDev(limerockDev) 
    { 
    }
protected:
    virtual unique_ptr<FalconImpl> GetFalcon() noexcept override;
    virtual void GetHulkFalconBinaryArray(unsigned char const** pArray, size_t* pArrayLength) noexcept override;
    virtual void GetLoadStatus(UINT32* progress, UINT32* errCode) noexcept override;
};