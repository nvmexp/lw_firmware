/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SURFMGR_H
#define INCLUDED_SURFMGR_H

#include <map>
#include <vector>

#include "mdiag/utils/types.h"
#include "mdiag/utils/randstrm.h"
#include "mdiag/utils/surf_types.h"

#include "core/include/cmdline.h"

#include "mdiag/utils/surfutil.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"

#include "mdiag/utils/IGpuVerif.h"

class LWGpuChannel;
class LWGpuSurfaceMgr;
class LWGpuSubChannel;
class LWGpuResource;
class ZLwll;
class GpuVerif;
class Trace3DTest;

class LWGpuSurfaceMgr : public IGpuSurfaceMgr {
public:
    // the channel needs to know which number it is, and how to talk to the HW
    LWGpuSurfaceMgr(Trace3DTest* pTest, LWGpuResource *in_lwgpu, LWGpuSubChannel *in_sch, const ArgReader *params);
    ~LWGpuSurfaceMgr();

    // IIfaceObject Interface
    void AddRef();
    void Release();
    IIfaceObject* QueryIface(IID_TYPE id) { return NULL; }

    virtual MdiagSurf *EnableSurface(SurfaceType surfaceType,
        MdiagSurf *surface, bool loopback, vector<UINT32> peerIDs);
    virtual void DisableSurface(SurfaceType surfaceType);

