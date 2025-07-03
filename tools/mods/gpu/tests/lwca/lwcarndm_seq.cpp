/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012,2015-2016,2018-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdarndm.h"

namespace LwdaRandom
{

//-----------------------------------------------------------------------------
// Describe a kernel launch for either the Seq32 or Seq64 kernels of LwdaRandom.
//
class SeqLaunchParam : public LaunchParam
{
public:
    SeqLaunchParam()
        : LaunchParam()
    {
        memset(&kernelParam, 0, sizeof(kernelParam));
    }
    virtual ~SeqLaunchParam() { }

    void Print(Tee::Priority pri) const;

    SeqKernelParam kernelParam;  // defined in lwdarandom.h (host/gpu common)
};

void SeqLaunchParam::Print(Tee::Priority pri) const
{
    Printf(pri,
           "Kernel Params:\n"
           "NumOps:%d OpIdx:%d\n",
           kernelParam.numOps,
           kernelParam.opIdx);
    PrintTile(pri, "MatrixA", kernelParam.A);
    PrintTile(pri, "MatrixB", kernelParam.B);
    PrintTile(pri, "MatrixC", kernelParam.C);
}

//-----------------------------------------------------------------------------
const char * SeqOpcodeToString(UINT32 Opcode)
{
    #define OPCODE_STRING(x)   case x: return #x
    switch (Opcode)
    {
        default: return "Unknown Opcode";
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_None);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Copy1);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Copy2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Add);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Sub);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Mul);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Mul24);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Sad);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_uSad);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Div);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Abs);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Min);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Max);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Sin);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Cos);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Tan);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Log);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Log2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Log10);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Pow);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Int2Float);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Iadd);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Isub);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Imul);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Idiv);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicAdd);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicSub);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicExch);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicMin);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicMax);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicInc);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicDec);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicCas);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicAnd);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicOr);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_AtomicXor);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_fmarn);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_fmarz);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_fmaru);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_fmard);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_addrn);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_addrz);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_addru);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_addrd);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_mulrn);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_mulrz);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_mulru);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_mulrd);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_divrn );
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_divrz );
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_divru );
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_divrd );
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_rcprn );
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_rcprz );
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_rcpru );
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_rcprd );
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrn);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrz);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_sqrtru);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrd);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_ldsldu);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vadd);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vsub);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vmin);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vmax);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vshl);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vshr);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vmad);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vset);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vadd2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vsub2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vavrg2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vmin2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vmax2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vset2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vadd4);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vsub4);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vavrg4);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff4);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vmin4);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vmax4);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_vset4);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_prmt);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_addcc);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_subcc);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_madcchi);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_madcclo);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_shfl);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_ldg);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_shf);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_dp4a);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Imul32i);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Fadd32i);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Fmul32i);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_Tanhf);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hTanh);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Tanh);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hEx2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Ex2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hSin);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Sin);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hCos);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Cos);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hLg2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Lg2);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hSqrt);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Sqrt);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hRsqrt);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Rsqrt);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hRcp);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Rcp);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hfmaE8M7);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2fmaE8M7);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hmaxE8M7);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hminE8M7);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hmax);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hmin);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2max);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2min);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hfmaMMA);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hfmaRELU);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_fmaxNaN);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_fminNaN);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_bmmaAnd);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_bmmaXor);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_clmad);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_scatter);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_gather);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_genMetadata);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_fmaxXorSign);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_fminXorSign);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hmaxXorSign);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hminXorSign);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2maxBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2minBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hfmaRELUBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hmaxXorSignBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hminXorSignBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hTanhBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2TanhBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hEx2BF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Ex2BF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hSinBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2SinBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hCosBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2CosBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hLg2BF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2Lg2BF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hSqrtBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2SqrtBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hRsqrtBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2RsqrtBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_hRcpBF16);
        OPCODE_STRING(CRTST_KRNL_SEQ_MTX_OPCODE_h2RcpBF16);
    }
}

