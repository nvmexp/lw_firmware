/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdarndm.h"

namespace LwdaRandom
{

//-----------------------------------------------------------------------------
// Describe a kernel launch for either the Stress kernel of LwdaRandom.
//
class StressLaunchParam : public LaunchParam
{
public:
    StressLaunchParam()
        : LaunchParam()
    {
        memset(&kernelParam, 0, sizeof(kernelParam));
    }
    virtual ~StressLaunchParam() { }

    void Print(Tee::Priority pri) const;

    SeqKernelParam kernelParam;  // defined in lwdarandom.h (host/gpu common)
};

void StressLaunchParam::Print(Tee::Priority pri) const
{
    Printf(pri,
           "Kernel Params:\n"
           "NumOps:%d\n",
           kernelParam.numOps);
    PrintTile(pri, "MatrixA", kernelParam.A);
    PrintTile(pri, "MatrixB", kernelParam.B);
    PrintTile(pri, "MatrixC", kernelParam.C);
}

//-----------------------------------------------------------------------------
// Hooks the Stress kernel to the core of LwdaRandom.
//
class StressSubtest: public Subtest
{
public:
    StressSubtest(LwdaRandomTest * pParent)
        : Subtest(pParent, "StressTest")
    {
    }
    virtual ~StressSubtest() { }
    virtual LaunchParam * NewLaunchParam();
    virtual void FillLaunchParam(LaunchParam* pLP);
    virtual void FillSurfaces(LaunchParam* pLP);
    virtual RC Launch(LaunchParam * pLP);
    virtual void Print(Tee::Priority pri, const LaunchParam * pLP);
};

LaunchParam * StressSubtest::NewLaunchParam()
{
    StressLaunchParam * pLP = new StressLaunchParam;
    pLP->pSubtest = this;
    return pLP;
}

void StressSubtest::FillLaunchParam(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    StressLaunchParam * pLP = static_cast<StressLaunchParam*>(pLPbase);

    // Randomly choose how many ops to use during this launch.
    pLP->kernelParam.numOps = Pick(CRTST_KRNL_STRESS_NUM_OPS);

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
}

void StressSubtest::FillSurfaces(LaunchParam* pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    StressLaunchParam * pLP = static_cast<StressLaunchParam*>(pLPbase);

    float loBound = FPick(CRTST_KRNL_STRESS_MAT_C_LOW_BOUND);
    float hiBound = FPick(CRTST_KRNL_STRESS_MAT_C_HIGH_BOUND);

    FillTilesFloat32Exp(&pLP->kernelParam.A, &pLP->kernelParam.B, NULL, pLP);
    FillTileFloat32Range(&pLP->kernelParam.C, loBound, hiBound,
                         GetDebugFill(mxOutputC), pLP);
}

RC StressSubtest::Launch(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    StressLaunchParam * pLP = static_cast<StressLaunchParam*>(pLPbase);

    SetLaunchDim(pLP);
    return GetFunction().Launch(GetStream(pLP->streamIdx), pLP->kernelParam, pLP->gFP);
}

void StressSubtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const StressLaunchParam * pLP = static_cast<const StressLaunchParam*>(pLPbase);

    pLP->Print(pri);
}

Subtest * NewStressSubtest(LwdaRandomTest * pTest)
{
    return new StressSubtest(pTest);
}

} // namespace LwdaRandom
