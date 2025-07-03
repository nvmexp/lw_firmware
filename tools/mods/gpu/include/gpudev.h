/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GPUDEV_H
#define INCLUDED_GPUDEV_H

#ifndef INCLUDE_GPU
#ifndef INCLUDED_GPU_STUB_H
    #include "stub/gpu_stub.h"
#endif // INCLUDED_GPU_STUB_H

#else

#ifndef INCLUDED_MEMTYPES_H
#include "core/include/memtypes.h"
#endif

#ifndef INCLUDED_GPU_H
#include "core/include/gpu.h"
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#include "gpusbdev.h"
#include "core/include/setget.h"
#include "class/cl2080.h" // LW2080_MAX_SUBDEVICES
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"

class Channel;
class GpuTestConfiguration;
class DisplayMgr;
class Runlist;
class UserdManager;
class Display;
class ProtectedRegionManager;

void SetupSimulatedEdids(UINT32 GpuInstance);

class GpuDevice : public Device
{
  public:
    static constexpr UINT32 ILWALID_DEVICE = 0xffffffff;

    GpuDevice();
    virtual ~GpuDevice();

    virtual RC Initialize();
    virtual RC Initialize(UINT32 numUserdAllocs, Memory::Location userdLoc);
    virtual RC Initialize(UINT32 numUserdAllocs, Memory::Location userdLoc, UINT32 devInst);
    virtual RC Shutdown();

    RC SetDeviceInst(UINT32 devInst);
    UINT32 GetDeviceInst() const { return m_Device; }

    UINT32 GetNumSubdevices() const;
    GpuSubdevice *GetSubdevice(UINT32 subdev) const;
    RC AddSubdevice(GpuSubdevice *gpuSubdev, UINT32 loc);

    UINT32 FixSubdeviceInst(UINT32 subdevInst);
    UINT32 GetDisplaySubdeviceInst() const;
    GpuSubdevice *GetDisplaySubdevice() const;

    virtual Display *GetDisplay() { return m_pDisplay; }

    DisplayMgr *GetDisplayMgr() const { return m_pDisplayMgr; }
    UserdManager *GetUserdManager() const { return m_pUserdManager; }
    RC GetProtectedRegionManager(ProtectedRegionManager **manager);

    enum GpuFamily
    {
        None
        ,Celsius
        ,Rankine
        ,Lwrie
        ,Tesla
        ,Fermi
        ,Kepler
        ,Maxwell
        ,Pascal
        ,Volta
        ,Turing
        ,Ampere
        ,Ada
        ,IG00X
        ,Hopper
        ,Blackwell
        ,G00X
        ,NUM_GpuFamily
    };

    //!
    //! \brief Get the GPU family. Primary mechanism of getting the GPU family.
    //!
    //! \see GetFamilyFromDeviceId See for situational alternative. For use when
    //! \a m_Family has not been set.
    //!
    GpuFamily GetFamily() const { return m_Family; }

    //!
    //! \brief Get the GPU family from the device ID. For use only when the GPU
    //! family has not yet been determined in the GPU initialization sequence.
    //!
    //! \see GetFamily Use this instead where possible.
    //!
    GpuFamily GetFamilyFromDeviceId() const;

    enum CapsBits
    {
        ILWALID_CAP           = 0,
        AA_LINES              = 1,
        AA_POLYS              = 2,
        LOGIC_OPS             = 3,
        TWOSIDED_LIGHTING     = 4,
        QUADRO_GENERIC        = 5,
        TURBOCIPHER_SUPPORTED = 6,
        BUG_114871            = 7,
        BUG_115115            = 8,
        RENDER_TO_SYSMEM      = 9,
        SYSMEM_COMPRESSION    = 10,
        TITAN                 = 11,
    };

    RC LoadGrCaps();
    RC LoadHostCaps();
    RC LoadFbCaps();
    RC LoadCaps();
    bool CheckGrCapsBit(CapsBits bit);
    bool CheckHostCapsBit(CapsBits bit);
    bool CheckFbCapsBit(CapsBits bit);
    bool CheckCapsBit(CapsBits bit);
    void PrintCaps();

    virtual Gpu::LwDeviceId DeviceId() const;

    RC AllocClientStuff(); // formerly RmInitClient
    RC FreeClientStuff();  // formerly RmDestroyClient

    UINT32 GetFbMem(LwRm *pLwRm = 0);
    RC SetFbCtxDma(LwRm::Handle CtxDmaHandle, LwRm *pLwRm = 0);
    UINT32 GetFbCtxDma(LwRm *pLwRm = 0);
    UINT32 GetGartCtxDma(LwRm *pLwRm = 0);
    UINT32 GetMemSpaceCtxDma(Memory::Location Loc, bool ForcePhysical,
                             LwRm *pLwRm = 0);
    RC AllocFlaVaSpace();
    LwRm::Handle GetVASpace(UINT32 vaSpaceClass) const
    {
        return GetVASpace(vaSpaceClass, 0, 0);
    }
    LwRm::Handle GetVASpace(UINT32 vaSpaceClass, LwRm* pLwRm) const
    {
        return GetVASpace(vaSpaceClass, 0, pLwRm);
    }
    LwRm::Handle GetVASpace(UINT32 vaSpaceClass, UINT32 index, LwRm* pLwRm) const;
    UINT32 GetVASpaceClassByHandle(LwRm::Handle vaSpaceHandle) const;
    bool SupportsRenderToSysMem();
    void FreeNonEssentialAllocations();

