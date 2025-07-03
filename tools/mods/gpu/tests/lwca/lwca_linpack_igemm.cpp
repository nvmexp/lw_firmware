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

class LwdaLinpackIgemm : public LwdaLinpack
{
    public:
        LwdaLinpackIgemm();
        ~LwdaLinpackIgemm() override {}
        RC Setup() override;
        void TestHelp(JSContext *pCtx, regex *pRegex) override;

    private:
        RC ValidateAlphaBeta() override;
        RC ScaleReferenceData(UINT32 scale) override;
        RC ResetReferenceData() override;
        RC RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize) override;

        Lwca::Function m_ScaleMatrixFunc;
};

//-----------------------------------------------------------------------
//! \brief Constructor
//!
LwdaLinpackIgemm::LwdaLinpackIgemm()
{
    SetName("LwdaLinpackIgemm");
    m_Instr = Instr::IDP4A_S32_S8;
    m_GemmData.moduleName = "arch_igemm_nt";
    m_GemmData.genRandomFuncName = "GenRandomDataI";
    m_GemmData.genConstantFuncName = "GenConstantDataI";
    m_GemmData.checkFuncName = "CompareOnDeviceI";
    m_GemmData.naiveGemmFuncName = "NaiveIgemm";
    AddKernelFromArch(Lwca::Capability::SM_61, maxwell_igemm_int8_128x128_ldg4_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_70,   volta_igemm_int8_128x128_ldg4_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_72,   volta_igemm_int8_128x128_ldg4_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_75,  turing_igemm_int8_128x128_ldg4_mods_nt_params);
    AddKernelFromArch(Lwca::Capability::SM_80,  ampere_igemm_int8_128x128_ldg4_mods_nt_params);

    // For IGEMM kernel Alpha and Beta must be 1.
    // Set Alpha and Beta to 1 here so that the JS prints reflect this.
    m_DefaultAlpha = 1.0;
    m_DefaultBeta = 1.0;
}

RC LwdaLinpackIgemm::Setup()
{
    RC rc;
    CHECK_RC(LwdaLinpack::Setup());

    // This is only relevant for IGEMM
    m_ScaleMatrixFunc = m_Module.GetFunction("ScaleMatrixI");
    CHECK_RC(m_ScaleMatrixFunc.InitCheck());
    m_ScaleMatrixFunc.SetGridDim(m_NumBlocks);
    m_ScaleMatrixFunc.SetBlockDim(m_NumThreads);
    return rc;
}

void LwdaLinpackIgemm::TestHelp(JSContext *pCtx, regex *pRegex)
{
    bool bObjectFound;
    Help::JsObjHelp(GetJSObject(),pCtx,pRegex,0,NULL,&bObjectFound);
    LwdaLinpack::PropertyHelp(pCtx,pRegex);
}

//------------------------------------------------------------------------------
//! \brief Check that Alpha and Beta are valid (unless the check is overridden)
//!
//! Alpha and Beta are ignored by IGEMM since the kernel only accepts 1 or 0.
//! We always init with Alpha=1, Beta=0 and run with Alpha=1, Beta=1.
//!
//! Fail if the user sets Alpha and Beta to anything other than 1,
//! since it would be misleading to override without failing.
//!
RC LwdaLinpackIgemm::ValidateAlphaBeta()
{
    if ((m_Alpha != 1.0 || m_Beta != 1.0) && !m_SkipAlphaBetaCheck)
    {
        Printf(Tee::PriError, "Both Alpha and Beta must be set to 1 for IGEMM.\n");
        return RC::BAD_PARAMETER;
    }
    else
    {
        return OK;
    }
}

//-----------------------------------------------------------------------------------
//! \brief Calls LWCA kernel to Run IGEMM
//!
RC LwdaLinpackIgemm::RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize)
{
    RC rc;

    // Igemm only accepts alpha/beta values having 1 or 0
    INT32 alpha = 1;
    INT32 beta = initialize ? 0 : 1;

    ByteStream paramBuffer;
    FillParamBufferCommon(A, B, C, alpha, beta, paramBuffer);

    CHECK_RC(Launch(paramBuffer));

    return rc;
}

//-----------------------------------------------------------------------------------------
//! \brief Calls the Function for Scaling the Reference Data
//!
RC LwdaLinpackIgemm::ScaleReferenceData(UINT32 scale)
{
    RC rc;
    if (UseReferenceData())
    {
        CHECK_RC(m_ScaleMatrixFunc.Launch(
                        CalcRefMatrixCPtr(),
                        m_OutputBufferLen,
                        scale));
    }
    else
    {
        CHECK_RC(m_ScaleMatrixFunc.Launch(
                        CalcMatrixCPtr(),
                        m_OutputBufferLen,
                        scale));
        CallwlateCrc(CalcMatrixCPtr(), CalcRefCrcBufPtr());
    }
    return rc;
}

//-----------------------------------------------------------------------------------------
//! \brief Resets the reference data
//!
RC LwdaLinpackIgemm::ResetReferenceData()
{
    auto cPtr = (UseReferenceData()) ? CalcRefMatrixCPtr() : CalcMatrixCPtr();
    return RunGemm(CalcMatrixAPtr(), CalcMatrixBPtr(), cPtr, true);
}

JS_CLASS_INHERIT(LwdaLinpackIgemm,LwdaLinpack,"Run LwdaLinpack Igemm Test");
