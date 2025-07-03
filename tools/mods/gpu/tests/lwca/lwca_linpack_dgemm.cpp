/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/
#include "lwda_linpack.h"
#include "core/include/help.h"
#include "core/include/utility.h"

#include "linpack/maxwell_mods_decls.h"
#include "linpack/volta_mods_decls.h"
#include "linpack/turing_mods_decls.h"
#include "linpack/ampere_mods_decls.h"

class LwdaLinpackDgemm : public LwdaLinpack
{
    public:
        LwdaLinpackDgemm();
        ~LwdaLinpackDgemm() override {}
        void TestHelp(JSContext *pCtx, regex *pRegex) override;

    private:
        RC RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize) override;
        RC AllocMemory() override;
};

//-----------------------------------------------------------------------
//! \brief Constructor. Initialize GEMM specific data
//!
LwdaLinpackDgemm::LwdaLinpackDgemm()
{
    SetName("LwdaLinpackDgemm");
    m_Instr = Instr::DFMA;
    m_GemmData.moduleName = "arch_dgemm_nt";
    m_GemmData.genRandomFuncName = "GenRandomDataD";
    m_GemmData.genConstantFuncName = "GenConstantDataD";
    m_GemmData.checkFuncName = "CompareOnDeviceD";
    m_GemmData.naiveGemmFuncName = "NaiveDgemm";

    AddKernelFromArch(Lwca::Capability::SM_50, maxwell_dgemm_128x64_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_52, maxwell_dgemm_128x64_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_53, maxwell_dgemm_128x64_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_60, maxwell_dgemm_128x64_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_61, maxwell_dgemm_128x64_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_70, volta_dgemm_128x64_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_72, volta_dgemm_128x64_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_75, turing_dgemm_128x64_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_80, ampere_dgemm_128x64_mods_nt_params);
}

void LwdaLinpackDgemm::TestHelp(JSContext *pCtx, regex *pRegex)
{
    bool bObjectFound;
    Help::JsObjHelp(GetJSObject(),pCtx,pRegex,0,NULL,&bObjectFound);
    LwdaLinpack::PropertyHelp(pCtx,pRegex);
}

//-----------------------------------------------------------------------------------
//! \brief Calls LWCA kernel to Run IGEMM
//!
RC LwdaLinpackDgemm::RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize)
{
    RC rc;

    double alpha = 0, beta = 0;

    // C = alpha * AB + beta * C (C is initialized to A*B, with alpha = 1, beta = 0)
    // Theoretically any value of alpha/beta should work that sum up to 1.0
    // If RunDgemm is called to Initialize Matrix C with A*B, Set alpha = 1, beta = 0
    if (initialize)
    {
        alpha      = 1.0;
        beta       = 0.0;
    }
    // else, values like alpha = 0.5, beta = 0.5, yields C = 0.5*AB+ 0.5*AB = AB once again.
    else
    {
        alpha = m_Alpha;
        beta  = m_Beta;
    }

    ByteStream paramBuffer;
    switch (m_ComputeCap)
    {
        case Lwca::Capability::SM_50:
        case Lwca::Capability::SM_52:
        case Lwca::Capability::SM_53:
        case Lwca::Capability::SM_60:
        case Lwca::Capability::SM_61:
        case Lwca::Capability::SM_70:
        case Lwca::Capability::SM_72:
        case Lwca::Capability::SM_75:
        case Lwca::Capability::SM_80:
            FillParamBufferCommon(A, B, C, alpha, beta, paramBuffer);
            break;

        default:
            MASSERT(0);
    }

    CHECK_RC(Launch(paramBuffer));
    return rc;
}

//-----------------------------------------------------------------------------------------
//! \brief Allocates memory with the correct datatype
//!
RC LwdaLinpackDgemm::AllocMemory()
{
    RC rc;
    CHECK_RC(AllocZeroMemoryBuffer<double>());
    return LwdaLinpack::AllocMemory();
}

JS_CLASS_INHERIT(LwdaLinpackDgemm, LwdaLinpack, "Run the Lwca DGEMM stress test.");
