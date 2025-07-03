/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_DUMP_H
#define INCLUDED_DUMP_H

#include "mdiag/utils/types.h"
#include "mdiag/utils/surfutil.h"
#include <queue>
#include <functional>

class Trace3DTest;
class CachedSurface;
class MdiagSurf;
class ArgReader;
typedef std::priority_queue<UINT32,vector<UINT32>,greater<UINT32> > ImageList_t;
typedef multimap<UINT32, UINT32> HookedMethodMap_t;
typedef HookedMethodMap_t::iterator HookedMethodMapIter;
typedef HookedMethodMap_t::const_iterator HookedMethodMapConstIter;

#define ADD_ATTR_OP(type, name) \
    void Set##name(type u) { m_##name = u; } \
    type Get##name() const { return m_##name; }

class RenderTarget
{
public:
    RenderTarget(Trace3DTest *test, bool b, UINT32 u);
    virtual ~RenderTarget();

    MdiagSurf* GetActive() const;
    MdiagSurf* Create();
    MdiagSurf* Create(MdiagSurf *surf);
    void Delete();
    void SetIsRtt(bool b) { m_IsRtt = b; }
    bool GetIsRtt() const { return m_IsRtt; }
    bool GetIsZeta() const { return m_IsZeta; }
    void SetEnabled(bool b) { m_Enabled = b; }
    bool GetEnabled() const { return m_Enabled; }
    void SetSelect(UINT32 n) { m_SelectedAs = n; }
    UINT32 GetSelect() const { return m_SelectedAs; }
    UINT32 GetMaxSize() const; // Return max size of Standard or Rtt

    virtual RC LoadSurface(); // Load surface when all attributes are ready

    ADD_ATTR_OP(UINT32, OffsetUpper);
    ADD_ATTR_OP(UINT32, OffsetLower);
    ADD_ATTR_OP(UINT32, ColorFormat);
    ADD_ATTR_OP(UINT32, LogBlockWidth);
    ADD_ATTR_OP(UINT32, LogBlockHeight);
    ADD_ATTR_OP(UINT32, LogBlockDepth);
    ADD_ATTR_OP(UINT32, ArrayPitch);
    ADD_ATTR_OP(UINT32, Width);
    ADD_ATTR_OP(UINT32, Height);
    ADD_ATTR_OP(UINT32, Depth);
    ADD_ATTR_OP(UINT32, Layout);
    ADD_ATTR_OP(UINT32, ArraySize);
    ADD_ATTR_OP(bool, WriteEnabled);
    ADD_ATTR_OP(bool, IsSettingDepth);

    ADD_ATTR_OP(bool, OverwriteColorFormat);
    ADD_ATTR_OP(bool, OverwriteLogBlockWidth);
    ADD_ATTR_OP(bool, OverwriteLogBlockHeight);
    ADD_ATTR_OP(bool, OverwriteLogBlockDepth);
    ADD_ATTR_OP(bool, OverwriteArrayPitch);
    ADD_ATTR_OP(bool, OverwriteWidth);
    ADD_ATTR_OP(bool, OverwriteHeight);
    ADD_ATTR_OP(bool, OverwriteDepth);
    ADD_ATTR_OP(bool, OverwriteLayout);
    ADD_ATTR_OP(bool, OverwriteArraySize);

protected:
    Trace3DTest *m_Test;
    MdiagSurf *m_Standard;
    MdiagSurf *m_Rtt;
    bool     m_IsRtt;
    bool     m_IsZeta;
    bool     m_Enabled;
    bool     m_InitialSetup;
    UINT32   m_RtNumber;    // It's fixed to be 0, 1, ..., 7
    UINT32   m_SelectedAs;  // Remapped to be rt#

    // Surface attributes
    UINT32 m_OffsetUpper;
    UINT32 m_OffsetLower;
    UINT32 m_Width;
    UINT32 m_Height;
    UINT32 m_Depth;
    UINT32 m_ColorFormat;
    UINT32 m_LogBlockWidth;
    UINT32 m_LogBlockHeight;
    UINT32 m_LogBlockDepth;
    UINT32 m_ArrayPitch;
    UINT32 m_Layout;
    UINT32 m_ArraySize;
    bool   m_WriteEnabled;
    bool   m_IsSettingDepth;

