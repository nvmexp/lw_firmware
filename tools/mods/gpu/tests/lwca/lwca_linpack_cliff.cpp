/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/
#include "lwda_linpack.h"
#include "core/include/help.h"
#include "core/include/utility.h"

class LwdaLinpackCliff : public LwdaLinpack
{
    public:
        LwdaLinpackCliff();
        ~LwdaLinpackCliff() override {}
        RC Setup() override;
        void TestHelp(JSContext *pCtx, regex *pRegex) override;

        enum TestType
        {
            TYPE_A10,
            TYPE_A30,
            NO_TYPE
        };
        SETGET_PROP(TestType, UINT32);
    private:
        UINT32 m_TestType = NO_TYPE;
        RC InitializeNParameterNormal() override;
        RC InitializeKParameterNormal() override;
        RC RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize) override;
};

//-----------------------------------------------------------------------
//! \brief Constructor
//!
LwdaLinpackCliff::LwdaLinpackCliff()
{
    SetName("LwdaLinpackCliff");
    m_GemmData.genRandomFuncName   = "GenRandomDataF";
    m_GemmData.genConstantFuncName = "GenConstantDataF";
    m_GemmData.checkFuncName       = "CompareOnDeviceF";
    m_GemmData.naiveGemmFuncName   = ""; // We don't check the results

    // Initialize this in Setup, since there are multiple kernels to choose from
    m_GemmData.moduleName = "";
    m_GemmData.linpackFuncNames[Lwca::Capability::SM_53] = "";
    m_GemmData.linpackFuncNames[Lwca::Capability::SM_60] = "";
    m_GemmData.linpackFuncNames[Lwca::Capability::SM_61] = "";
    m_GemmData.linpackFuncNames[Lwca::Capability::SM_70] = "";
    m_GemmData.linpackFuncNames[Lwca::Capability::SM_72] = "";
    m_GemmData.linpackFuncNames[Lwca::Capability::SM_75] = "";
    m_GemmData.smParameters[Lwca::Capability::SM_53] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_60] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_61] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_70] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_72] = {};
    m_GemmData.smParameters[Lwca::Capability::SM_75] = {};

    // Alpha and Beta are only used by A30
    // Since we don't check kernel results we can skip the Alpha/Beta check
    m_DefaultAlpha = 0.0;
    m_DefaultBeta  = 0.0;
    m_SkipAlphaBetaCheck = true;

    // Always use LaunchDelay by default
    m_LaunchDelayUs = 30;

    // We don't care about results, this test is designed to trip the over-voltage limit
    m_VerifyResults = false;
}

RC LwdaLinpackCliff::Setup()
{
    // Set module and function names based on kernel type
    if (m_TestType == TYPE_A10)
    {
        // A10 is only supported on GP100
        m_GemmData.moduleName = "arch_cliff_A10_nt";
        m_GemmData.linpackFuncNames[Lwca::Capability::SM_60] = "maxwell_sgemm_64x64_voltageSpike256_nt";

        // We ignore the parameters from the file, which says to use 64x64 and k=1 min
        m_GemmData.smParameters[Lwca::Capability::SM_60] =
        {
            64, 1, 128, 128, 128, 128, 4,
            R_32F, R_32F,
            false, 0, 0, 2, 16, 0, 2, 16, 0
        };
    }
    else if (m_TestType == TYPE_A30)
    {
        // The Volta kernel is quite different from the Maxwell kernel, but both are the most stressful variants
        // so they share the "A30" name.
        m_GemmData.moduleName = "arch_cliff_A30_nt";
        m_GemmData.linpackFuncNames[Lwca::Capability::SM_53] = "maxwell_sgemm_128x128_voltageSpike472_0_nt";
        m_GemmData.linpackFuncNames[Lwca::Capability::SM_60] = "maxwell_sgemm_128x128_voltageSpike472_0_nt";
        m_GemmData.linpackFuncNames[Lwca::Capability::SM_61] = "maxwell_sgemm_128x128_voltageSpike472_0_nt";
        m_GemmData.linpackFuncNames[Lwca::Capability::SM_70] = "volta_sgemm_128x128_voltageSpike224_32_nt";
        m_GemmData.linpackFuncNames[Lwca::Capability::SM_72] = "volta_sgemm_128x128_voltageSpike224_32_nt";
        m_GemmData.linpackFuncNames[Lwca::Capability::SM_75] = "volta_sgemm_128x128_voltageSpike224_32_nt";
        m_GemmData.smParameters[Lwca::Capability::SM_53] =
        {
            256, 1, 128, 128, 128, 128, 4,
            R_32F, R_32F,
            false, 0, 0, 2, 16, 0, 2, 16, 0
        };
        m_GemmData.smParameters[Lwca::Capability::SM_60] =
            m_GemmData.smParameters[Lwca::Capability::SM_53];
        m_GemmData.smParameters[Lwca::Capability::SM_61] =
            m_GemmData.smParameters[Lwca::Capability::SM_53];
        m_GemmData.smParameters[Lwca::Capability::SM_70] =
            m_GemmData.smParameters[Lwca::Capability::SM_53];
        m_GemmData.smParameters[Lwca::Capability::SM_72] =
            m_GemmData.smParameters[Lwca::Capability::SM_53];
        m_GemmData.smParameters[Lwca::Capability::SM_75] =
            m_GemmData.smParameters[Lwca::Capability::SM_53];
    }
    else
    {
        Printf(Tee::PriError, "TestType %u not supported!\n", m_TestType);
        return RC::SOFTWARE_ERROR;
    }

    // Run rest of setup
    return LwdaLinpack::Setup();
}

