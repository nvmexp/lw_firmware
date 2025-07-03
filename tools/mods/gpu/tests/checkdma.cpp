/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file Check Dma stress test
//!
//! Based on gpudma.cpp, modified to keep all GPUs and the CPU busy at once
//! to stress non-SLI peer dmas.

//@@@ To Do/Nice to Have:
// - add counters of total bytes Peer-R, Peer-W, Pex-R, Pex-W.
// - add timestamp semaphores to get benchmarking data
// - use lwca for src-fill and dst-check to speed up test on large surfs
// - allow mix of SLI and non-SLI GpuDevices
// - restore coverage of BR04 broadcast CPU writes

#include "class/cl502d.h"
#include "class/cl902d.h"
#include "class/cl90b5.h"
#include "class/cla06fsubch.h"
#include "class/cla0b5.h"
#include "class/clb0b5.h"
#include "class/clc0b5.h" //PASCAL_DMA_COPY_A
#include "class/clc1b5.h" //PASCAL_DMA_COPY_B
#include "class/clc3b5.h" //VOLTA_DMA_COPY_A
#include "class/clc5b5.h" //TURING_DMA_COPY_A
#include "class/clc6b5.h" //AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A
#include "core/include/channel.h"
#include "core/include/circsink.h"
#include "core/include/golden.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl0000.h" // LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS
#include "device/interface/pcie.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/js/fpk_comm.h"
#include "gputestc.h"
#include "gpu/tests/pcie/pextest.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/pcie/pexdev.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "random.h"

#include <list>
#include <stdio.h>
#include <algorithm>  // for min<UINT32>(a,b)
//------------------------------------------------------------------------------
class CheckDma : public PexTest
{
public:
    CheckDma();
    virtual ~CheckDma();
    virtual bool IsSupported();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC RunTest();
    virtual RC Cleanup();
    virtual RC SetupJsonItems();

    //! True if this test exercises all subdevices at once, false if this
    //! test should be run once per subdevice to get full coverage.
    virtual bool GetTestsAllSubdevices() { return true; }

protected:
    virtual RC LockGpuPStates();
    virtual void ReleaseGpuPStates();

private:
    // private types & constants:
    enum ChanId
    {
        Twod
        ,CopyEngine0
        ,CopyEngine1
        ,CopyEngine2
        ,CopyEngine3
        ,CopyEngine4
        ,CopyEngine5
        ,CopyEngine6
        ,CopyEngine7
        ,CopyEngine8
        ,CopyEngine9
        ,ChanId_NUM  // must be last
    };
    static constexpr UINT32 NUM_SUPPORTED_COPY_ENGINES = CopyEngine9 - CopyEngine0 + 1;

    class Surf;
    class Action;
    typedef list<Action *>              ActionList;
    typedef list<Action *>::iterator    ActionListIter;

    //! Per-GpuDevice state (channel, objects, pex-err counts).
    class Dev
    {
    private:
        struct Chan
        {
            LwRm::Handle    hCh;
            Channel *       pCh;
            Surface2D       SysmemSemaphore;
            GrClassAllocator *  pGrAlloc;
            UINT32          Flushes;  //!< Num calls to WriteSemaphoreAndFlush.
            UINT32          Waits;    //!< Num calls to WaitSemaphore.

            Chan()
            :    hCh(0)
                ,pCh(0)
                ,pGrAlloc(0)
                ,Flushes(0)
                ,Waits(0)
            {
            }
        };

        GpuTestConfiguration * m_pTstCfg;
        GpuDevice *   m_pGpuDevice;
        GpuSubdevice * m_pGpuSubdevice;
        TwodAlloc     m_TwodAlloc;
        DmaCopyAlloc  m_DmaCopyAllocs[LW2080_ENGINE_TYPE_COPY_SIZE];
        UINT32        m_MaxTransferSize;
        Chan          m_Chans[ChanId_NUM];

        RC AllocChan
        (
            int chanId
        );

    public:
        Dev();
        ~Dev();
        RC Alloc
        (
            GpuDevice * pGpuDev,
            GpuTestConfiguration * pTstCfg,
            UINT32 maxTransferSize
        );
        RC Free();
        RC InstantiateObjects();
        RC WriteSemaphoreAndFlush();
        RC WaitSemaphore();
        RC FillTwod
        (
            Surface2D & surf,
            UINT32      pattern
        );
        RC CopyTwod
        (
            Surface2D & src,
            UINT32      srcX,
            UINT32      srcY,
            Surface2D & dst,
            UINT32      dstX,
            UINT32      dstY,
            UINT32      width,
            UINT32      height
        );
        RC CopyCopyEngine
        (
            int         engine,  //!< i.e. 0 vs. 1
            Surface2D & src,
            UINT32      srcX,
            UINT32      srcY,
            bool        bSrcPhysAddr,
            Surface2D & dst,
            UINT32      dstX,
            UINT32      dstY,
            bool        bDstPhysAddr,
            UINT32      width,
            UINT32      height
        );
        RC CopyDefault
        (
            Surface2D & src,
            UINT32      srcX,
            UINT32      srcY,
            Surface2D & dst,
            UINT32      dstX,
            UINT32      dstY,
            UINT32      width,
            UINT32      height
        );

        RC GetPcieErrors();

        GpuDevice *     GpuDev() const { return m_pGpuDevice; }
        GpuSubdevice *  GpuSubdev() const { return m_pGpuSubdevice; }
        LwRm::Handle    HCh(int chanId) const;
        UINT32          DevInst() const { return m_pGpuDevice->GetDeviceInst(); }
        UINT64          GetCtxDmaOffset(Surface2D & surf);
        UINT32          DefaultCopyAction();
        bool            IsPhysAddrSupported(int chanId) const;
        bool            IsPeerPhysAddrSupported(int chanId) const;
    };
    typedef vector<Dev*>           Devs;
    typedef vector<Dev*>::iterator DevsIter;

    //! \brief A surface (data source and dest).
    //!
    //! We consider the FB surfaces on each subdevice/gpu of an SLI device
    //! to be separate "things" in this test.
    //!
    //! A Surface2D that is in FB memory on the other hand, actually exists
    //! in parallel across all subdevices/gpus of an SLI device (required by
    //! the way the RM implementes SLI FB heap management).
    //!
    //! So a Surf in FB shares a common Surface2D object with it's counterparts
    //! on the other subdevices/gpus.  The Subdev0 Surf does the alloc/free.
    //!
    //! We use the bottom half (high addresses) as the read-only source pattern
    //! while the top half gets overwritten each frame.
    class Surf
    {
    private:
        Surface2D   m_Src;        //!< Source surface, used only for reads
        Surface2D   m_Dst;        //!< Destination surface, for writes
        Dev *       m_pDev;
        UINT32      m_SrcFill;    //!< This is the physical address of the first
                                  //!<  byte of the source part of the surface.
        UINT08      m_DstFill;    //!< Most recent dst fill byte
        string      m_Name;       //!< name for pretty-printing

        //! List of actions that write to m_Dst, in order of starting offset.
        ActionList m_CopyActions;

        //! List of rectangular blocks of m_Dst that are not targets of any
        //! action in CopyActions.
        //!
        //! At all times, m_Dst is jointly tiled by CopyActions and FreeBlocks,
        //! without overlaps.
        ActionList m_FreeBlocks;

    public:
        Surf();
        ~Surf();
        RC Alloc
        (
            Memory::Location loc,
            Dev * pDev,
            UINT32 pitch,
            const GpuTestConfiguration & tstCfg,
            UINT32 size
        );
        RC Free();
        RC FillSrc();
        RC ClearDst(UINT08 pattern);

        void     ClearFreeBlocks();
        Action * ConsumeFreeBlock();
        void     AddFreeBlock(Action * pFree);
        void     AddCopyAction(Action * pAct);

        bool        Allocated() const   { return m_Src.GetMemHandle() != 0; }
        Surface2D & GetSrc()            { return m_Src; }
        Surface2D & GetDst()            { return m_Dst; }
        Dev *       GetDev() const      { return m_pDev; }

        bool        IsFull() const      { return m_FreeBlocks.size() == 0; }
        bool        HasUncheckedActions();
        const char *GetName() const     { return m_Name.c_str(); }
        const string& GetNameStr() const       { return m_Name; }
        Memory::Location GetLocation() const { return m_Src.GetLocation(); }

        ActionList & GetActions()       { return m_CopyActions; }
        ActionList & GetFreeBlocks()    { return m_FreeBlocks; }
        UINT08 GetDstFill() const       { return m_DstFill; }
        UINT32 GetSrcFill(UINT32 byteOffset) const
        {
            // For better SSO stress, ilwert the pattern on each alternating
            // pixel and each alternating line (1-pixel checkerboard moire).
            // We assume 1024-word lines here, not that it matters in a
            // practical sense.

            const UINT32 wordOffset = byteOffset / sizeof(UINT32);
            const UINT32 ilwert = 1 & ((wordOffset) ^ (wordOffset / 1024));
            return (m_SrcFill + wordOffset) ^ (0xffffffff * ilwert);
        }

        void Dump
        (
            bool DumpDst,
            bool IsCopy,
            UINT32 errX,
            UINT32 errY,
            SurfaceUtils::MappingSurfaceReader & reader
        );

        enum {
            // We want 32b src-fill patterns that are distinctive.
            //
            // We use 5 MSB for surface number and 27 LSB for offset>>2, which
            // should make each src word unique across the test so long as:
            //  - we have no more than 30 gpu surfaces + 2 sysmem surfs
            //  - each surf is < 2^28 words (256MB), with 1/2 for src
            SrcFillShift = 27,
            DumpBytes    = 16
        };
    };
    typedef vector<Surf>           Surfs;
    typedef vector<Surf>::iterator SurfsIter;

    //! \brief A copy-action.
    //!
    //! Describes one operation of copying memory from one surface's Source area
    //! to the same or another surface's Dest area.
    //! Can be a GPU dma operation or a CPU read/write.
    class Action
    {
    public:
        enum State
        {
            Free,               //!< Free-space list or uninitialized
            Ready,              //!< filled in, ready to send
            Sent,               //!< sent to pbuffer/exelwted on CPU
            Checked_Pass,       //!< results checked OK
            Checked_Failed,     //!< results checked, errors
            Skipped,            //!< don't send, wait, or check
            NUM_States
        };

    private:
        UINT32 m_Type;          //!< FPK_CHECKDMA_ACTION_* from fpk_comm.h
        State  m_State;
        Surf * m_pSrc;
        Surf * m_pDst;
        Dev *  m_pDev;          //!< Acting gpu
        UINT32 m_SrcX;          //!< in bytes
        UINT32 m_SrcY;          //!< in lines
        UINT32 m_X;             //!< in bytes
        UINT32 m_Y;             //!< in lines
        UINT32 m_W;             //!< in bytes
        UINT32 m_H;             //!< in lines
        UINT32 m_Loop;
        bool   m_Incr;          //!< if true, use incrementing addresses
        bool   m_bSrcAddrPhysical; //!< if true, use physical addressing for the source surface
        bool   m_bDstAddrPhysical; //!< if true, use physical addressing for the destination
                                   //!< surface
        Tee::Priority m_Pri;    //!< for the FPK_CHECKDMA_VERBOSE feature

    public:
        Action();
        Action(const Action & A);
        ~Action();
        RC Init
        (
            UINT32 iType,
            Surf * pSrc,
            Surf * pDst,
            bool   doPeerWrite,
            UINT32 loop,
            bool bSrcAddrPhysical,
            bool bDstAddrPhysical,
            bool skip,
            Tee::Priority pri
        );
        void SetSize
        (
            UINT32 size,
            UINT32 width,
            UINT32 srcOff,
            UINT64 mapLimit
        );
        RC SendPbuf();          //!< Put GPU action into pushbuffer
        RC SendCpu();           //!< Execute cpu copy action
        void Skip();            //!< Mark this one "Skipped".
        void Print(Tee::Priority pri);
        void PrintHeader(Tee::Priority pri);
        void SetFreeBlock(UINT32 x, UINT32 y, UINT32 w, UINT32 h, Surf * pDst);
        void SetDirection(bool incrementing) { m_Incr = incrementing; }

        Surf * GetSrc() const { return m_pSrc; }
        Surf * GetDst() const { return m_pDst; }
        State  GetState() const { return m_State; }
        UINT32 GetSize() const { return m_W * m_H; }
        UINT32 GetType() const  { return m_Type; }
        UINT32 GetLoop() const  { return m_Loop; }

        RC Check
        (
            SurfaceUtils::MappingSurfaceReader & reader,
            bool     CheckFullSurface,
            bool     IsCopy,
            bool *   pPrintedHeader,
            UINT32 * pRtnBadByteCount
        );

    private:
        RC CheckClear
        (
            SurfaceUtils::MappingSurfaceReader & dstReader,
            bool IsCopy,
            UINT32 * pRtnBadByteCount
        );
        RC CheckCopy
        (
            SurfaceUtils::MappingSurfaceReader & dstReader,
            bool IsCopy,
            bool * pPrintedHeader,
            UINT32 * pRtnBadByteCount
        );
    };

    // private functions:
    RC AllocResources();
    RC FreeResources();
    RC InstantiateObjects();
    RC PickActions(UINT32 Frame);
    RC PickAction(UINT32 a);
    void PickActionAddresses(UINT32 a);
    void PickFbSurfMapping();
    RC CheckSurfaces(bool destroyHostCSurface);
    RC CheckSurface(Surf * pSurf, Surface2D * pLocalCopy);
    RC CheckPexErrCounts(UINT32 loop);
    RC WriteSemaphoreAndFlush();
    RC WaitSemaphore();
    void FreeErrorData();
    void ClearErrors();
    RC InnerRun();
    RC MapSurfsToCpu();
    void UnmapSurfsFromCpu();
    void ReleaseFirstNGpuPStates(UINT32 n);

    // JS config values
    GpuTestConfiguration *  m_pTstCfg;
    Goldelwalues       *    m_pGolden;
    UINT32                  m_SurfSizeOverride;
    bool                    m_CheckFullSurface;
    bool                    m_AutoStatus;
    UINT32                  m_MaxCpuCopySize;
    bool                    m_DontAllocHostC;
    UINT32                  m_MaxMem2MemSize;
    bool                    m_CheckSurfacesOnlyIfWritten;
    UINT32                  m_DeviceMask;
    UINT64                  m_MappingSize;
    UINT32                  m_SrcOffsetToCorrupt;
    bool                    m_BenchP2P;
    FancyPicker::FpContext* m_pFpCtx;
    FancyPickerArray *      m_pPickers;

    // Runtime resources
    //! All possible surfaces -- hostC, hostNC, dev0 FB to dev31 FB.
    //! Most of the FB surfaces won't be allocated of course.
    //! Note that we index into the FB surfaces by GpuDevice instance.
    Surfs           m_Surfs;

    //! Gpu devices used by this test, without gaps.
    //! Not indexed by GpuDevice instance!
    //! I.e. if we are testing dev 2 and 3, m_Devs[0] will hold GpuDevice 2.
    Devs            m_Devs;
    vector<Action>  m_Actions;
    vector<UINT32>  m_ActionIndices;
    bool            m_HasHelpMessageBeenPrinted;
    bool            m_PrintedHeader;
    bool            m_DoPreFlushCallback;
    bool            m_DoPreCheckCallback;
    UINT32          m_DevInstForCpuOps;

    bool            m_CanP2PRead;
    bool            m_CanP2PWrite;

    // Resources that persist after Run() for error logging.
    ActionList m_FailedActions;
    UINT32     m_BadBytesTotal;

    bool       m_EnablePexCheck;

    // For each subdevice to prevent pstate change during the test.
    PStateOwner m_PStatesOwner[LW0080_MAX_DEVICES];

public:
    // JS interface functions:

    RC SetDefaultsForPicker(UINT32 idx);

    SETGET_PROP(SurfSizeOverride, UINT32);
    SETGET_PROP(DontAllocHostC, bool);
    SETGET_PROP(CheckFullSurface, bool);
    SETGET_PROP(MaxCpuCopySize, UINT32);
    SETGET_PROP(AutoStatus, bool);
    SETGET_PROP(MaxMem2MemSize, UINT32);
    SETGET_PROP(CheckSurfacesOnlyIfWritten, bool);
    SETGET_PROP(EnablePexCheck, bool);
    SETGET_PROP(SrcOffsetToCorrupt, UINT32);
    SETGET_PROP(BenchP2P, bool);
    UINT32 GetDeviceMask();
    RC SetDeviceMask(UINT32 m);
    SETGET_PROP(MappingSize, UINT64);
    UINT32 GetErrorCount() const;
    void PrintErrors(Tee::Priority pri);
    RC Status ();
    bool CanP2PRead() const { return m_CanP2PRead; }
    bool CanP2PWrite() const { return m_CanP2PWrite; }
};