    // Indicator of using value from pb or SurfMgr in RTT case
    bool m_OverwriteWidth;
    bool m_OverwriteHeight;
    bool m_OverwriteDepth;
    bool m_OverwriteColorFormat;
    bool m_OverwriteLogBlockWidth;
    bool m_OverwriteLogBlockHeight;
    bool m_OverwriteLogBlockDepth;
    bool m_OverwriteArrayPitch;
    bool m_OverwriteLayout;
    bool m_OverwriteArraySize;
};

class BufferDumper
{
public:
    virtual ~BufferDumper();
    void Execute(UINT32 ptr,
                 UINT32 method_offset,
                 CachedSurface *cs,
                 const string &name,
                 UINT32 pbSize,
                 UINT32 class_id);

private:
    virtual bool IsBeginOp(UINT32 class_id, UINT32 op) const = 0;
    virtual bool IsEndOp(UINT32 class_id, UINT32 op) const = 0;
    virtual bool IsSetColorTargetOp(UINT32 class_id, UINT32 op, int *color_num) const = 0;
    virtual bool IsSetZetaTargetOp(UINT32 class_id, UINT32 op) const = 0;
    virtual bool IsSetCtCtrlOp(UINT32 class_id, UINT32 op) const = 0;
    virtual void SetCtCtrl(UINT32 method_offset, CachedSurface *cs, UINT32 ptr, UINT32 pbSize) = 0;
    virtual void SetActiveCSurface(CachedSurface *cs, UINT32 ptr,
        int color_num, UINT32 pbSize) = 0;
    virtual void SetActiveZSurface(CachedSurface *cs, UINT32 ptr, UINT32 pbSize) = 0;

    bool IsHookedOp(UINT32 class_id, UINT32 op) const;

    void DumpSurfaceToFile(const string &file_name_postfix);
    void DumpZlwllToFile(ImageList_t *DumpZlwllList);
    void DumpBufferToFile(CachedSurface *cs, MdiagSurf *dma, const string &name);
    void PrintSurfData(MdiagSurf* psurf, bool surface_z, bool isRTT) const;
    UINT32* Get032Addr(CachedSurface *cs, UINT32 offset, UINT32 gpuSubdevIdx);
    UINT32 GetSize(CachedSurface *cs) const;

protected:
    BufferDumper(Trace3DTest *test, const ArgReader *params);
    MdiagSurf* GetSurface(SurfaceType s, UINT32 subdev) const;
    UINT32 Get032(CachedSurface *cs, UINT32 gpuSubdevIdx, UINT32 offset);
    bool NeedToDumpLwrrBegin();
    bool NeedToDumpLwrrEnd();

    Trace3DTest *m_Test;
    const ArgReader *m_params;

    RenderTarget *m_Ct[MAX_RENDER_TARGETS];
    RenderTarget *m_Zt;
    UINT32    m_CtCount;
    UINT32    m_ZtCount;
    bool m_LogInfoEveryEnd;
    bool m_DumpImageEveryEnd;
    bool m_DumpZlwllEveryEnd;
    bool m_DumpImageEveryBegin;
    vector<UINT32> m_DumpEveryNBegin;
    vector<UINT32> m_DumpEveryNEnd;
    vector<UINT32> m_DumpRangeMinBegin;
    vector<UINT32> m_DumpRangeMaxBegin;
    vector<UINT32> m_DumpRangeMinEnd;
    vector<UINT32> m_DumpRangeMaxEnd;

    // Method count for begin/end method
    UINT32 m_BeginMethodCount;
    UINT32 m_EndMethodCount;

    // Map of methods specified by -dump_every_method
    HookedMethodMap_t m_HookedMethodMap;
};

class BufferDumperFermi: public BufferDumper
{
public:
    BufferDumperFermi(Trace3DTest *test, const ArgReader *params);
    virtual ~BufferDumperFermi();

protected:
    virtual bool IsBeginOp(UINT32 class_id, UINT32 op) const;
    virtual bool IsEndOp(UINT32 class_id, UINT32 op) const;
    virtual bool IsSetColorTargetOp(UINT32 class_id, UINT32 op, int *color_num) const;
    virtual bool IsSetZetaTargetOp(UINT32 class_id, UINT32 op) const;
    virtual bool IsSetCtCtrlOp(UINT32 class_id, UINT32 op) const;
    virtual void SetCtCtrl(UINT32 method_offset, CachedSurface *cs, UINT32 ptr, UINT32 pbSize);
    virtual void SetActiveCSurface(CachedSurface *cs, UINT32 ptr, int color_num, UINT32 pbSize);
    virtual void SetActiveZSurface(CachedSurface *cs, UINT32 ptr, UINT32 pbSize);
};
#endif