//-----------------------------------------------------------------------------
// Hooks the Seq32 kernel to the core of LwdaRandom.
//
class Seq32Subtest: public Subtest
{
public:
    Seq32Subtest
    (
        LwdaRandomTest        * pParent
        ,const Lwca::Instance * pInst
        ,Lwca::Device           dev
        ,const char           * testName = "SeqMatrixArithmeticTest"
    )
        : Subtest(pParent, testName)
         ,m_OpVec(Seq32OpsSize,
                  SeqKernelOp(),
                  Lwca::HostMemoryAllocator<SeqKernelOp>(pInst, dev))
    {
    }
    virtual ~Seq32Subtest() { }
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
    //! This maps to Seq32KernelOps in lwrkernel.lw.
    Lwca::Global m_Seq32KernelOps;

    //! Sysmem copy of Seq32KernelOps.
    typedef vector<SeqKernelOp, Lwca::HostMemoryAllocator<SeqKernelOp> > OpVec;
    OpVec m_OpVec;

protected:
    void ReadKernelOps();
    const SeqKernelOp& GetSeqKernelOp(UINT32 idx);
};

RC Seq32Subtest::Setup()
{
    RC rc;
    CHECK_RC(Subtest::Setup());
    m_Seq32KernelOps = GetModule()->GetGlobal("Seq32KernelOps");
    return rc;
}

RC Seq32Subtest::Cleanup()
{
    // Use erase because experience shows it is faster than clear
    m_OpVec.erase(m_OpVec.begin(), m_OpVec.end());
    return OK;
}

RC Seq32Subtest::RandomizeOncePerRestart(UINT32 Frame)
{
    for (size_t i=0; i < m_OpVec.size(); i++)
    {
        SeqKernelOp* pOp = &m_OpVec[i];
        pOp->OpCode   = Pick(CRTST_KRNL_SEQ_MTX_OPCODE);
        pOp->Operand1 = Pick(CRTST_KRNL_SEQ_MTX_OPERAND);
        pOp->Operand2 = Pick(CRTST_KRNL_SEQ_MTX_OPERAND);
        pOp->Operand3 = CRTST_KRNL_SEQ_MTX_OPERAND_None;

        // Enforce requirements:
        //   OpCode should be legal.
        //   Operands should be legal.
        //   Operand1 and Operand2 should be different.

        if (pOp->OpCode < CRTST_KRNL_SEQ_MTX_OPCODE_None)
            pOp->OpCode = CRTST_KRNL_SEQ_MTX_OPCODE_None;

        if (pOp->Operand1 >= CRTST_KRNL_SEQ_MTX_OPERAND_Max)
            pOp->Operand1 = CRTST_KRNL_SEQ_MTX_OPERAND_MatrixA;

        if (pOp->Operand2 >= CRTST_KRNL_SEQ_MTX_OPERAND_Max)
            pOp->Operand2 = CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB;

        if (pOp->Operand1 == pOp->Operand2)
            pOp->Operand2 ^= 1;
    }

    // Copy to constant device memory
    m_Seq32KernelOps.Set(&m_OpVec[0], 0/*stream*/,
                         Seq32OpsSize * sizeof(SeqKernelOp));
    return OK;
}

void Seq32Subtest::ReadKernelOps()
{
    m_Seq32KernelOps.Get(&m_OpVec[0], 0/*stream*/,
                         Seq32OpsSize * sizeof(SeqKernelOp));
}

LaunchParam * Seq32Subtest::NewLaunchParam()
{
    SeqLaunchParam * pLP = new SeqLaunchParam;
    pLP->pSubtest = this;
    return pLP;
}

void Seq32Subtest::FillLaunchParam(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);

    // Randomly choose which arithmetic ops to use during this launch.
    pLP->kernelParam.numOps = min(
        Pick(CRTST_KRNL_SEQ_MTX_NUM_OPS), Seq32OpsSize);
    pLP->kernelParam.opIdx = min(
        Pick(CRTST_KRNL_SEQ_MTX_OP_IDX),
        Seq32OpsSize - pLP->kernelParam.numOps);
    pLP->kernelParam.inc = Pick(CRTST_KRNL_SEQ_MTX_DEBUG_IDX_INC);

    // Output tile is correct already, we call it "C" in lwrkernel.lw.
    pLP->kernelParam.C = pLP->outputTile;

    // Input tiles are corresponding parts of A,B matrices.
    pLP->kernelParam.A = pLP->outputTile;
    pLP->kernelParam.B = pLP->outputTile;
    pLP->kernelParam.SMID = pLP->outputTile;

    // Need to fix A,B,&SMID gpu virtual addresses after copying from C.
    pLP->kernelParam.A.elements = DevTileAddr(mxInputA, &pLP->kernelParam.A);
    pLP->kernelParam.B.elements = DevTileAddr(mxInputB, &pLP->kernelParam.B);
    pLP->kernelParam.SMID.elements = DevTileAddr(mxOutputSMID, &pLP->kernelParam.SMID);

    // Check the returned SMID values to ensure that the kernel did actual work
    pLP->checkSmid = true;
}