static UINT32 Class2Subchannel(GrClassAllocator *pGrAlloc)
{
    switch(pGrAlloc->GetClass())
    {
        case FERMI_TWOD_A:
            return LWA06F_SUBCHANNEL_2D;
        case KEPLER_DMA_COPY_A:
        case MAXWELL_DMA_COPY_A:
        case PASCAL_DMA_COPY_A:
        case PASCAL_DMA_COPY_B:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
        case AMPERE_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
        case HOPPER_DMA_COPY_A:
        case BLACKWELL_DMA_COPY_A:
            return LWA06F_SUBCHANNEL_COPY_ENGINE;
        case GF100_DMA_COPY:
        case LW50_TWOD:
            return 0;
        default:
            MASSERT(!"CheckDma::Class2Subchannel Unknown ClassId ");
            return 0xFFFFFFFF;
    }
}

//------------------------------------------------------------------------------
CheckDma::CheckDma()
 :  m_SurfSizeOverride(0),
    m_CheckFullSurface(true),
    m_AutoStatus(false),
    m_MaxCpuCopySize(8*1024),
    m_DontAllocHostC(false),
    m_MaxMem2MemSize(0),
    m_CheckSurfacesOnlyIfWritten(true),
    m_DeviceMask(0xffffffff),
    m_MappingSize(1*1024*1024),
    m_SrcOffsetToCorrupt(0xffffffff),
    m_BenchP2P(false),
    m_pFpCtx(0),
    m_pPickers(0),
    m_Surfs(FPK_CHECKDMA_SURF_NUM_SURFS),
    m_Devs(),
    m_Actions(),
    m_ActionIndices(),
    m_HasHelpMessageBeenPrinted(false),
    m_PrintedHeader(false),
    m_DoPreFlushCallback(true),
    m_DoPreCheckCallback(true),
    m_DevInstForCpuOps(0),
    m_CanP2PRead(false),
    m_CanP2PWrite(false),
    m_FailedActions(),
    m_BadBytesTotal(0)
{
    SetName("CheckDma");

    ct_assert(NUM_SUPPORTED_COPY_ENGINES == LW2080_ENGINE_TYPE_COPY_SIZE);

    m_pTstCfg = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_CHECKDMA_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
    m_EnablePexCheck = false;
}

//------------------------------------------------------------------------------
CheckDma::~CheckDma()
{
    // Free persistent resources used for error and perf reporting.
    FreeErrorData();
}

//------------------------------------------------------------------------------
bool CheckDma::IsSupported()
{
    if (!PexTest::IsSupported())
    {
        return false;
    }

    // For now this is not supported on SOC GPUs
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        return false;
    }

    // Filter out nonexistent devices and SLI devices in mask.
    SetDeviceMask(GetDeviceMask());

    // If we are left with no devices, not supported.
    if (0 == GetDeviceMask())
    {
        Printf(Tee::PriLow, "CheckDma DeviceMask matches no usable GpuDevices.\n");
        return false;
    }

    // All selected devices must support paging, and if >1 must support P2P.
    LwRmPtr pLwRm;
    GpuDevMgr * pMgr = (GpuDevMgr*) DevMgrMgr::d_GraphDevMgr;

    bool needP2P = (Utility::CountBits(GetDeviceMask()) > 1);
    LW0000_CTRL_SYSTEM_GET_P2P_CAPS_PARAMS  p2pCapsParams;
    memset(&p2pCapsParams, 0, sizeof(p2pCapsParams));

    for (GpuDevice * pDev = pMgr->GetFirstGpuDevice();
         pDev;
         pDev = pMgr->GetNextGpuDevice(pDev))
    {
        UINT32 x = (1 << pDev->GetDeviceInst());
        if (0 == (x & GetDeviceMask()) || pDev->GetSubdevice(0)->IsSOC())
            continue;

        if (!pDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
        {
            Printf(Tee::PriLow, "CheckDma requires paging (g80+).\n");
            return false;
        }

        if (needP2P)
        {
            p2pCapsParams.gpuIds[p2pCapsParams.gpuCount] = pDev->GetSubdevice(0)->GetGpuId();
            Printf(Tee::PriLow, "Adding gpuId x 0x%x\n", p2pCapsParams.gpuIds[p2pCapsParams.gpuCount]);
            p2pCapsParams.gpuCount++;
        }
    }

    if (needP2P)
    {
        RC rc = pLwRm->Control(pLwRm->GetClientHandle() , LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS,
                               &p2pCapsParams, sizeof(p2pCapsParams));

        if (OK != rc)
        {
            Printf(Tee::PriHigh, "CheckDma failed call to LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS.\n");
            return false;
        }

        if ((FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _READS_SUPPORTED, _FALSE,
             p2pCapsParams.p2pCaps) &&
             FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _WRITES_SUPPORTED, _FALSE,
             p2pCapsParams.p2pCaps)))
        {
            Printf(Tee::PriHigh, "CheckDma requires peer-dma support when 2+ devices are selected.\n");
            return false;
        }

        m_CanP2PRead = FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _READS_SUPPORTED, _TRUE,
                                    p2pCapsParams.p2pCaps);
        m_CanP2PWrite = FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _WRITES_SUPPORTED, _TRUE,
                                    p2pCapsParams.p2pCaps);
        const char * canMsg;

        if (m_CanP2PWrite && m_CanP2PRead)
            canMsg = " read and write";
        else if (m_CanP2PWrite)
            canMsg = " write";
        else if (m_CanP2PRead)
            canMsg = " read";
        else
            canMsg = "not access";

        Printf(Tee::PriLow, "CheckDma can%s peer FB over bus.\n", canMsg);
    }
    // All selected devices are non-sli, support m2m, and P2P if 2+ devices.
    return true;
}

//------------------------------------------------------------------------------
UINT32 CheckDma::GetDeviceMask()
{
    return m_DeviceMask;
}

//------------------------------------------------------------------------------
RC CheckDma::SetDeviceMask(UINT32 m)
{
    if (DevMgrMgr::d_GraphDevMgr)
    {
        // If we have a mask, and the Gpu code is set up, accept only bits
        // that match actual gpu devices.
        // Thus it is safe to set a mask of 0xffff to test all gpus.

        m_DeviceMask = 0;

        GpuDevMgr * pMgr = (GpuDevMgr*) DevMgrMgr::d_GraphDevMgr;
        for (GpuDevice * pDev = pMgr->GetFirstGpuDevice();
                pDev;
                    pDev = pMgr->GetNextGpuDevice(pDev))
        {
            UINT32 x = (1 << pDev->GetDeviceInst());
            if ((m & x)
                    && (pDev->GetNumSubdevices() < 2)
                    && !pDev->GetSubdevice(0)->IsSOC())
                m_DeviceMask |= x;
        }
    }
    else
    {
        m_DeviceMask = m;
    }
    return OK;
}

