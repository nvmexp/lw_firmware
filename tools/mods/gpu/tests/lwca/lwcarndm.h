/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013,2015-2016,2019,2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
/**
 * @file lwdarndm.h
 * class LwdaRandom
 *
 * The LwdaRandom test is an LWCA based random arithmetic operation test.
 */

#ifndef INCLUDED_LWDARNDM_H
#define INCLUDED_LWDARNDM_H

#include "gpu/tests/lwdastst.h"
#include "gpu/tests/lwca/random/lwrkernel.h"
#include "lwgoldensurfaces.h"
#include <algorithm> // for min, max
#include <memory>
#include <unordered_set>
#include <utility>

template <class T> const T& MinMax(const T& low, const T& v, const T& high)
{
    return max(low, min(v,high));
}

// Number of texture and surface references available to LwdaRandomTest
#define NUM_TEX  8
#define NUM_SURF 2

// Namespace LwdaRandom
//
// Wrapping the test and its helper classes in a namespace lets us use
// shorter names (i.e. Subtest) w/o colliding with the rest of mods.
//
// The main test is named LwdaRandom::LwdaRandomTest instead of just
// LwdaRandom::Test so that the JS linkage macros work right.

namespace LwdaRandom
{

#include "gpu/js/lwr_comm.h"
class LwdaRandomTest;
class Subtest;
class LaunchParam;

// indexes for the input/output matrices
enum MatrixIdx
{
    mxInputA,
    mxInputB,
    mxOutputC,
    mxOutputSMID,
    mxNumMatrices,  // debug feature to have the kernels write their SMID value on each thread.
    mxNumInputMatrices = 2,
    mxNumOutputMatrices = 2,
};

typedef vector<float, Lwca::HostMemoryAllocator<float> > FVector;

// Lwca Array parameters
struct ArrayParam
{
    UINT32 width;
    UINT32 height;
    UINT32 depth;
    LWarray_format format;
    UINT32 channels;
    UINT32 flags;
    LWaddress_mode addrMode;
    UINT32 texSurfRefFlags;
    const char * name;
};

// To Track per kernel Exelwted Opcodes
struct CoverageTracker
{
    UINT32 KernelNumOpcodes[CRTST_CTRL_KERNEL_NUM_KERNELS];
    unordered_set<UINT32> KernelOpcodeStore[CRTST_CTRL_KERNEL_NUM_KERNELS];

    template <typename T>
    void OpcodeAdd(UINT32 kernelIdx, UINT32 numOps, UINT32 opIdx, T& opVec)
    {
        for (UINT32 i = 0; i < numOps; i++)
        {
            KernelOpcodeStore[kernelIdx].insert(opVec[opIdx+i].OpCode);
        }
    }
};

extern CoverageTracker coverageTracker;

//-----------------------------------------------------------------------------
//! LWCA-based Random arithmetic operations test
//!
class LwdaRandomTest : public LwdaStreamTest
{
public:
                    LwdaRandomTest();
    virtual         ~LwdaRandomTest();
    virtual RC      Cleanup() override;
    virtual RC      InitFromJs() override;
    virtual RC      Run() override;
    virtual RC      Setup() override;
    void            Print(const Tee::Priority Pri);
    void            PrintJsProperties(Tee::Priority Pri) override;
    bool IsSupportedVf() override { return true; }

private:
    unique_ptr<Subtest> m_Subtests[CRTST_CTRL_KERNEL_NUM_KERNELS];
    friend class Subtest;

