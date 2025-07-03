/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2013,2015,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdarndm.h"
#include "core/include/utility.h"

namespace LwdaRandom
{

//-----------------------------------------------------------------------------
// Describe a kernel launch for the Surface kernels of LwdaRandom.
//
class SurfaceLaunchParam : public LaunchParam
{
public:
    SurfaceLaunchParam() : LaunchParam() { }
    virtual ~SurfaceLaunchParam() { }

    void Print(Tee::Priority pri) const;

    UINT32 numOps;
    UINT32 opIdx;

    UINT32 rndBlockx;
    UINT32 rndBlocky;
    UINT32 subSurfOffset;
};

void SurfaceLaunchParam::Print(Tee::Priority pri) const
{
    Printf(pri,
           "Kernel Params:\n"
           "NumOps:%d OpIdx:%d\n",
           numOps,
           opIdx);
    PrintTile(pri, "MatrixC", outputTile);
}

//-----------------------------------------------------------------------------
const char * SurfaceOpcodeToString(UINT32 Opcode)
{
    #define OPCODE_STRING(x)   case x: return #x
    switch (Opcode)
    {
        default: return "Unknown Opcode";
        OPCODE_STRING(CRTST_KRNL_SURF_OPCODE_suld1DA);
        OPCODE_STRING(CRTST_KRNL_SURF_OPCODE_suld2DA);
    }
}

//-----------------------------------------------------------------------------
// Hooks the SurfaceTest kernel to the core of LwdaRandom.
//
class SurfaceSubtest: public Subtest
{
public:
    SurfaceSubtest
    (
        LwdaRandomTest       * pParent
       ,const Lwca::Instance * pInst
       ,Lwca::Device           dev
    )
        : Subtest(pParent, "SurfaceRead", "SurfaceWrite")
         ,m_OpVec(SurfOpsSize,
                  SeqKernelOp(),
                  Lwca::HostMemoryAllocator<SeqKernelOp>(pInst, dev))
    {
    }
    virtual ~SurfaceSubtest() { }
    virtual RC Setup();
    virtual RC Cleanup();
    virtual RC RandomizeOncePerRestart(UINT32 Frame);
    virtual LaunchParam * NewLaunchParam();
    virtual void FillLaunchParam(LaunchParam* pLP);
    virtual void FillSurfaces(LaunchParam* pLP);
    virtual RC Launch(LaunchParam * pLP);
    virtual void Print(Tee::Priority pri, const LaunchParam * pLP);

private:
    //! Per-frame global array used in kernel.
    //! This maps to SurfKernelOps in lwrkernel.lw.
    Lwca::Global m_SurfKernelOps;

    //! System copy of SurfKernelOps.
    typedef vector<SeqKernelOp, Lwca::HostMemoryAllocator<SeqKernelOp> > OpVec;
    OpVec m_OpVec;

