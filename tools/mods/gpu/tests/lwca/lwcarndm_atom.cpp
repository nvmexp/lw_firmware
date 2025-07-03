/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012,2015-2016, 2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdarndm.h"
#include "gpu/tests/lwca/random/lwrkernel.h"

namespace LwdaRandom
{
class AtomLaunchParam : public LaunchParam
{
public:
    AtomLaunchParam()
        : LaunchParam()
        ,opIdx(0)
        ,numOps(0)
    {
    }
    virtual ~AtomLaunchParam() { }

    UINT32 opIdx;
    UINT32 numOps;
};

const char * AtomOpcodeToString(UINT32 Opcode)
{
    const char * opcodeStrings[CRTST_KRNL_ATOM_NUM_OPCODES] =
    {
        "noop"
        ,"atom_global_and_b32"
        ,"atom_global_or_b32"
        ,"atom_global_xor_b32"
        ,"atom_global_cas_b32"
        ,"atom_global_cas_b64"
        ,"atom_global_exch_b32"
        ,"atom_global_exch_b64"
        ,"atom_global_add_u32"
        ,"atom_global_add_s32"
        ,"atom_global_add_f32"
        ,"atom_global_add_u64"
        ,"atom_global_inc_u32"
        ,"atom_global_dec_u32"
        ,"atom_global_min_u32"
        ,"atom_global_min_s32"
        ,"atom_global_max_u32"
        ,"atom_global_max_s32"
        ,"atom_shared_and_b32"
        ,"atom_shared_or_b32"
        ,"atom_shared_xor_b32"
        ,"atom_shared_cas_b32"
        ,"atom_shared_cas_b64"
        ,"atom_shared_exch_b32"
        ,"atom_shared_exch_b64"
        ,"atom_shared_add_u32"
        ,"atom_shared_add_u64"
        ,"atom_shared_inc_u32"
        ,"atom_shared_dec_u32"
        ,"atom_shared_min_u32"
        ,"atom_shared_max_u32"
        ,"red_global_and_b32"
        ,"red_global_or_b32"
        ,"red_global_xor_b32"
        ,"red_global_add_u32"
        ,"red_global_add_s32"
        ,"red_global_add_f32"
        ,"red_global_add_u64"
        ,"red_global_inc_u32"
        ,"red_global_dec_u32"
        ,"red_global_min_u32"
        ,"red_global_min_s32"
        ,"red_global_max_u32"
        ,"red_global_max_s32"
        ,"red_shared_and_b32"
        ,"red_shared_or_b32"
        ,"red_shared_xor_b32"
        ,"red_shared_cas_b32"
        ,"red_shared_cas_b64"
        ,"red_shared_add_u32"
        ,"red_shared_add_u64"
        ,"red_shared_inc_u32"
        ,"red_shared_dec_u32"
        ,"red_shared_min_u32"
        ,"red_shared_max_u32"
        ,"atom_global_and_b64"
        ,"atom_global_or_b64"
        ,"atom_global_xor_b64"
        ,"atom_global_min_u64"
        ,"atom_global_max_u64"
        ,"atom_shared_and_b64"
        ,"atom_shared_or_b64"
        ,"atom_shared_xor_b64"
        ,"atom_shared_min_u64"
        ,"atom_shared_max_u64"
        ,"redux_sync_add_u32"
        ,"redux_sync_min_u32"
        ,"redux_sync_max_u32"
        ,"redux_sync_and_b32"
        ,"redux_sync_or_b32"
        ,"redux_sync_xor_b32"
        ,"debug"
    };

    if (Opcode < CRTST_KRNL_ATOM_NUM_OPCODES)
        return opcodeStrings[Opcode];
    return "out of range!";
}

//-----------------------------------------------------------------------------
// A kernel that tests atom and red(uce) instructions.
class AtomSubtest: public Subtest
{
public:
    AtomSubtest
    (
        LwdaRandomTest       * pParent,
        const Lwca::Instance * pInst,
        Lwca::Device           dev
    )
        : Subtest(pParent, "AtomTest")
         ,m_OpVec(AtomOpsSize,
                  AtomKernelOp(),
                  Lwca::HostMemoryAllocator<AtomKernelOp>(pInst, dev))
    {

    }
    virtual ~AtomSubtest() { }

    virtual RC Setup();
    virtual RC Cleanup();
    virtual RC RandomizeOncePerRestart(UINT32 Frame);

    virtual LaunchParam * NewLaunchParam()
    {
        AtomLaunchParam * pLP = new AtomLaunchParam;
        pLP->pSubtest = this;
        return pLP;
    }