    // JS controls:
    UINT32  m_SurfSizeOverride;
    bool    m_GoldenCheckInputs;            //!< run golden checking on input matrices
    bool    m_PrintHex;                     //!< debug: Print data in hex
    bool    m_SetMatrixValue;               //!< debug: Enable filling matrices with user values
    double  m_MatrixAValue;                 //!< debug; Fill Matrix A with value;
    double  m_MatrixBValue;                 //!< debug; Fill Matrix B with value;
    double  m_MatrixCValue;                 //!< debug; Fill Matrix C with value;
    double  m_MatrixSMIDValue;              //!< debug; Fill Matrix SMID with value;
    bool    m_SetSmallInt;                  //!< debug; Enable filling of matrices with UINT16 or UINT08 in SeqIntSubtest
    bool    m_SetByteInt;                   //!< debug; Fill Matrix with UINT08 when true otherwise, fill with UINT16
    bool    m_FillSurfaceOnGPU;             //!< Fill input surfaces using GPU
    bool    m_StopOnError;                  //!< Stop testing on first error
    bool    m_ClearSurfacesWithGPUFill;     //!< Clear all surfaces with GPU Fill
    bool    m_Coverage;                     //!< Print Opcode Coverage Stats

    // Internal controls

    // Lwca events are really semaphores, I think. We use a bunch of them to
    // keep cpu & gpu in sync and to record how much time has elapsed.
    enum EventIdxs
    {
        evBegin,
        evEnd,
        evNumEvents
    };

    enum GoldenSurfIdx
    {
        gldOutputC,
        gldInputA,
        gldInputB,
        gldNumSurfaces,
        gldNumInputSurfaces = 2,
        gldNumOutputSurfaces = 1,
    };

    struct Stats
    {
        UINT32  numThreads; // number of threads launched
        UINT32  totBytes;   // total bytes written on this surface
        UINT32  totBlks;    // total blks launched
        UINT32  maxBytes;   // max bytes on this surface
        UINT32  skipped;    // number of loops skipped
        UINT32  kernels[CRTST_CTRL_KERNEL_NUM_KERNELS];
        float   msPerKernel[CRTST_CTRL_KERNEL_NUM_KERNELS];
        Stats()
        :   numThreads(0),
            totBytes(0),
            totBlks(0),
            maxBytes(0),
            skipped(0)
        {
            for (int k = 0; k < CRTST_CTRL_KERNEL_NUM_KERNELS; k++)
            {
                kernels[k] = 0; // launches
                msPerKernel[k] = 0.0;
            }
        }
    };

    // wrapper to load a lwca module
    Lwca::Module            m_Module;

    // device memory for input/output matrices
    Lwca::DeviceMemory      m_devMemory[mxNumMatrices];

    // synchronization events to prevent us from overflowing the pushbuffer
    Lwca::Event             m_Events[evNumEvents];

    // Gpu internally won't issue any CTAs from a grid/launch until all
    // CTAs from the previous launch on the same stream retire.
    // I.e. it is assumed that the next launch on a stream may read results from
    // previous launches on the same stream safely.
    //
    // Since all LwdaRandom launches are data-independent of each other, we can
    // assign launches to streams randomly (launches can be done in any order).
    Lwca::Stream            m_Streams[CRTST_LAUNCH_STREAM_Num];

    // Compute capabilities for this device
    Lwca::Capability m_Cap;

    // texture and surface memory
    Lwca::Array m_InputTex[NUM_TEX];

    // Internal controls
    UINT32                  m_LoopsPerSurface;  //!< Number of different kernels to run on this surface
    UINT32                  m_SurfWidth;        //!< Number of bytes in the X dimension
    UINT32                  m_SurfHeight;       //!< Number of rows in the Y dimension
    // GPU specific properties
    bool                    m_GotProperties;
    LWdevprop               m_Property;

    // input/output matrices in system memory
    typedef vector<UINT08, Lwca::HostMemoryAllocator<UINT08> > SysMatrixVec;
    unique_ptr<SysMatrixVec> m_sysMatrix[mxNumMatrices];

    // description of the input/output matrices used on the device
    Tile                    m_devSurface[mxNumMatrices];

    typedef list<Tile*>             Tiles;

    // list of free non-overlapping tiles in the output matrix
    Tiles                   m_FreeTiles;

    // Indices to m_LaunchParams, can be shuffled to randomize launch order.
    vector<UINT32>          m_LaunchParamIndices;

    // all the kernel launch parameters for a single frame
    vector<LaunchParam*>    m_LaunchParams;