void LwdaLinpackCliff::TestHelp(JSContext *pCtx, regex *pRegex)
{
    bool bObjectFound;
    Help::JsObjHelp(GetJSObject(),pCtx,pRegex,0,NULL,&bObjectFound);
    LwdaLinpack::PropertyHelp(pCtx,pRegex);
}

//--------------------------------------------------------------------
//! \brief Initialize N variable for Max Power if it hasn't been set.
//!
RC LwdaLinpackCliff::InitializeNParameterNormal()
{
    RC rc;

    if (m_Nsize)
    {
        return rc;
    }

    switch (m_ComputeCap)
    {
        case Lwca::Capability::SM_53:
        case Lwca::Capability::SM_60:
        case Lwca::Capability::SM_61:
        case Lwca::Capability::SM_70:
        case Lwca::Capability::SM_72:
        case Lwca::Capability::SM_75:
            m_Nsize = (m_TestType == TYPE_A10) ? 256 : 128;
            break;

        default:
            rc = RC::SOFTWARE_ERROR;
            MASSERT(!"Unhandled compute when setting N parameter!");
    }

    return rc;
}

//----------------------------------------------------------------------
//! \brief Initialize K variable for max power if it hasnt been set.
//!
RC LwdaLinpackCliff::InitializeKParameterNormal()
{
    RC rc;

    m_Kset.clear();

    // Don't overwrite if Ksize has been set by user
    if (m_Ksize)
    {
        m_Kset.push_back(m_Ksize);
        return rc;
    }

    switch (m_ComputeCap)
    {
        case Lwca::Capability::SM_53:
        case Lwca::Capability::SM_60:
        case Lwca::Capability::SM_61:
            if (m_TestType == TYPE_A10)
            {
                m_Ksize = 96;
            }
            else
            {
                m_Ksize = 24;
            }
            break;
        case Lwca::Capability::SM_70:
        case Lwca::Capability::SM_72:
        case Lwca::Capability::SM_75:
            m_Ksize = 132;
            break;

        default:
            rc = RC::SOFTWARE_ERROR;
            MASSERT(!"Unhandled compute when setting K parameter!");
    }

    m_Kset.push_back(m_Ksize);

    return rc;
}

//-----------------------------------------------------------------------------------
//! \brief Calls LWCA kernel to Run SGEMM
//!
RC LwdaLinpackCliff::RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize)
{
    RC rc;

    float alpha = static_cast<float>(m_Alpha);
    float beta  = static_cast<float>(m_Beta);

    ByteStream paramBuffer;
    UINT32 tempData = 0;
    INT32 tempDataI = 0;
    UINT64 tempData64 = 0;
    INT64 tempDataI64 = 0;

    switch (m_ComputeCap)
    {
        // Many of these parameters (such as shiftFast and multiplerSlow)
        // are not used by the kernels.
        //
        // We set them to the same values used by SGEMM
        case Lwca::Capability::SM_53:
        case Lwca::Capability::SM_60:
        case Lwca::Capability::SM_61:
        case Lwca::Capability::SM_70:
        case Lwca::Capability::SM_72:
        case Lwca::Capability::SM_75:
        {
            paramBuffer.PushPadded(A);
            paramBuffer.PushPadded(B);
            paramBuffer.PushPadded(C);

            tempData64 = static_cast<UINT64>(m_Msize) << m_KernParams.shiftFastA;
            paramBuffer.PushPadded(tempData64);
            tempData64 = static_cast<UINT64>(m_Nsize) << m_KernParams.shiftFastB;
            paramBuffer.PushPadded(tempData64);

            tempDataI64 =
                static_cast<INT64>(m_Msize) * m_KernParams.multiplierSlowA + m_KernParams.offsetSlowA;
            paramBuffer.PushPadded(tempDataI64);
            tempDataI64 =
                static_cast<INT64>(m_Nsize) * m_KernParams.multiplierSlowB + m_KernParams.offsetSlowB;
            paramBuffer.PushPadded(tempDataI64);

            tempData64 = 0;
            paramBuffer.PushPadded(tempData64); // matrixStrideA
            paramBuffer.PushPadded(tempData64); // matrixStrideB
            paramBuffer.PushPadded(tempData64); // matrixStrideC

            paramBuffer.PushPadded(m_Msize);
            paramBuffer.PushPadded(m_Nsize);
            paramBuffer.PushPadded(m_Msize);

            paramBuffer.PushPadded(m_Msize);
            paramBuffer.PushPadded(m_Nsize);
            paramBuffer.PushPadded(m_Ktrue);

            tempData64 = 0;
            paramBuffer.PushPadded(tempData64); // sync
            tempData = 0;
            paramBuffer.PushPadded(tempData);   // k chunk size
            paramBuffer.PushPadded(tempData);   // mode

            // Alpha/beta ref addresses. Arch expects a value of 1 to specify these
            // addresses are unused
            tempData64 = 1;
            paramBuffer.PushPadded(tempData64); // Alpha ref
            paramBuffer.PushPadded(tempData64); // Beta ref

            paramBuffer.PushPadded(alpha); // alpha
            paramBuffer.PushPadded(beta); // beta

            tempDataI = 0;
            paramBuffer.PushPadded(tempDataI); // alphaBetaByRef

        }
        break;

        default:
            MASSERT(0);
    }

    CHECK_RC(Launch(paramBuffer));
    return rc;
}

JS_CLASS_INHERIT(LwdaLinpackCliff, LwdaLinpack, "Run the Lwca SGEMM stress test.");
CLASS_PROP_READWRITE(LwdaLinpackCliff, TestType, UINT32, "Specify type of Cliff Linpack test to run.");
PROP_CONST(LwdaLinpackCliff, TYPE_A10, LwdaLinpackCliff::TYPE_A10);
PROP_CONST(LwdaLinpackCliff, TYPE_A30, LwdaLinpackCliff::TYPE_A30);
