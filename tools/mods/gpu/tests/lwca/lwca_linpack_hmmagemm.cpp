/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/
#include "lwda_linpack.h"
#include "core/include/utility.h"
#include "core/include/help.h"
#include "gpu/include/gpusbdev.h"

#include "linpack/volta_mods_modular_decls.h"
#include "linpack/turing_mods_modular_decls.h"
#include "linpack/ampere_mods_modular_decls.h"

class LwdaLinpackHMMAgemm : public LwdaLinpack
{
    public:
        LwdaLinpackHMMAgemm();
        ~LwdaLinpackHMMAgemm() override {}
        void PrintJsProperties(Tee::Priority pri) override;
        RC Setup() override;
        void TestHelp(JSContext *pCtx, regex *pRegex) override;

        SETGET_PROP(KernelType, string);

    protected:
        void AddKernelFromArch(Lwca::Capability::Enum cap,
                               const ShaderParams& params) override;

    private:
        RC ValidateAlphaBeta() override;
        RC RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize) override;

        UINT32 m_SizeofAclwmulate;

        // JS properties
        string m_KernelType;
};

//-----------------------------------------------------------------------
//! \brief Constructor
//!
LwdaLinpackHMMAgemm::LwdaLinpackHMMAgemm()
{
    SetName("LwdaLinpackHMMAgemm");
    m_GemmData.moduleName = ""; // Initialize this in Setup, since there are multiple kernels to choose from
    m_GemmData.genRandomFuncName = "GenRandomDataH";
    m_GemmData.genConstantFuncName = "GenConstantDataH";
    m_GemmData.checkFuncName = "CompareOnDeviceHLegacy";
    m_GemmData.naiveGemmFuncName = ""; // Lwrrently not supported for HMMA
    m_GemmData.smParameters[Lwca::Capability::SM_70] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_72] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_75] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_80] = {};
    m_KernelType = "";
    m_SizeofAclwmulate = 0;
}

void LwdaLinpackHMMAgemm::PrintJsProperties(Tee::Priority pri)
{
    LwdaLinpack::PrintJsProperties(pri);
    Printf(pri, "   KernelType          %s\n", m_KernelType.c_str());
}

void LwdaLinpackHMMAgemm::AddKernelFromArch
(
    Lwca::Capability::Enum cap,
    const ShaderParams& params
)
{
    m_SizeofAclwmulate = static_cast<UINT32>(elementDataSize(params.typeEpilog));
    LwdaLinpack::AddKernelFromArch(cap, params);
}