void Seq32Subtest::FillSurfaces(LaunchParam* pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);
    if (Pick(CRTST_INPUT_DATA_FILL_STYLE) == rs_range)
    {
        FillTilesFloat32Range(&pLP->kernelParam.A,
                              &pLP->kernelParam.B,
                              &pLP->kernelParam.C, pLP);
    }
    else
    {
        FillTilesFloat32Exp(&pLP->kernelParam.A,
                            &pLP->kernelParam.B,
                            &pLP->kernelParam.C, pLP);
    }

}

RC Seq32Subtest::Launch(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);

    // For Opcode coverage stats
    if (strcmp(pLPbase->pSubtest->Name(), "SeqMatrixArithmeticTest") == 0)
    {
        coverageTracker.OpcodeAdd<OpVec>(CRTST_CTRL_KERNEL_SeqMatrix,
                                         pLP->kernelParam.numOps,
                                         pLP->kernelParam.opIdx,
                                         m_OpVec);
    }
    SetLaunchDim(pLP);
    return GetFunction().Launch(GetStream(pLP->streamIdx), pLP->kernelParam, pLP->gFP);
}

void Seq32Subtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const SeqLaunchParam * pLP = static_cast<const SeqLaunchParam*>(pLPbase);

    pLP->Print(pri);

    for (UINT32 i = 0; i < pLP->kernelParam.numOps; i++)
    {
        const int opIdx = pLP->kernelParam.opIdx + i;
        Printf(pri, "Index:%d Operand1:%d Operand2:%d Operand3:%d OpCode:%s \n",
               opIdx,
               m_OpVec[opIdx].Operand1,
               m_OpVec[opIdx].Operand2,
               m_OpVec[opIdx].Operand3,
               SeqOpcodeToString(m_OpVec[opIdx].OpCode));
    }
    if (pLP->kernelParam.A.fillStyle == rs_exp)
    {
        Printf(pri, "Fill EXP A%d:%d\n",
               pLP->kernelParam.A.min.i, pLP->kernelParam.A.max.i);
    }
    else
    {
        Printf(pri, "Fill A%f:%f\n",
               pLP->kernelParam.A.min.f, pLP->kernelParam.A.max.f);
    }
    if (pLP->kernelParam.B.fillStyle == rs_exp)
    {
        Printf(pri, "Fill EXP B%d:%d\n",
               pLP->kernelParam.B.min.i, pLP->kernelParam.B.max.i);
    }
    else
    {
        Printf(pri, "Fill B%f:%f\n",
               pLP->kernelParam.B.min.f, pLP->kernelParam.B.max.f);
    }
    if (pLP->kernelParam.C.fillStyle == rs_exp)
    {
        Printf(pri, "Fill EXP C%d:%d\n",
               pLP->kernelParam.C.min.i, pLP->kernelParam.C.max.i);
    }
    else
    {
        Printf(pri, "Fill C%f:%f\n",
               pLP->kernelParam.C.min.f, pLP->kernelParam.C.max.f);
    }
}

const SeqKernelOp& Seq32Subtest::GetSeqKernelOp(UINT32 idx)
{
    return m_OpVec[idx];
}

Subtest * NewSeq32Subtest
(
    LwdaRandomTest       * pTest,
    const Lwca::Instance * pInst,
    Lwca::Device           dev
)
{
    return new Seq32Subtest(pTest, pInst, dev);
}

