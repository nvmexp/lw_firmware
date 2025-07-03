/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006,2016-2017,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/cmdline.h"
#include "core/include/js_param.h"
#include "gpu/utility/ecov_verifier.h"

//------------------------------------------------------------------------------
// Script properties and methods.
//------------------------------------------------------------------------------
JS_CLASS_NO_ARGS(EcovVerifier, "Ecov veirifier obj", "Usage: new EcovVerifier()");

C_(EcovSetup);
static SMethod Global_EcovSetup
(
    EcovVerifier_Object,
    "EcovSetup",
    C_EcovSetup,
    0,
    "Ecover Verifier Setup"
);

C_(EcovCleanup);
static SMethod Global_EcovCleanup
(
    EcovVerifier_Object,
    "EcovCleanup",
    C_EcovCleanup,
    0,
    "Ecover Verifier Cleanup"
);

C_(EcovVerify);
static SMethod Global_EcovVerify
(
    EcovVerifier_Object,
    "EcovVerify",
    C_EcovVerify,
    0,
    "Verify the Ecover"
);

// Implementation
C_(EcovSetup)
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        // get ecovverifier object
        EcovVerifier* pEcovVerifier = (EcovVerifier *)JS_GetPrivate(pContext, pObject);
        if(!pEcovVerifier)
        {
            JS_ReportError(pContext, "EcovSetup(): object EcovVerifier does not exist.\n");
            return JS_FALSE;
        }

        // Set test name
        string strTestName = CommandLine::Argv()[0];//script file name
        if (!strTestName.empty())
            pEcovVerifier->SetTestName(strTestName); //default name "test"

        // create ArgReader
        ArgReader* pReader = new ArgReader(CommandLine::ParamList()->GetParamDeclArray(),
                                           CommandLine::ParamList()->GetParamConstraintsArray());
        pEcovVerifier->m_TestParams = pReader;
        if(!pReader || !pReader->ParseArgs(CommandLine::ArgDb()))
        {
            JS_ReportError(pContext, "EcovSetup(): Parse TestParams failed.\n");
            return JS_FALSE;
        }

        // enable dumping
        Platform::EscapeWrite("CHECK_ECOVER", 0, 4, ENABLE_ECOV_DUMPING);
    }
    else
    {
        JS_ReportError(pContext, "EcovSetup(): Lwrrently -enable_ecov_checking only works on fmodel\n");
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(EcovCleanup)
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        // get ecovverifier object
        EcovVerifier* pEcovVerifier = (EcovVerifier *)JS_GetPrivate(pContext, pObject);
        if(!pEcovVerifier)
        {
            JS_ReportError(pContext, "EcovSetup(): object EcovVerifier does not exist.\n");
            return JS_FALSE;
        }

        // release testparams ArgReader
        if(pEcovVerifier->m_TestParams)
            delete pEcovVerifier->m_TestParams;

        // disable ecov dumping
        Platform::EscapeWrite("CHECK_ECOVER", 0, 4, DISABLE_ECOV_DUMPING);
    }
    else
    {
        JS_ReportError(pContext, "EcovSetup(): Lwrrently -enable_ecov_checking only works on fmodel\n");
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(EcovVerify)
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pReturlwalue != 0);

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        // Tell chiplib current test finished so it can dump ecover results
        Platform::EscapeWrite("LwrrentTestEnded", 0, 4, 0);

        // verify Ecov and return result
        EcovVerifier* pEcovVerifier = (EcovVerifier *)JS_GetPrivate(pContext, pObject);
        if(!pEcovVerifier)
        {
            JS_ReportError(pContext, "EcovVerify(): object EcovVerifier does not exist.\n");
            return JS_FALSE;
        }

        if(pEcovVerifier->VerifyECov() != EcovVerifier::TEST_SUCCEEDED)
        {
            RETURN_RC(RC::DATA_MISMATCH);
        }
        else
        {
            RETURN_RC(OK);
        }
    }
    else
    {
        JS_ReportError(pContext, "EcovSetup(): Lwrrently -enable_ecov_checking only works on fmodel\n");
        return JS_FALSE;
    }

    return JS_TRUE;
}