    Lwca::Array m_InputSurf[NUM_SURF];
    // number of available sub-surfaces for surface test kernels
    UINT32                m_NumSubSurf;
    // number of allocated sub-surfaces
    UINT32                m_UsedSubSurf;

protected:
    void ReadKernelOps();
    const SeqKernelOp& GetSurfKernelOp(UINT32 idx);
};

RC SurfaceSubtest::Setup()
{
    RC rc;
    CHECK_RC(Subtest::Setup());
    m_SurfKernelOps = GetModule()->GetGlobal("SurfKernelOps");
    m_NumSubSurf = Pick(CRTST_INPUT_NUM_SUBSURF);
    m_UsedSubSurf = 0;

    const UINT32 TxSz = LwdaRandomTexSize;
    const UINT32 TxW  = m_NumSubSurf * LwdaRandomTexSize;
    const LWarray_format Format = LW_AD_FORMAT_FLOAT;
    const UINT32 Surface = LWDA_ARRAY3D_SURFACE_LDST;
    const LWaddress_mode Clamp = LW_TR_ADDRESS_MODE_CLAMP;
    const ArrayParam surfParam[] =
    {
        { TxW,  0,    0, Format, 1, Surface, Clamp, 0, "LwdaRandomSurfInput1DA" }
        ,{TxW,  TxSz, 0, Format, 1, Surface, Clamp, 0, "LwdaRandomSurfInput2DA" }
    };

    MASSERT(NUM_SURF == NUMELEMS(surfParam));

    AllocateArrays(m_InputSurf, NUM_SURF, surfParam);
    CHECK_RC(BindSurfaces(m_InputSurf, NUM_SURF, surfParam));

    return rc;
}

RC SurfaceSubtest::Cleanup()
{
    // Use erase because experience shows it is faster than clear
    m_OpVec.erase(m_OpVec.begin(), m_OpVec.end());
    for (UINT32 i = 0; i < NUM_SURF; i+=1)
        m_InputSurf[i].Free();
    return OK;
}

RC SurfaceSubtest::RandomizeOncePerRestart(UINT32 Frame)
{
    for (size_t i=0; i < m_OpVec.size(); i++)
    {
        SeqKernelOp* pOp = &m_OpVec[i];
        pOp->OpCode   = Pick(CRTST_KRNL_SURF_OPCODE);

        // Enforce requirements:
        //   OpCode should be legal.
        //   Operands should be legal.
        //   Operand1 and Operand2 should be different.

        if (pOp->OpCode < CRTST_KRNL_SURF_OPCODE_FIRST)
            pOp->OpCode = CRTST_KRNL_SURF_OPCODE_FIRST;
        if (pOp->OpCode > CRTST_KRNL_SURF_OPCODE_LAST)
            pOp->OpCode = CRTST_KRNL_SURF_OPCODE_LAST;
    }

    // Copy to constant device memory
    m_SurfKernelOps.Set(&m_OpVec[0], 0/*stream*/,
                         SurfOpsSize * sizeof(SeqKernelOp));

    // Reset number of used sub-surfaces
    m_UsedSubSurf = 0;

    FillArray(&(m_InputSurf[0]), NUM_SURF, m_NumSubSurf * LwdaRandomTexSize);

    return OK;
}

void SurfaceSubtest::ReadKernelOps()
{
    m_SurfKernelOps.Get(&m_OpVec[0], 0/*stream*/,
                         SurfOpsSize * sizeof(SeqKernelOp));
}

LaunchParam * SurfaceSubtest::NewLaunchParam()
{
    SurfaceLaunchParam * pLP = new SurfaceLaunchParam;
    pLP->pSubtest = this;
    return pLP;
}

void SurfaceSubtest::FillLaunchParam(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SurfaceLaunchParam * pLP = static_cast<SurfaceLaunchParam*>(pLPbase);

    // Randomly choose how many ops to use during this launch.
    pLP->opIdx = Pick(CRTST_KRNL_SURF_OP_IDX) % SurfOpsSize;
    pLP->numOps = min(Pick(CRTST_KRNL_SURF_NUM_OPS), SurfOpsSize - pLP->opIdx);

    // Get sub-surface offset. If the offset is less than zero
    // then we're out of available sub-surfaces, so skip this launch.
    pLP->subSurfOffset = m_UsedSubSurf * LwdaRandomTexSize;
    ++m_UsedSubSurf;
    if (m_UsedSubSurf >= m_NumSubSurf)
        pLP->skip = true;

    pLP->rndBlockx = Pick(CRTST_KRNL_SURF_RND_BLOCK);
    pLP->rndBlocky = Pick(CRTST_KRNL_SURF_RND_BLOCK);
}

void SurfaceSubtest::FillSurfaces(LaunchParam* pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SurfaceLaunchParam * pLP = static_cast<SurfaceLaunchParam*>(pLPbase);

    FillTilesFloat32Exp(NULL, NULL, &pLP->outputTile, pLP);
}

RC SurfaceSubtest::Launch(LaunchParam * pLPbase)
{
    Tee::Priority pri = pLPbase->print ? Tee::PriNormal : Tee::PriLow;

    MASSERT(pLPbase->pSubtest == this);
    SurfaceLaunchParam * pLP = static_cast<SurfaceLaunchParam*>(pLPbase);

    // For Opcode coverage stats
    coverageTracker.OpcodeAdd<OpVec>(CRTST_CTRL_KERNEL_Surface,
                                     pLP->numOps,
                                     pLP->opIdx,
                                     m_OpVec);

    SetLaunchDim(pLP);

    Printf(pri, "Launching %s kernel.\n",Name());

    // Read from surfaces before writes from kernel
    RC rc = GetFunction().Launch(GetStream(pLP->streamIdx), pLP->outputTile,
            pLP->numOps, pLP->opIdx, pLP->subSurfOffset, pLP->gFP);
    if (rc != OK)
        return rc;

    Printf(pri, "Launching %s kernel.\n",Name2());
    // Write to surfaces from kernel
    rc = GetFunction2().Launch(GetStream(pLP->streamIdx),
         pLP->outputTile, pLP->rndBlockx, pLP->rndBlocky, pLP->subSurfOffset);
    if (rc != OK)
        return rc;

    Printf(pri, "Launching %s kernel.\n",Name());
    // Read from surfaces to verify writes from previous launch
    pLP->gFP.GPUFill = false;
    return GetFunction().Launch(GetStream(pLP->streamIdx), pLP->outputTile,
           pLP->numOps, pLP->opIdx, pLP->subSurfOffset, pLP->gFP);

}

void SurfaceSubtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const SurfaceLaunchParam * pLP = static_cast<const SurfaceLaunchParam*>(pLPbase);

    pLP->Print(pri);

    for (UINT32 i = 0; i < pLP->numOps; i++)
    {
        const int idx = pLP->opIdx + i;
        Printf(pri, "Index:%d OpCode:%s \n",
               idx,
               SurfaceOpcodeToString(m_OpVec[idx].OpCode));
    }
}

const SeqKernelOp& SurfaceSubtest::GetSurfKernelOp(UINT32 idx)
{
    return m_OpVec[idx];
}

Subtest * NewSurfaceSubtest
(
    LwdaRandomTest       * pTest,
    const Lwca::Instance * pInst,
    Lwca::Device           dev
)
{
    return new SurfaceSubtest(pTest, pInst, dev);
}

} // namespace LwdaRandom

