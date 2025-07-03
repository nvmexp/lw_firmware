/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _IGPUSURFACE_MGR_H_
#define _IGPUSURFACE_MGR_H_

#include "core/include/rc.h"

#include "TestInfo.h"

#include "sim/ITypes.h"
#include "sim/IIface.h"

#include "surf_types.h"
#include "surfutil.h"
#include "IGpuVerif.h"

#include "core/include/cmdline.h"
#include "ITestStateReport.h"

#define USE_DEFAULT_RM_MAPPING  (UINT32)~0

// Nope, these are not all just 1 apart from one another...

const SurfaceType ColorSurfaceTypes[MAX_RENDER_TARGETS] =
{
    SURFACE_TYPE_COLORA,
    SURFACE_TYPE_COLORB,
    SURFACE_TYPE_COLORC,
    SURFACE_TYPE_COLORD,
    SURFACE_TYPE_COLORE,
    SURFACE_TYPE_COLORF,
    SURFACE_TYPE_COLORG,
    SURFACE_TYPE_COLORH,
};

class Rect {
public:
    Rect(UINT32 xmin, UINT32 xmax, UINT32 ymin, UINT32 ymax) : m_xmin(xmin), m_xmax(xmax), m_ymin(ymin), m_ymax(ymax)
    {};

    UINT32 m_xmin;
    UINT32 m_xmax;
    UINT32 m_ymin;
    UINT32 m_ymax;
};

// This structure is used to store information about a
// partial mapping of a color/z surface as specified
// by a -map_region command-line argument.
struct surfaceMapData
{
    MdiagSurf physMem;
    MdiagSurf map;
    UINT64 minOffset;
    UINT64 maxOffset;
};
typedef vector<surfaceMapData*> SurfaceMapDatas;

struct PartialMapData
{
    // The x,y coordinates specify a rectangular region of pixels
    // with the first coordinate being the upper left-hand corner
    // (inclusive) and the second coordinate representing the lower
    // right-hand corner (exclusive).
    UINT32 x1;
    UINT32 y1;
    UINT32 x2;
    UINT32 y2;
    UINT32 arrayIndex;
    UINT64 minOffset;
    UINT64 maxOffset;

    SurfaceMapDatas surfaceMapDatas;

    static bool SortCompare(const PartialMapData *mapData1,
        const PartialMapData *mapData2)
    {
        if (mapData1->minOffset < mapData2->minOffset)
        {
            return true;
        }

        if (mapData1->minOffset == mapData2->minOffset)
        {
            if (mapData1->maxOffset < mapData2->maxOffset)
            {
                return true;
            }
        }

        return false;
    }
};

typedef vector<PartialMapData*> PartialMappings;

typedef vector<MdiagSurf *> PagePoolMappings;

class LWGpuChannel;
class BuffInfo;
class ZLwll;
class GpuVerif;

class IGpuSurfaceMgr: public IIfaceObject {
public:
    virtual MdiagSurf *EnableSurface(SurfaceType surfaceType, MdiagSurf *surface,
        bool loopback, vector<UINT32> peerIDs) = 0;
    virtual void DisableSurface(SurfaceType surfaceType) = 0;

    virtual RC AllocateSurfaces(const ArgReader *tparams, BuffInfo*,
                                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) = 0;

    virtual RC AllocateSurface(SurfaceType surfaceType,
        BuffInfo *buffinfo) = 0;

    virtual RC ProcessSurfaceViews(const ArgReader *tparams, BuffInfo*,
        UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) = 0;

    struct SurfaceTypeData
    {
        SurfaceTypeData() : bMapLoopback(false)
        {
            peerIDs.push_back(USE_DEFAULT_RM_MAPPING);
        }
        MdiagSurf surface;
        bool       bMapLoopback;
        vector<UINT32> peerIDs;
    };
    typedef map<SurfaceType, SurfaceTypeData> SurfaceTypeMap;
    virtual void UseTraceSurfaces() = 0;
    virtual RC InitSurfaceParams(const ArgReader *tparams, UINT32 subdev) = 0;

    virtual BufferConfig* GetBufferConfig() const = 0;

    virtual RC SetupForceVisible() = 0;

    virtual RC Clear(const ArgReader *params, GpuVerif* gpuVerif, const RGBAFloat&, const ZFloat&, const Stencil& ) = 0;

