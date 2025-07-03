/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2009, 2014-2018, 2020 by LWPU Corporation. All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef ZLWLL_H
#define ZLWLL_H

#include "mdiag/utils/types.h"
#include "mdiag/utils/surf_types.h"

class GpuSubdevice;
class LWGpuSubChannel;
class ArgReader;
class SmcEngine;

class ZLwll
{
public:
    ZLwll() : m_Params(0), m_ZLwllRam(nullptr) {}
    ~ZLwll() {}

    UINT32 SanityCheck(UINT32* zbuffer, MdiagSurf* z, LW50Raster* pRaster,
                       UINT32 w, UINT32 startY, UINT32 h, AAmodetype aa, bool check,
                       LWGpuSubChannel *subch, UINT32 gpuSubdevIdx, LWGpuResource *pGpuRes);
    void   PrintZLwllInfo(const set<UINT32> &ids, UINT32 gpuSubdevIdx,
        LWGpuResource *pGpuRes, LwRm* pLwRm, SmcEngine* pSmcEngine) const;
    void   SetZlwllValidBit(const set<UINT32> &ids, LWGpuSubChannel *subch, UINT32 gpuSubdevIdx, LWGpuResource *pGpuRes);

    void SetParams(const ArgReader *params) { m_Params = params; }
    const UINT32* Image() const { return (m_Image.size() > 0) ? &m_Image[0] : 0; }

private:
    MdiagSurf* GetZLwllStorage(GpuSubdevice *pSubdev, LwRm::Handle hVASpace, LwRm *pLwRm, SmcEngine* pSmcEngine);
    RC ReadZLwllRam(UINT32 region, LWGpuSubChannel *subch, GpuSubdevice *pSubdev, LwRm *pLwRm);
    void loadGpcMap(UINT32  *gpcMap, LWGpuSubChannel *subch, UINT32 gpuSubdevIdx, LWGpuResource *pGpuRes);
    void loadZlwllBankMap(UINT32  *zlwll_bank_in_gpc_number_map, LWGpuSubChannel *subch,UINT32 gpuSubdevIdx, LWGpuResource *pGpuRes);

    vector<UINT32> m_Image;
    const ArgReader* m_Params;
    unique_ptr<MdiagSurf> m_ZLwllRam;
};

#endif /* ZLWLL_H */