    FancyPicker::FpContext *m_pFpCtx;
    FancyPickerArray *      m_pPickers;

    struct FpCtxNode
    {
        UINT32 Id;
        FancyPicker::FpContext Ctx;
        explicit FpCtxNode(UINT32 x): Id(x) {}
    };
    vector<FpCtxNode>       m_FpCtxMap;

    UINT32                  m_NumSMs;   // Number of SMs found in this GPU
    float                   m_MsecTot;  // Total time the kernels are exelwting

    // scaled base on the #SMs for this GPU
    UINT32                  m_AllocatedThreadsPerFrame;
    UINT32                  m_MaxThreadsPerFrame;
    LwdaGoldenSurfaces      m_GSurfs;

    enum FpCtxArgs
    {
        Seed,
        LoopNum,
        RestartNum
    };
    void            LinkFpCtxMap(FancyPicker* fp, UINT32 index);
    void            SetFpCtxMap(const vector<pair<FpCtxArgs,UINT32>>& FpParams);

    void            CollectStats(Stats * pS);
    RC              CopyMemToDevice();
    void            InitSysMatrix(MatrixIdx mtx);
    void            InitializeFreeTiles(Tile * const pTile);
    RC              InnerRun();
    RC              Launch(const UINT32 Frame);
    const char *    OpcodeToString( UINT32 Opcode);
    LaunchParam *   NewLaunchParam();
    void            FillLaunchParam(LaunchParam * const pLP);
    void            PickOutputTile(Tile * pOutputTile,UINT32 elementSize);
    void            PrintOpcodes(const Tee::Priority Pri, int idx, int numOps);
    void            PrintTile(MatrixIdx mtx, const Tile* pTile);
    void            PrintLaunch(Tee::Priority Pri, const LaunchParam * pLP);
    RC              ProcessGoldelwalues(const UINT32 Frame);
    RC              RandomizeOncePerRestart(const UINT32 Frame);
    void            MarkSkippedLoops(const UINT32 Frame);

    RC              RandomizeTexSurfData();
    RC              RandomizeTexSurfHelper
                    (
                        Lwca::Array * input
                        ,FVector * hostTexSurf
                        ,const size_t pitch
                    );

    RC              SetDefaultsForPicker(UINT32 Index) override;
    void            FreeLaunchParams();
    UINT64          DevTileAddr(MatrixIdx idx, const Tile * pTile) const;
    void*           SysTileAddr(MatrixIdx idx, const Tile * pTile) const;
    void*           SysTileAddr(const Tile * pTile) const;
    RC              CheckMatrix(MatrixIdx mtx, const Tile * pTile, LaunchParam * pLP);

public:
    // JS interface functions:
    SETGET_PROP(SurfSizeOverride, UINT32);
    SETGET_PROP(GoldenCheckInputs, bool);
    SETGET_PROP(PrintHex, bool);
    SETGET_PROP(SetMatrixValue, bool);
    SETGET_PROP(MatrixAValue, double);
    SETGET_PROP(MatrixBValue, double);
    SETGET_PROP(MatrixCValue, double);
    SETGET_PROP(SetSmallInt, bool);
    SETGET_PROP(SetByteInt, bool);
    SETGET_PROP(FillSurfaceOnGPU, bool);
    SETGET_PROP(StopOnError, bool);
    SETGET_PROP(ClearSurfacesWithGPUFill, bool);
    SETGET_PROP(Coverage, bool);
}; // class LwdaRandom::LwdaRandomTest

//------------------------------------------------------------------------------
// Describes one "launch" aka "loop" (part of a frame) in LwdaRandomTest.
// Derived classes add kernel-specific properties, but all share this part.
//
class LaunchParam
{
public:
    LaunchParam()
        : pSubtest(NULL)
        ,numThreads(0)
        ,gridWidth(0)
        ,gridHeight(0)
        ,blockWidth(0)
        ,blockHeight(0)
        ,skip(false)
        ,checkMat(false)
        ,checkSmid(false)
        ,print(false)
        ,sync(false)
        ,streamIdx(0)
    {
        memset(&outputTile, 0, sizeof(outputTile));
        outputTile.fillType = ft_uint32;
    }
    virtual ~LaunchParam()
    {
    }

