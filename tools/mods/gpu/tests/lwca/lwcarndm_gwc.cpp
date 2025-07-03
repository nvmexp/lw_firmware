/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdarndm.h"
#include "core/include/version.h"
#include "core/include/xp.h"

namespace LwdaRandom
{

//-----------------------------------------------------------------------------
// Describe a kernel launch for the GpuWorkCreation kernel of LwdaRandom.
//
class GpuWorkCreationLaunchParam : public LaunchParam
{
public:
    GpuWorkCreationLaunchParam()
        : LaunchParam()
    {
        memset(&kernelParam, 0, sizeof(kernelParam));
    }
    virtual ~GpuWorkCreationLaunchParam() { }

    void Print(Tee::Priority pri) const;

    GwcKernelParam kernelParam;  // defined in lwrkernel.h (host/gpu common)
};

void GpuWorkCreationLaunchParam::Print(Tee::Priority pri) const
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
// Hooks the GpuWorkCreation kernel to the core of LwdaRandom.
//
class GpuWorkCreationSubtest: public Subtest
{
public:
    GpuWorkCreationSubtest(LwdaRandomTest * pParent)
        : Subtest(pParent, "GpuWorkCreationTest")
    {
    }
    virtual ~GpuWorkCreationSubtest() { }
    virtual LaunchParam * NewLaunchParam();
    virtual void FillLaunchParam(LaunchParam* pLP);
    virtual void FillSurfaces(LaunchParam* pLP);
    virtual RC Launch(LaunchParam * pLP);
    virtual void Print(Tee::Priority pri, const LaunchParam * pLP);

    virtual bool IsSupported() const
    {
        // GM107 A01 and GM108 A01 have a bug that is worked-around in
        // the lwca compiler, except for in the CNP/GWC library.
        if (HasBug(1316294))
            return false;

        // The MODS GWC kernel requires the ability for the kernel to trap
        // back into the kernel driver.  This trap functionality requires the
        // lwbins to be compiled using the same ARCH (and presumably platform)
        // as the MODS exelwtable.  Without it, the trap doesnt function and
        // the GWC kernel fails to launch the internal kernel resulting in
        // test failures.
        //
        // Lwrrently the MODS LWBIN makefiles for dGPU SM architectures create
        // 64bit kernels by default since we build them on 64bit machines (the
        // LWCA lwcc compiler uses gcc as one of its steps.  The GCC used is the
        // one installed on whatever machine the LWBIN is built on).
        //
        // For SM architectures that are used on Android where the LWCA version
        // is sufficient to support the GWC kernels (> 3.5), the MODS
        // LWBIN makefiles compile for aarch64.
        //
        // If we ever want to support the GWC kernels on all MODS platforms
        // and architectures we would need to create different lwbins
        // per-platform and per-architecture
        bool bSupported = false;
        string sBuildArch = g_BuildArch;
        if (((Xp::GetOperatingSystem() == Xp::OS_LINUX) && (sBuildArch == "amd64")) ||
            ((Xp::GetOperatingSystem() == Xp::OS_LINUXSIM) && (sBuildArch == "amd64")) ||
            ((Xp::GetOperatingSystem() == Xp::OS_ANDROID) && (sBuildArch == "aarch64")))
        {
            bSupported = true;
        }

        return bSupported;
    }
};

LaunchParam * GpuWorkCreationSubtest::NewLaunchParam()
{
    GpuWorkCreationLaunchParam * pLP = new GpuWorkCreationLaunchParam;
    pLP->pSubtest = this;
    return pLP;
}

void GpuWorkCreationSubtest::FillLaunchParam(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    GpuWorkCreationLaunchParam * pLP = static_cast<GpuWorkCreationLaunchParam*>(pLPbase);

    // Pass the grid w,h to the kernel, which will launch StressTest for us.
    pLP->kernelParam.gridW = pLP->gridWidth;
    pLP->kernelParam.gridH = pLP->gridHeight;

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

    // Check the returned SMID values to ensure that the kernel did actual work
    pLP->checkSmid = true;
}

void GpuWorkCreationSubtest::FillSurfaces(LaunchParam* pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    GpuWorkCreationLaunchParam * pLP = static_cast<GpuWorkCreationLaunchParam*>(pLPbase);

    FillTilesFloat32Range(&pLP->kernelParam.A,
                          &pLP->kernelParam.B,
                          &pLP->kernelParam.C, pLP);
}

RC GpuWorkCreationSubtest::Launch(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    GpuWorkCreationLaunchParam * pLP = static_cast<GpuWorkCreationLaunchParam*>(pLPbase);

    // We launch exactly one block, it launches the others.
    pLP->gridWidth = 1;
    pLP->gridHeight = 1;
    SetLaunchDim(pLP);

    // Restore args so the printing code doesn't get confused.
    pLP->gridWidth = pLP->kernelParam.gridW;
    pLP->gridHeight = pLP->kernelParam.gridH;

    return GetFunction().Launch(GetStream(pLP->streamIdx), pLP->kernelParam, pLP->gFP);
}

void GpuWorkCreationSubtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const GpuWorkCreationLaunchParam * pLP = static_cast<const GpuWorkCreationLaunchParam*>(pLPbase);

    pLP->Print(pri);
}

Subtest * NewGpuWorkCreationSubtest(LwdaRandomTest * pTest)
{
    return new GpuWorkCreationSubtest(pTest);
}

} // namespace LwdaRandom