    virtual void FillLaunchParam(LaunchParam* pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        AtomLaunchParam * pLP = static_cast<AtomLaunchParam*>(pLPbase);

        pLP->numOps = min(Pick(CRTST_KRNL_ATOM_NUM_OPS),
                          AtomOpsSize);
        pLP->opIdx = min(Pick(CRTST_KRNL_ATOM_OP_IDX),
                         AtomOpsSize - pLP->numOps);
    }

    virtual void FillSurfaces(LaunchParam* pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        AtomLaunchParam * pLP = static_cast<AtomLaunchParam*>(pLPbase);

        FillTileUint32(&pLP->outputTile, Pick(CRTST_KRNL_ATOM_OUTPUT_MIN),
                       Pick(CRTST_KRNL_ATOM_OUTPUT_MAX), pLP);
    }

    virtual RC Launch(LaunchParam * pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        AtomLaunchParam * pLP = static_cast<AtomLaunchParam*>(pLPbase);

        // For Opcode coverage stats
        coverageTracker.OpcodeAdd<OpVec>(CRTST_CTRL_KERNEL_Atom,
                                         pLP->numOps,
                                         pLP->opIdx,
                                         m_OpVec);

        SetLaunchDim(pLP);
        return GetFunction().Launch(
            GetStream(pLP->streamIdx),
            pLP->outputTile,
            pLP->gFP,
            pLP->opIdx,
            pLP->numOps);
    }

    virtual void Print(Tee::Priority pri, const LaunchParam * pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        const AtomLaunchParam * pLP = static_cast<const AtomLaunchParam*>(pLPbase);

        Printf(pri,
               "Kernel Params:\n"
               "NumOps:%d OpIdx:%d\n",
               pLP->numOps,
               pLP->opIdx);
        PrintTile(pri, "Out", pLP->outputTile);

        for (UINT32 i = 0; i < pLP->numOps; i++)
        {
            const int opIdx = pLP->opIdx + i;
            Printf(pri, "Index:%d Args:%08x %.1f (%s warp) OpCode:%s \n",
                   opIdx,
                   m_OpVec[opIdx].Arg0,
                   m_OpVec[opIdx].Arg1,
                   m_OpVec[opIdx].UseSameWarp ? "same" : "diff",
                   AtomOpcodeToString(m_OpVec[opIdx].OpCode));
        }
    }
private:
    //! Per-frame global array used in kernel.
    //! This maps to AtomKernelOps in lwrkernel.lw.
    Lwca::Global m_AtomKernelOps;

    //! Sysmem copy of AtomKernelOps.
    typedef vector<AtomKernelOp, Lwca::HostMemoryAllocator<AtomKernelOp> > OpVec;
    OpVec m_OpVec;
};

RC AtomSubtest::Setup()
{
    RC rc;
    CHECK_RC(Subtest::Setup());
    m_AtomKernelOps = GetModule()->GetGlobal("AtomKernelOps");
    return rc;
}

RC AtomSubtest::Cleanup()
{
    // Use erase because experience shows it is faster than clear
    m_OpVec.erase(m_OpVec.begin(), m_OpVec.end());
    return OK;
}

RC AtomSubtest::RandomizeOncePerRestart(UINT32 Frame)
{
    // "list" style FancyPickers index by FbContext->LoopNum.
    // If this doesn't change during RandomizeOncePerRestart, we
    // get the same list value for every pick!
    // Force the AtomKernelOp index into the LoopNum to allow "list"
    // style pickers, restoring the real Loop afterwards.
    const UINT32 RealLoopNum = GetFpCtx()->LoopNum;

    for (size_t i=0; i < m_OpVec.size(); i++)
    {
        AtomKernelOp* pOp = &m_OpVec[i];
        GetFpCtx()->LoopNum = static_cast<UINT32>(i);

        pOp->OpCode = min(Pick(CRTST_KRNL_ATOM_OPCODE),
                          UINT32(CRTST_KRNL_ATOM_NUM_OPCODES-1));
        pOp->Arg0 = Pick(CRTST_KRNL_ATOM_ARG0);
        pOp->Arg1 = FPick(CRTST_KRNL_ATOM_ARG1);
        pOp->UseSameWarp = Pick(CRTST_KRNL_ATOM_SAME_WARP);
    }
    GetFpCtx()->LoopNum = RealLoopNum;

    // Copy to constant device memory
    m_AtomKernelOps.Set(&m_OpVec[0], 0/*stream*/,
                         AtomOpsSize * sizeof(AtomKernelOp));
    return OK;
}

Subtest * NewAtomSubtest
(
    LwdaRandomTest       * pTest,
    const Lwca::Instance * pInst,
    Lwca::Device           dev
)
{
    return new AtomSubtest(pTest, pInst, dev);
}

} // namespace LwdaRandom