    Subtest* pSubtest;
    UINT32   numThreads;
    UINT32   gridWidth;
    UINT32   gridHeight;
    UINT32   blockWidth;
    UINT32   blockHeight;
    bool     skip;
    bool     checkMat;
    bool     checkSmid;
    bool     print;
    bool     sync;
    int      streamIdx;
    Lwca::Event event;

    GPUFillParam gFP;

    // Output rectangular subsurface to be written (in surface C).
    // Note: this launch will also "own" the corresponding tile of
    // input surfaces A and B.
    Tile   outputTile;
};

//------------------------------------------------------------------------------
// Utility functions
extern void PrintTile(Tee::Priority pri, const char * name, const Tile & t);

//------------------------------------------------------------------------------
// Describe one kernel used by LwdaRandomTest.
// Knows how to build LaunchParam-derived objects for this kernel.
class Subtest
{
public:
    Subtest
    (
        LwdaRandomTest * pParent
        ,const char * functionName
    );

    Subtest
    (
        LwdaRandomTest * pParent
        ,const char * functionName
        ,const char * functionName2
    );

    virtual ~Subtest();

    const char *   Name() const { return m_FunctionName; }
    const char *   Name2() const { return m_FunctionName2; }
    virtual bool   IsSupported() const { return true; }
    virtual UINT32 ElementSize() const { return sizeof(FLOAT32); }

    //! One-time init of device resources.  Base class sets up Function.
    virtual RC Setup();

    virtual RC Cleanup() { return OK; }

    //! Per-frame init of device resources.
    virtual RC RandomizeOncePerRestart(UINT32 Frame) { return OK; }

    //! Alloc and return a LaunchParam-derived object.
    virtual LaunchParam * NewLaunchParam() = 0;

    //! Uses FancyPickers to generate randomized launch parameters.
    virtual void FillLaunchParam(LaunchParam* pLP) { }
    virtual void FillSurfaces(LaunchParam* pLP) { }

    //! Fill tiles with data
    void FillTileFloat32Exp(Tile * pTile, INT32 p2Min, INT32 p2Max,
                            FLOAT64 dbgFill, LaunchParam * pLP);
    void FillTileFloat32Range(Tile * pTile, FLOAT32 milwal, FLOAT32 maxVal,
                              FLOAT64 dbgFill, LaunchParam * pLP);
    void FillTileFloat64Range(Tile * pTile, FLOAT64 milwal, FLOAT64 maxVal,
                              FLOAT64 dbgFill, LaunchParam * pLP);
    void FillTileFloat64Exp(Tile * pTile, INT32 p2Min, INT32 p2Max,
                            FLOAT64 dbgFill, LaunchParam * pLP);
    void FillTileUint32(Tile * pTile, UINT32 milwal,
                        UINT32 maxVal, LaunchParam * pLP);
    void FillTileUint16(Tile * pTile, UINT32 milwal,
                        UINT32 maxVal, LaunchParam * pLP);
    void FillTileUint08(Tile * pTile, UINT32 milwal,
                        UINT32 maxVal, LaunchParam * pLP);

    void FillTilesFloat32Exp(Tile* pA, Tile* pB,
                             Tile* pC, LaunchParam * pLP);
    void FillTilesFloat32Range(Tile* pA, Tile* pB,
                             Tile* pC, LaunchParam * pLP);
    void FillTilesFloat64Exp(Tile* pA, Tile* pB,
                             Tile* pC, LaunchParam * pLP);

    void FillTilesFloat64Range(Tile* pA, Tile* pB,
                             Tile* pC, LaunchParam * pLP);
    //! Tell the gpu to start exelwting this launch.
    //! TODO: specify which stream.
    virtual RC Launch(LaunchParam * pLP) = 0;