//------------------------------------------------------------------------------
RC CheckDma::AllocResources()
{
    RC rc;

    Printf(Tee::PriDebug, "CheckDma::AllocResources\n");

    // Delete possible left-over data from previous runs.
    CHECK_RC(FreeResources());
    FreeErrorData();

    // Alloc one frame worth of Action objects, which will be used
    // in each new frame.
    UINT32 numActions = m_pTstCfg->RestartSkipCount();
    m_Actions.resize(numActions);
    m_ActionIndices.resize(numActions);

    // Alloc our per-dev data.
    GpuDevMgr *pMgr       = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    GpuDevice *pGpuDevice = pMgr->GetFirstGpuDevice();

    m_Devs.resize(Utility::CountBits(GetDeviceMask()));

    for (DevsIter i = m_Devs.begin(); i != m_Devs.end(); ++i)
    {
        // We shouldn't run out of GpuDevices to match our Dev vector,
        // because SetDeviceMask() is supposed to make sure that
        // all the bits in m_DeviceMask match a real device's inst.
        //
        // So pGpuDevice should never be null.
        MASSERT(pGpuDevice);

        // Skip over devices not in m_DeviceMask.
        while (0 == (m_DeviceMask & (1 << pGpuDevice->GetDeviceInst())))
        {
            pGpuDevice = pMgr->GetNextGpuDevice(pGpuDevice);

            // pGpuDevice should not be null here either.
            MASSERT(pGpuDevice);
        }

        // Set up the CheckDma::Dev object with this selected GpuDevice.
        // Here is where Channel allocation oclwrs.
        *i = new Dev;
        CHECK_RC( (*i)->Alloc(pGpuDevice, m_pTstCfg, m_MaxMem2MemSize) );
        pGpuDevice = pMgr->GetNextGpuDevice(pGpuDevice);
    }
    m_pTstCfg->BindGpuDevice(GetBoundGpuDevice());

    // Alloc surfaces: host C and NC plus 32 FB surfaces.
    // Not all of the FB surfaces will be used, of course.

    Printf(Tee::PriLow, "About to alloc surfs.\n");

    UINT32 pitch = m_pTstCfg->SurfaceWidth() * m_pTstCfg->DisplayDepth() / 8;
    CHECK_RC(AdjustPitchForScanout(&pitch));

    // Allocate host coherent memory
    if (!m_DontAllocHostC && !m_BenchP2P)
    {
        CHECK_RC( m_Surfs[FPK_CHECKDMA_SURF_HostC].Alloc(
                Memory::Coherent,
                m_Devs.front(),
                pitch,
                *m_pTstCfg,
                m_SurfSizeOverride));
    }

    // Allocate host non-coherent memory
    if (!m_BenchP2P)
    {
        CHECK_RC( m_Surfs[FPK_CHECKDMA_SURF_HostNC].Alloc(
                Memory::NonCoherent,
                m_Devs.front(),
                pitch,
                *m_pTstCfg,
                m_SurfSizeOverride));
    }

    // Allocate FB surfaces
    for (UINT32 idx = 0; idx < m_Devs.size(); idx++)
    {
        UINT32 DevInst = m_Devs[idx]->DevInst();
        CHECK_RC( m_Surfs[FPK_CHECKDMA_SURF_FbGpu0 + DevInst].Alloc(
                Memory::Fb,
                m_Devs[idx],
                pitch,
                *m_pTstCfg,
                m_SurfSizeOverride));
    }

    if (m_MappingSize > m_Surfs[FPK_CHECKDMA_SURF_HostNC].GetSrc().GetSize())
        m_MappingSize = m_Surfs[FPK_CHECKDMA_SURF_HostNC].GetSrc().GetSize();

    // More per-surface configuration -- bindings and mappings.

    for (SurfsIter iSurf = m_Surfs.begin(); iSurf != m_Surfs.end(); ++iSurf)
    {
        if (! (*iSurf).Allocated())
            continue;

        // Bind this surface to each channel.
        for (DevsIter iDev = m_Devs.begin(); iDev != m_Devs.end(); ++iDev)
        {
            for (int chan = 0; chan < ChanId_NUM; chan++)
            {
                const LwRm::Handle h = (*iDev)->HCh(chan);
                if (h != 0)
                {
                    CHECK_RC( (*iSurf).GetSrc().BindGpuChannel(h) );
                    CHECK_RC( (*iSurf).GetDst().BindGpuChannel(h) );
                }
            }
        }

        // Map this surface to all other gpu devices.
        for (DevsIter iDev = m_Devs.begin(); iDev != m_Devs.end(); ++iDev)
        {
            GpuDevice * peerDev = (*iDev)->GpuDev();
            GpuDevice * localDev = (*iSurf).GetDev()->GpuDev();

            if (peerDev == localDev)
                continue;

            if ((*iSurf).GetLocation() == Memory::Fb)
            {
                CHECK_RC( (*iSurf).GetSrc().MapPeer(peerDev) );
                CHECK_RC( (*iSurf).GetDst().MapPeer(peerDev) );
            }
            else
            {
                CHECK_RC( (*iSurf).GetSrc().MapShared(peerDev) );
                CHECK_RC( (*iSurf).GetDst().MapShared(peerDev) );
            }
        }
    }

    UINT32 devInst = 0;
    GpuDevice *pGpuDev = GetDisplayMgrTestContext()->GetOwningDev();
    if (pGpuDev != nullptr)
    {
        devInst = pGpuDev->GetDeviceInst();
    }
    const auto &dispSurf2D = m_Surfs[FPK_CHECKDMA_SURF_FbGpu0 + devInst].GetDst();

    GpuUtility::DisplayImageDescription desc;
    desc.CtxDMAHandle   = dispSurf2D.GetCtxDmaHandleIso();
    desc.Offset         = dispSurf2D.GetCtxDmaOffsetIso();
    // Our surface may be oversized, use only the first SurfaceHeight lines.
    desc.Height         = m_pTstCfg->SurfaceHeight();
    desc.Width          = dispSurf2D.GetWidth();
    desc.AASamples      = 1;
    desc.Layout         = dispSurf2D.GetLayout();
    desc.Pitch          = dispSurf2D.GetPitch();
    desc.LogBlockHeight = dispSurf2D.GetLogBlockHeight();
    desc.NumBlocksWidth = dispSurf2D.GetBlockWidth();
    desc.ColorFormat    = dispSurf2D.GetColorFormat();

    CHECK_RC(GetDisplayMgrTestContext()->DisplayImage(&desc,
        DisplayMgr::DONT_WAIT_FOR_UPDATE));

    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::FreeResources()
{
    Printf(Tee::PriDebug, "CheckDma::FreeResources\n");

    // Free everything that we don't need for printing error listings.

    StickyRC firstRc;

    if (m_Surfs.size())
    {
        firstRc = GetDisplayMgrTestContext()->DisplayImage(
            static_cast<Surface2D*>(nullptr), DisplayMgr::WAIT_FOR_UPDATE);
    }
    for (SurfsIter i = m_Surfs.begin();
            i != m_Surfs.end();
                ++i)
    {
        firstRc = (*i).Free();
    }
    for (DevsIter i = m_Devs.begin();
            i != m_Devs.end();
                ++i)
    {
        firstRc = (*i)->Free();
        delete *i;
        *i = NULL;
    }
    m_pTstCfg->BindGpuDevice(GetBoundGpuDevice());

    m_Actions.clear();
    m_ActionIndices.clear();

    return firstRc;
}

//------------------------------------------------------------------------------
void CheckDma::ClearErrors()
{
    ActionListIter i;
    for (i = m_FailedActions.begin(); i != m_FailedActions.end(); ++i)
        delete (*i);

    m_FailedActions.clear();

    m_BadBytesTotal = 0;
    m_PrintedHeader = false;
}

//------------------------------------------------------------------------------
void CheckDma::FreeErrorData()
{
    // Delete stuff we leave around after ::Run to support detailed
    // error-reporting.  All the RM/hw resources were already freed.
    ClearErrors();

    m_Devs.clear();
}

//------------------------------------------------------------------------------
RC CheckDma::Cleanup()
{
    StickyRC firstRc;

    firstRc = GetDisplayMgrTestContext()->DisplayImage(
        static_cast<Surface2D*>(nullptr), DisplayMgr::WAIT_FOR_UPDATE);

    // Free resources
    firstRc = FreeResources();
    firstRc = PexTest::Cleanup();

    return firstRc;
}

//-----------------------------------------------------------------------------
RC CheckDma::InstantiateObjects()
{
    RC rc;

    for (DevsIter i = m_Devs.begin(); i != m_Devs.end(); ++i)
    {
        Printf(Tee::PriDebug, "CheckDma::InstantiateObjects dev %d\n",
            (*i)->GpuDev()->GetDeviceInst());

        CHECK_RC((*i)->InstantiateObjects());
        CHECK_RC((*i)->WriteSemaphoreAndFlush());
        CHECK_RC((*i)->WaitSemaphore());
    }
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Setup()
{
    RC rc;

    // Fail if wrong SW branch for this HW.

    CHECK_RC(PexTest::Setup());

    // This test uses one channel per device.
    m_TestConfig.SetAllowMultipleChannels(true);

    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());

    // Alloc channels, surfaces, LW objects, semaphores, etc.
    CHECK_RC(AllocResources());

    // Setup PCIE error counters for all tested devices.
    for (DevsIter it=m_Devs.begin(); it != m_Devs.end(); ++it)
    {
        CHECK_RC(SetupPcieErrors((*it)->GpuSubdev()));
    }

    // Install objects in subchannels.
    CHECK_RC(InstantiateObjects());

    // If the user requested OnlyP2P but we don't have at least 2 gpus,
    // inform them and quit
    if (m_BenchP2P && m_Devs.size() < 2)
    {
        Printf(Tee::PriError,
            "Cannot use the testarg OnlyP2P on a system with only one gpu.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Put unique patterns into the source memory (top half of each Surf).
    for (UINT32 s = 0; s < FPK_CHECKDMA_SURF_NUM_SURFS; s++)
        m_Surfs[s].FillSrc();

    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::RunTest()
{
    ClearErrors();

    RC rc = InnerRun();

    // We shouldn't be able to get here with errors and an OK rc!
    MASSERT((0 == m_BadBytesTotal) || (OK != rc));

    if (OK != rc)
    {
        if (m_AutoStatus)
        {
            PrintErrors(Tee::PriNormal);
        }
        else if (!m_HasHelpMessageBeenPrinted)
        {
            m_HasHelpMessageBeenPrinted = true;
            Printf(Tee::PriNormal,
                    "Errors found. Use CheckDma.Status() for more info.\n");
        }

        if (RC::MEM_TO_MEM_RESULT_NOT_MATCH != rc)
        {
            // Abnormal exit, the gpu or bus might be hung.
            // Dump all available data, since we'll probably CPU-hang on teardown.
            Tee::GetCirlwlarSink()->Dump(Tee::FileOnly);
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::InnerRun()
{
    RC rc;
    StickyRC firstRc;
    UINT32 s,a;  // Surf, Action indices

    // Note that user might have asked us to start or stop partway through
    // a frame (i.e. they might want just one loop from the middle).
    // In any frame that is even partly used, we must still do all
    // the per-frame work *except* sending and checking the loops that
    // are outside the StartLoop to StartLoop+Loops range.

    UINT32 LoopsPerFrame    = m_pTstCfg->RestartSkipCount();
    UINT32 StartLoop        = m_pTstCfg->StartLoop();
    UINT32 EndLoop          = StartLoop + m_pTstCfg->Loops();
    UINT32 StartFrame       = StartLoop / LoopsPerFrame;
    UINT32 EndFrame         = (EndLoop + LoopsPerFrame - 1) / LoopsPerFrame;
    UINT32 LoopsPerCheck    = m_pGolden->GetSkipCount();

    if (LoopsPerCheck == 0)
        LoopsPerCheck = 1;
    if (LoopsPerCheck > LoopsPerFrame)
        LoopsPerCheck = LoopsPerFrame;

    //----------------------------------------------------------------------
    UINT32 Frame;
    for (Frame = StartFrame; Frame < EndFrame; Frame++)
    {
        UINT32 StartA = (Frame == StartFrame)   ?
                        (StartLoop % LoopsPerFrame) : 0;
        UINT32 EndA   = (Frame == (EndFrame-1)) ?
                        (EndLoop - (Frame * LoopsPerFrame)) : LoopsPerFrame;

        // Initialize our PRNG and FancyPicker objects.
        m_pFpCtx->RestartNum = Frame;
        m_pFpCtx->Rand.SeedRandom(m_pTstCfg->Seed() + Frame * LoopsPerFrame);
        m_pPickers->Restart();

        // Pick one of the FB surfaces for CPU access (or the BC mapping).
        PickFbSurfMapping();

        // Clear destination memory.
        for (s = 0; s < FPK_CHECKDMA_SURF_NUM_SURFS; s++)
            CHECK_RC( m_Surfs[s].ClearDst((Frame << 4) + s) );

        // Set up our array of copy actions for this frame.
        CHECK_RC( PickActions(Frame) );

        for (a = 0; a < LoopsPerFrame; a++)
        {
            if ((a < StartA) || (a >= EndA))
                m_Actions[a].Skip();

            m_ActionIndices[a] = a;
        }

        if ((*m_pPickers)[FPK_CHECKDMA_SHUFFLE_ACTIONS].Pick())
        {
            // Exectute the Actions out-of-order for possibly better testing.
            m_pFpCtx->Rand.Shuffle(LoopsPerFrame, &m_ActionIndices[0]);
        }

        UINT32 SentUpToA  = StartA;

        UINT64 totalDataTransferred = 0;
        // Loop over the Actions array, sending, waiting, and checking.
        for (a = 0; a < LoopsPerFrame; a++)
        {
            // Send/check in the shuffled order.
            UINT32 ai = m_ActionIndices[a];

            // If the action is ready then increment total data transferred
            if (m_Actions[ai].GetState() == Action::Ready)
                totalDataTransferred += m_Actions[ai].GetSize();

            // Write the GPU actions to the pushbuffer.
            CHECK_RC( m_Actions[ai].SendPbuf() );

            // After LoopsPerCheck iterations, flush the pushbuffer,
            // perform CPU work, and (eventually) obtain benchmarking data
            UINT32 loop = a + Frame * LoopsPerFrame;
            if ((loop % LoopsPerCheck) == (LoopsPerCheck-1))
            {
                // Map the chosen FB surface for use by CPU operations.
                if (!m_BenchP2P)
                    CHECK_RC( MapSurfsToCpu() );

                UINT64 startTime = Xp::GetWallTimeNS();

                // Flush the pushbuffers and write the outer semaphores.
                CHECK_RC( WriteSemaphoreAndFlush() );

                // While the GPUs are busy, perform all the CPU copy actions.
                for (UINT32 tmpA = SentUpToA; tmpA <= a; tmpA++)
                {
                    UINT32 tmpAI = m_ActionIndices[tmpA];
                    CHECK_RC( m_Actions[tmpAI].SendCpu() );
                }

                // Wait for all GPUs to write outer semaphores.
                CHECK_RC( WaitSemaphore() );

                UINT64 endTime = Xp::GetWallTimeNS();

                FLOAT64 transferSize = static_cast<FLOAT64>(totalDataTransferred);
                FLOAT64 timeElapsedInNS = static_cast<FLOAT64>(endTime-startTime);
                // Colwert to MB/s
                FLOAT64 transferRate = transferSize / timeElapsedInNS *
                                       (1000000000.0/(1024.0*1024.0));

                if (m_BenchP2P)
                {
                    // FB0 transfers to FB1 for Frame 0 and vice versa for Frame 1
                    if (Frame == 0)
                        Printf(Tee::PriNormal, "FB0 ---> FB1 :\n");
                    else
                        Printf(Tee::PriNormal, "FB1 ---> FB0 :\n");

                    // Benchmark stats
                    Printf(Tee::PriNormal, "\tTotal Memory Transferred: %f MB\n",
                            transferSize/1048576.0);
                    Printf(Tee::PriNormal, "\tTime Elapsed: %f ms\n",
                            timeElapsedInNS/1000000.0);
                    Printf(Tee::PriNormal, "\tAverage Payload Transfer Rate: %f MB/s\n",
                            transferRate);

                    // Account for PCIe overhead
                    FLOAT64 eccFactor = 0.0;
                    FLOAT64 payloadHeaderFactor = 148.0 / 128.0;
                    FLOAT64 flowControlFactor = 100.0 / 95.0;

                    UINT32 linkSpeed = GetGpuPcie()->GetLinkSpeed(Pci::SpeedUnknown);
                    if (linkSpeed == Pci::Speed2500MBPS ||
                        linkSpeed == Pci::Speed5000MBPS)
                    {
                        // PCI generation 1 and 2 use a 10b/8b error correction code
                        eccFactor = 10.0 / 8.0;
                    }
                    else if (linkSpeed == Pci::Speed8000MBPS ||
                             linkSpeed == Pci::Speed16000MBPS ||
                             linkSpeed == Pci::Speed32000MBPS)
                    {
                        // PCI generation 3, 4, and 5 use a 130b/128b error correction code
                        eccFactor = 130.0 / 128.0;
                    }
                    else
                    {
                        MASSERT(!"Unknown PEX speed");
                    }

                    FLOAT64 pciOverhead = eccFactor *
                                          payloadHeaderFactor *
                                          flowControlFactor;

                    Printf(Tee::PriNormal,
                           "\tAverage Transfer Rate w/ Overhead: %f MB/s\n",
                           transferRate * pciOverhead);
                }

                if (!m_BenchP2P)
                    UnmapSurfsFromCpu();

                if (m_DoPreCheckCallback)
                {
                    JavaScriptPtr pJs;
                    RC tmpRc = pJs->CallMethod(GetJSObject(), "PreCheckCallback");
                    if (tmpRc)
                        m_DoPreCheckCallback = false;
                }

                bool isEndOfFrame = (a == (EndA-1));

                if (!m_BenchP2P)
                {
                    firstRc = CheckSurfaces(isEndOfFrame);
                    firstRc = CheckPexErrCounts(loop);

                    if ((OK != firstRc) && m_pGolden->GetStopOnError())
                        return firstRc;
                }

                SentUpToA = a + 1;

            } // check
        } // send & check
    } // frame

    if (m_EnablePexCheck)
    {
        for (DevsIter i = m_Devs.begin(); i != m_Devs.end(); ++i)
        {
            firstRc = CheckPcieErrors((*i)->GpuSubdev());
        }
    }
    return firstRc;
}

//------------------------------------------------------------------------------
RC CheckDma::CheckPexErrCounts(UINT32 loop)
{
    StickyRC firstRc;

    if (m_EnablePexCheck)
    {
        for (DevsIter i = m_Devs.begin(); i != m_Devs.end(); ++i)
        {
            firstRc = GetPcieErrors((*i)->GpuSubdev());
        }
    }
    if (OK != firstRc)
    {
        Printf(Tee::PriHigh, "New PCI-Express errors seen after loop %d\n", loop);
    }
    return firstRc;
}

//------------------------------------------------------------------------------
RC CheckDma::PickActions(UINT32 Frame)
{
    // Build the full array of random DMA Actions for this frame.

    RC rc;
    UINT32 LoopsPerFrame    = m_pTstCfg->RestartSkipCount();

    // Pick action type, src, dst, skip, etc.
    for (UINT32 a = 0; a < LoopsPerFrame; a++)
    {
        m_pFpCtx->LoopNum = a + Frame * LoopsPerFrame;

        CHECK_RC( PickAction(a) );
        PickActionAddresses(a);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a random DMA copy action.
// Each action is a copy from the source (top) half of a Surf to the
// dest (bottom) half of the same or other Surf.
//   - mem2mem by any GPU from the GPU's FB or BAR1 to any surf
//   - twod by any GPU from any surf to the GPU's FB or BAR1
//   - memcpy by CPU from any Surf to any Surf
// No Action may overwrite memory written by any other action.
//
RC CheckDma::PickAction (UINT32 a)
{
    RC rc;

    Action * pAction = &m_Actions[a];

    UINT32 loopsPerFrame = m_pTstCfg->RestartSkipCount();
    UINT32 actionNum = m_pFpCtx->LoopNum;
    UINT32 frameNum = actionNum / loopsPerFrame;

    // Pick the DMA Action type (mem2mem, scimg, cpucpy, etc.).
    UINT32 actType = (*m_pPickers)[FPK_CHECKDMA_ACTION].Pick();
    UINT32 PeerDir = (*m_pPickers)[FPK_CHECKDMA_PEER_DIRECTION].Pick();

    // Pick source and destination surfaces.
    UINT32 iSrc = (*m_pPickers)[FPK_CHECKDMA_SRC].Pick() % FPK_CHECKDMA_SURF_NUM_SURFS;
    UINT32 iDst = (*m_pPickers)[FPK_CHECKDMA_DST].Pick() % FPK_CHECKDMA_SURF_NUM_SURFS;

    if ((*m_pPickers)[FPK_CHECKDMA_FORCE_PEER2PEER].Pick())
    {
        // Force src and dst to be 2 different allocated GPU FB surfaces.

        // Rotate dst surface index to the right until it's a valid FB surf.
        for (UINT32 i = 0; i < FPK_CHECKDMA_SURF_NUM_SURFS; i++)
        {
            const UINT32 tmpIDst = (iDst + i) % FPK_CHECKDMA_SURF_NUM_SURFS;
            if (m_Surfs[tmpIDst].Allocated() &&
                (Memory::Fb == m_Surfs[tmpIDst].GetLocation()))
            {
                if (m_BenchP2P)
                {
                    // For the first frame, make the Dst FB_1
                    if (frameNum == 0)
                    {
                        if (m_Surfs[tmpIDst].GetNameStr() == "FB_1")
                        {
                            iDst = tmpIDst;
                            break;
                        }
                    }
                    // For the second frame, make the Dst FB_0
                    else
                    {
                        if (m_Surfs[tmpIDst].GetNameStr() == "FB_0")
                        {
                            iDst = tmpIDst;
                            break;
                        }
                    }
                }
                else
                {
                    iDst = tmpIDst;
                    break;
                }
            }
        }

        // Rotate src surface index to the left until it's a valid FB surf,
        // and if we have 2+ gpus, not the same as dst.
        for (UINT32 i = 0; i < FPK_CHECKDMA_SURF_NUM_SURFS; i++)
        {
            const UINT32 tmpISrc = (iSrc + FPK_CHECKDMA_SURF_NUM_SURFS - i)
                    % FPK_CHECKDMA_SURF_NUM_SURFS;
            if (m_Surfs[tmpISrc].Allocated() &&
                (Memory::Fb == m_Surfs[tmpISrc].GetLocation()) &&
                 ((m_Devs.size() < 2) || (tmpISrc != iDst)))
            {
                if (m_BenchP2P)
                {
                    // For the first frame, make the Src FB_0
                    if (frameNum == 0)
                    {
                        if (m_Surfs[tmpISrc].GetNameStr() == "FB_0")
                        {
                            iSrc = tmpISrc;
                            break;
                        }
                    }
                    // For the second frame, make the Src FB_1
                    else
                    {
                        if (m_Surfs[tmpISrc].GetNameStr() == "FB_1")
                        {
                            iSrc = tmpISrc;
                            break;
                        }
                    }
                }
                else
                {
                    iSrc = tmpISrc;
                    break;
                }
            }
        }

        // Force action type to be a GPU action.
        if (actType == FPK_CHECKDMA_ACTION_CpuCpy)
            actType = m_Surfs[iSrc].GetDev()->DefaultCopyAction();

    }
    else
    {
        // Force src and dst to be allocated surfaces.
        while (!m_Surfs[iDst].Allocated())
        {
            iDst = (iDst + 1) % FPK_CHECKDMA_SURF_NUM_SURFS;
        }
        while (!m_Surfs[iSrc].Allocated())
        {
            iSrc = (iSrc - 1) % FPK_CHECKDMA_SURF_NUM_SURFS;
        }
    }

    // Force a valid action type, and force src/dst surface requirements
    // for that type.
    switch (actType)
    {
        default:
            actType = FPK_CHECKDMA_ACTION_CpuCpy;
            // fall through
        case FPK_CHECKDMA_ACTION_CpuCpy:
            // Only one FB surface is mapped to CPU at a time, we must use
            // that one for cpu operations.
            if (iSrc >= FPK_CHECKDMA_SURF_FbGpu0)
                iSrc = FPK_CHECKDMA_SURF_FbGpu0 + m_DevInstForCpuOps;
            if (iDst >= FPK_CHECKDMA_SURF_FbGpu0)
                iDst = FPK_CHECKDMA_SURF_FbGpu0 + m_DevInstForCpuOps;
            break;

        case FPK_CHECKDMA_ACTION_Noop:
            break;

        case FPK_CHECKDMA_ACTION_Twod:
        case FPK_CHECKDMA_ACTION_CopyEng0:
        case FPK_CHECKDMA_ACTION_CopyEng1:
        case FPK_CHECKDMA_ACTION_CopyEng2:
        case FPK_CHECKDMA_ACTION_CopyEng3:
        case FPK_CHECKDMA_ACTION_CopyEng4:
        case FPK_CHECKDMA_ACTION_CopyEng5:
        case FPK_CHECKDMA_ACTION_CopyEng6:
        case FPK_CHECKDMA_ACTION_CopyEng7:
        case FPK_CHECKDMA_ACTION_CopyEng8:
        case FPK_CHECKDMA_ACTION_CopyEng9:
            const int cid = Twod + (actType - FPK_CHECKDMA_ACTION_Twod);
            if (0 == m_Surfs[iSrc].GetDev()->HCh(cid))
            {
                // The src surface's GpuDevice doesn't support this HW class.
                // Force a valid HW class.
                actType = m_Surfs[iSrc].GetDev()->DefaultCopyAction();
            }
            break;
    }

    // If requested, avoid peer dma operations (debug feature).

    bool isPeer = ((iDst >= FPK_CHECKDMA_SURF_FbGpu0) &&
                   (iSrc >= FPK_CHECKDMA_SURF_FbGpu0) &&
                   (iDst != iSrc));

    if ((*m_pPickers)[FPK_CHECKDMA_FORCE_NOT_PEER2PEER].Pick() && isPeer)
    {
        iDst = iSrc;    // we know the src surface's GPU supports the class.
        isPeer = false;
    }

    if (actType == FPK_CHECKDMA_ACTION_Twod)
    {
        if (isPeer && (PeerDir == FPK_CHECKDMA_PEER_DIRECTION_Write))
        {
            // g8x ROP can't write peer, so Twod actions can't write peer.
            // Change the action to m2m or CE which can write peer.
            actType = m_Surfs[iSrc].GetDev()->DefaultCopyAction();
        }

        if ((m_Surfs[iDst].GetLocation() != Memory::Fb) &&
            (m_Surfs[iSrc].GetDev()->GpuDev()->SupportsRenderToSysMem() == false))
        {
            // This gpu can't render to sysmem, so we can't use Twod.
            actType = m_Surfs[iSrc].GetDev()->DefaultCopyAction();
        }
    }

    if ((actType >= FPK_CHECKDMA_ACTION_CopyEng0 &&
         actType <= FPK_CHECKDMA_ACTION_CopyEng9) &&
        isPeer &&
        (((PeerDir == FPK_CHECKDMA_PEER_DIRECTION_Read) &&
          m_Surfs[iDst].GetDev()->GpuSubdev()->HasBug(586059)) ||
         (m_Surfs[iSrc].GetDev()->GpuSubdev()->HasBug(586059)))
       )
    {
        // This gpu (gt218) hangs on shutdown if peer-dma is done via CopyEng.
        actType = m_Surfs[iSrc].GetDev()->DefaultCopyAction();
    }

    // Force a working peer direction.
    if (isPeer)
    {
        bool srcCanWrite = CanP2PWrite();
        bool dstCanRead  = CanP2PRead();

        if (PeerDir == FPK_CHECKDMA_PEER_DIRECTION_Read)
        {
            if (!dstCanRead)
            {
                if (srcCanWrite)
                {
                    PeerDir = FPK_CHECKDMA_PEER_DIRECTION_Write;
                }
                else
                {
                    iSrc = iDst;
                    isPeer = false;
                }
            }
        }
        else
        {
            if (!srcCanWrite)
            {
                if (dstCanRead)
                {
                    PeerDir = FPK_CHECKDMA_PEER_DIRECTION_Read;
                }
                else
                {
                    iSrc = iDst;
                    isPeer = false;
                }
            }
        }
    }

    // For the P2P benchmark, we always perform writes
    if (m_BenchP2P)
        PeerDir = FPK_CHECKDMA_PEER_DIRECTION_Write;

    const UINT32 srcAddrMode = (*m_pPickers)[FPK_CHECKDMA_SRC_ADDR_MODE].Pick();
    const UINT32 dstAddrMode = (*m_pPickers)[FPK_CHECKDMA_DST_ADDR_MODE].Pick();

    // Set up the Action with our picks.
    // It will choose the "acting" GPU to follow our picks.
    CHECK_RC( pAction->Init(
            actType,
            &m_Surfs[iSrc],
            &m_Surfs[iDst],
            PeerDir == FPK_CHECKDMA_PEER_DIRECTION_Write,
            m_pFpCtx->LoopNum,
            srcAddrMode == FPK_CHECKDMA_SRC_ADDR_MODE_physical,
            dstAddrMode == FPK_CHECKDMA_SRC_ADDR_MODE_physical,
            (*m_pPickers)[FPK_CHECKDMA_SKIP].Pick() != 0,
            (*m_pPickers)[FPK_CHECKDMA_VERBOSE].Pick() ? Tee::PriNormal : Tee::PriSecret));

    bool incr = FPK_CHECKDMA_CPU_WRITE_DIRECTION_incr == 
                (*m_pPickers)[FPK_CHECKDMA_CPU_WRITE_DIRECTION].Pick();
    if (actType != FPK_CHECKDMA_ACTION_CpuCpy)
        incr = true;

    pAction->SetDirection(incr);

    return rc;
}

//------------------------------------------------------------------------------
void CheckDma::PickActionAddresses (UINT32 a)
{
    // Pick the size of the 2d-block of memory to be copied.
    UINT32 size = (*m_pPickers)[FPK_CHECKDMA_SIZE_BYTES].Pick();
    UINT32 w    = (*m_pPickers)[FPK_CHECKDMA_WIDTH_BYTES].Pick();
    UINT32 off  = (*m_pPickers)[FPK_CHECKDMA_SRC_OFFSET].Pick();

    if (m_Actions[a].GetType() == FPK_CHECKDMA_ACTION_CpuCpy)
    {
        size = min(size, m_MaxCpuCopySize);
    }

    // Adjust the size of this copy to fit the first free rect in the
    // destination surface (and the size of the source surface), callwlate
    // the resulting x,y,w,h of the rect and save it.
    // Update the free rect list of the destination surface for next time.
    // If there is no free rect in the dest, mark this action Skipped.
    m_Actions[a].SetSize(size, w, off, m_MappingSize);
}

//------------------------------------------------------------------------------
RC CheckDma::MapSurfsToCpu()
{
    RC rc;

    for (SurfsIter iSurf = m_Surfs.begin(); iSurf != m_Surfs.end(); ++iSurf)
    {
        if (! (*iSurf).Allocated())
            continue;

        if ((*iSurf).GetLocation() == Memory::Fb)
        {
            if ((*iSurf).GetDev()->DevInst() == m_DevInstForCpuOps)
            {
                // Map a limited part of this surface through BAR1 to CPU.
                CHECK_RC((*iSurf).GetSrc().MapRegion(0, m_MappingSize, 0));
                CHECK_RC((*iSurf).GetDst().MapRegion(0, m_MappingSize, 0));
            }
            // Else leave this FB surface unmapped.
        }
        else
        {
            // Map all of this sysmem surface, since we aren't limited by
            // BAR1 size.
            CHECK_RC((*iSurf).GetSrc().Map(0));
            CHECK_RC((*iSurf).GetDst().Map(0));
        }
    }

    if (m_SrcOffsetToCorrupt < m_MappingSize)
    {
        // To test error reporting, deliberately corrupt one pixel in
        // the mapped FB src surface.
        Surf * pSurf = &m_Surfs[FPK_CHECKDMA_SURF_FbGpu0 + m_DevInstForCpuOps];
        char * p = m_SrcOffsetToCorrupt +
                   (char *) pSurf->GetSrc().GetAddress();
        MEM_WR32(p, 0xffffffff ^ MEM_RD32(p));
    }

    return rc;
}

//------------------------------------------------------------------------------
void CheckDma::UnmapSurfsFromCpu()
{

    if (m_SrcOffsetToCorrupt < m_MappingSize)
    {
        // Undo the XOR previously done in MapSurfsToCpu.
        Surf * pSurf = &m_Surfs[FPK_CHECKDMA_SURF_FbGpu0 + m_DevInstForCpuOps];
        char * p = m_SrcOffsetToCorrupt +
                   (char *) pSurf->GetSrc().GetAddress();
        MEM_WR32(p, 0xffffffff ^ MEM_RD32(p));
    }

    for (SurfsIter iSurf = m_Surfs.begin(); iSurf != m_Surfs.end(); ++iSurf)
    {
        if (! (*iSurf).Allocated())
            continue;

        if (((*iSurf).GetLocation() != Memory::Fb) ||
            ((*iSurf).GetDev()->DevInst() == m_DevInstForCpuOps))
        {
            (*iSurf).GetSrc().Unmap();
            (*iSurf).GetDst().Unmap();
        }
    }
}

//------------------------------------------------------------------------------
RC CheckDma::CheckSurfaces
(
    bool destroyHostCSurface
)
{
    if (m_pGolden->GetAction() != Goldelwalues::Check)
        return OK;

    RC rc;
    StickyRC firstRc;

    if (! m_Surfs[FPK_CHECKDMA_SURF_HostC].Allocated())
        destroyHostCSurface = false;

    if (destroyHostCSurface)
    {
        // If we are testing at the very end of the frame, we may safely
        // overwrite the host Coherent surface after checking it.
        // In this case, we can *massively* speed up the checking of the
        // other surfaces by copying them onto hostC first.
        firstRc = CheckSurface(&m_Surfs[FPK_CHECKDMA_SURF_HostC], 0);
    }

    for (UINT32 s = 0; s < FPK_CHECKDMA_SURF_NUM_SURFS; s++)
    {
        Surf * pSurf = &m_Surfs[s];

        if (!pSurf->Allocated())
            continue;

        if (m_CheckSurfacesOnlyIfWritten && !pSurf->HasUncheckedActions())
            continue;

        RC FastRC;
        if (destroyHostCSurface && (pSurf->GetLocation() == Memory::Fb))
        {
            // Try copying this GPU surface onto hostC, and check it that way.
            CHECK_RC( pSurf->GetDev()->CopyDefault(
                    pSurf->GetDst(),
                    0,
                    0,
                    m_Surfs[FPK_CHECKDMA_SURF_HostC].GetDst(),
                    0,
                    0,
                    pSurf->GetDst().GetPitch(),
                    pSurf->GetDst().GetHeight()));

            CHECK_RC( pSurf->GetDev()->WriteSemaphoreAndFlush() );
            CHECK_RC( pSurf->GetDev()->WaitSemaphore() );

            FastRC = CheckSurface(pSurf, &m_Surfs[FPK_CHECKDMA_SURF_HostC].GetDst());
            firstRc = FastRC;
            if (FastRC == OK)
            {
                // No need to re-check if the fast check passed.
                continue;
            }
            Printf(Tee::PriLow,
                    "Fast check failed on surface %s, now retrying with direct CPU byte-reads.\n",
                    pSurf->GetName());
        }

        // Either we aren't fast-checking, or the fast-check failed.
        // Check slowly, with 4-byte CPU reads over the bus.
        RC SlowRC = CheckSurface(pSurf, 0);
        firstRc = SlowRC;
        if (FastRC != OK && SlowRC == OK)
        {
            Printf(Tee::PriHigh,
                    "Slow re-check did not fail on surface %s.\n",
                    pSurf->GetName());
        }
    }

    return firstRc;
}

//------------------------------------------------------------------------------
RC CheckDma::CheckSurface
(
    CheckDma::Surf * pSurf,
    Surface2D *           pLocalCopy
)
{
    Printf(Tee::PriDebug, "CheckDma::CheckSurface %s %s\n",
            pSurf->GetName(),
            pLocalCopy ? "(local copy)" : "");

    RC rc;
    const bool IsCopy = (pLocalCopy != NULL);
    UINT32 BadBytesThisSurf = 0;

    SurfaceUtils::MappingSurfaceReader reader(IsCopy ? *pLocalCopy : pSurf->GetDst());

    // Check the list of Copy Actions to see that the Sent ones match
    // the src surface.
    //
    // If m_CheckFullSurface, also check that unsent actions are still
    // at the clear values, and that previously checked actions haven't
    // been overwritten since the last check.

    for (ActionListIter i = pSurf->GetActions().begin();
            i != pSurf->GetActions().end();
                ++i)
    {
        UINT32 BadBytesThisAction = 0;
        CHECK_RC((*i)->Check(
                reader,
                m_CheckFullSurface,
                IsCopy,
                &m_PrintedHeader,
                &BadBytesThisAction));

        if (BadBytesThisAction)
        {
            BadBytesThisSurf += BadBytesThisAction;

            if (!IsCopy)
            {
                // Log error info, since this is the definitive check.
                Action * pA = new Action(*(*i));
                m_FailedActions.push_back(pA);
                GetJsonExit()->AddFailLoop(pA->GetLoop());
            }
        }
    }

    if (m_CheckFullSurface)
    {
        // Check that any unallocated parts of the destination surface are
        // still at their cleared state.

        for (ActionListIter i = pSurf->GetFreeBlocks().begin();
                i != pSurf->GetFreeBlocks().end();
                    ++i)
        {
            UINT32 BadBytesThisAction = 0;
            CHECK_RC((*i)->Check(
                    reader,
                    m_CheckFullSurface,
                    IsCopy,
                    &m_PrintedHeader,
                    &BadBytesThisAction));

            if (BadBytesThisAction)
            {
                BadBytesThisSurf += BadBytesThisAction;

                if (!IsCopy)
                {
                    // Log error info, since this is the definitive check.
                    Action * pA = new Action(*(*i));
                    m_FailedActions.push_back(pA);
                }
            }
        }
    }

    if (!IsCopy)
        m_BadBytesTotal += BadBytesThisSurf;

    if (BadBytesThisSurf)
        return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
    else
        return OK;
}

//------------------------------------------------------------------------------
RC CheckDma::WriteSemaphoreAndFlush()
{
    RC rc;
    for (DevsIter i = m_Devs.begin(); i != m_Devs.end(); ++i)
    {
        CHECK_RC((*i)->WriteSemaphoreAndFlush());
    }
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::WaitSemaphore()
{
    RC rc;
    for (DevsIter i = m_Devs.begin(); i != m_Devs.end(); ++i)
    {
        CHECK_RC((*i)->WaitSemaphore());
    }
    return rc;
}

//------------------------------------------------------------------------------
void CheckDma::PickFbSurfMapping()
{
    m_DevInstForCpuOps = (*m_pPickers)[FPK_CHECKDMA_FB_MAPPING].Pick();

    // Force a valid FB surface:
    if ((m_DevInstForCpuOps >= CHECKDMA_MAX_GPUS) ||
        (!m_Surfs[FPK_CHECKDMA_SURF_FbGpu0 + m_DevInstForCpuOps].Allocated()))
    {
        m_DevInstForCpuOps = m_Devs[0]->DevInst();
    }

    Printf(Tee::PriDebug, "CheckDma::PickFbSurfMapping Gpu_%d",
            m_DevInstForCpuOps);
}

//------------------------------------------------------------------------------
void CheckDma::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    const char * FTstr[] = { "false", "true" };

    // Print the options we arrived at, so the user can detect .js file typos.
    Printf(pri, "CheckDma Js Properties:\n");
    Printf(pri, "\tSurfSizeOverride:\t\t0x%x (%.1fe6 bytes)\n",
            m_SurfSizeOverride,
            m_SurfSizeOverride/1.0e6);
    Printf(pri, "\tDontAllocHostC:\t\t\t%s\n", FTstr[m_DontAllocHostC]);
    Printf(pri, "\tCheckFullSurface:\t\t%s\n", FTstr[m_CheckFullSurface]);
    Printf(pri, "\tMaxCpuCopySize:\t\t\t%d\n", m_MaxCpuCopySize);
    Printf(pri, "\tAutoStatus:\t\t\t%s\n", FTstr[m_AutoStatus]);
    Printf(pri, "\tMaxMem2MemSize:\t\t\t%d\n", m_MaxMem2MemSize);
    Printf(pri, "\tCheckSurfacesOnlyIfWritten:\t%s\n", FTstr[m_CheckSurfacesOnlyIfWritten]);

    Printf(pri, "\tDeviceMask:\t\t\t%x (dev.sub", m_DeviceMask);

    GpuDevMgr *pMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    for (GpuDevice * pDev = pMgr->GetFirstGpuDevice();
            pDev;
                pDev = pMgr->GetNextGpuDevice(pDev))
    {
        if (GetDeviceMask() & (1<<pDev->GetDeviceInst()))
        {
            for (UINT32 s = 0; s < pDev->GetNumSubdevices(); s++)
                Printf(pri, " %d.%d", pDev->GetDeviceInst(), s);
        }
    }
    Printf(pri, ")\n");

    Printf(pri, "\tMappingSize:\t\t\t0x%llx (%.1fe6 bytes)\n",
           m_MappingSize,
           m_MappingSize/1.0e6);
    Printf(pri, "\tSrcOffsetToCorrupt:\t\t0x%x\n", m_SrcOffsetToCorrupt);
    Printf(pri, "\n");
}

//------------------------------------------------------------------------------
RC CheckDma::InitFromJs()
{
    RC rc;

    CHECK_RC( GpuTest::InitFromJs() );

    if (m_MaxCpuCopySize > m_MappingSize)
        m_MaxCpuCopySize = (UINT32) m_MappingSize;

    // Initially assume these are defined, clear them later if callback fails.
    m_DoPreFlushCallback = true;
    m_DoPreCheckCallback = true;
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::SetupJsonItems()
{
    RC rc;
    CHECK_RC(ModsTest::SetupJsonItems());

    JsArray jsaGpuInsts;
    for (int i = 0; i < 32; i++)
    {
        if (m_DeviceMask & (1<<i))
            jsaGpuInsts.push_back(INT_TO_JSVAL(i));
    }
    GetJsonEnter()->SetField("gpu_insts", &jsaGpuInsts);
    GetJsonExit()->SetField("gpu_insts", &jsaGpuInsts);

    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::SetDefaultsForPicker(UINT32 idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &fp = (*pPickers)[idx];

    switch (idx)
    {
        case FPK_CHECKDMA_SKIP:
            fp.ConfigConst(0);
            break;

        case FPK_CHECKDMA_SRC:
        case FPK_CHECKDMA_DST:
            fp.ConfigRandom();
            fp.AddRandRange(8, FPK_CHECKDMA_SURF_FbGpu0, 
                    FPK_CHECKDMA_SURF_FbGpu0 + CHECKDMA_MAX_GPUS - 1);
            fp.AddRandItem(1, FPK_CHECKDMA_SURF_HostC);
            fp.AddRandItem(1, FPK_CHECKDMA_SURF_HostNC);
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_ACTION:
            fp.ConfigRandom();
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CpuCpy);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_Twod);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng0);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng1);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng2);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng3);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng4);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng5);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng6);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng7);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng8);
            fp.AddRandItem(1, FPK_CHECKDMA_ACTION_CopyEng9);
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_SIZE_BYTES:
            fp.ConfigRandom();
            fp.AddRandRange(75,       1,            16*1024); // up to 16k
            fp.AddRandRange(15,  16*1024,          256*1024); // 16 to 256k
            fp.AddRandRange( 8, 256*1024,      16*1024*1024); // 256k to 16m
            fp.AddRandRange( 2, 16*1024*1024, 512*1024*1024); // 16m to 512m
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_WIDTH_BYTES:
            fp.ConfigRandom();
            fp.AddRandItem(14, 0);           // match screen pitch
            fp.AddRandItem(1, 16);
            fp.AddRandItem(1, 32);
            fp.AddRandItem(1, 64);
            fp.AddRandItem(1, 1024);
            fp.AddRandItem(1, 2048);
            fp.AddRandRange(1, 1, 8*1024);  // up to 8k
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_SRC_OFFSET:
            fp.ConfigRandom();
            fp.AddRandItem (1, 0);               // origin
            fp.AddRandRange(1, 0, 16*1024);      // small
            fp.AddRandRange(1, 0, 512*1024*1024);  // any
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_SHUFFLE_ACTIONS:
            fp.ConfigConst(1);
            break;

        case FPK_CHECKDMA_FORCE_PEER2PEER:
            fp.ConfigRandom();
            fp.AddRandItem(3, 1); // 75% peer-to-peer
            fp.AddRandItem(1, 0); // 25% any
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_VERBOSE:
            fp.ConfigConst(0);
            break;

        case FPK_CHECKDMA_FB_MAPPING:
            fp.ConfigRandom();
            fp.AddRandItem (1, FPK_CHECKDMA_FB_MAPPING_FbGpu0);
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_PEER_DIRECTION:
            fp.ConfigRandom();
            fp.AddRandItem(1, FPK_CHECKDMA_PEER_DIRECTION_Read);
            fp.AddRandItem(1, FPK_CHECKDMA_PEER_DIRECTION_Write);
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_CPU_WRITE_DIRECTION:
            fp.ConfigRandom();
            fp.AddRandItem(1, FPK_CHECKDMA_CPU_WRITE_DIRECTION_incr);
            fp.AddRandItem(1, FPK_CHECKDMA_CPU_WRITE_DIRECTION_decr);
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_FORCE_NOT_PEER2PEER:
            fp.ConfigConst(0);
            break;

        case FPK_CHECKDMA_SRC_ADDR_MODE:
            fp.ConfigRandom();
            fp.AddRandItem(1, FPK_CHECKDMA_SRC_ADDR_MODE_virtual);
            fp.AddRandItem(1, FPK_CHECKDMA_SRC_ADDR_MODE_physical);
            fp.CompileRandom();
            break;

        case FPK_CHECKDMA_DST_ADDR_MODE:
            fp.ConfigRandom();
            fp.AddRandItem(1, FPK_CHECKDMA_DST_ADDR_MODE_virtual);
            fp.AddRandItem(1, FPK_CHECKDMA_DST_ADDR_MODE_physical);
            fp.CompileRandom();
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
UINT32 CheckDma::GetErrorCount() const
{
   return m_BadBytesTotal;
}

//------------------------------------------------------------------------------
void CheckDma::PrintErrors(Tee::Priority pri)
{
    UINT32 numBadActions = 0;
    UINT32 numBadFreeBlocks = 0;

    if (m_BadBytesTotal)
    {
        ActionListIter i = m_FailedActions.begin();

        Printf(pri, "Failed copy Actions in last run were:\n");
        (*i)->PrintHeader(pri);

        for (/**/; i != m_FailedActions.end(); ++i)
        {
            (*i)->Print(pri);

            if ((*i)->GetState() == Action::Checked_Failed)
                numBadActions++;
            else
                numBadFreeBlocks++;
        }
        Printf(pri, "\n");
    }
    Printf(pri, "Total Bad: bytes=%d, Dmas=%d, Free Blocks=%d\n",
            m_BadBytesTotal, numBadActions, numBadFreeBlocks);
}
RC CheckDma::Status()
{
    PrintErrors(Tee::PriNormal);
    return OK;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CheckDma::Dev::Dev()
  : m_pTstCfg(0),
    m_pGpuDevice(0),
    m_pGpuSubdevice(0),
    m_MaxTransferSize(0)
{
}

//------------------------------------------------------------------------------
CheckDma::Dev::~Dev()
{
    Free();
}

//------------------------------------------------------------------------------
RC CheckDma::Dev::AllocChan (int cid)
{
    MASSERT(cid < ChanId_NUM);

    RC rc;
    Chan & chan = m_Chans[cid];
    chan.Flushes = 0;
    chan.Waits = 0;

    UINT32 engine = LW2080_ENGINE_TYPE_ALLENGINES;
    bool IsCopyEngine = false;

    // Figure out which HW object goes with this Chan.

    switch (cid)
    {
        case Twod:
            chan.pGrAlloc = &m_TwodAlloc;
            break;

        case CopyEngine0:
        case CopyEngine1:
        case CopyEngine2:
        case CopyEngine3:
        case CopyEngine4:
        case CopyEngine5:
        case CopyEngine6:
        case CopyEngine7:
        case CopyEngine8:
        case CopyEngine9:
            chan.pGrAlloc = &m_DmaCopyAllocs[cid - CopyEngine0];
            IsCopyEngine = true;
            engine = LW2080_ENGINE_TYPE_COPY(cid - CopyEngine0);
            break;
        default:
            MASSERT(!"Unknown ChanId");
    }

    // Skip allocating the channel if the HW object isn't supported.
    if (! chan.pGrAlloc->IsSupported(m_pGpuDevice))
        return OK;

    if (IsCopyEngine)
    {
        // Some GPU types have 1 copy-engine, others have 2 (or 0, though such
        // GPUs ought to have failed the IsSupported check above).

        vector<UINT32> engines;
        vector<UINT32> classIds;

        classIds.push_back(GF100_DMA_COPY);
        classIds.push_back(KEPLER_DMA_COPY_A);
        classIds.push_back(MAXWELL_DMA_COPY_A);
        classIds.push_back(PASCAL_DMA_COPY_A);
        classIds.push_back(PASCAL_DMA_COPY_B);
        classIds.push_back(VOLTA_DMA_COPY_A);
        classIds.push_back(TURING_DMA_COPY_A);
        classIds.push_back(AMPERE_DMA_COPY_A);
        classIds.push_back(AMPERE_DMA_COPY_B);
        classIds.push_back(HOPPER_DMA_COPY_A);
        classIds.push_back(BLACKWELL_DMA_COPY_A);

        CHECK_RC(m_pGpuDevice->GetPhysicalEnginesFilteredByClass(classIds, &engines));

        vector<UINT32>::const_iterator it = find(
                engines.begin(), engines.end(), engine);

        if (it == engines.end())
            return OK;
    }

    // The object is supported -- alloc a channel first.
    CHECK_RC(m_pTstCfg->AllocateChannelWithEngine(&chan.pCh,
                                                  &chan.hCh,
                                                  chan.pGrAlloc,
                                                  engine));

    // We want to control when flushing oclwrs, but we also would prefer not
    // to crash the test if we fill the pushbuffer without flushing.
    // Enable autoflush, but set the auto-flush threshold to the max allowed.

    CHECK_RC(chan.pCh->SetAutoFlush(true, 0xffffffff));
    CHECK_RC(chan.pCh->SetAutoGpEntry(true, 0xffffffff));

    // Alloc a small sysmem surface to use for semaphore writes.

    LwRm * pLwRm = chan.pCh->GetRmClient();
    const int x86PageSize = 4096;

    chan.SysmemSemaphore.SetLocation (m_pTstCfg->NotifierLocation());
    chan.SysmemSemaphore.SetLayout (Surface2D::Pitch);
    chan.SysmemSemaphore.SetPitch (x86PageSize);
    chan.SysmemSemaphore.SetWidth (x86PageSize);
    chan.SysmemSemaphore.SetHeight (1);
    chan.SysmemSemaphore.SetDepth (1);
    chan.SysmemSemaphore.SetKernelMapping (true);
    chan.SysmemSemaphore.SetColorFormat(ColorUtils::Y8);
    CHECK_RC(chan.SysmemSemaphore.Alloc (m_pGpuDevice, pLwRm));
    CHECK_RC(chan.SysmemSemaphore.Map(0));
    CHECK_RC(chan.SysmemSemaphore.BindGpuChannel(chan.hCh, pLwRm));
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Dev::Alloc
(
    GpuDevice * pGpuDev,
    GpuTestConfiguration * pTstCfg,
    UINT32 maxTransferSize
)
{
    RC rc;
    m_pGpuDevice = pGpuDev;
    m_pGpuSubdevice = m_pGpuDevice->GetSubdevice(0);
    m_pTstCfg = pTstCfg;
    m_MaxTransferSize = maxTransferSize;

    if (0 == m_MaxTransferSize)
    {
        m_MaxTransferSize = 0xffffffff;

        // User didn't specify.  Choose a sane default.
        if (m_pGpuDevice->CheckCapsBit(GpuDevice::BUG_114871) ||
            m_pGpuDevice->CheckCapsBit(GpuDevice::BUG_115115))
        {
            // Split transfers into ~256 K chunks (bug WAR).
            m_MaxTransferSize = 256*1024;
        }
    }
    Printf(Tee::PriDebug,
            "CheckDma::Dev::Alloc %d set MaxTransferSize to %d\n",
            m_pGpuDevice->GetDeviceInst(), m_MaxTransferSize);

    m_pTstCfg->BindGpuDevice(m_pGpuDevice);

    // Allocate a channel & object for each supported HW object.
    // The HW classes supported vary, some of these allocs are expected to
    // be skipped on any given GPU type.
    // At least one of them should work though!

    int numSupported = 0;
    for (int cid = 0; cid < ChanId_NUM; cid++)
    {
        CHECK_RC(AllocChan(cid));

        if (m_Chans[cid].hCh)
            numSupported++;
    }
    if (0 == numSupported)
    {
        Printf(Tee::PriHigh, "%s: No supported HW classes!\n", __FUNCTION__);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Dev::Free()
{
    for (int cid = 0; cid < ChanId_NUM; cid++)
    {
        if (m_Chans[cid].hCh)
        {
            MASSERT(m_Chans[cid].pGrAlloc);
            MASSERT(m_Chans[cid].pCh);

            m_Chans[cid].pGrAlloc->Free();

            m_pTstCfg->BindGpuDevice(m_pGpuDevice);
            m_pTstCfg->FreeChannel(m_Chans[cid].pCh);

            m_Chans[cid].SysmemSemaphore.Free();

            m_Chans[cid].hCh = 0;
            m_Chans[cid].pCh = NULL;
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
//! Set each object in its channel and point it at the semaphore/notifier surf.
RC CheckDma::Dev::InstantiateObjects()
{
    RC rc;

    for (int cid = 0; cid < ChanId_NUM; cid++)
    {
        if (HCh(cid))
        {
            Channel *   pCh  = m_Chans[cid].pCh;
            Surface2D & surf = m_Chans[cid].SysmemSemaphore;
            LwRm *      pLwRm = pCh->GetRmClient();

            CHECK_RC(pCh->SetObject (Class2Subchannel(m_Chans[cid].pGrAlloc),
                m_Chans[cid].pGrAlloc->GetHandle()));

            CHECK_RC(pCh->SetContextDmaSemaphore(
                    surf.GetCtxDmaHandleGpu(pLwRm)));
            CHECK_RC(pCh->SetSemaphoreOffset(
                    surf.GetCtxDmaOffsetGpu(pLwRm)));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Dev::WriteSemaphoreAndFlush()
{
    RC rc;

    for (int cid = 0; cid < ChanId_NUM; cid++)
    {
        if (HCh(cid))
        {
            Chan & chan = m_Chans[cid];
            if (chan.Flushes != chan.Waits)
            {
                // This channel is already Flushed since the last Wait.
                continue;
            }

            MEM_WR32(chan.SysmemSemaphore.GetAddress(), 0xffffffff);
            CHECK_RC(chan.pCh->SemaphoreRelease(++chan.Flushes));
            CHECK_RC(chan.pCh->Flush());
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
struct PollArgs
{
    void*  pSemaphoreMem;
    UINT32 ExpectedValue;
};

static bool PollSemaphoreWritten (void * pvPollArgs)
{
    PollArgs * pPollArgs = static_cast<PollArgs*>(pvPollArgs);

    return pPollArgs->ExpectedValue == MEM_RD32(pPollArgs->pSemaphoreMem);
}

RC CheckDma::Dev::WaitSemaphore()
{
    RC rc;

    for (int cid = 0; cid < ChanId_NUM; cid++)
    {
        Chan & chan = m_Chans[cid];
        if (chan.Flushes != chan.Waits)
        {
            const FLOAT64 timeoutMs = m_pTstCfg->TimeoutMs();
            PollArgs pa;
            pa.pSemaphoreMem = chan.SysmemSemaphore.GetAddress();
            pa.ExpectedValue = chan.Flushes;

            if (chan.SysmemSemaphore.GetLocation() == Memory::Fb)
            {
                rc = POLLWRAP_HW( &PollSemaphoreWritten, &pa, timeoutMs );
            }
            else
            {
                rc = POLLWRAP( &PollSemaphoreWritten, &pa, timeoutMs );
            }
            if (OK != rc)
                return rc;

            chan.Waits = chan.Flushes;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Dev::FillTwod
(
    Surface2D & surf,
    UINT32      pattern
)
{
    RC rc;
    UINT64 off = GetCtxDmaOffset(surf);
    UINT32 pitch = surf.GetPitch();
    UINT32 width = pitch / sizeof(UINT32);
    UINT32 height = UINT32(surf.GetSize() / pitch);

    Channel * pCh = m_Chans[Twod].pCh;
    MASSERT(pCh);

    // Send one "pixel from CPU" that will be magnified to cover the entire
    // dest half-surface.

    MASSERT(surf.GetWidth() <= width);
    MASSERT(surf.GetLayout() == Surface2D::Pitch);

    const UINT32 SubChan = Class2Subchannel(m_Chans[Twod].pGrAlloc);

    switch(m_TwodAlloc.GetClass())
    {
        case LW50_TWOD:
            CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_CONTEXT_DMA,
                m_pGpuDevice->GetMemSpaceCtxDma(surf.GetLocation(), false)));
            break;
        default:
            ;
    }

    UINT32 heightRemaining = height;

    while (heightRemaining > 0)
    {
        // If there are >= 64K lines process (64K-1) of them
        // Otherwise process all remaining lines
        UINT32 heightChunk = min<UINT32>(65535, heightRemaining);
        heightRemaining -= heightChunk;

        CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_OFFSET_UPPER,  0xff & UINT32(off >> 32)));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_OFFSET_LOWER,  UINT32(off)));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_FORMAT,        LW502D_SET_DST_FORMAT_V_A8R8G8B8));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_WIDTH,         width));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_HEIGHT,        heightChunk));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_MEMORY_LAYOUT, LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_PITCH,         pitch));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_OPERATION,         LW502D_SET_OPERATION_V_SRCCOPY));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE, 2,
                             LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR,
                             LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8));
        CHECK_RC(pCh->Write(SubChan, LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH, 10,
            /* LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH */      1,
            /* LW502D_SET_PIXELS_FROM_CPU_SRC_HEIGHT */     1,
            /* LW502D_SET_PIXELS_FROM_CPU_DX_DU_FRAC */     0,
            /* LW502D_SET_PIXELS_FROM_CPU_DX_DU_INT */      width,
            /* LW502D_SET_PIXELS_FROM_CPU_DY_DV_FRAC */     0,
            /* LW502D_SET_PIXELS_FROM_CPU_DY_DV_INT */      heightChunk,
            /* LW502D_SET_PIXELS_FROM_CPU_DST_X0_FRAC */    0,
            /* LW502D_SET_PIXELS_FROM_CPU_DST_X0_INT */     0,
            /* LW502D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC */    0,
            /* LW502D_SET_PIXELS_FROM_CPU_DST_Y0_INT */     0));
        CHECK_RC(pCh->Write(SubChan, LW502D_PIXELS_FROM_CPU_DATA,     pattern));
        CHECK_RC(WriteSemaphoreAndFlush());
        CHECK_RC(WaitSemaphore());
    }
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Dev::CopyTwod
(
    Surface2D & src,
    UINT32      srcX,
    UINT32      srcY,
    Surface2D & dst,
    UINT32      dstX,
    UINT32      dstY,
    UINT32      width,
    UINT32      height
)
{
    RC rc;

    Channel * pCh = m_Chans[Twod].pCh;
    MASSERT(pCh);

    const UINT32 SubChan = Class2Subchannel(m_Chans[Twod].pGrAlloc);

    // Assumes pitch layout.
    MASSERT(src.GetLayout() == Surface2D::Pitch);
    MASSERT(dst.GetLayout() == Surface2D::Pitch);

    UINT64 srcBase64 = GetCtxDmaOffset(src);
    UINT64 dstBase64 = GetCtxDmaOffset(dst);

    UINT32 srcPitch = src.GetPitch();
    UINT32 dstPitch = dst.GetPitch();

    switch(m_TwodAlloc.GetClass())
    {
        case LW50_TWOD:
            CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_CONTEXT_DMA,
                    m_pGpuDevice->GetMemSpaceCtxDma(dst.GetLocation(), false)));
            break;
        default:
            ;
    }

    // Y8 layout is inefficient, a8r8g8b8 is much faster on g8x/g9x/gt2xx.
    // But that probably doesn't matter when we're bottlenecked on PEX bus,
    // and it would impose alignment problems on starting/ending X offsets.
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_OFFSET_UPPER,  0xff & UINT32(dstBase64 >> 32)));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_OFFSET_LOWER,  UINT32(dstBase64)));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_FORMAT,        LW502D_SET_DST_FORMAT_V_Y8));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_WIDTH,         dstPitch));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_HEIGHT,        dst.GetHeight()));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_MEMORY_LAYOUT, LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_DST_PITCH,         dstPitch));

    switch(m_TwodAlloc.GetClass())
    {
        case LW50_TWOD:
            CHECK_RC(pCh->Write(SubChan, LW502D_SET_SRC_CONTEXT_DMA,
                    m_pGpuDevice->GetMemSpaceCtxDma(src.GetLocation(), false)));
            break;
        default:
            ;
    }
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_SRC_OFFSET_UPPER,  0xff & UINT32(srcBase64 >> 32)));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_SRC_OFFSET_LOWER,  UINT32(srcBase64)));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_SRC_FORMAT,        LW502D_SET_SRC_FORMAT_V_Y8));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_SRC_WIDTH,         srcPitch));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_SRC_HEIGHT,        src.GetHeight()));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_SRC_MEMORY_LAYOUT, LW502D_SET_SRC_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(pCh->Write(SubChan, LW502D_SET_SRC_PITCH,         srcPitch));

    CHECK_RC(pCh->Write(SubChan, LW502D_SET_PIXELS_FROM_MEMORY_DST_X0, 12,
            /* LW502D_SET_PIXELS_FROM_MEMORY_DST_X0 */          dstX,
            /* LW502D_SET_PIXELS_FROM_MEMORY_DST_Y0 */          dstY,
            /* LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH */       width,
            /* LW502D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT */      height,
            /* LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_FRAC */      0,
            /* LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_INT */       1,
            /* LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_FRAC */      0,
            /* LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_INT */       1,
            /* LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_FRAC */     0,
            /* LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT */      srcX,
            /* LW502D_SET_PIXELS_FROM_MEMORY_SRC_Y0_FRAC */     0,
            /* LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT */          srcY));

    return OK;
}