    virtual RC AllocateSurfaces(const ArgReader *tparams,
        BuffInfo*, UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    virtual RC AllocateSurface(SurfaceType surfaceType, BuffInfo *buffinfo);

    virtual RC ProcessSurfaceViews(const ArgReader *tparams,
        BuffInfo*, UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    void UseTraceSurfaces();
    RC InitSurfaceParams(const ArgReader *tparams, UINT32 subdev);

    // ZCompression is actually turned on/off by a Kelvin method
    int SetupZCompress(MdiagSurf *surf);
    int SetupColorCompress(MdiagSurf *surf);

    // Do a privileged write on behalf of trace_cels
    RC SetupForceVisible();

    RC Clear(const ArgReader *params, GpuVerif* gpuVerif, const RGBAFloat& , const ZFloat&, const Stencil&);

    // These methods can be used when surfaces are consistent, so all color and Z surfaces have the same
    // properties.  If this is not the case, then the user needs to use properties off the MdiagSurf they are targeting.
    Surface2D::Layout GetGlobalSurfaceLayout();

    UINT32 GetWidth() { return m_Width; }
    UINT32 GetHeight() { return m_Height; }
    UINT32 GetTargetDisplayWidth() { return m_TargetDisplayWidth; }
    UINT32 GetTargetDisplayHeight() { return m_TargetDisplayHeight; }
    void SetTargetDisplay(UINT32 w, UINT32 h, UINT32 d, UINT32 ArraySize);
    bool GetMultiSampleOverride() { return m_MultiSampleOverride; }
    UINT32 GetMultiSampleOverrideValue() { return m_MultiSampleOverrideValue; }
    bool GetNonMultisampledZOverride() { return m_NonMultisampledZOverride; }
    bool GetAAUsed() const { return m_AAUsed; }

    bool IsZlwllEnabled() const { return m_ZlwllEnabled; }
    bool IsSlwllEnabled() const { return m_SlwllEnabled; }

    void SetSurfaceNull(SurfaceType surfType, bool setval,
                        UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    void SetSurfaceUsed(SurfaceType surfType, bool setval,
                        UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    bool IsSurfaceValid(SurfaceType surfType,
                        UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    MdiagSurf *GetSurfaceHead(UINT32 subdev=Gpu::UNSPECIFIED_SUBDEV);
    MdiagSurf *GetSurfaceNext(UINT32 subdev=Gpu::UNSPECIFIED_SUBDEV);
    MdiagSurf *GetSurface(SurfaceType surf, UINT32 subdev=Gpu::UNSPECIFIED_SUBDEV);
    MdiagSurf *GetSurface(UINT32 idtag, UINT32 subdev=Gpu::UNSPECIFIED_SUBDEV);
    MdiagSurf *GetClipIDSurface() { return &m_ClipID; }
    UINT32 GetClipIDBlockHeight() const { return m_ClipBlockHeight; }

    void SetForceNull(MdiagSurf *surf, bool forceNull);
    bool GetForceNull(MdiagSurf *surf) const;
    void SetIsActiveSurface(MdiagSurf *surf, bool active);
    bool GetIsActiveSurface(MdiagSurf *surf) const;
    void SetValid(MdiagSurf *surf, bool valid);
    bool GetValid(MdiagSurf *surf) const;
    void SetMapSurfaceLoopback(MdiagSurf *surf, bool loopback, vector<UINT32> peerIDs);
    bool GetMapSurfaceLoopback(MdiagSurf *surf, vector<UINT32>* peerIDs) const;
    virtual bool IsPartiallyMapped(MdiagSurf *surface) const;

    UINT32 DoZLwllSanityCheck(UINT32* zbuffer, UINT32 subdev);
    const UINT32* GetZLwllSanityImage(UINT32 rasterFormat);

    UINT32 ColorFormatFromName(const char *name);
    UINT32 ZetaFormatFromName(const char *name);
    const char* ColorNameFromColorFormat(UINT32 cformat);
    const char* ZetaNameFromZetaFormat(UINT32 zformat);
    ColorUtils::Format FormatFromDeviceFormat(UINT32 format) const;
    UINT32 DeviceFormatFromFormat(ColorUtils::Format format) const;
    LW50Raster* GetRaster(ColorUtils::Format format) const;

    bool IsUserSpecifiedWindowOffset() { return userSpecifiedWindowOffset; }
    INT32 GetUnscaledWindowOffsetX() { return m_UnscaledWOffsetX; }
    INT32 GetUnscaledWindowOffsetY() { return m_UnscaledWOffsetY; }
    UINT32 ScaleWidth(float data, float isOffset, float offset) const;
    UINT32 ScaleHeight(float data, float isOffset, float offset) const;

    UINT32 EnableRawFBAccess(_RAWMODE mode);
    UINT32 DisableRawFBAccess();
    _RAWMODE GetRawImageMode() { return m_rawImageMode; }

    BufferConfig* GetBufferConfig() const;
    ZLwll* GetZLwll() { return m_ZLwll.get(); }

    bool SetSLIScissorSpec(MdiagSurf *surf, const SLIScissorSpec& sliScissorSpec);
    SLIScissorSpec GetSLIScissorSpec(MdiagSurf *surf, UINT32 height=0) const;
    SLIScissorSpec AdjustSLIScissorToHeight(const SLIScissorSpec& sliScissorSpec, UINT32 height) const;

    void SetClearInit(bool b) { m_HasClearInit = b; }
    bool GetClearInit() const { return m_HasClearInit; }

    void SetNeedClipID(bool b) { m_NeedClipID = b; }
    bool GetNeedClipID() const { return m_NeedClipID; }

    virtual RC DoPartialMaps();
    virtual RC RestoreFullMaps();
    virtual string GetPartialMapCrcString(MdiagSurf *surface);

    virtual PagePoolMappings *GetSurfacePagePoolMappings(MdiagSurf *surface);
    virtual PartialMappings *GetSurfacePartialMappings(MdiagSurf *surface);

    virtual bool UsingGpuClear() const {
        return (m_FastClearColor || m_FastClearDepth); }

private:
    UINT32 GetIDTag(MdiagSurf *surf) const;
    MdiagSurf* AddSurface();
    RC AllocateSurface(const ArgReader *tparams, MdiagSurf *pSurf, BuffInfo*);
    RC DoFullMap(MdiagSurf *surface);
    RC Clear2SubDev(const ArgReader *params, GpuVerif* gpuVerif, const RGBAFloat& , const ZFloat&, const Stencil&, UINT32 subdev);
    RC DoPartialMap(MdiagSurf *surface);
    RC GetPartialMappingPageSize(MdiagSurf *surface, UINT64 *pageSize);
    RC GetPartialMappingFromCoords(MdiagSurf *surface, UINT64 pageSize,
        PartialMapData *mapData);
    UINT64 GetBlocklinearOffset(MdiagSurf *surface, UINT32 x, UINT32 y,
        UINT32 z);
    UINT64 GetPitchOffset(MdiagSurf *surface, UINT32 x, UINT32 y, UINT32 z);
    RC RestoreFullMap(MdiagSurf *surface);

    UINT32 Scale(UINT64 size, float data, float isOffset, float offset, UINT64 targetSize) const;

    RC CheckAAMode(AAmodetype mode, const ArgReader *params);
    static bool ValidAAMode(UINT32 mode, const ArgReader * tparams = 0);

    RC ClearHelper(const ArgReader *params, GpuVerif* gpuVerif, bool fc, bool zc, const RGBAFloat& color,
                   const ZFloat& z, const Stencil& stencil, bool SrgbWrite, UINT32 subdev);

    RC HWClearColor
    (
        const ArgReader *params,
        GpuVerif* gpuVerif,
        const ZFloat& z,
        const Stencil& s,
        bool SrgbWrite,
        UINT32 subdev
    );

    RC HWClearDepth
    (
        const ArgReader *params,
        GpuVerif* gpuVerif,
        const RGBAFloat &color,
        const ZFloat &z,
        const Stencil &s,
        bool SrgbWrite,
        UINT32 subdev
    );
    RC ZlwllOnlyClear(const ZFloat &z, const Stencil &s, const MdiagSurf* surf);

    RC BackDoorClear
    (
        const ArgReader *params,
        GpuVerif* gpuVerif,
        MdiagSurf *surf,
        const RGBAFloat &color,
        const ZFloat &z,
        const Stencil &s,
        bool SrgbWrite,
        UINT32 subdev
    );

    void FillLinearGobDepth
    (
        UINT08* Data,
        MdiagSurf* surf,
        const RGBAFloat& color,
        const ZFloat& z,
        const Stencil& s,
        bool SrgbWrite
    );

    RC DumpRawMemory(MdiagSurf *surface, UINT32 size, const ArgReader* params);

    RC WriteSurface(const ArgReader *params, GpuVerif* gpuVerif, MdiagSurf& surf, UINT08* data, size_t dataSize, UINT32 subdev, bool useCE = false);

    RC ZeroSurface(const ArgReader *params, GpuVerif* gpuVerif, MdiagSurf& surf, UINT32 subdev, bool useCE = false);

    Trace3DTest* m_pTest;
    LWGpuResource *lwgpu;
    LWGpuChannel *ch;
    LWGpuSubChannel *m_subch;
    int refCount;

    bool m_FastClearColor;
    bool m_FastClearDepth;
    bool m_SrgbWrite;

    UINT32 m_DefaultWidth;
    UINT32 m_DefaultHeight;
    UINT32 m_DefaultDepth;
    UINT32 m_DefaultArraySize;
    UINT32 m_TargetDisplayWidth;
    UINT32 m_TargetDisplayHeight;
    UINT32 m_Width;
    UINT32 m_Height;

    bool m_ZlwllEnabled;
    bool m_SlwllEnabled;
    bool m_MultiSampleOverride;
    UINT32 m_MultiSampleOverrideValue;
    bool m_NonMultisampledZOverride;
    bool m_AAUsed;
    Compression m_ComprMode;
    ZLwllEnum m_ZlwllMode;

    enum PagePoolUsage
    {
        NoPagePool,
        SmallPagePool,
        BigPagePool
    };

    struct SurfaceData
    {
        MdiagSurf surfaces[LW2080_MAX_SUBDEVICES];
        UINT32 numSurfaces;
        bool active;
        bool forceNull;
        bool valid;
        bool loopback;
        vector<UINT32> peerIDs;
        SLIScissorSpec sliScissor;
        MdiagSurf fullMap;

        // Note:  when boost is fully supported in MODS, this should
        // be changed to a ptr_vector so that the memory is automatically
        // freed when the vector goes out of scope.
        PartialMappings partialMaps;

        PagePoolUsage pagePoolUsage;
        PagePoolMappings pagePoolMappings;
    };
    typedef map<UINT32, SurfaceData> SurfaceContainer;
    typedef SurfaceContainer::iterator SurfaceIterator;
    typedef SurfaceContainer::const_iterator ConstSurfaceIterator;
    typedef map<MdiagSurf*, UINT32> IDTagMap;

    MdiagSurf* GetSubSurface(UINT32 subdev, SurfaceData& surfData);
    SurfaceData *GetSurfaceDataByType(SurfaceType surfaceType);
    SurfaceData *GetSurfaceDataBySurface(MdiagSurf *surf);

    SurfaceContainer m_RenderSurfaces;
    SurfaceIterator m_surfaceLwr;
    IDTagMap m_IDTagMap;
    UINT32 m_LastIdTag;

    MdiagSurf m_ClipID;
    string m_ClipIDPteKindName;
    UINT32 m_ClipBlockHeight;

    // Added for window offset extension 6/23/02 cleroy //
    bool userSpecifiedWindowOffset;
    INT32 m_UnscaledWOffsetX;
    INT32 m_UnscaledWOffsetY;

    unique_ptr<ZLwll> m_ZLwll;
    bool m_DoZlwllSanityCheck;
    bool m_IsSliSurfClip;

    _RAWMODE m_rawImageMode;

    bool ForceVisible;
    bool m_HasClearInit;

    UINT32 m_ActiveColorSurfaces;
    bool m_AllColorsFixed;
    bool m_NeedClipID; // Skip ClipID allocation if it's false

    // Page pool functions and data members.
    //
    // Page pools are used to test aliasing multiple virtual address ranges
    // to the same physical memory pages.

    RC AllocPagePool(GpuDevice *pGpuDev, MdiagSurf **pagePool, UINT64 poolSize,
        UINT64 pageSize, Surface2D::Layout layout);
    RC MapSurfaceToPagePool(GpuDevice *pGpuDev, MdiagSurf *virt,
        MdiagSurf *pagePool, UINT64 poolSize, UINT64 pageSize);
    RC MapPage(GpuDevice *pGpuDev, MdiagSurf *virt, MdiagSurf *phys,
        UINT64 size, UINT64 virtOffset, UINT64 physOffset);

    MdiagSurf *m_PoolOfSmallPages;
    MdiagSurf *m_PoolOfBigPages;
    UINT64 m_SmallPagePoolSize;
    UINT64 m_BigPagePoolSize;
    UINT64 m_BigPageSize;
    const ArgReader *m_Params;

    typedef map<const string, LW50Raster*> BuffConfigType;
    BuffConfigType  m_colorBuffConfigs, m_zetaBuffConfigs;

    bool   m_ProgZtAsCt0;
    bool   m_SendRTMethods;
};

#endif