    virtual void Print(Tee::Priority pri, const LaunchParam* pLP) { }

protected:
    // This base class is a friend of LwdaRandomTest.
    // Derived subtests are not, but they can call these protected funcs.

    FancyPicker::FpContext* GetFpCtx() { return m_pParent->m_pFpCtx; }
    Random *                GetRand()  { return &m_pParent->m_pFpCtx->Rand; }
    UINT32          Pick(int i) { return (*m_pParent->m_pPickers)[i].Pick(); }
    FLOAT32         FPick(int i){ return (*m_pParent->m_pPickers)[i].FPick(); }
    Lwca::Module*   GetModule() { return &(m_pParent->m_Module); }
    UINT64          DevTileAddr(MatrixIdx idx, const Tile * pTile) const;
    const Lwca::Stream& GetStream(int streamIdx) const;
    UINT32 GetIntType();

    //FLOAT64 GetRandomFloat(MatrixIdx idx, double min, double max, bool isExp);
    const Lwca::Capability& GetCap() { return m_pParent->m_Cap; }
    FLOAT64 GetDebugFill(MatrixIdx idx);
    const Lwca::Capability& GetCap() const { return m_pParent->m_Cap; }

    void SetLaunchDim(const LaunchParam * pLP);
    const Lwca::Function & GetFunction() const { return m_Function; }
    const Lwca::Function & GetFunction2() const { return m_Function2; }

    void AllocateArrays(Lwca::Array inputArray[], UINT32 arraySize, const ArrayParam params[]);
    RC FillArray(Lwca::Array inputArray[], UINT32 arraySize, UINT32 width);
    RC BindSurfaces(Lwca::Array inputArray[], UINT32 arraySize, const ArrayParam params[]);

    bool HasBug(UINT32 bugNo) const;

private:
    LwdaRandomTest * m_pParent;

    //! lwca function name
    const char *   m_FunctionName;
    const char *   m_FunctionName2;

    //! kernel entry point
    Lwca::Function m_Function;
    Lwca::Function m_Function2;
};

extern Subtest * NewSeq32Subtest(LwdaRandomTest * pTest, const Lwca::Instance *pInst, Lwca::Device dev);
extern Subtest * NewSeq64Subtest(LwdaRandomTest * pTest, const Lwca::Instance *pInst, Lwca::Device dev);
extern Subtest * NewSeq32StressSubtest(LwdaRandomTest * pTest, const Lwca::Instance *pInst, Lwca::Device dev);
extern Subtest * NewSeq64StressSubtest(LwdaRandomTest * pTest, const Lwca::Instance *pInst, Lwca::Device dev);
extern Subtest * NewSeqIntSubtest(LwdaRandomTest * pTest, const Lwca::Instance *pInst, Lwca::Device dev);
extern Subtest * NewStressSubtest(LwdaRandomTest * pTest);
extern Subtest * NewDebugSubtest(LwdaRandomTest * pTest);
extern Subtest * NewBranchSubtest(LwdaRandomTest * pTest);
extern Subtest * NewTextureSubtest(LwdaRandomTest * pTest, const Lwca::Instance *pInst, Lwca::Device dev);
extern Subtest * NewSurfaceSubtest(LwdaRandomTest * pTest, const Lwca::Instance *pInst, Lwca::Device dev);
extern Subtest * NewMatrixInitSubtest(LwdaRandomTest * pTest);
extern Subtest * NewAtomSubtest(LwdaRandomTest * pTest, const Lwca::Instance *pInst, Lwca::Device dev);
extern Subtest * NewGpuWorkCreationSubtest(LwdaRandomTest * pTest);

struct TileDataCounts
{
    TileDataCounts() : zero(0), inf(0), nan(0), other(0) {}
    void Update(FLOAT64 x);

    unsigned zero;  // count of Updates with x== +-0.0
    unsigned inf;   // ... +-INFINITY
    unsigned nan;   // ... +-NOT A NUMBER
    unsigned other; // ... actual decent float values
};

} // namespace LwdaRandom

#endif

