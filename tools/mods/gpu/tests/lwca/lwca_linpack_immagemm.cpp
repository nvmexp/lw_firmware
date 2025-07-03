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
#include "core/include/help.h"
#include "core/include/utility.h"

#include "linpack/volta_mods_modular_decls.h"
#include "linpack/turing_mods_modular_decls.h"
#include "linpack/ampere_mods_modular_decls.h"

class LwdaLinpackIMMAgemm : public LwdaLinpack
{
    public:
        LwdaLinpackIMMAgemm();
        ~LwdaLinpackIMMAgemm() override {}
        void PrintJsProperties(Tee::Priority pri) override;
        RC Setup() override;
        void TestHelp(JSContext *pCtx, regex *pRegex) override;

        SETGET_PROP(KernelType, string);
    private:
        RC ValidateAlphaBeta() override;
        RC InitializeMParameterNormal() override;
        RC InitializeNParameterNormal() override;
        RC InitializeKParameterNormal() override;
        RC ScaleReferenceData(UINT32 scale) override;
        RC ResetReferenceData() override;
        RC RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize) override;

        Lwca::Function m_ScaleMatrixFunc;
        bool m_INT32Result = false;

        // JS properties
        string m_KernelType;
};

//-----------------------------------------------------------------------
//! \brief Constructor
//!
LwdaLinpackIMMAgemm::LwdaLinpackIMMAgemm()
{
    SetName("LwdaLinpackIMMAgemm");

    // Initialize this in Setup, since there are multiple kernels to choose from
    m_GemmData.smParameters[Lwca::Capability::SM_72] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_75] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_80] = {};

    m_KernelType = "";
}

void LwdaLinpackIMMAgemm::TestHelp(JSContext *pCtx, regex *pRegex)
{
    bool bObjectFound;
    Help::JsObjHelp(GetJSObject(),pCtx,pRegex,0,NULL,&bObjectFound);
    LwdaLinpack::PropertyHelp(pCtx,pRegex);
}

void LwdaLinpackIMMAgemm::PrintJsProperties(Tee::Priority pri)
{
    LwdaLinpack::PrintJsProperties(pri);
    Printf(pri, "   KernelType          %s\n", m_KernelType.c_str());
}