//-----------------------------------------------------------------------------
// Hooks the Seq64 kernel into the core of LwdaRandom.
//
class Seq64Subtest: public Subtest
{
public:
    Seq64Subtest
    (
        LwdaRandomTest        * pParent
        ,const Lwca::Instance * pInst
        ,Lwca::Device           dev
        ,const char           * testName = "SeqMatrixArithmeticTestDouble"
    )
        : Subtest(pParent, testName)
         ,m_OpVec(Seq64OpsSize,
                  SeqKernelOp(),
                  Lwca::HostMemoryAllocator<SeqKernelOp>(pInst, dev))
    {
    }
    virtual ~Seq64Subtest() { }
    virtual RC Setup();
    virtual RC Cleanup();
    virtual UINT32 ElementSize() const { return sizeof(FLOAT64); }
    virtual RC RandomizeOncePerRestart(UINT32 Frame);
    virtual LaunchParam * NewLaunchParam();
    virtual void FillLaunchParam(LaunchParam* pLP);
    virtual void FillSurfaces(LaunchParam* pLP);
    virtual RC Launch(LaunchParam * pLPbase);
    virtual void Print(Tee::Priority pri, const LaunchParam * pLP);

private:
    //! Per-frame global array used in kernel.
    Lwca::Global m_Seq64KernelOps;

    //! Sysmem copy of Seq64KernelOps.
    typedef vector<SeqKernelOp, Lwca::HostMemoryAllocator<SeqKernelOp> > OpVec;
    OpVec m_OpVec;

protected:
    void ReadKernelOps();
    const SeqKernelOp& GetSeqKernelOp(UINT32 idx);
};

RC Seq64Subtest::Setup()
{
    RC rc;
    CHECK_RC(Subtest::Setup());
    m_Seq64KernelOps = GetModule()->GetGlobal("Seq64KernelOps");
    return rc;
}

RC Seq64Subtest::Cleanup()
{
    // Use erase because experience shows it is faster than clear
    m_OpVec.erase(m_OpVec.begin(), m_OpVec.end());
    return OK;
}

RC Seq64Subtest::RandomizeOncePerRestart(UINT32 Frame)
{
    for (size_t i=0; i < m_OpVec.size(); i++)
    {
        SeqKernelOp* pOp = &m_OpVec[i];
        pOp->OpCode   = Pick(CRTST_KRNL_SEQ_MTX_DBL_OPCODE);
        pOp->Operand1 = Pick(CRTST_KRNL_SEQ_MTX_OPERAND);
        pOp->Operand2 = Pick(CRTST_KRNL_SEQ_MTX_OPERAND);
        pOp->Operand3 = CRTST_KRNL_SEQ_MTX_OPERAND_None;

        // Enforce requirements:
        //   OpCode should be legal.
        //   Operands should be legal -- Seq64 fails with tex inputs!
        //   Operand1 and Operand2 should be different.

        if (pOp->OpCode < CRTST_KRNL_SEQ_MTX_OPCODE_None)
            pOp->OpCode = CRTST_KRNL_SEQ_MTX_OPCODE_None;

        if (pOp->Operand1 > CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB)
            pOp->Operand1 = CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB;

        if (pOp->Operand2 >= CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB)
            pOp->Operand2 = CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB;

        if (pOp->Operand1 == pOp->Operand2)
            pOp->Operand2 ^= 1;
    }

    // Copy to constant device memory
    m_Seq64KernelOps.Set(&m_OpVec[0], 0/*stream*/,
                         Seq64OpsSize * sizeof(SeqKernelOp));
    return OK;
}

void Seq64Subtest::ReadKernelOps()
{
    m_Seq64KernelOps.Get(&m_OpVec[0], 0/*stream*/,
                         Seq64OpsSize * sizeof(SeqKernelOp));
}

LaunchParam * Seq64Subtest::NewLaunchParam()
{
    SeqLaunchParam * pLP = new SeqLaunchParam;
    pLP->pSubtest = this;
    return pLP;
}