    virtual bool IsStrictSku();
    virtual bool HasBug(UINT32 bugNum) const;

    //
    // Gob Information
    //
    // m_GobHeight is initialized to 0 in the constructor (meaning unset).
    // If set to zero when getting Height or Size, use Subdevice values,
    //    otherwise get the value reflecting the m_GobHeight (manually set).
    void SetGobHeight(UINT32 newHeight) { m_GobHeight = newHeight; }

    //! Number of bytes (=scan lines) in a Gob vertically
    UINT32 GobHeight() const { return (m_GobHeight ? m_GobHeight :
                                        GetDisplaySubdevice()->GobHeight()); }
    //! Number of bytes in a Gob horizontally
    UINT32 GobWidth() const { return GetDisplaySubdevice()->GobWidth(); }
    //! Number of bytes (=layers) in a Gob in the third dimension
    UINT32 GobDepth() const { return GetDisplaySubdevice()->GobDepth(); }
    //! Number of bytes in a Gob
    UINT32 GobSize() const { return (m_GobHeight ? m_GobHeight * GobWidth() * GobDepth() :
                                        GetDisplaySubdevice()->GobSize()); }
    //! Number of bytes in a horizontal word of a Gob
    UINT32 GobWord() const { return GetDisplaySubdevice()->GobWord(); }

    //
    // Tile Information
    //
    //! Number of bytes (=layers) in a Tile vertically
    UINT32 TileHeight() const { return GetDisplaySubdevice()->TileHeight(); }
    //! Number of bytes in a Tile horizontally
    UINT32 TileWidth() const { return GetDisplaySubdevice()->TileWidth(); }
    //! Number of bytes in a Tile
    UINT32 TileSize() const { return GetDisplaySubdevice()->TileSize(); }

    void SetupSimulatedEdids();
    void SetupSimulatedEdids(UINT32 fakeMask);

    enum PageSize : UINT64
    {
        PAGESIZE_4KB   =   4_KB,
        PAGESIZE_64KB  =  64_KB,
        PAGESIZE_128KB = 128_KB,
        PAGESIZE_2MB   =   2_MB,
        PAGESIZE_512MB = 512_MB,

        PAGESIZE_SMALL = PAGESIZE_4KB,
        PAGESIZE_BIG1  = PAGESIZE_64KB,
        PAGESIZE_BIG2  = PAGESIZE_128KB,
        PAGESIZE_HUGE  = PAGESIZE_2MB,
    };
    bool   SupportsPageSize(UINT64 pageSize) const;
    RC     GetSupportedPageSizes(vector<UINT64>* pSizes);
    UINT64 GetMaxPageSize() const;
    UINT64 GetMaxPageSize(UINT64 lowerBound, UINT64 upperBound) const;
    UINT64 GetBigPageSize() const;

    //! Get gpu compression page size in bytes
    RC GetCompressionPageSize(UINT64 *size);

    //! Get gpu va bits count
    RC GetGpuVaBitCount(UINT32 *bitCount);

