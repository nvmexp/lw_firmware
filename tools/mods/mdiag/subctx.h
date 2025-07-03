/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef SUBCTX_H
#define SUBCTX_H

#include <string>
using std::string;
#include "core/include/lwrm.h"
#include "mdiag/utils/types.h"
#include "core/include/massert.h"
#ifdef _WIN32
#   pragma warning(push)
#   pragma warning(disable: 4265) // class has virtual functions, but destructor is not virtual
#endif
#ifdef _WIN32
#   pragma warning(pop)
#endif
#include "mdiag/tests/gpu/trace_3d/pe_configurefile.h"

class TraceTsg;
class LWGpuResource;
class VaSpace;
typedef map<UINT32, UINT32> ValidPLs;
#define UNINITIALIZED_WATERMARK_VALUE _UINT32_MAX

class SubCtx
{
public:
    SubCtx(LWGpuResource * pGpuRes,
           LwRm* pLwRm,
           UINT32 testId,
           string name);
    ~SubCtx();
    LwRm::Handle GetHandle();
    RC SetVaSpace(shared_ptr<VaSpace> pVaSpace);
    shared_ptr<VaSpace> GetVaSpace() const { return m_pVaSpace; }
    RC SetTraceTsg(TraceTsg * pTraceTsg);
    TraceTsg* GetTraceTsg() const { return m_pTraceTsg; }
    string GetName() const { return m_Name; }
    void SetVeid(UINT32 veid) { m_Veid = veid; MASSERT(IsVeidValid()); }
    UINT32 GetVeid() const { return m_Veid; }
    bool IsVeidValid() const { return m_Veid < VEID_END; }
    RC SetSpecifiedVeid(UINT32 veid);
    UINT32 GetSpecifiedVeid() const { return m_SpecifiedVeid; }
    bool IsSpecifiedVeid() const { return m_SpecifiedVeid < VEID_END; }
    LWGpuResource * GetGpuResource() const { return m_pGpuResource; }
    LwRm* GetLwRmPtr() const { return m_pLwRm; }

    RC SetGrSubContext();
    RC SetI2memSubContext();
    bool IsGrSubContext() const { return m_IsGrSubContext; }
    bool IsI2memSubContext() const { return m_IsI2memSubContext; }
    RC SetTpcMask(const vector<UINT32> & tpcMask);
    ValidPLs::const_iterator Begin() const { return m_ValidPLs.begin(); }
    ValidPLs::const_iterator End() const { return m_ValidPLs.end(); }
    void SetTestId(UINT32 id) { m_TestId = id; }
    UINT32 GetTestId() const { return m_TestId; }
    void SetMaxTpcCount(UINT32 maxTpcCount) { m_MaxTpcCount = maxTpcCount; }
    UINT32 GetMaxTpcCount() { return m_MaxTpcCount; }
    void SetMaxSingletonTpcGpcCount(UINT32 maxSingletonTpcGpcCount) { m_MaxSingletonTpcGpcCount = maxSingletonTpcGpcCount; }
    UINT32 GetMaxSingletonTpcGpcCount() { return m_MaxSingletonTpcGpcCount; }
    void SetMaxPluralTpcGpcCount(UINT32 maxPluralTpcGpcCount) { m_MaxPluralTpcGpcCount = maxPluralTpcGpcCount; }
    UINT32 GetMaxPluralTpcGpcCount() { return m_MaxPluralTpcGpcCount; }
    bool IsSetTpcMask() { return m_IsSetTpcMask; }
    void SetWatermark(UINT32 watermark) { m_Watermark = watermark; }
    UINT32 GetWatermark() { return m_Watermark; }
    void SetWatermarkFlag(bool flag) { m_HasSetWatermark = flag; }
    bool HasSetWatermarkFlag() {return m_HasSetWatermark; }

    static const UINT32 VEID_END= 64;
private:
    void ConfigValidPL();
private:
    LWGpuResource * m_pGpuResource;
    LwRm* m_pLwRm;
    UINT32 m_TestId;
    string m_Name;
    LwRm::Handle m_Handle;
    shared_ptr<VaSpace> m_pVaSpace;
    // ToDo: For shared tsg, this pTraceTsg just keep the last traceTsg Info.
    TraceTsg * m_pTraceTsg;
    UINT32 m_Veid;
    vector<UINT32> m_TpcMasks;
    ValidPLs m_ValidPLs;
    bool m_IsGrSubContext = false;
    bool m_IsI2memSubContext = false;
    UINT32 m_SpecifiedVeid;
    bool m_IsSetTpcMask;
    UINT32 m_MaxTpcCount;
    UINT32 m_MaxSingletonTpcGpcCount;
    UINT32 m_MaxPluralTpcGpcCount;
    UINT32 m_Watermark;
    bool m_HasSetWatermark;
};
#endif