void Seq64Subtest::FillLaunchParam(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);

    pLP->kernelParam.numOps = min(
        Pick(CRTST_KRNL_SEQ_MTX_DBL_NUM_OPS), Seq64OpsSize);
    pLP->kernelParam.opIdx = min(
        Pick(CRTST_KRNL_SEQ_MTX_DBL_OP_IDX),
        Seq64OpsSize - pLP->kernelParam.numOps);
    pLP->kernelParam.inc = Pick(CRTST_KRNL_SEQ_MTX_DEBUG_IDX_INC);

    // Output tile is correct already, we call it "C" in lwrkernel.lw.
    pLP->kernelParam.C = pLP->outputTile;

    // Input tiles are corresponding parts of A,B matrices.
    pLP->kernelParam.A = pLP->outputTile;
    pLP->kernelParam.B = pLP->outputTile;
    pLP->kernelParam.SMID = pLP->outputTile;

    // Need to fix A,B gpu virtual addresses after copying from C.
    pLP->kernelParam.A.elements = DevTileAddr(mxInputA, &pLP->kernelParam.A);
    pLP->kernelParam.B.elements = DevTileAddr(mxInputB, &pLP->kernelParam.B);
    pLP->kernelParam.SMID.elements = DevTileAddr(mxOutputSMID, &pLP->kernelParam.SMID);

    // Check the returned SMID values to ensure that the kernel did actual work
    pLP->checkSmid = true;

    MASSERT(pLP->kernelParam.C.elements == DevTileAddr(mxOutputC, &pLP->kernelParam.C));
}

void Seq64Subtest::FillSurfaces(LaunchParam* pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);
    if (Pick(CRTST_INPUT_DATA_FILL_STYLE) == rs_range)
    {
        FillTilesFloat64Range(&pLP->kernelParam.A,
                              &pLP->kernelParam.B,
                              &pLP->kernelParam.C, pLP);
    }
    else
    {
        FillTilesFloat64Exp(&pLP->kernelParam.A,
                            &pLP->kernelParam.B,
                            &pLP->kernelParam.C, pLP);
    }
}

RC Seq64Subtest::Launch(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);

    // For Opcode coverage stats
    if (strcmp(pLPbase->pSubtest->Name(), "SeqMatrixArithmeticTestDouble") == 0)
    {
        coverageTracker.OpcodeAdd<OpVec>(CRTST_CTRL_KERNEL_SeqMatrixDbl,
                                         pLP->kernelParam.numOps,
                                         pLP->kernelParam.opIdx,
                                         m_OpVec);
    }
    SetLaunchDim(pLP);
    return GetFunction().Launch(GetStream(pLP->streamIdx), pLP->kernelParam, pLP->gFP);
}

void Seq64Subtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const SeqLaunchParam * pLP = static_cast<const SeqLaunchParam*>(pLPbase);

    pLP->Print(pri);

    for (UINT32 i = 0; i < pLP->kernelParam.numOps; i++)
    {
        const int opIdx = pLP->kernelParam.opIdx + i;
        Printf(pri, "Index:%d Operand1:%d Operand2:%d Operand3:%d OpCode:%s \n",
               opIdx,
               m_OpVec[opIdx].Operand1,
               m_OpVec[opIdx].Operand2,
               m_OpVec[opIdx].Operand3,
               SeqOpcodeToString(m_OpVec[opIdx].OpCode));
    }
    if (pLP->kernelParam.A.fillStyle == rs_exp)
    {
        Printf(pri, "Fill EXP A%d:%d\n",
               pLP->kernelParam.A.min.i, pLP->kernelParam.A.max.i);
    }
    else
    {
        Printf(pri, "Fill A%f:%f\n",
               pLP->kernelParam.A.min.f, pLP->kernelParam.A.max.f);
    }
    if (pLP->kernelParam.B.fillStyle == rs_exp)
    {
        Printf(pri, "Fill EXP B%d:%d\n",
               pLP->kernelParam.B.min.i, pLP->kernelParam.B.max.i);
    }
    else
    {
        Printf(pri, "Fill B%f:%f\n",
               pLP->kernelParam.B.min.f, pLP->kernelParam.B.max.f);
    }
    if (pLP->kernelParam.C.fillStyle == rs_exp)
    {
        Printf(pri, "Fill EXP C%d:%d\n",
               pLP->kernelParam.C.min.i, pLP->kernelParam.C.max.i);
    }
    else
    {
        Printf(pri, "Fill C%f:%f\n",
               pLP->kernelParam.C.min.f, pLP->kernelParam.C.max.f);
    }
}

const SeqKernelOp& Seq64Subtest::GetSeqKernelOp(UINT32 idx)
{
    return m_OpVec[idx];
}

Subtest * NewSeq64Subtest
(
    LwdaRandomTest       * pTest,
    const Lwca::Instance * pInst,
    Lwca::Device           dev
)
{
    return new Seq64Subtest(pTest, pInst, dev);
}