    //! Perform an escape read/write into the simulator
    virtual int EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value);
    virtual int EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value);
    virtual int EscapeWriteBuffer(const char *Path, UINT32 Index, size_t Size, const void *Buf);
    virtual int EscapeReadBuffer(const char *Path, UINT32 Index, size_t Size, void *Buf) const;

    //! Get the runlist associated with the indicated engine, creating
    //! it if needed.
    RC GetRunlist(UINT32 Engine, LwRm *pLwRm, bool MustBeAllocated,
                  Runlist **ppRunlist);

    //! Get the runlist that contains the indicated channel, or NULL if none
    Runlist *GetRunlist(Channel *pChannel) const;

    //! Get all runlists that have been allocated on the device
    vector<Runlist*> GetAllocatedRunlists() const;

    //! Get the engines on the device
    RC GetEngines(vector<UINT32> *pEngines, LwRm* pLwRm = nullptr);

    //! Get the engines on the device filtered by supported classId
    RC GetEnginesFilteredByClass(const vector<UINT32>& classIds,
                                 vector<UINT32> *pEngines,
                                 LwRm* pLwRm = nullptr);

    //! Get the engines on the device filtered by supported classId and by unique physical engines
    RC GetPhysicalEnginesFilteredByClass(const vector<UINT32>& classIds,
                                         vector<UINT32> *pEngines,
                                         LwRm* pLwRm = nullptr);

    //! Get the name of an engine
    static const char *GetEngineName(UINT32 engine);

    //! Stop per engine runlist
    RC StopEngineRunlist(UINT32 engine, LwRm* pLwRm = nullptr);
    //! Restart per engine runlist
    RC StartEngineRunlist(UINT32 engine, LwRm* pLwRm = nullptr);

    //! Configure the device to use vista-style two-stage robust
    //! channel error recovery, which ilwolves direct callbacks
    //! between mods and RM.  Client side resman only.
    RC SetUseRobustChannelCallback(bool useRobustChannelCallback);
    bool GetUseRobustChannelCallback() const { return m_UseRobustChCallback; }

    //! Override RM's initial register value for this context-switch register.
    //! Affects all subdevices.
    RC SetContextOverride
    (
        UINT32 regAddr,
        UINT32 andMask,
        UINT32 orMask,
        LwRm   *pLwRm = nullptr
    );

    //! This calls GpuSubdevice InitializeWithoutRm
    RC InitializeWithoutRm();

    RC Reset(UINT32 lw0080CtrlBifResetType);
    bool GetResetInProgress() const { return m_ResetInProgress; }
    RC RecoverFromReset();
    RC RecoverFromResetInit(UINT32 resetControlsMask);
    RC RecoverFromResetShutdown(UINT32 resetControlsMask);

    SETGET_PROP(ForcePerf20, bool);

    //! Used by AllocateEntireFramebuffer to ensure two tests don't try to do it at the same time
    class LockAllocFb
    {
        public:
            explicit LockAllocFb(GpuDevice* pSubdev);
            ~LockAllocFb();

            LockAllocFb(const LockAllocFb&)            = delete;
            LockAllocFb& operator=(const LockAllocFb&) = delete;

        private:
            void* m_Mutex;
    };

    RC AllocDevices();
    RC Alloc(LwRm *pLwRm);
    RC Free(LwRm *pLwRm);
    RC AllocSingleClientStuff(LwRm *pLwRm, GpuSubdevice *pSubdev);

private:
    //! Tear down the GpuSubdevice
    void ShutdownWithoutRm();

    //! Set m_Family
    RC SetChipFamily();

    //! Get info by LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS
    RC GetGpuVaCaps();

    void FreeSingleClientStuff(LwRm *pLwRm);

    RC AllocFlaVaSpace(LwRm *pLwRm);
    void FreeFlaVaSpace(LwRm *pLwRm);

    bool m_Initialized;
    bool m_InitializedWithoutRm;
    UINT32 m_InitDone;
    UINT32 m_Device;
    bool m_GrCapsLoaded;
    bool m_HostCapsLoaded;
    bool m_FbCapsLoaded;

    GpuSubdevice *m_Subdevices[LW2080_MAX_SUBDEVICES];
    UINT32 m_NumSubdevices;

    UINT08 m_GrCapsTable[LW0080_CTRL_GR_CAPS_TBL_SIZE];
    UINT08 m_HostCapsTable[LW0080_CTRL_HOST_CAPS_TBL_SIZE];
    UINT08 m_FbCapsTable[LW0080_CTRL_FB_CAPS_TBL_SIZE];

    // Handles created on a per-RMClient basis
    struct GpuDeviceHandles
    {
        UINT32 hFrameBufferMem;
        UINT32 hFrameBufferDmaCtx;
        UINT32 hGpuGartMem;
        UINT32 hCohGartDmaCtx;
        UINT32 hNcohGartDmaCtx;
        LwRm::Handle hFermiVASpace;
        LwRm::Handle hFermiFLAVASpace;
        LwRm::Handle hTegraVASpace;
        LwRm::Handle hGPUSMMUVASpace;
        LwRm::Handle hVprRegion;
    };
    typedef map<LwRm *, GpuDeviceHandles> Handles;
    Handles m_Handles;

    Display *m_pDisplay;
    DisplayMgr * m_pDisplayMgr;
    UserdManager * m_pUserdManager;
    unique_ptr<ProtectedRegionManager> m_ProtectedRegionManager;

    UINT32 m_GobHeight;

    multimap<UINT32, Runlist*> m_Runlists;
    map<LwRm*, vector<UINT32>> m_Engines;
    vector<UINT64> m_PageSizes;
    UINT64 m_BigPageSize;
    UINT64 m_CompressionPageSize;
    UINT32 m_GpuVaBitsCount;
    bool m_UseRobustChCallback;
    bool m_ResetInProgress; //!< Reset was called, but not RecoverFromReset
    UINT32 m_ResetType;     //!< Arg passed to Reset, if m_ResetInProgress=true
    bool m_ForcePerf20;
    UINT32 m_NumUserdAllocs;
    Memory::Location m_UserdLocation;
    GpuFamily m_Family;
    void* m_AllocFbMutex;
    bool m_DevicesAllocated;
    UINT32 m_HeapSizeKb;
    UINT64 m_FrameBufferLimit;
    UINT64 m_GpuGartLimit;
};

#endif // INCLUDE_GPU
#endif // INCLUDED_GPUDEV_H
