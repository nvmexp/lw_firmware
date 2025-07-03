/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/include/gpusbdev.h"

class RC;

namespace HulkLicenseVerifier
{
   // HulkXmlCertArgs needs to be public for the expat parser handlers
    struct HulkXmlCertArgs
    {
        UINT32 mask;
        UINT32 data;
        UINT32 addr;
    };

    // ParserArgs needs to be public for the expat parser handlers
    struct ParserArgs
    {
        vector<HulkXmlCertArgs> hulkXmlCertArgList;
    };

    RC VerifyHulkLicense(const TestDevicePtr pTestDevice, const string& hulkXmlCert);
    RC ParseHulkXmlCert(ParserArgs &parserArgs, const string& hulkXmlCert);
    void StartElementHandler(void *userData, const char* name, const char** atts);
}