//-----------------------------------------------------------------------------
// Hooks the Seq32Stress kernel to the core of LwdaRandom.
//
class Seq32StressSubtest: public Seq32Subtest
{
public:
    Seq32StressSubtest
    (
        LwdaRandomTest       * pParent
       ,const Lwca::Instance * pInst
       ,Lwca::Device           dev
    )
        : Seq32Subtest(pParent, pInst, dev, "SeqMatrixArithmeticTestStress")
    {
    }
    virtual ~Seq32StressSubtest() {}
    virtual RC RandomizeOncePerRestart(UINT32 Frame);
    virtual void Print(Tee::Priority pri, const LaunchParam * pLP);
};

RC Seq32StressSubtest::RandomizeOncePerRestart(UINT32 Frame)
{
    // Parent class Seq32Subtest has already initialized the
    // FB copy of the kernel ops table.
    // Read it so we have an accurate sysmem copy also.
    ReadKernelOps();
    return OK;
}

void Seq32StressSubtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const SeqLaunchParam * pLP = static_cast<const SeqLaunchParam*>(pLPbase);

    pLP->Print(pri);

    SeqKernelOp kernelOp = GetSeqKernelOp(pLP->kernelParam.opIdx);

    Printf(pri, "Starting OpCode:%s \n",
        SeqOpcodeToString(kernelOp.OpCode));

    for (UINT32 i = 0; i < pLP->kernelParam.numOps; i++)
    {
        const int opIdx = pLP->kernelParam.opIdx + i;
        const SeqKernelOp kernelOp = GetSeqKernelOp(opIdx);
        Printf(pri, "Index:%d Operand1:%d Operand2:%d Operand3:%d \n",
               opIdx,
               kernelOp.Operand1,
               kernelOp.Operand2,
               kernelOp.Operand3);
    }

}

Subtest * NewSeq32StressSubtest
(
    LwdaRandomTest       * pTest,
    const Lwca::Instance * pInst,
    Lwca::Device           dev
)
{
    return new Seq32StressSubtest(pTest, pInst, dev);
}

//-----------------------------------------------------------------------------
// Hooks the Seq64Stress kernel into the core of LwdaRandom.
//
class Seq64StressSubtest: public Seq64Subtest
{
public:
    Seq64StressSubtest
    (
        LwdaRandomTest       * pParent
       ,const Lwca::Instance * pInst
       ,Lwca::Device           dev
    )
        : Seq64Subtest(pParent, pInst, dev, "SeqMatrixArithmeticTestDoubleStress")
    {
    }
    virtual ~Seq64StressSubtest() {}
    virtual RC RandomizeOncePerRestart(UINT32 Frame);
    virtual void Print(Tee::Priority pri, const LaunchParam * pLP);
};

RC Seq64StressSubtest::RandomizeOncePerRestart(UINT32 Frame)
{
    // Parent class Seq64Subtest has already initialized the
    // FB copy of the kernel ops table.
    // Read it so we have an accurate sysmem copy also.
    ReadKernelOps();
    return OK;
}

void Seq64StressSubtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const SeqLaunchParam * pLP = static_cast<const SeqLaunchParam*>(pLPbase);

    pLP->Print(pri);

    SeqKernelOp kernelOp = GetSeqKernelOp(pLP->kernelParam.opIdx);

    Printf(pri, "Starting OpCode:%s \n",
        SeqOpcodeToString(kernelOp.OpCode));

    for (UINT32 i = 0; i < pLP->kernelParam.numOps; i++)
    {
        const int opIdx = pLP->kernelParam.opIdx + i;
        const SeqKernelOp kernelOp = GetSeqKernelOp(opIdx);
        Printf(pri, "Index:%d Operand1:%d Operand2:%d Operand3:%d \n",
               opIdx,
               kernelOp.Operand1,
               kernelOp.Operand2,
               kernelOp.Operand3);
    }

}

Subtest * NewSeq64StressSubtest
(
    LwdaRandomTest       * pTest,
    const Lwca::Instance * pInst,
    Lwca::Device           dev
)
{
    return new Seq64StressSubtest(pTest, pInst, dev);
}