RC LwdaLinpackIMMAgemm::Setup()
{
    RC rc;

    Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    const bool allow8816  = (cap <  Lwca::Capability::SM_80);
    const bool allow16832 = (cap >= Lwca::Capability::SM_80);

    // If KernelType isn't set, apply defaults
    if (m_KernelType == "")
    {
        if (allow16832)
        {
            m_KernelType = "int32_i16832_128x128";
        }
        else if (allow8816)
        {
            m_KernelType = "int32_i8816_128x128";
        }
    }

    // Select kernel depending on IMMA type
    if (m_KernelType == "int8_i8816_256x128" && allow8816)
    {
        m_Instr = Instr::IMMA_8816_S32_S8;
        m_GemmData.moduleName = "arch_int8_i8816gemm_256x128_nt";
        m_GemmData.genRandomFuncName = "GenRandomDataIMMA";
        m_GemmData.genConstantFuncName = "GenConstantDataI";
        m_GemmData.checkFuncName = "CompareOnDeviceIMMA";
        m_GemmData.naiveGemmFuncName = "";
        AddKernelFromArch(Lwca::Capability::SM_72,
                          volta_int8_i8816gemm_int8_256x128_ldg16_relu_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_75,
                          turing_int8_i8816gemm_int8_256x128_ldg16_relu_mods_nt_params);

        // Set flag for whether we should scale results and overflow (like IGEMM)
        m_INT32Result = false; 

        // Kernel supports relu and bias
        m_FullyConnected = true;

        // For IMMAgemm kernel Alpha and Beta must be integers adding to 1.0, with Alpha > 0
        // (See ValidateAlphaBeta for why these constraints exist)
        m_DefaultAlpha = 2.0;
        m_DefaultBeta = -1.0;
    }
    else if (m_KernelType == "int8_i8816_128x128" && allow8816)
    {
        m_Instr = Instr::IMMA_8816_S32_S8;
        m_GemmData.moduleName = "arch_int8_i8816gemm_128x128_nt";
        m_GemmData.genRandomFuncName = "GenRandomDataIMMA";
        m_GemmData.genConstantFuncName = "GenConstantDataI";
        m_GemmData.checkFuncName = "CompareOnDeviceIMMA";
        m_GemmData.naiveGemmFuncName = "";
        AddKernelFromArch(Lwca::Capability::SM_72,
                          volta_int8_i8816gemm_int8_128x128_ldg16_relu_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_75,
                          turing_int8_i8816gemm_int8_128x128_ldg16_relu_mods_nt_params);

        // Set flag for whether we should scale results and overflow (like IGEMM)
        m_INT32Result = false; 

        // Kernel supports relu and bias
        m_FullyConnected = true;

        // For IMMAgemm kernel Alpha and Beta must be integers adding to 1.0, with Alpha > 0
        // (See ValidateAlphaBeta for why these constraints exist)
        m_DefaultAlpha = 2.0;
        m_DefaultBeta = -1.0;
    }
    else if (m_KernelType == "int32_i8816_256x128" && allow8816)
    {
        m_Instr = Instr::IMMA_8816_S32_S8;
        m_GemmData.moduleName = "arch_int32_i8816gemm_256x128_nt";
        m_GemmData.genRandomFuncName = "GenRandomDataI";
        m_GemmData.genConstantFuncName = "GenConstantDataI";
        m_GemmData.checkFuncName = "CompareOnDeviceI";
        m_GemmData.naiveGemmFuncName = "";

        AddKernelFromArch(Lwca::Capability::SM_72,
                          volta_int32_i8816gemm_int8_256x128_ldg16_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_75,
                          turing_int32_i8816gemm_int8_256x128_ldg16_mods_nt_params);

        // Set flag for whether we should scale results and overflow (like IGEMM)
        m_INT32Result = true;

        // For the INT32 output kernel, Alpha and Beta must be 1
        m_DefaultAlpha = 1.0;
        m_DefaultBeta =  1.0;
    }
    else if (m_KernelType == "int32_i8816_128x128" && allow8816)
    {
        m_Instr = Instr::IMMA_8816_S32_S8;
        m_GemmData.moduleName = "arch_int32_i8816gemm_128x128_nt";
        m_GemmData.genRandomFuncName = "GenRandomDataI";
        m_GemmData.genConstantFuncName = "GenConstantDataI";
        m_GemmData.checkFuncName = "CompareOnDeviceI";
        m_GemmData.naiveGemmFuncName = "";

        AddKernelFromArch(Lwca::Capability::SM_72,
                          volta_int32_i8816gemm_int8_128x128_ldg16_mods_nt_params);
        AddKernelFromArch(Lwca::Capability::SM_75,
                          turing_int32_i8816gemm_int8_128x128_ldg16_mods_nt_params);

        // Set flag for whether we should scale results and overflow (like IGEMM)
        m_INT32Result = true;

        // For the INT32 output kernel, Alpha and Beta must be 1
        m_DefaultAlpha = 1.0;
        m_DefaultBeta =  1.0;
    }
    else if (m_KernelType == "int8_i16832_256x128" && allow16832)
    {
        m_Instr = Instr::IMMA_16832_S32_S8;
        m_GemmData.moduleName = "arch_int8_i16832gemm_256x128_nt";
        m_GemmData.genRandomFuncName = "GenRandomDataIMMA";
        m_GemmData.genConstantFuncName = "GenConstantDataI";
        m_GemmData.checkFuncName = "CompareOnDeviceIMMA";
        m_GemmData.naiveGemmFuncName = "";
        AddKernelFromArch(Lwca::Capability::SM_80,
                          ampere_int8_i16832gemm_int8_256x128_ldg16_relu_mods_stages_64x3_nt_params);

        // Set flag for whether we should scale results and overflow (like IGEMM)
        m_INT32Result = false; 

        // Kernel supports relu and bias
        m_FullyConnected = true;

        // LDGSTS instructions require setting descriptor
        m_UseRegDescriptor = true;

        // For IMMAgemm kernel Alpha and Beta must be integers adding to 1.0, with Alpha > 0
        // (See ValidateAlphaBeta for why these constraints exist)
        m_DefaultAlpha = 2.0;
        m_DefaultBeta = -1.0;
    }
    else if (m_KernelType == "int8_i16832_128x128" && allow16832)
    {
        m_Instr = Instr::IMMA_16832_S32_S8;
        m_GemmData.moduleName = "arch_int8_i16832gemm_128x128_nt";
        m_GemmData.genRandomFuncName = "GenRandomDataIMMA";
        m_GemmData.genConstantFuncName = "GenConstantDataI";
        m_GemmData.checkFuncName = "CompareOnDeviceIMMA";
        m_GemmData.naiveGemmFuncName = "";
        AddKernelFromArch(Lwca::Capability::SM_80,
                          ampere_int8_i16832gemm_int8_128x128_ldg16_relu_mods_stages_64x3_nt_params);

        // Set flag for whether we should scale results and overflow (like IGEMM)
        m_INT32Result = false; 

        // Kernel supports relu and bias
        m_FullyConnected = true;

        // LDGSTS instructions require setting descriptor
        m_UseRegDescriptor = true;

        // For IMMAgemm kernel Alpha and Beta must be integers adding to 1.0, with Alpha > 0
        // (See ValidateAlphaBeta for why these constraints exist)
        m_DefaultAlpha = 2.0;
        m_DefaultBeta = -1.0;
    }
    else if (m_KernelType == "int32_i16832_256x128" && allow16832)
    {
        m_Instr = Instr::IMMA_16832_S32_S8;
        m_GemmData.moduleName = "arch_int32_i16832gemm_256x128_nt";
        m_GemmData.genRandomFuncName = "GenRandomDataI";
        m_GemmData.genConstantFuncName = "GenConstantDataI";
        m_GemmData.checkFuncName = "CompareOnDeviceI";
        m_GemmData.naiveGemmFuncName = "";

        AddKernelFromArch(Lwca::Capability::SM_80,
                          ampere_int32_i16832gemm_int8_256x128_ldg16_mods_stages_64x3_nt_params);

        // Set flag for whether we should scale results and overflow (like IGEMM)
        m_INT32Result = true;

        // LDGSTS instructions require setting descriptor
        m_UseRegDescriptor = true;

        // For the INT32 output kernel, Alpha and Beta must be 1
        m_DefaultAlpha = 1.0;
        m_DefaultBeta =  1.0;
    }
    else if (m_KernelType == "int32_i16832_128x128" && allow16832)
    {
        m_Instr = Instr::IMMA_16832_S32_S8;
        m_GemmData.moduleName = "arch_int32_i16832gemm_128x128_nt";
        m_GemmData.genRandomFuncName = "GenRandomDataI";
        m_GemmData.genConstantFuncName = "GenConstantDataI";
        m_GemmData.checkFuncName = "CompareOnDeviceI";
        m_GemmData.naiveGemmFuncName = "";

        AddKernelFromArch(Lwca::Capability::SM_80,
                          ampere_int32_i16832gemm_int8_128x128_ldg16_mods_stages_64x3_nt_params);

        // Set flag for whether we should scale results and overflow (like IGEMM)
        m_INT32Result = true;

        // LDGSTS instructions require setting descriptor
        m_UseRegDescriptor = true;

        // For the INT32 output kernel, Alpha and Beta must be 1
        m_DefaultAlpha = 1.0;
        m_DefaultBeta =  1.0;
    }
    else
    {
        Printf(Tee::PriError, "Unsupported IMMA kernel type %s\n", m_KernelType.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    // Override KSize alignment due to an IMMAgemm-specific requirement
    m_GemmData.smParameters[Lwca::Capability::SM_72].kSizeMultiple = 32;
    m_GemmData.smParameters[Lwca::Capability::SM_75].kSizeMultiple = 32;
    m_GemmData.smParameters[Lwca::Capability::SM_80].kSizeMultiple = 32;

    // VectorLanes is 32 only for IMMAgemm
    m_GemmData.smParameters[Lwca::Capability::SM_72].vectorLanes = 32;
    m_GemmData.smParameters[Lwca::Capability::SM_75].vectorLanes = 32;
    m_GemmData.smParameters[Lwca::Capability::SM_80].vectorLanes = 32;

    // Run rest of setup
    CHECK_RC(LwdaLinpack::Setup());

    // Init matrix scaling func if required
    if (m_INT32Result)
    {
        m_ScaleMatrixFunc = m_Module.GetFunction("ScaleMatrixI");
        CHECK_RC(m_ScaleMatrixFunc.InitCheck());
        m_ScaleMatrixFunc.SetGridDim(m_NumBlocks);
        m_ScaleMatrixFunc.SetBlockDim(m_NumThreads);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Check that Alpha and Beta are valid (unless the check is overridden)
//!
RC LwdaLinpackIMMAgemm::ValidateAlphaBeta()
{
    StickyRC stickyRc;
    if (m_INT32Result)
    {
        // The INT32-output kernel behaves the same as IGEMM
        if ((m_Alpha != 1.0 || m_Beta != 1.0) && !m_SkipAlphaBetaCheck)
        {
            Printf(Tee::PriError, "Both Alpha and Beta must be set to 1 for INT32 IMMAGEMM.\n");
            stickyRc = RC::BAD_PARAMETER;
        }
    }
    else
    {
        // Alpha and Beta work with INT8 IMMAGEMM, but must be integral since they are colwerted
        // by the kernel from float to INT32.
        //
        // Alpha must be positive to ensure consistency. This is because Alpha scales a 32bit
        // intermediate value while Beta scales an upcolwerted 8bit result. Since the IMMA results
        // saturate to -128 or 127, a negative Alpha would result in overflowed values swapping signs.
        //
        // For instance, consider the case where Alpha=-1 and Beta=2:
        // -300 * -1 + -128 * 2
        //  300      + -256
        //   44
        //   44 != -128
        //
        // Now if Alpha=2 and Beta=-1 everything is fine since the result saturates:
        // -300 * 2  + -128 * -1
        // -600      +  128
        // -472
        // -128 == -128
        stickyRc = LwdaLinpack::ValidateAlphaBeta();
        if ((floor(m_Alpha) != m_Alpha || floor(m_Beta) != m_Beta) && !m_SkipAlphaBetaCheck)
        {
            Printf(Tee::PriError, "Alpha and Beta must be integers for INT8 IMMAGEMM.\n");
            stickyRc = RC::BAD_PARAMETER;
        }
        if (m_Alpha < 0 && !m_SkipAlphaBetaCheck)
        {
            Printf(Tee::PriError, "Alpha must be >= 0 for INT8 IMMAGEMM.\n");
            stickyRc = RC::BAD_PARAMETER;
        }
    }
    return stickyRc;
}

//----------------------------------------------------------------------
//! \brief Initialize M variable for Max Power if it hasn't been set.
//!
RC LwdaLinpackIMMAgemm::InitializeMParameterNormal()
{
    RC rc;

    // Don't overwrite if Msize has been set by user
    if (m_Msize)
    {
        return rc;
    }
    CHECK_RC(LwdaLinpack::InitializeMParameterNormal());

    // For GV10b we use larger matrices
    if (m_ComputeCap == Lwca::Capability::SM_72)
    {
        m_Msize *= 4;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Initialize N variable for Max Power if it hasn't been set.
//!
RC LwdaLinpackIMMAgemm::InitializeNParameterNormal()
{
    RC rc;

    if (m_Nsize)
    {
        return rc;
    }
    CHECK_RC(LwdaLinpack::InitializeNParameterNormal());

    // For GV10b we use larger matrices
    if (m_ComputeCap == Lwca::Capability::SM_72)
    {
        m_Nsize *= 48;
    }
    return rc;
}

//----------------------------------------------------------------------
//! \brief Initialize K variable for max power if it hasn't been set.
//!
RC LwdaLinpackIMMAgemm::InitializeKParameterNormal()
{
    // For GV10b we use a smaller Ksize since Msize and Nsize are larger
    if (m_ComputeCap == Lwca::Capability::SM_72)
    {
        m_Kset.clear();
        if (!m_Ksize)
        {
            m_Ksize = 3072;
        }
        m_Kset.push_back(m_Ksize);
        return OK;
    }
    else
    {
        return LwdaLinpack::InitializeKParameterNormal();
    }
}

//-----------------------------------------------------------------------------------
//! \brief Calls LWCA kernel to Run IMMAGEMM
//!
RC LwdaLinpackIMMAgemm::RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize)
{
    RC rc;

    ByteStream paramBuffer;
    if (m_INT32Result)
    {
        INT32 alpha = 1;
        INT32 beta = initialize ? 0 : 1;
        FillParamBufferCommon(A, B, C, alpha, beta, paramBuffer);
    }
    else
    {
        float alpha = 0.0;
        float beta  = 0.0;
        if (initialize)
        {
            alpha = 1.0;
            beta = 0.0;
        }
        else
        {
            alpha = static_cast<float>(m_Alpha);
            beta  = static_cast<float>(m_Beta);
        }
        FillParamBufferCommon(A, B, C, alpha, beta, paramBuffer);
    }

    CHECK_RC(Launch(paramBuffer));
    return rc;
}

//-----------------------------------------------------------------------------------------
//! \brief Calls the Function for Scaling the Reference Data
//!
RC LwdaLinpackIMMAgemm::ScaleReferenceData(UINT32 scale)
{
    RC rc;
    if (m_INT32Result)
    {
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
    }
    return rc;
}

//-----------------------------------------------------------------------------------------
//! \brief Resets the reference data
//!
RC LwdaLinpackIMMAgemm::ResetReferenceData()
{
    RC rc;
    if (m_INT32Result)
    {
        auto cPtr = (UseReferenceData()) ? CalcRefMatrixCPtr() : CalcMatrixCPtr();
        return RunGemm(CalcMatrixAPtr(), CalcMatrixBPtr(), cPtr, true);
    }
    return rc;
}

JS_CLASS_INHERIT(LwdaLinpackIMMAgemm, LwdaLinpack, "Run LwdaLinpack IMMAgemm Test");
CLASS_PROP_READWRITE(LwdaLinpackIMMAgemm, KernelType, string, "Explicity specify type of IMMA kernel.");
