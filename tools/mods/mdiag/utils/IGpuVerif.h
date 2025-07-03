/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _IGPUVERIF_H_
#define _IGPUVERIF_H_

#include "TestInfo.h"

#include "sim/IIface.h"
#include "sim/ITypes.h"

#include "TestInfo.h"

#include "ITestStateReport.h"

class BufferConfig;

class MdiagSurf;
typedef MdiagSurf DmaBuffer;

struct VP2TilingParams;
class BlockLinear;

enum CHECK_METHOD
{
     NO_CHECK = 0,
    CRC_CHECK,
    REF_CHECK
};

class IGpuVerif : public IIfaceObject
{
public:
// IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;

// IGpuVerif
    virtual bool Setup() = 0;
    virtual bool Setup(BufferConfig*) = 0;
    virtual RC AddDmaBuffer(MdiagSurf *pDmaBuf, CHECK_METHOD check, const char *Filename,
                            UINT32 Offset, UINT32 Size, UINT32 Granularity,
                            const VP2TilingParams *pTilingParams,
                            BlockLinear *pBLockLinear, UINT32 Width, UINT32 Height, bool CrcMatch) = 0;
    virtual bool AddCrc(const char* crc) = 0;
    typedef set<const MdiagSurf*> Surfaces;
    virtual TestEnums::TEST_STATUS DoCrcCheck(ITestStateReport *report, UINT32 gpuSubdevice, const Surfaces *skipSurfaces) = 0;

    virtual CrcEnums::CRC_MODE GetCrcCheckState() = 0;
    virtual void SetCrcCheckState(CrcEnums::CRC_MODE mode) = 0;
    virtual bool DumpActiveSurface(MdiagSurf* surface,
            const char* fname,
            UINT32 gpuSubdevice = 0) = 0;
};

#endif // _IGPUVERIF_H_