//-----------------------------------------------------------------------------
// Hooks the SeqInt kernel into the core of LwdaRandom.
//
class SeqIntSubtest: public Subtest
{
public:
    SeqIntSubtest
    (
        LwdaRandomTest        * pParent
        ,const Lwca::Instance * pInst
        ,Lwca::Device           dev
        ,const char           * testName = "SeqMatrixIntegerTest"
    )
        : Subtest(pParent, testName)
         ,m_OpVec(SeqIntOpsSize,
                  SeqKernelOp(),
                  Lwca::HostMemoryAllocator<SeqKernelOp>(pInst, dev))
         ,m_pFillTile(nullptr)
    {
    }
    virtual ~SeqIntSubtest() { }
    virtual RC Setup();
    virtual RC Cleanup();
    virtual UINT32 ElementSize() const { return sizeof(INT32); }
    virtual RC RandomizeOncePerRestart(UINT32 Frame);
    virtual LaunchParam * NewLaunchParam();
    virtual void FillLaunchParam(LaunchParam* pLP);
    virtual void FillSurfaces(LaunchParam* pLP);
    virtual RC Launch(LaunchParam * pLPbase);
    virtual void Print(Tee::Priority pri, const LaunchParam * pLP);

private:
    //! Per-frame global array used in kernel.
    Lwca::Global m_SeqIntKernelOps;

    //! Sysmem copy of SeqIntKernelOps.
    typedef vector<SeqKernelOp, Lwca::HostMemoryAllocator<SeqKernelOp> > OpVec;
    OpVec m_OpVec;

    //! Pointer to member function for filling with correct type of int
    void (SeqIntSubtest::*m_pFillTile)(Tile*, UINT32, UINT32, LaunchParam*);

protected:
    void ReadKernelOps();
    const SeqKernelOp& GetSeqKernelOp(UINT32 idx);
};

RC SeqIntSubtest::Setup()
{
    RC rc;
    CHECK_RC(Subtest::Setup());
    m_SeqIntKernelOps = GetModule()->GetGlobal("SeqIntKernelOps");
    switch (GetIntType())
    {
        case ft_uint08:
            m_pFillTile = &SeqIntSubtest::FillTileUint08;
            break;
        case ft_uint16:
            m_pFillTile = &SeqIntSubtest::FillTileUint16;
            break;
        default:
            m_pFillTile = &SeqIntSubtest::FillTileUint32;
            break;
    }

    return rc;
}

RC SeqIntSubtest::Cleanup()
{
    // Use erase because experience shows it is faster than clear
    m_OpVec.erase(m_OpVec.begin(), m_OpVec.end());
    return OK;
}

RC SeqIntSubtest::RandomizeOncePerRestart(UINT32 Frame)
{
    for (size_t i=0; i < m_OpVec.size(); i++)
    {
        SeqKernelOp* pOp = &m_OpVec[i];
        pOp->OpCode   = Pick(CRTST_KRNL_SEQ_MTX_INT_OPCODE);
        pOp->Operand1 = Pick(CRTST_KRNL_SEQ_MTX_OPERAND);
        pOp->Operand2 = Pick(CRTST_KRNL_SEQ_MTX_OPERAND);
        pOp->Operand3 = CRTST_KRNL_SEQ_MTX_OPERAND_None;

        // Enforce requirements:
        //   OpCode should be legal.
        //   Operands should be legal -- SeqInt fails with tex inputs!
        //   Operand1 and Operand2 should be different.

        if (pOp->OpCode < CRTST_KRNL_SEQ_MTX_OPCODE_None)
            pOp->OpCode = CRTST_KRNL_SEQ_MTX_OPCODE_None;

        if (pOp->Operand1 > CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB)
            pOp->Operand1 = CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB;

        if (pOp->Operand2 >= CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB)
            pOp->Operand2 = CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB;

        if (pOp->Operand1 == pOp->Operand2)
            pOp->Operand2 ^= 1;
    }

    // Copy to constant device memory
    m_SeqIntKernelOps.Set(&m_OpVec[0], 0/*stream*/,
                         SeqIntOpsSize * sizeof(SeqKernelOp));
    return OK;
}

void SeqIntSubtest::ReadKernelOps()
{
    m_SeqIntKernelOps.Get(&m_OpVec[0], 0/*stream*/,
                         SeqIntOpsSize * sizeof(SeqKernelOp));
}

