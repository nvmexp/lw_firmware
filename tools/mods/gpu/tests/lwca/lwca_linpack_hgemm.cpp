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
#include "core/include/utility.h"
#include "core/include/help.h"

#include "linpack/maxwell_mods_decls.h"
#include "linpack/volta_mods_decls.h"
#include "linpack/turing_mods_decls.h"
#include "linpack/ampere_mods_decls.h"

class LwdaLinpackHgemm : public LwdaLinpack
{
    public:
        LwdaLinpackHgemm();
        ~LwdaLinpackHgemm() override {}
        void TestHelp(JSContext *pCtx, regex *pRegex) override;

    private:
        RC ValidateAlphaBeta() override;
        RC RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize) override;
};

//-----------------------------------------------------------------------
//! \brief Constructor
//!
LwdaLinpackHgemm::LwdaLinpackHgemm()
{
    SetName("LwdaLinpackHgemm");
    m_Instr = Instr::HFMA2;
    m_GemmData.moduleName = "arch_hgemm_nt";
    m_GemmData.genRandomFuncName = "GenRandomDataH";
    m_GemmData.genConstantFuncName = "GenConstantDataH";
    m_GemmData.checkFuncName = "CompareOnDeviceHLegacy";
    m_GemmData.naiveGemmFuncName = "NaiveHgemm";
    AddKernelFromArch(Lwca::Capability::SM_53, maxwell_hgemm_256x128_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_60, maxwell_hgemm_256x128_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_61, maxwell_hgemm_256x128_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_70,   volta_hgemm_256x128_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_72,   volta_hgemm_256x128_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_75,  turing_hgemm_256x128_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_80,  ampere_hgemm_256x128_mods_nt_params);
}

void LwdaLinpackHgemm::TestHelp(JSContext *pCtx, regex *pRegex)
{
    bool bObjectFound;
    Help::JsObjHelp(GetJSObject(),pCtx,pRegex,0,NULL,&bObjectFound);
    LwdaLinpack::PropertyHelp(pCtx,pRegex);
}

//------------------------------------------------------------------------------
//! \brief Check that Alpha and Beta are valid (unless the check is overridden)
//
//! HGEMM requires that Alpha and Beta are <= 1.0, due to issues with the
//! rounding of floating point numbers.
//!
RC LwdaLinpackHgemm::ValidateAlphaBeta()
{
    StickyRC stickyRc = LwdaLinpack::ValidateAlphaBeta();
    if ((m_Alpha > 1.0 || m_Beta > 1.0) && !m_SkipAlphaBetaCheck)
    {
        Printf(Tee::PriError, "Alpha and Beta must be each <= 1.0 for HGEMM\n");
        stickyRc = RC::BAD_PARAMETER;
    }

    return stickyRc;
}

//-----------------------------------------------------------------------------------
//! \brief Calls LWCA kernel to Run HGEMM
//!
RC LwdaLinpackHgemm::RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize)
{
    RC rc;

    UINT16 alpha_half = 0, beta_half = 0;

    // C = alpha * AB + beta * C (C is initialized to A*B, with alpha = 1, beta = 0)
    // Theoretically any value of alpha/beta should work that sum up to 1.0
    // If RunDgemm is called to Initialize Matrix C with A*B, Set alpha = 1, beta = 0
    if (initialize)
    {
        alpha_half = Utility::Float32ToFloat16(1.0);
        beta_half  = Utility::Float32ToFloat16(0.0);
    }
    // else, values like alpha = 0.5, beta = 0.5, yields C = 0.5*AB+ 0.5*AB = AB once again.
    else
    {
        alpha_half = Utility::Float32ToFloat16(static_cast<FLOAT32>(m_Alpha));
        beta_half  = Utility::Float32ToFloat16(static_cast<FLOAT32>(m_Beta));
    }

    ByteStream paramBuffer;
    FillParamBufferCommon(A, B, C, alpha_half, beta_half, paramBuffer);

    CHECK_RC(Launch(paramBuffer));
    return rc;
}

JS_CLASS_INHERIT(LwdaLinpackHgemm, LwdaLinpack, "Run the Lwca HGEMM stress test.");