//------------------------------------------------------------------------------
RC CheckDma::Dev::CopyCopyEngine
(
    int         engine,
    Surface2D & src,
    UINT32      srcX,
    UINT32      srcY,
    bool        bSrcPhysAddr,
    Surface2D & dst,
    UINT32      dstX,
    UINT32      dstY,
    bool        bDstPhysAddr,
    UINT32      width,
    UINT32      height
)
{
    RC rc;

    Chan & chan = m_Chans[CopyEngine0 + engine];
    Channel * pCh = chan.pCh;
    Surface2D & SysmemSemaphore = chan.SysmemSemaphore;
    MASSERT(pCh);

    const UINT32 SubChan = Class2Subchannel(m_Chans[CopyEngine0 + engine].pGrAlloc);

    // Assumes pitch layout.
    MASSERT(src.GetLayout() == Surface2D::Pitch);
    MASSERT(dst.GetLayout() == Surface2D::Pitch);

    const UINT32 srcPitch = src.GetPitch();
    const UINT32 dstPitch = dst.GetPitch();
    UINT64 srcOffset64;
    if (bSrcPhysAddr)
    {
        MASSERT(src.GetLocation() == Memory::Fb);
        srcOffset64 = src.GetVidMemOffset();
    }
    else
    {
        srcOffset64 = GetCtxDmaOffset(src);
    }
    srcOffset64 += srcX + static_cast<UINT64>(srcY) * srcPitch;

    UINT64 dstOffset64;
    if (bDstPhysAddr)
    {
        MASSERT(dst.GetLocation() == Memory::Fb);
        dstOffset64 = dst.GetVidMemOffset();
    }
    else
    {
        dstOffset64 = GetCtxDmaOffset(dst);
    }
    dstOffset64 += dstX + static_cast<UINT64>(dstY) * dstPitch;

    // Offset the semaphore by 16 bytes to avoid the low-16-byte section
    // being used for channel semaphores.
    const UINT64 semaBase64 = SysmemSemaphore.GetCtxDmaOffsetGpu() + 16;

    const bool isZeroBytes = (0 == (width * height));

    UINT32 ClassID = chan.pGrAlloc->GetClass();
    MASSERT((ClassID == GF100_DMA_COPY)     || (ClassID == KEPLER_DMA_COPY_A) ||
            (ClassID == MAXWELL_DMA_COPY_A) || (ClassID == PASCAL_DMA_COPY_A) ||
            (ClassID == PASCAL_DMA_COPY_B)  || (ClassID == VOLTA_DMA_COPY_A)  ||
            (ClassID == TURING_DMA_COPY_A)  || (ClassID == AMPERE_DMA_COPY_A) ||
            (ClassID == AMPERE_DMA_COPY_B)  || (ClassID == HOPPER_DMA_COPY_A) ||
            (ClassID == BLACKWELL_DMA_COPY_A));

    if (ClassID == GF100_DMA_COPY)
    {
        CHECK_RC(pCh->Write(SubChan, LW90B5_SET_APPLICATION_ID,
                    DRF_DEF(90B5, _SET_APPLICATION_ID, _ID, _NORMAL)));
    }

    CHECK_RC(pCh->Write(SubChan, LW90B5_OFFSET_IN_UPPER, 8,
            /* LW90B5_OFFSET_IN_UPPER */
            UINT32(srcOffset64 >> 32),
            /* LW90B5_OFFSET_IN_LOWER */
            UINT32(srcOffset64),
            /* LW90B5_OFFSET_OUT_UPPER */
            UINT32(dstOffset64 >> 32),
            /* LW90B5_OFFSET_OUT_LOWER */
            UINT32(dstOffset64),
            /* LW90B5_PITCH_IN */
            srcPitch,
            /* LW90B5_PITCH_OUT */
            dstPitch,
            /* LW90B5_LINE_LENGTH_IN */
            width,
            /* LW90B5_LINE_COUNT */
            height));

    CHECK_RC(pCh->Write(SubChan, LW90B5_SET_SEMAPHORE_A, 2,
            /* LW90B5_SET_SEMAPHORE_A */
            UINT32(semaBase64 >> 32),
            /* LW90B5_SET_SEMAPHORE_B */
            UINT32(semaBase64)));
            /* LW90B5_SET_SEMAPHORE_PAYLOAD -- don't care */

    UINT32 layoutMem =
            DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);

    if (ClassID == GF100_DMA_COPY)
    {
        UINT32 AddressMode = bSrcPhysAddr ? DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _PHYSICAL) :
                                            DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _VIRTUAL);

        AddressMode |= bDstPhysAddr ? DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _PHYSICAL) :
                                      DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _VIRTUAL);

        switch (src.GetLocation())
        {
            case Memory::Fb:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _LOCAL_FB);
                break;

            case Memory::Coherent:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _COHERENT_SYSMEM);
                break;

            case Memory::NonCoherent:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _NONCOHERENT_SYSMEM);
                break;

            default:
                MASSERT(!"Bad src location");
        }
        switch (dst.GetLocation())
        {
            case Memory::Fb:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET, _LOCAL_FB);
                break;

            case Memory::Coherent:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET, _COHERENT_SYSMEM);
                break;

            case Memory::NonCoherent:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET, _NONCOHERENT_SYSMEM);
                break;

            default:
                MASSERT(!"Bad dst location");
        }

        CHECK_RC(pCh->Write(SubChan, LW90B5_ADDRESSING_MODE, AddressMode));
    }
    else
    {
        layoutMem |= bSrcPhysAddr ? DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _PHYSICAL) :
                                    DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
        layoutMem |= bDstPhysAddr ? DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _PHYSICAL) :
                                    DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);

        GpuDevMgr *pMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
        if (bSrcPhysAddr)
        {
            UINT32 srcPhysMode;
            if (src.GetGpuDev() == m_pGpuDevice)
            {
                srcPhysMode = DRF_DEF(C6B5, _SET_SRC_PHYS_MODE, _TARGET, _LOCAL_FB);
            }
            else
            {
                UINT32 peerId;
                CHECK_RC(pMgr->GetPeerId(chan.pCh->GetRmClient(),
                                         m_pGpuDevice->GetSubdevice(0),
                                         src.GetGpuDev()->GetSubdevice(0),
                                         GpuDevMgr::GPA,
                                         &peerId));
                srcPhysMode = DRF_DEF(C6B5, _SET_SRC_PHYS_MODE, _TARGET, _PEERMEM) |
                              DRF_NUM(C6B5, _SET_SRC_PHYS_MODE, _PEER_ID, peerId);

            }
            CHECK_RC(pCh->Write(SubChan, LWC6B5_SET_SRC_PHYS_MODE, srcPhysMode));
        }
        if (bDstPhysAddr)
        {
            UINT32 dstPhysMode;
            if (dst.GetGpuDev() == m_pGpuDevice)
            {
                dstPhysMode = DRF_DEF(C6B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB);
            }
            else
            {
                UINT32 peerId;
                CHECK_RC(pMgr->GetPeerId(chan.pCh->GetRmClient(),
                                         m_pGpuDevice->GetSubdevice(0),
                                         dst.GetGpuDev()->GetSubdevice(0),
                                         GpuDevMgr::GPA,
                                         &peerId));
                dstPhysMode = DRF_DEF(C6B5, _SET_DST_PHYS_MODE, _TARGET, _PEERMEM) |
                              DRF_NUM(C6B5, _SET_DST_PHYS_MODE, _PEER_ID, peerId);

            }
            CHECK_RC(pCh->Write(SubChan, LWC6B5_SET_DST_PHYS_MODE, dstPhysMode));
        }
    }

    UINT32 LaunchArg =
            /* 0 == LW90B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE */
            /* 0 == LW90B5_LAUNCH_DMA_REMAP_ENABLE_FALSE */
            layoutMem |
            DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
            DRF_DEF(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _TRUE);

    if (isZeroBytes)
    {
        LaunchArg = LaunchArg |
            DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
            DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NONE);
    }
    else
    {
        // Use NON_PIPELINED to WAR hw bug 618956.
        LaunchArg = LaunchArg |
            DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _NONE) |
            DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED);
    }

    CHECK_RC(pCh->Write(SubChan, LW90B5_LAUNCH_DMA, LaunchArg));

    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Dev::CopyDefault