LaunchParam * SeqIntSubtest::NewLaunchParam()
{
    SeqLaunchParam * pLP = new SeqLaunchParam;
    pLP->pSubtest = this;
    return pLP;
}

void SeqIntSubtest::FillLaunchParam(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);

    pLP->kernelParam.numOps = min(
        Pick(CRTST_KRNL_SEQ_MTX_INT_NUM_OPS), SeqIntOpsSize);
    pLP->kernelParam.opIdx = min(
        Pick(CRTST_KRNL_SEQ_MTX_INT_OP_IDX),
        SeqIntOpsSize - pLP->kernelParam.numOps);
    pLP->kernelParam.inc = Pick(CRTST_KRNL_SEQ_MTX_DEBUG_IDX_INC);

    // Output tile is correct already, we call it "C" in lwrkernel.lw.
    pLP->kernelParam.C = pLP->outputTile;

    // Input tiles are corresponding parts of A,B matrices.
    pLP->kernelParam.A = pLP->outputTile;
    pLP->kernelParam.B = pLP->outputTile;
    pLP->kernelParam.SMID = pLP->outputTile;

    // Need to fix A,B gpu virtual addresses after copying from C.
    pLP->kernelParam.A.elements = DevTileAddr(mxInputA, &pLP->kernelParam.A);
    pLP->kernelParam.B.elements = DevTileAddr(mxInputB, &pLP->kernelParam.B);
    pLP->kernelParam.SMID.elements = DevTileAddr(mxOutputSMID, &pLP->kernelParam.SMID);

    // Check the returned SMID values to ensure that the kernel did actual work
    pLP->checkSmid = true;

    MASSERT(pLP->kernelParam.C.elements == DevTileAddr(mxOutputC, &pLP->kernelParam.C));
}

void SeqIntSubtest::FillSurfaces(LaunchParam* pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);

    (this->*m_pFillTile)(&pLP->kernelParam.A, Pick(CRTST_KRNL_SEQ_MTX_INT_MAT_A_MIN),
                         Pick(CRTST_KRNL_SEQ_MTX_INT_MAT_A_MAX), pLP);
    (this->*m_pFillTile)(&pLP->kernelParam.B, Pick(CRTST_KRNL_SEQ_MTX_INT_MAT_B_MIN),
                         Pick(CRTST_KRNL_SEQ_MTX_INT_MAT_B_MAX), pLP);
    (this->*m_pFillTile)(&pLP->kernelParam.C, Pick(CRTST_KRNL_SEQ_MTX_INT_MAT_C_MIN),
                         Pick(CRTST_KRNL_SEQ_MTX_INT_MAT_C_MAX), pLP);
}

RC SeqIntSubtest::Launch(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    SeqLaunchParam * pLP = static_cast<SeqLaunchParam*>(pLPbase);

    // For Opcode coverage stats
    coverageTracker.OpcodeAdd<OpVec>(CRTST_CTRL_KERNEL_SeqMatrixInt,
                                     pLP->kernelParam.numOps,
                                     pLP->kernelParam.opIdx,
                                     m_OpVec);

    SetLaunchDim(pLP);
    return GetFunction().Launch(GetStream(pLP->streamIdx), pLP->kernelParam, pLP->gFP);
}

void SeqIntSubtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const SeqLaunchParam * pLP = static_cast<const SeqLaunchParam*>(pLPbase);

    pLP->Print(pri);

    for (UINT32 i = 0; i < pLP->kernelParam.numOps; i++)
    {
        const int opIdx = pLP->kernelParam.opIdx + i;
        Printf(pri, "Index:%d Operand1:%d Operand2:%d Operand3:%d OpCode:%s \n",
               opIdx,
               m_OpVec[opIdx].Operand1,
               m_OpVec[opIdx].Operand2,
               m_OpVec[opIdx].Operand3,
               SeqOpcodeToString(m_OpVec[opIdx].OpCode));
    }
}

const SeqKernelOp& SeqIntSubtest::GetSeqKernelOp(UINT32 idx)
{
    return m_OpVec[idx];
}

Subtest * NewSeqIntSubtest
(
    LwdaRandomTest       * pTest,
    const Lwca::Instance * pInst,
    Lwca::Device           dev
)
{
    return new SeqIntSubtest(pTest, pInst, dev);
}

} // namespace LwdaRandom