    // These methods can be used when surfaces are consistent, so all color and Z surfaces have the same
    // properties.  If this is not the case, then the user needs to use properties off the surface they are targeting.
    virtual Surface::Layout GetGlobalSurfaceLayout() = 0;

    virtual UINT32 GetWidth() = 0;
    virtual UINT32 GetHeight() = 0;
    virtual UINT32 GetTargetDisplayWidth() = 0;
    virtual UINT32 GetTargetDisplayHeight() = 0;
    virtual void SetTargetDisplay(UINT32 w, UINT32 h, UINT32 d, UINT32 ArraySize) = 0;
    virtual bool GetMultiSampleOverride() = 0;
    virtual UINT32 GetMultiSampleOverrideValue() = 0;
    virtual bool GetNonMultisampledZOverride() = 0;
    virtual bool GetAAUsed() const = 0;

    virtual bool IsZlwllEnabled() const = 0;
    virtual bool IsSlwllEnabled() const = 0;

    virtual void SetSurfaceNull(SurfaceType surfType, bool setval,
                                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) = 0;
    virtual void SetSurfaceUsed(SurfaceType surfType, bool setval,
                                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) = 0;
    virtual bool IsSurfaceValid(SurfaceType surfType,
                                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) = 0;

    virtual MdiagSurf *GetSurfaceHead(UINT32 subdev=Gpu::UNSPECIFIED_SUBDEV) = 0;
    virtual MdiagSurf *GetSurfaceNext(UINT32 subdev=Gpu::UNSPECIFIED_SUBDEV) = 0;
    virtual MdiagSurf *GetSurface(SurfaceType surf, UINT32 subdev=Gpu::UNSPECIFIED_SUBDEV) = 0;
    virtual MdiagSurf *GetSurface(UINT32 idtag, UINT32 subdev=Gpu::UNSPECIFIED_SUBDEV) = 0;
    virtual MdiagSurf *GetClipIDSurface() = 0;
    virtual UINT32 GetClipIDBlockHeight() const = 0;

    virtual bool GetForceNull(MdiagSurf *surf) const = 0;
    virtual bool GetValid(MdiagSurf *surf) const = 0;

    virtual UINT32 DoZLwllSanityCheck(UINT32* zbuffer, UINT32 subdev) = 0;
    virtual const UINT32* GetZLwllSanityImage(UINT32 rasterFormat) = 0;

    virtual const char* ColorNameFromColorFormat(UINT32 cformat) = 0;
    virtual const char* ZetaNameFromZetaFormat(UINT32 zformat) = 0;
    virtual ColorUtils::Format FormatFromDeviceFormat(UINT32 format) const = 0;
    virtual UINT32 DeviceFormatFromFormat(ColorUtils::Format format) const = 0;
    virtual LW50Raster* GetRaster(ColorUtils::Format format) const = 0;

    virtual bool IsUserSpecifiedWindowOffset() = 0;
    virtual INT32 GetUnscaledWindowOffsetX() = 0;
    virtual INT32 GetUnscaledWindowOffsetY() = 0;
    virtual UINT32 ScaleWidth(float data, float isOffset, float offset) const = 0;
    virtual UINT32 ScaleHeight(float data, float isOffset, float offset) const = 0;

    virtual UINT32 EnableRawFBAccess(_RAWMODE mode) = 0;
    virtual UINT32 DisableRawFBAccess() = 0;

    virtual ZLwll* GetZLwll() = 0;

    typedef vector<UINT32> SLIScissorSpec;
    virtual bool SetSLIScissorSpec(MdiagSurf *surf, const SLIScissorSpec& sliScissorSpec) = 0;
    virtual SLIScissorSpec GetSLIScissorSpec(MdiagSurf *surf, UINT32 height=0) const = 0;
    virtual SLIScissorSpec AdjustSLIScissorToHeight(const SLIScissorSpec& sliScissorSpec, UINT32 height) const = 0;

    virtual void SetClearInit(bool b) = 0;

    virtual void SetNeedClipID(bool b) = 0;
    virtual bool GetNeedClipID() const = 0;

    virtual bool IsPartiallyMapped(MdiagSurf *surface) const = 0;
    virtual string GetPartialMapCrcString(MdiagSurf *surface) = 0;

    virtual PartialMappings *GetSurfacePartialMappings(MdiagSurf *surface) = 0;

    virtual bool UsingGpuClear() const = 0;
};

#endif // _IGPUSURFACE_MGR_H_