RC LwdaLinpackHMMAgemm::Setup()
{
    Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    const bool allow884   = (cap <= Lwca::Capability::SM_75);
    const bool allow1688  = (cap >= Lwca::Capability::SM_75 && cap < Lwca::Capability::SM_80);
    const bool allow16816 = (cap >= Lwca::Capability::SM_80);

    // If KernelType isn't set, apply defaults
    if (m_KernelType == "")
    {
        if (allow16816)
        {
            m_KernelType = "s16816_128x128";
        }
        else if (allow1688)
        {
            m_KernelType = "s1688_128x128";
        }
        else if (allow884)
        {
            m_KernelType = "s884_128x128";
        }
    }

    // Select kernel depending on HMMA type
    if (m_KernelType == "h884_256x128" && allow884)
    {
        m_Instr = Instr::HMMA_884_F16_F16;
        m_GemmData.moduleName = "arch_h884gemm_256x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_70,  volta_h884gemm_256x128_ldg8_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_72,  volta_h884gemm_256x128_ldg8_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_75, turing_h884gemm_256x128_ldg8_mods_nt_params);
    }
    else if (m_KernelType == "h884_128x128" && allow884)
    {
        m_Instr = Instr::HMMA_884_F16_F16;
        m_GemmData.moduleName = "arch_h884gemm_128x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_70,  volta_h884gemm_128x128_ldg8_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_72,  volta_h884gemm_128x128_ldg8_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_75, turing_h884gemm_128x128_ldg8_mods_nt_params);
    }
    else if (m_KernelType == "s884_256x128" && allow884)
    {
        m_Instr = Instr::HMMA_884_F32_F16;
        m_GemmData.moduleName = "arch_fp16_s884gemm_fp16_256x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_70,  volta_fp16_s884gemm_fp16_256x128_ldg8_f2f_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_72,  volta_fp16_s884gemm_fp16_256x128_ldg8_f2f_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_75, turing_fp16_s884gemm_fp16_256x128_ldg8_f2f_mods_nt_params);
    }
    else if (m_KernelType == "s884_128x128" && allow884)
    {
        m_Instr = Instr::HMMA_884_F32_F16;
        m_GemmData.moduleName = "arch_fp16_s884gemm_fp16_128x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_70,  volta_fp16_s884gemm_fp16_128x128_ldg8_f2f_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_72,  volta_fp16_s884gemm_fp16_128x128_ldg8_f2f_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_75, turing_fp16_s884gemm_fp16_128x128_ldg8_f2f_mods_nt_params);
    }
    else if (m_KernelType == "h1688_256x128" && allow1688)
    {
        m_Instr = Instr::HMMA_1688_F16_F16;
        m_GemmData.moduleName = "arch_h1688gemm_256x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_75, turing_h1688gemm_256x128_ldg8_mods_nt_params);
    }
    else if (m_KernelType == "h1688_128x128" && allow1688)
    {
        m_Instr = Instr::HMMA_1688_F16_F16;
        m_GemmData.moduleName = "arch_h1688gemm_128x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_75, turing_h1688gemm_128x128_ldg8_mods_nt_params);
    }
    else if (m_KernelType == "s1688_256x128" && allow1688)
    {
        m_Instr = Instr::HMMA_1688_F32_F16;
        m_GemmData.moduleName = "arch_fp16_s1688gemm_fp16_256x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_75, turing_fp16_s1688gemm_fp16_256x128_ldg8_f2f_mods_nt_params);
    }
    else if (m_KernelType == "s1688_128x128" && allow1688)
    {
        m_Instr = Instr::HMMA_1688_F32_F16;
        m_GemmData.moduleName = "arch_fp16_s1688gemm_fp16_128x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_75, turing_fp16_s1688gemm_fp16_128x128_ldg8_f2f_mods_nt_params);
    }
    else if (m_KernelType == "h16816_256x128" && allow16816)
    {
        m_Instr = Instr::HMMA_16816_F16_F16;
        m_UseRegDescriptor = true;
        m_GemmData.moduleName = "arch_h16816gemm_256x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_80, ampere_h16816gemm_256x128_ldg8_mods_stages_64x3_nt_params);
    }
    else if (m_KernelType == "h16816_128x128" && allow16816)
    {
        m_Instr = Instr::HMMA_16816_F16_F16;
        m_UseRegDescriptor = true;
        m_GemmData.moduleName = "arch_h16816gemm_128x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_80, ampere_h16816gemm_128x128_ldg8_mods_stages_64x3_nt_params);
    }
    else if (m_KernelType == "s16816_256x128" && allow16816)
    {
        m_Instr = Instr::HMMA_16816_F32_F16;
        m_UseRegDescriptor = true;
        m_GemmData.moduleName = "arch_fp16_s16816gemm_fp16_256x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_80, ampere_fp16_s16816gemm_fp16_256x128_ldg8_f2f_mods_stages_64x3_nt_params);
    }
    else if (m_KernelType == "s16816_128x128" && allow16816)
    {
        m_Instr = Instr::HMMA_16816_F32_F16;
        m_UseRegDescriptor = true;
        m_GemmData.moduleName = "arch_fp16_s16816gemm_fp16_128x128_nt";
        AddKernelFromArch(Lwca::Capability::SM_80, ampere_fp16_s16816gemm_fp16_128x128_ldg8_f2f_mods_stages_64x3_nt_params);
    }
    else
    {
        Printf(Tee::PriError, "Unsupported HMMA kernel type %s\n", m_KernelType.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    // Run rest of setup
    return LwdaLinpack::Setup();
}

void LwdaLinpackHMMAgemm::TestHelp(JSContext *pCtx, regex *pRegex)
{
    bool bObjectFound;
    Help::JsObjHelp(GetJSObject(),pCtx,pRegex,0,NULL,&bObjectFound);
    LwdaLinpack::PropertyHelp(pCtx,pRegex);
}

//------------------------------------------------------------------------------
//! \brief Check that Alpha and Beta are valid (unless the check is overridden)
//
//! HMMAGEMM requires that Alpha and Beta are <= 1.0, due to issues with the
//! rounding of floating point numbers.
//!
RC LwdaLinpackHMMAgemm::ValidateAlphaBeta()
{
    StickyRC stickyRc = LwdaLinpack::ValidateAlphaBeta();
    if ((m_Alpha > 1.0 || m_Beta > 1.0) && !m_SkipAlphaBetaCheck)
    {
        Printf(Tee::PriError, "Alpha and Beta must be each <= 1.0 for HMMAGEMM\n");
        stickyRc = RC::BAD_PARAMETER;
    }

    return stickyRc;
}

//-----------------------------------------------------------------------------------
//! \brief Calls LWCA kernel to Run HGEMM
//!
RC LwdaLinpackHMMAgemm::RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize)
{
    RC rc;

    UINT16 alpha_half = 0, beta_half = 0;
    FLOAT32 alpha = 0, beta = 0;

    // C = alpha * AB + beta * C (C is initialized to A*B, with alpha = 1, beta = 0)
    // Theoretically any value of alpha/beta should work that sum up to 1.0
    // If RunGemm is called to Initialize Matrix C with A*B, Set alpha = 1, beta = 0
    if (initialize)
    {
        alpha_half = Utility::Float32ToFloat16(1.0);
        beta_half  = Utility::Float32ToFloat16(0.0);

        alpha = 1.0;
        beta  = 0.0;
    }
    // else, values like alpha = 0.5, beta = 0.5, yields C = 0.5*AB+ 0.5*AB = AB once again.
    else
    {
        alpha_half = Utility::Float32ToFloat16(static_cast<FLOAT32>(m_Alpha));
        beta_half  = Utility::Float32ToFloat16(static_cast<FLOAT32>(m_Beta));

        alpha = static_cast<FLOAT32>(m_Alpha);
        beta  = static_cast<FLOAT32>(m_Beta);
    }

    // The size of Alpha and Beta depends on the precision of the aclwmulator,
    // not the input or output matrix.
    ByteStream paramBuffer;
    if (m_SizeofAclwmulate == sizeof(float))
    {
        FillParamBufferCommon(A, B, C, alpha, beta, paramBuffer);
    }
    else
    {
        FillParamBufferCommon(A, B, C, alpha_half, beta_half, paramBuffer);
    }

    CHECK_RC(Launch(paramBuffer));
    return rc;
}

JS_CLASS_INHERIT(LwdaLinpackHMMAgemm, LwdaLinpack, "Run the Lwca HMMAGEMM stress test.");
CLASS_PROP_READWRITE(LwdaLinpackHMMAgemm, KernelType, string, "Explicity specify type of HMMA kernel.");