(
    Surface2D & src,
    UINT32      srcX,
    UINT32      srcY,
    Surface2D & dst,
    UINT32      dstX,
    UINT32      dstY,
    UINT32      width,
    UINT32      height
)
{
    for (UINT32 cpyEng = 0; cpyEng < NUM_SUPPORTED_COPY_ENGINES; ++cpyEng)
    {
        if (HCh(CopyEngine0 + cpyEng))
        {
            return CopyCopyEngine(cpyEng,
                                  src, srcX, srcY, false,
                                  dst, dstX, dstY, false,
                                  width, height);
        }
    }

    MASSERT(!"All gpus are expected to support CE");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
UINT64 CheckDma::Dev::GetCtxDmaOffset(Surface2D & surf)
{
    if (surf.GetGpuDev() != m_pGpuDevice)
    {
        if (surf.GetLocation() == Memory::Fb)
            return surf.GetCtxDmaOffsetGpuPeer(0, m_pGpuDevice, 0);
        else
            return surf.GetCtxDmaOffsetGpuShared(m_pGpuDevice);
    }
    else
    {
        return surf.GetCtxDmaOffsetGpu();
    }
}

//------------------------------------------------------------------------------
LwRm::Handle CheckDma::Dev::HCh (int cid) const
{
    MASSERT(cid < ChanId_NUM);
    return m_Chans[cid].hCh;
}

//------------------------------------------------------------------------------
UINT32 CheckDma::Dev::DefaultCopyAction()
{
    for (UINT32 cpyEng = 0; cpyEng < NUM_SUPPORTED_COPY_ENGINES; ++cpyEng)
    {
        if (HCh(CopyEngine0 + cpyEng))
            return FPK_CHECKDMA_ACTION_CopyEng0 + cpyEng;
    }

    MASSERT(!"All gpus are expected to support CE");
    return FPK_CHECKDMA_ACTION_CpuCpy;
}

//------------------------------------------------------------------------------
bool CheckDma::Dev::IsPhysAddrSupported(int chanId) const
{
    if ((chanId >= CopyEngine0) && (chanId <= CopyEngine9))
    {
        const Chan & chan = m_Chans[chanId];
        MASSERT(m_Chans[chanId].hCh);
        if (m_Chans[chanId].hCh == 0)
            return false;
        switch (chan.pGrAlloc->GetClass())
        {
            // Fermi code should be removed, but kepler seems to be handling
            // physical addresses in a different way than Maxwell+ resulting in
            // arbitrary rectangle copy operations not copying the expected
            // rectangle and causing miscompares.
            case GF100_DMA_COPY:
            case KEPLER_DMA_COPY_A:
                break;
            case MAXWELL_DMA_COPY_A:
            case PASCAL_DMA_COPY_A:
            case PASCAL_DMA_COPY_B:
            case VOLTA_DMA_COPY_A:
            case TURING_DMA_COPY_A:
            case AMPERE_DMA_COPY_A:
            case AMPERE_DMA_COPY_B:
            case HOPPER_DMA_COPY_A:
            case BLACKWELL_DMA_COPY_A:
                return true;
            default:
                MASSERT(!"Unknown copy engine class");
                break;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
bool CheckDma::Dev::IsPeerPhysAddrSupported(int chanId) const
{
    if ((chanId >= CopyEngine0) && (chanId <= CopyEngine9))
    {
        const Chan & chan = m_Chans[chanId];
        MASSERT(m_Chans[chanId].hCh);
        if (m_Chans[chanId].hCh == 0)
            return false;
        switch (chan.pGrAlloc->GetClass())
        {
            case GF100_DMA_COPY:
            case KEPLER_DMA_COPY_A:
            case MAXWELL_DMA_COPY_A:
            case PASCAL_DMA_COPY_A:
            case PASCAL_DMA_COPY_B:
            case VOLTA_DMA_COPY_A:
            case TURING_DMA_COPY_A:
                break;
            case AMPERE_DMA_COPY_A:
            case AMPERE_DMA_COPY_B:
            case HOPPER_DMA_COPY_A:
            case BLACKWELL_DMA_COPY_A:
                return true;
            default:
                MASSERT(!"Unknown copy engine class");
                break;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
CheckDma::Surf::Surf
(
) : m_pDev(0),
    m_SrcFill(0),
    m_DstFill(0)
{
}

//------------------------------------------------------------------------------
CheckDma::Surf::~Surf()
{
    Free();
    ClearFreeBlocks();
}

//------------------------------------------------------------------------------
RC CheckDma::Surf::Alloc
(
    Memory::Location loc,
    CheckDma::Dev * pDev,
    UINT32 pitch,
    const GpuTestConfiguration & tstCfg,
    UINT32 sizeOverride
)
{
    RC rc;

    m_pDev  = pDev;

    if (loc == Memory::Fb)
    {
        char tmp[20];
        sprintf(tmp, "FB_%d", m_pDev->DevInst());
        m_Name = tmp;
    }
    else
    {
        if (loc == Memory::Coherent)
            m_Name = "HostCoh";
        else if (loc == Memory::NonCoherent)
            m_Name = "HostNC";
        else
            MASSERT(!"Invalid parameter 'loc'.");  // code rot, shouldn't get here!
    }

    Printf(Tee::PriDebug, "CheckDma::Surf::Alloc %s\n", m_Name.c_str());

    UINT32 height = tstCfg.SurfaceHeight();
    if (sizeOverride)
        height = max(UINT32(16), sizeOverride / pitch);

    UINT32 width = pitch / ((tstCfg.DisplayDepth() + 7)/8);

    for (UINT32 i = 0; i < 2; i++)
    {
        Surface2D * pSurf;
        if (0 == i)
            pSurf = &m_Src;
        else
            pSurf = &m_Dst;

        if (loc == Memory::Fb)
        {
            pSurf->SetDisplayable(true);

            // This is required for physical copies, in general RM will always give out
            // contiguous FB allocations but force it just in case
            pSurf->SetPhysContig(true);
        }
        else
        {
            pSurf->SetDisplayable(false);
        }
        pSurf->SetAddressModel(Memory::Paging);
        pSurf->SetWidth(width);
        pSurf->SetHeight(height);
        pSurf->SetColorFormat(ColorUtils::ColorDepthToFormat(tstCfg.DisplayDepth()));
        pSurf->SetLocation(loc);
        pSurf->SetProtect(Memory::ReadWrite);
        pSurf->SetLayout(Surface2D::Pitch);
        pSurf->SetDmaProtocol(tstCfg.DmaProtocol());
        CHECK_RC(pSurf->Alloc(m_pDev->GpuDev()));

        // Don't map the surface to CPU address space.
        // FB surfaces may be bigger than the GPU's BAR1 address space.
        // Keep this test simpler by handling mapping operations the same way for
        // all surfaces.
    }
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Surf::Free()
{
    m_Src.Free();
    m_Dst.Free();
    return OK;
}

//------------------------------------------------------------------------------
RC CheckDma::Surf::FillSrc()
{
    if (!Allocated())
        return OK;

    RC rc;

    Printf(Tee::PriDebug, "CheckDma::Surf::FillSrc %s\n", m_Name.c_str());

    // Set src fill pattern.
    switch (m_Src.GetLocation())
    {
        case Memory::Coherent:
            m_SrcFill = FPK_CHECKDMA_SURF_HostC;
            break;
        case Memory::NonCoherent:
            m_SrcFill = FPK_CHECKDMA_SURF_HostNC;
            break;
        default:
            m_SrcFill = FPK_CHECKDMA_SURF_FbGpu0 + m_pDev->DevInst();
            break;
    }
    m_SrcFill <<= SrcFillShift;

    // Use a buffer in sysmem as a staging buf, to avoid inefficient
    // 32-bit writes over pex bus.
    const UINT32 bufBytes = 4096*2;
    const UINT32 bufWords = bufBytes/sizeof(UINT32);
    vector<UINT32> buf (bufWords, 0);

    // We use a utility class to handle details of mapping parts of the
    // surface to CPU address space.  It may be too big to map all at once.
    SurfaceUtils::SurfaceMapper mapper(m_Src);

    for (UINT64 offset = 0; offset < m_Src.GetSize(); /**/)
    {
        // Outer loop: map a few MB of the buf to BAR1.
        CHECK_RC(mapper.Remap(offset, SurfaceUtils::SurfaceMapper::ReadWrite));

        const UINT64 mappedOffset = mapper.GetMappedOffset();
        const UINT64 endOffset    = mappedOffset + mapper.GetMappedSize();

        while (offset < endOffset)
        {
            // Inner loop: fill a chunk of pattern, copy to BAR1.
            for (UINT32 i = 0; i < bufWords; ++i)
                buf[i] = GetSrcFill(UINT32(offset) + i*sizeof(UINT32));

            const UINT32 bytesToCopy = min(bufBytes, UINT32(endOffset-offset));

            Platform::VirtualWr(
                    mapper.GetMappedAddress() + (offset - mappedOffset),
                    &buf[0], bytesToCopy);
            offset += bytesToCopy;
        }
    }
    mapper.Unmap();

    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Surf::ClearDst(UINT08 pattern)
{
    if (!Allocated())
        return OK;

    RC rc;
    m_DstFill = pattern;

    // Clear the list of copy actions that will write to this surface.
    m_CopyActions.clear();

    // Reset the list of blocks describing unwritten parts of this surface.
    ClearFreeBlocks();
    Action * pAct = new Action;
    pAct->SetFreeBlock( 0, 0, m_Dst.GetPitch(), m_Dst.GetHeight(), this );
    AddFreeBlock( pAct );

    UINT32 pattern32 = pattern | (pattern<<8) | (pattern<<16) | (pattern<<24);

    if (m_Dst.GetLocation() == Memory::Fb && m_pDev->HCh(Twod))
    {
        // Use fast GPU fill to avoid slow pex bus operations.
        CHECK_RC(m_pDev->FillTwod(m_Dst, pattern32));
    }
    else
    {
        // Use CPU writes to fill.  Still pretty fast if using sysmem.

        // We use a utility class to handle details of mapping parts of the
        // surface to CPU address space.  It may be too big to map all at once.
        SurfaceUtils::SurfaceMapper mapper(m_Dst);

        for (UINT64 offset = 0; offset < m_Dst.GetSize(); /**/)
        {
            CHECK_RC(mapper.Remap(offset, SurfaceUtils::SurfaceMapper::ReadWrite));

            UINT32 * p    = (UINT32*)(mapper.GetMappedAddress() +
                                      offset - mapper.GetMappedOffset());
            UINT32 * pEnd = (UINT32*)(mapper.GetMappedAddress() +
                                      mapper.GetMappedSize());

            while (p < pEnd)
            {
                MEM_WR32(p, pattern32);
                p++;
            }
            offset = mapper.GetMappedEndOffset();;
        }
        mapper.Unmap();
    }
    return rc;
}

//------------------------------------------------------------------------------
void CheckDma::Surf::ClearFreeBlocks()
{
    for (ActionListIter i = m_FreeBlocks.begin(); i != m_FreeBlocks.end(); ++i)
        delete (*i);

    m_FreeBlocks.clear();
}

//------------------------------------------------------------------------------
//! Release the first free block to the caller (Action.SetSize).
//! Caller is responsible for the memory.
CheckDma::Action * CheckDma::Surf::ConsumeFreeBlock()
{
    Action * tmp = m_FreeBlocks.front();
    m_FreeBlocks.pop_front();
    return tmp;
}

//------------------------------------------------------------------------------
void CheckDma::Surf::AddFreeBlock(Action * pFree)
{
    m_FreeBlocks.push_front(pFree);
}

//------------------------------------------------------------------------------
void CheckDma::Surf::AddCopyAction(Action * pAct)
{
    m_CopyActions.push_back(pAct);
}

//------------------------------------------------------------------------------
bool CheckDma::Surf::HasUncheckedActions()
{
    for (ActionListIter i = m_CopyActions.begin();
            i != m_CopyActions.end(); ++i)
    {
        if ((*i)->GetState() == Action::Sent)
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
void CheckDma::Surf::Dump
(
    bool DumpDst,
    bool IsCopy,
    UINT32 errX,
    UINT32 errY,
    SurfaceUtils::MappingSurfaceReader & reader
)
{
    // We will dump a rectangular region of the dest containing the error,
    // containing DumpBytes bytes or so.

    Surface2D * pSurf2D = DumpDst ? &m_Dst : &m_Src;
    UINT32 pitch = pSurf2D->GetPitch();

    // Horizontal start of dumped area, align to 4 bytes.
    UINT32 dumpX = (errX == 0) ? 0 : ((errX - 1) & ~3);

    // Width (bytes) of dumped area (usually same as DumpBytes).
    UINT32 dumpW = DumpBytes;
    if (DumpBytes + dumpX > pitch)
        dumpW = pitch - dumpX;

    Printf(Tee::PriLow, "  Dumping %s%s.%s @%d,%d to %d,%d (err at %d,%d):",
            IsCopy ? "(copy of)" : "",
            GetName(),
            DumpDst ? "dst" : "src",
            dumpX,
            errY,
            dumpX + dumpW - 1,
            errY,
            errX,
            errY);

    vector<UINT08> linebuf(dumpW, 0);
    RC rc = reader.ReadBytes(pitch * errY + dumpX, &linebuf[0], dumpW);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "Unexpected error in MappingSurfaceReader::ReadBytes\n");
        return;
    }

    if (pSurf2D->GetLocation() == Memory::Fb)
    {
        UINT64 offs = pSurf2D->GetVidMemOffset() + dumpX + errY * pitch;
        Printf(Tee::PriLow, "\n    FbOff %08X ", UINT32(offs));
    }
    else
    {
        UINT64 offs = pSurf2D->GetPhysAddress() + dumpX + errY * pitch;
        Printf(Tee::PriLow, "\n    offs  %08X ", UINT32(offs));
    }

    for (UINT32 x = 0; x < dumpW; x++)
    {
        Printf(Tee::PriLow, " %02x", linebuf[x]);
    }
    Printf(Tee::PriLow, "\n");
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CheckDma::Action::Action()
{
    SetFreeBlock(0,0,0,0,0);
}

//------------------------------------------------------------------------------
void CheckDma::Action::SetFreeBlock
(
    UINT32 x,
    UINT32 y,
    UINT32 w,
    UINT32 h,
    CheckDma::Surf * pDst
)
{
    m_Type = FPK_CHECKDMA_ACTION_Noop;
    m_State = Free;
    m_pSrc = 0;
    m_pDst = pDst;
    m_pDev = pDst ? pDst->GetDev() : 0;
    m_SrcX = 0;
    m_SrcY = 0;
    m_X    = x;
    m_Y    = y;
    m_W    = w;
    m_H    = h;
    m_Loop = 0;
    m_Incr = true;
    m_bSrcAddrPhysical = false;
    m_bDstAddrPhysical = false;
    m_Pri  = Tee::PriSecret;
}

//------------------------------------------------------------------------------
CheckDma::Action::Action(const Action & A)
{
    m_Type = A.m_Type;
    m_State = A.m_State;
    m_pSrc = A.m_pSrc;
    m_pDst = A.m_pDst;
    m_pDev = A.m_pDev;
    m_SrcX = A.m_SrcX;
    m_SrcY = A.m_SrcY;
    m_X = A.m_X;
    m_Y = A.m_Y;
    m_W = A.m_W;
    m_H = A.m_H;
    m_Loop = A.m_Loop;
    m_Incr = true;
    m_bSrcAddrPhysical = A.m_bSrcAddrPhysical;
    m_bDstAddrPhysical = A.m_bDstAddrPhysical;
    m_Pri = A.m_Pri;
}

//------------------------------------------------------------------------------
CheckDma::Action::~Action()
{
}

//------------------------------------------------------------------------------
void CheckDma::Action::PrintHeader(Tee::Priority pri)
{
    Printf(pri,
"Loop State OpType   Dev Source  SrcAddr Destin  DstAddr bytes   srcX Y    W    H    dstX Y    dir Bus Traffic\n");
    Printf(pri,
"==== ===== ======== === ======= ======= ======= ======= ======= ==== ==== ==== ==== ==== ==== === ===========\n");
}

//------------------------------------------------------------------------------
void CheckDma::Action::Print(Tee::Priority pri)
{
    const char * str_states[NUM_States] =
        {
            "Free",
            "Rdy",
            "Sent",
            "OK",
            "Fail",
            "Skip"
        };
    const char * str_types[FPK_CHECKDMA_ACTION_NUM_ACTIONS] =
        {
            "Noop",
            "CpuCpy",
            "Twod",
            "CopyEn0",
            "CopyEn1",
            "CopyEn2",
            "CopyEn3",
            "CopyEn4",
            "CopyEn5",
            "CopyEn6",
            "CopyEn7",
            "CopyEn8",
            "CopyEn9"
        };

    Printf(pri, "%4d %5s %8s",
            m_Loop,
            str_states[m_State],
            str_types[m_Type]);

    if (m_Type == FPK_CHECKDMA_ACTION_Noop)
    {
        Printf(pri, "\n");
        return;
    }

    switch (m_Type)
    {
        case FPK_CHECKDMA_ACTION_Twod:
        case FPK_CHECKDMA_ACTION_CopyEng0:
        case FPK_CHECKDMA_ACTION_CopyEng1:
        case FPK_CHECKDMA_ACTION_CopyEng2:
        case FPK_CHECKDMA_ACTION_CopyEng3:
        case FPK_CHECKDMA_ACTION_CopyEng4:
        case FPK_CHECKDMA_ACTION_CopyEng5:
        case FPK_CHECKDMA_ACTION_CopyEng6:
        case FPK_CHECKDMA_ACTION_CopyEng7:
        case FPK_CHECKDMA_ACTION_CopyEng8:
        case FPK_CHECKDMA_ACTION_CopyEng9:
            Printf(pri, " %3d", m_pDev->DevInst());
            break;

        case FPK_CHECKDMA_ACTION_CpuCpy:
            Printf(pri, "  na");
            break;

        default:
            MASSERT(!"bad Action type");
            break;
    }

    Printf(pri, " %7s %7s %7s %7s %7d %4d %-4d %4d %-4d %4d %-4d %s",
            m_pSrc->GetName(),
            m_bSrcAddrPhysical ? "Phys" : "Virt",
            m_pDst->GetName(),
            m_bDstAddrPhysical ? "Phys" : "Virt",
            m_W * m_H,
            m_SrcX,
            m_SrcY,
            m_W,
            m_H,
            m_X,
            m_Y,
            m_Incr ? "++ " : "-- "
            );

    bool dstIsFb = m_pDst->GetLocation() == Memory::Fb;
    bool srcIsFb = m_pSrc->GetLocation() == Memory::Fb;

    // Print what kind of bus traffic oclwrs in this action:

    if (m_Type == FPK_CHECKDMA_ACTION_CpuCpy)
    {
        if (dstIsFb)
            Printf(pri, " CpuWr_BAR1");
        if (srcIsFb)
            Printf(pri, " CpuRd_BAR1");
    }
    else
    {
        if (dstIsFb && m_pDev != m_pDst->GetDev())
            Printf(pri, " GpuWr_Peer");
        if (srcIsFb && m_pDev != m_pSrc->GetDev())
            Printf(pri, " GpuRd_Peer");
        if (!dstIsFb)
            Printf(pri, " GpuWr_SysMem");
        if (!srcIsFb)
            Printf(pri, " GpuRd_SysMem");
    }

    Printf(pri, "\n");
}

//------------------------------------------------------------------------------
RC CheckDma::Action::Init
(
    UINT32 iType,
    Surf * pSrc,
    Surf * pDst,
    bool   doPeerWrite,
    UINT32 loop,
    bool   bSrcAddrPhysical,
    bool   bDstAddrPhysical,
    bool   skip,
    Tee::Priority pri
)
{
    m_Type  = iType;
    m_State = Ready;
    m_pSrc  = pSrc;
    m_pDst  = pDst;
    m_Loop  = loop;
    m_Pri   = pri;

    if (skip)
        m_State = Skipped;

    bool isPeer = ((m_pSrc->GetLocation() == Memory::Fb) &&
                   (m_pDst->GetLocation() == Memory::Fb) &&
                   (m_pSrc != m_pDst));

    // Set m_Dev (which GPU will act, i.e. which channel to use).

    switch (m_Type)
    {
        case FPK_CHECKDMA_ACTION_Twod:
        case FPK_CHECKDMA_ACTION_CopyEng0:
        case FPK_CHECKDMA_ACTION_CopyEng1:
        case FPK_CHECKDMA_ACTION_CopyEng2:
        case FPK_CHECKDMA_ACTION_CopyEng3:
        case FPK_CHECKDMA_ACTION_CopyEng4:
        case FPK_CHECKDMA_ACTION_CopyEng5:
        case FPK_CHECKDMA_ACTION_CopyEng6:
        case FPK_CHECKDMA_ACTION_CopyEng7:
        case FPK_CHECKDMA_ACTION_CopyEng8:
        case FPK_CHECKDMA_ACTION_CopyEng9:
            if (isPeer)
            {
                if (doPeerWrite)
                {
                    m_pDev = m_pSrc->GetDev();
                }
                else
                {
                    m_pDev = m_pDst->GetDev();
                }
            }
            else if (m_pSrc->GetLocation() == Memory::Fb)
            {
                m_pDev = m_pSrc->GetDev();  // gpu writes host mem
            }
            else
            {
                m_pDev = m_pDst->GetDev();  // gpu reads host mem or host-host
            }
            {
                // Force a valid engine on the dev we'll actually use.
                const int cid = Twod + (m_Type - FPK_CHECKDMA_ACTION_Twod);
                if (0 == m_pDev->HCh(cid))
                    m_Type = m_pDev->DefaultCopyAction();
            }
            break;

        case FPK_CHECKDMA_ACTION_Noop:
        case FPK_CHECKDMA_ACTION_CpuCpy:
            m_pDev = m_pDst->GetDev();
            break;
    }

    m_bSrcAddrPhysical = false;
    m_bDstAddrPhysical = false;

    const int cid = Twod + (m_Type - FPK_CHECKDMA_ACTION_Twod);
    if (m_pDev->IsPhysAddrSupported(cid))
    {
        if (isPeer)
        {
            if (!m_pDev->IsPeerPhysAddrSupported(cid))
            {
                // If peer physical is not supported then only local fb can be physical
                if (doPeerWrite)
                    m_bSrcAddrPhysical = bSrcAddrPhysical;
                else
                    m_bDstAddrPhysical = bDstAddrPhysical;
            }
            else
            {
                m_bSrcAddrPhysical = bSrcAddrPhysical;
                m_bDstAddrPhysical = bDstAddrPhysical;
            }
        }
        else
        {
            if (m_pSrc->GetLocation() == Memory::Fb)
                m_bSrcAddrPhysical = bSrcAddrPhysical;
            if (m_pDst->GetLocation() == Memory::Fb)
                m_bDstAddrPhysical = bDstAddrPhysical;
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
//! Adjust the size of this copy to fit the first free rect in the
//! destination surface (and the size of the source surface), callwlate
//! the resulting x,y,w,h of the rect and save it.
//! Update the free rect list of the destination surface for next time.
//! If there is no free rect in the dest, mark this action Skipped.
void CheckDma::Action::SetSize
(
    UINT32 size,
    UINT32 width,
    UINT32 srcOff,
    UINT64 mapLimit
)
{
    if (m_pDst->IsFull() || (0 == size))
    {
        // Dst has no free space.  Mark this action unused.
        SetFreeBlock(0,0,0,0,m_pDst);
        return;
    }

    // Get a pointer to the next free block of the dest surface
    Action * pFree = m_pDst->ConsumeFreeBlock();

    // We will write to the upper-left portion of the dst free block.
    m_X = pFree->m_X;
    m_Y = pFree->m_Y;

    // Limit width to min of (input_width, freeblock_width, src_width).
    // Width of 0 is interpreted to mean max possible width.
    m_W = width;
    if ((m_W == 0) || (m_W > pFree->m_W))
        m_W = pFree->m_W;
    m_W = min(m_W, m_pSrc->GetSrc().GetPitch());

    // Callwlate height based on width, limit it as for width.
    MASSERT(m_W > 0);
    if (size < m_W)
    {
        m_H = 1;
        m_W = size;
    }
    else
    {
        m_H = size / m_W;
    }

    if (m_H > pFree->m_H)
        m_H = pFree->m_H;

    if ((m_Type == FPK_CHECKDMA_ACTION_CpuCpy) &&
        (m_pDst->GetLocation() == Memory::Fb) )
    {
        // Don't CPU-write past end of mapped part of FB dst surface.
        UINT32 pitch = m_pDst->GetDst().GetPitch();
        UINT32 dstStartOffset = m_X + m_Y * pitch;

        if (dstStartOffset >= mapLimit)
        {
            // Dst free block is past mappable region, skip this operation.
            SetFreeBlock(0,0,0,0,m_pDst);
            m_pDst->AddFreeBlock(pFree);
            return;
        }
        else
        {
            m_H = min(m_H, UINT32(mapLimit / pitch - m_Y));
        }
    }

    UINT32 srcPitch = m_pSrc->GetSrc().GetPitch();
    UINT32 srcHeight = m_pSrc->GetSrc().GetHeight();

    if ((m_Type == FPK_CHECKDMA_ACTION_CpuCpy) &&
        (m_pSrc->GetLocation() == Memory::Fb))
    {
        // Don't CPU-read past end of mapped part of FB src surface.
        srcHeight = min(srcHeight, UINT32(mapLimit / srcPitch));
    }

    m_H = min(m_H, srcHeight);

    // Callwlate src X and Y based on input srcOff.
    // Limit them to prevent crossing the edges of the source surface.
    m_SrcX = srcOff % srcPitch;
    m_SrcY = srcOff / srcPitch;

    if (m_SrcX + m_W > srcPitch)
        m_SrcX = srcPitch - m_W;
    if (m_SrcY + m_H > srcHeight)
        m_SrcY = srcHeight - m_H;

    // Prevent misaligned CPU accesses
    if (m_Type == FPK_CHECKDMA_ACTION_CpuCpy)
    {
        // Make m_SrcX misalignment the same as m_X misalignment
        m_SrcX = (m_SrcX & ~3U) + (m_X & 3U);

        // Ensure that we don't exceed the right edge of the surface
        if (m_SrcX + m_W > srcPitch)
        {
            if (m_SrcX >= 4)
            {
                m_SrcX -= 4;
            }
            else
            {
                MASSERT(m_W > 4);
                m_W -= 4;
            }
        }
    }

    // Update the free list of our dest surface.
    if ((m_W < pFree->m_W) && (m_H < pFree->m_H))
    {
        // We wrote neither the full width nor height, split it into 2 free blocks.
        Action * pFree2 = new Action;
        m_pDst->AddFreeBlock(pFree);
        m_pDst->AddFreeBlock(pFree2);

        if ((pFree->m_W - m_W) > (pFree->m_H - m_H))
        {
            // Largest free space is at the right.
            pFree2->SetFreeBlock(       // full-height block to the right
                    m_X + m_W,
                    m_Y,
                    pFree->m_W - m_W,
                    pFree->m_H,
                    m_pDst
                    );
            pFree->SetFreeBlock(        // little block below
                    m_X,
                    m_Y + m_H,
                    m_W,
                    pFree->m_H - m_H,
                    m_pDst
                    );
        }
        else
        {
            // Largest free space is at the bottom.
            pFree2->SetFreeBlock(     // little block to the right
                    m_X + m_W,
                    m_Y,
                    pFree->m_W - m_W,
                    m_H,
                    m_pDst
                    );
            pFree->SetFreeBlock(        // full-width block below
                    m_X,
                    m_Y + m_H,
                    pFree->m_W,
                    pFree->m_H - m_H,
                    m_pDst
                    );
        }
    }
    else if (m_W < pFree->m_W)
    {
        // We wrote the full height, but not the width.  Adjust freeblock in place.
        m_pDst->AddFreeBlock(pFree);
        pFree->SetFreeBlock(
                m_X + m_W,
                m_Y,
                pFree->m_W - m_W,
                m_H,
                m_pDst
                );
    }
    else if (m_H < pFree->m_H)
    {
        // We wrote the full width, but not the full height.  Adjust freeblock in place.
        m_pDst->AddFreeBlock(pFree);
        pFree->SetFreeBlock(
                m_X,
                m_Y + m_H,
                m_W,
                pFree->m_H - m_H,
                m_pDst
                );
    }
    else
    {
        // We wrote the whole free block.
        delete pFree;
    }

    // Store a pointer to this Action in a struct in the destination surface.
    m_pDst->AddCopyAction(this);
}

//------------------------------------------------------------------------------
RC CheckDma::Action::SendPbuf()
{
    if (m_State != Ready)
        return OK;

    switch (m_Type)
    {
        case FPK_CHECKDMA_ACTION_Noop:
        case FPK_CHECKDMA_ACTION_CpuCpy:
            // No-op for these types.
            return OK;
    }

    Printf(m_Pri, "CheckDma::Action::SendPbuf\n");
    Print(m_Pri);

    Surface2D & src = m_pSrc->GetSrc();
    Surface2D & dst = m_pDst->GetDst();

    RC rc;
    switch (m_Type)
    {
        case FPK_CHECKDMA_ACTION_Twod:
            rc = m_pDev->CopyTwod(
                    src, m_SrcX, m_SrcY,
                    dst, m_X, m_Y,
                    m_W, m_H);
            break;

        case FPK_CHECKDMA_ACTION_CopyEng0:
        case FPK_CHECKDMA_ACTION_CopyEng1:
        case FPK_CHECKDMA_ACTION_CopyEng2:
        case FPK_CHECKDMA_ACTION_CopyEng3:
        case FPK_CHECKDMA_ACTION_CopyEng4:
        case FPK_CHECKDMA_ACTION_CopyEng5:
        case FPK_CHECKDMA_ACTION_CopyEng6:
        case FPK_CHECKDMA_ACTION_CopyEng7:
        case FPK_CHECKDMA_ACTION_CopyEng8:
        case FPK_CHECKDMA_ACTION_CopyEng9:
            rc = m_pDev->CopyCopyEngine(
                    (m_Type - FPK_CHECKDMA_ACTION_CopyEng0),
                    src, m_SrcX, m_SrcY, m_bSrcAddrPhysical,
                    dst, m_X, m_Y, m_bDstAddrPhysical,
                    m_W, m_H);
            break;

        default:
            MASSERT(!"Missing CHECKDMA_ACTION type in SendPbuf");
    }

    if (OK == rc)
    {
        m_State = Sent;
    }
    else
    {
        Printf(Tee::PriHigh, "CheckDma::Action::SendPbuf failed!\n");
        Print(Tee::PriHigh);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC CheckDma::Action::SendCpu()
{
    if (m_State != Ready)
        return OK;

    if (m_Type != FPK_CHECKDMA_ACTION_CpuCpy)
        return OK;

    Printf(m_Pri, "CheckDma::Action::SendCpu\n");
    Print(m_Pri);

    Surface2D & src = m_pSrc->GetSrc();
    Surface2D & dst = m_pDst->GetDst();

    UINT32 srcPitch = src.GetPitch();
    UINT32 dstPitch = dst.GetPitch();

    const char * pSrcBase = m_SrcX + m_SrcY * srcPitch
            + (const char *)(src.GetAddress());
    char * pDstBase = m_X + m_Y * dstPitch + (char *) dst.GetAddress();

    if (m_Incr)
    {
        // Use CPU writes to incrementing addresses.
        // MemCopy can handle any misalignment problems for us.
        UINT32 line;
        for (line = 0; line < m_H; line++)
        {
            char *       pDst = pDstBase + dstPitch * line;
            const char * pSrc = pSrcBase + srcPitch * line;
            Platform::MemCopy(pDst, pSrc, m_W);
            Tasker::Yield();
        }
    }
    else
    {
        // Use a mix of byte- and word- writes to decrementing addresses.
        // Note that both the start and end of a row can be misaligned.
        // We will try to keep our writes aligned.
        UINT32 line;
        for (line = m_H; line; line--)
        {
            char *       pDstRow = pDstBase + dstPitch * (line-1);
            char *       pDst = pDstRow + m_W;
            const char * pSrcRow = pSrcBase + srcPitch * (line-1);
            const char * pSrc = pSrcRow + m_W;

            // 4-byte address aligned versions of the write pointers.
            char * pDstRowA = pDstRow + (4 - (LwUPtr(pDstRow) & 3));
            char * pDstA    = pDst    - (LwUPtr(pDst) & 3);

            if (pDstA < pDstRow)
                pDstA = pDstRow;

            while (pDst > pDstA)
            {
                --pSrc;
                --pDst;
                MEM_WR08(pDst, MEM_RD08(pSrc));
            }
            while (pDst > pDstRowA)
            {
                pSrc -= 4;
                pDst -= 4;
                MEM_WR32(pDst, MEM_RD32(pSrc));
            }
            while (pDst > pDstRow)
            {
                --pSrc;
                --pDst;
                MEM_WR08(pDst, MEM_RD08(pSrc));
            }
            Tasker::Yield();
        }
    }
    m_State = Sent;

    return OK;
}

//------------------------------------------------------------------------------
void CheckDma::Action::Skip()
{
    m_State = Skipped;
}

//------------------------------------------------------------------------------
RC CheckDma::Action::CheckClear
(
    SurfaceUtils::MappingSurfaceReader & reader,
    bool IsCopy,
    UINT32 * pRtnBadBytes
)
{
    UINT08 ClearByte = m_pDst->GetDstFill();
    UINT32 Pitch     = m_pDst->GetDst().GetPitch();
    *pRtnBadBytes    = 0;

    vector<UINT08> linebuf(m_W, 0);

    for (UINT32 y = 0; y < m_H; y++)
    {
        RC rc;
        CHECK_RC(reader.ReadBytes((m_Y + y)*Pitch + m_X, &linebuf[0], m_W));

        for (UINT32 x = 0; x < m_W; x++)
        {
            UINT08 b = linebuf[x];
            if (b != ClearByte)
            {
                (*pRtnBadBytes)++;

                if (*pRtnBadBytes == 1)
                {
                    // First bad byte of this Action.  Time for debug spew!
                    UINT32 errX = x + m_X;
                    UINT32 errY = y + m_Y;
                    Printf(Tee::PriLow,
                            "Clear-fill miscompare in surface %s byte %d of line %d.\n",
                            m_pDst->GetName(),
                            errX,
                            errY);
                    m_pDst->Dump(true, IsCopy, errX, errY, reader);
                    Printf(Tee::PriLow,
                            "Expected fill value is 0x%02x.\n", ClearByte);
                }
            }
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC CheckDma::Action::CheckCopy
(
    SurfaceUtils::MappingSurfaceReader & dstReader,
    bool IsCopy,
    bool * pPrintedHeader,
    UINT32 * pRtnBadBytes
)
{
    UINT32 SrcPitch  = m_pSrc->GetSrc().GetPitch();
    UINT32 DstPitch  = m_pDst->GetDst().GetPitch();
    *pRtnBadBytes = 0;

    vector<UINT08> linebuf(m_W, 0);

    for (UINT32 y = 0; y < m_H; y++)
    {
        RC rc;
        CHECK_RC(dstReader.ReadBytes((y + m_Y)*DstPitch + m_X, &linebuf[0], m_W));

        for (UINT32 x = 0; x < m_W; x++)
        {
            UINT08 b = linebuf[x];

            // Don't read src, we can callwlate what it *ought* to have at
            // any arbitrary offset.
            UINT32 srcX = m_SrcX + x;
            UINT32 wSrc = m_pSrc->GetSrcFill((y + m_SrcY) * SrcPitch + srcX);

            // Be careful of big vs. little-endian here.
            UINT08 bSrc = ((char *)&wSrc)[srcX & 3];

            if (b != bSrc)
            {
                (*pRtnBadBytes)++;
                if (*pRtnBadBytes == 1)
                {
                    // First bad byte of this Action.  Time for debug spew!
                    if (!*pPrintedHeader)
                    {
                        PrintHeader(Tee::PriLow);
                        *pPrintedHeader = true;
                    }
                    Print(Tee::PriLow);

                    UINT32 errX = x + m_X;
                    UINT32 errY = y + m_Y;
                    Printf(Tee::PriLow,
                            "Copy miscompare in dst surface %s byte %d of line %d.\n",
                            m_pDst->GetName(),
                            errX,
                            errY
                            );
                    m_pDst->Dump(true, IsCopy, errX, errY, dstReader);

                    SurfaceUtils::MappingSurfaceReader srcReader(m_pSrc->GetSrc());
                    m_pSrc->Dump(false, false, x + m_SrcX, y + m_SrcY, srcReader);
                }
            }
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC CheckDma::Action::Check
(
    SurfaceUtils::MappingSurfaceReader & reader,
    bool     CheckFullSurface,
    bool     IsCopy,
    bool *   pPrintedHeader,
    UINT32 * pRtnBadBytes
)
{
    RC rc;
    switch (m_State)
    {
        case Free:
        {
            if ((m_W > 0) && (m_H > 0) && CheckFullSurface)
            {
                CHECK_RC(CheckClear(reader, IsCopy, pRtnBadBytes));
            }
            break;
        }

        case Ready:
        case Skipped:
        {
            // Not yet sent, or send skipped by user.
            if (!CheckFullSurface)
                break;

            // Check that this rect of dest surf is still at the clear value.

            CHECK_RC(CheckClear(reader, IsCopy, pRtnBadBytes));
            if (*pRtnBadBytes)
            {
                if ((!IsCopy) && (m_State != Free))
                    m_State = Checked_Failed;
            }
            break;
        }

        case Checked_Pass:
        {
            if (!CheckFullSurface)
                break;
             // Else, fall through to re-check the sent values.
        }
        case Sent:
        {
            // Check that this rect of dest surf has a copy of the source image.

            CHECK_RC(CheckCopy(reader, IsCopy, pPrintedHeader, pRtnBadBytes));
            if (*pRtnBadBytes)
            {
                if (!IsCopy)
                    m_State = Checked_Failed;
            }
            else
            {
                if (!IsCopy)
                    m_State = Checked_Pass;
            }
            break;
        }

        case Checked_Failed:
        {
            // Already checked & failed this frame.  Don't re-check.
            break;
        }

        default:
            MASSERT(!"Invalid Action State");
    }
    return rc;
}

void CheckDma::ReleaseFirstNGpuPStates(UINT32 n)
{
    GpuDevMgr *pMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    UINT32 index = 0;

    for (GpuDevice * pDev = pMgr->GetFirstGpuDevice();
            (pDev && index < n);
                pDev = pMgr->GetNextGpuDevice(pDev), ++index)
    {
        UINT32 x = (1 << pDev->GetDeviceInst());
        if (0 == (x & GetDeviceMask()))
            continue;

        m_PStatesOwner[index].ReleasePStates();
    }
}

//-----------------------------------------------------------------------------
RC CheckDma::LockGpuPStates()
{
    RC rc;
    GpuDevMgr *pMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    UINT32 index = 0;

    for (GpuDevice * pDev = pMgr->GetFirstGpuDevice();
            pDev;
                pDev = pMgr->GetNextGpuDevice(pDev), ++index)
    {
        UINT32 x = (1 << pDev->GetDeviceInst());
        if (0 == (x & GetDeviceMask()))
            continue;

        // Request exclusive access.
        rc = m_PStatesOwner[index].ClaimPStates(pDev->GetSubdevice(0));
        if (rc != OK)
        {
            Printf(Tee::PriLow, "CheckDma cannot reserve PStates for all SubDevices.\n");
            ReleaseFirstNGpuPStates(index-1);
            break;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
void CheckDma::ReleaseGpuPStates()
{
    GpuDevMgr *pMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    ReleaseFirstNGpuPStates(pMgr->NumDevices());
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ CheckDma object.
//------------------------------------------------------------------------------

JS_CLASS_INHERIT(CheckDma, PexTest, "GPU dma test including peer dma");

JS_STEST_BIND_NO_ARGS(CheckDma, Status, "Print details of last run");

CLASS_PROP_READWRITE(CheckDma, SurfSizeOverride, UINT32,
        "Size in bytes of src and dst surfaces (if 0, matches mode).");
CLASS_PROP_READWRITE(CheckDma, DontAllocHostC, bool,
        "If true, don't alloc or use the host-coherent buffer (hostNC only).");
CLASS_PROP_READWRITE(CheckDma, CheckFullSurface, bool,
        "If true, check full surface each time, else just what has been newly written");
CLASS_PROP_READWRITE(CheckDma, MaxCpuCopySize, UINT32,
        "Set max size (in bytes) of a non-GPU operation (to prevent being CPU bound).");
CLASS_PROP_READWRITE(CheckDma, AutoStatus, bool,
        "If true, print list of failed copy Actions after failure.");
CLASS_PROP_READWRITE(CheckDma, MaxMem2MemSize, UINT32,
        "If !0, break up all mem2mem transfers into this size or smaller.");
CLASS_PROP_READWRITE(CheckDma, CheckSurfacesOnlyIfWritten, bool,
        "If false, check surfaces we didn't write, to see if stray writes hit them.");
CLASS_PROP_READWRITE(CheckDma, DeviceMask, UINT32,
        "If non-zero, tests GpuDevice instances in mask (else only bound device).");
CLASS_PROP_READWRITE(CheckDma, MappingSize, UINT64,
        "Maximum size of CPU-mapped part of a surface.");
CLASS_PROP_READONLY(CheckDma, ErrorCount, UINT32,
        "Error count (total num incorrect bytes) from the previous run");
CLASS_PROP_READONLY(CheckDma, Loop, UINT32,
        "Current test loop");
CLASS_PROP_READWRITE(CheckDma, EnablePexCheck, bool,
                     "UINT32: enable PEX check");
CLASS_PROP_READWRITE(CheckDma, SrcOffsetToCorrupt, UINT32,
        "Byte offset in src surface to corrupt to test error reporting.");
CLASS_PROP_READWRITE(CheckDma, BenchP2P, bool,
        "If true, callwlate the average write transfer rate between a pair of GPUs.");
