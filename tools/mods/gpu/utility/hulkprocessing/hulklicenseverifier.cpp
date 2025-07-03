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

#include "hulklicenseverifier.h"

#include "expat.h"
#include "core/include/massert.h"

// Lw64 define is used in some of the vbios headers there doesn't seem any
// additional need to include more header files from vbios location
#define Lw64 LwU64
#include "../vbios/cert20.h"

void HulkLicenseVerifier::StartElementHandler(void *userData, const char* name, const char** atts)
{
    MASSERT(userData);
    MASSERT(name);
    MASSERT(atts);
    HulkLicenseVerifier::ParserArgs* parserArgs = (HulkLicenseVerifier::ParserArgs*)userData;

    if (!strcmp(name, "extension_HULK_Register_Override"))
    {
        HulkLicenseVerifier::HulkXmlCertArgs hulkXmlCertArgs = {};
        bool bShouldVerify = false;
        for (UINT32 i = 0; atts[i]; i += 2)
        {
            if (!strcmp(atts[i], "addr"))
            {
                UINT32 addr = Utility::ColwertStrToHex(atts[i + 1]);
                UINT32 opCode = REF_VAL(CERT20_EXT_TYPE7_OPCODE, addr);
                if (opCode == CERT20_EXT_TYPE7_OPCODE_RMW ||
                    opCode == CERT20_EXT_TYPE7_OPCODE_RMW_RBV)
                {
                    bShouldVerify = true;
                    hulkXmlCertArgs.addr = REF_VAL(CERT20_EXT_TYPE7_REGOVERRIDE_ADDR, addr);
                }
            }
            else if (!strcmp(atts[i], "mask"))
            {
                hulkXmlCertArgs.mask = Utility::ColwertStrToHex(atts[i + 1]);
            }
            else if (!strcmp(atts[i], "data"))
            {
                hulkXmlCertArgs.data = Utility::ColwertStrToHex(atts[i + 1]);
            }
        }
        if (bShouldVerify)
        {
            parserArgs->hulkXmlCertArgList.push_back(hulkXmlCertArgs);
        }
    }
}

RC HulkLicenseVerifier::VerifyHulkLicense
(
    const TestDevicePtr pTestDevice,
    const string& hulkXmlCert
)
{
    RC rc;
    ParserArgs parserArgs = {};
    CHECK_RC(HulkLicenseVerifier::ParseHulkXmlCert(parserArgs, hulkXmlCert));

    bool bMismatchFound = false;
    for (auto &hulkXmlCertArgs: parserArgs.hulkXmlCertArgList)
    {
        UINT32 maskedRegisterVal = pTestDevice->RegRd32(hulkXmlCertArgs.addr) &
                                    ~hulkXmlCertArgs.mask;
        UINT32 expectedVal = hulkXmlCertArgs.data & ~hulkXmlCertArgs.mask;
        //[val@addr] & ~mask == (data & ~mask)
        if (maskedRegisterVal != expectedVal)
        {
            bMismatchFound = true;
            Printf(Tee::PriError,
                "HULK Cert mismatch at Addr:'0x%08x' with Mask:'0x%08x' and Data:'0x%08x' ",
                hulkXmlCertArgs.addr, hulkXmlCertArgs.mask, hulkXmlCertArgs.data);
            Printf(Tee::PriError,
                "(Masked off register value:'0x%08x' does not match expected value:'0x%08x')\n",
                maskedRegisterVal, expectedVal);
        }
    }
    if (bMismatchFound)
    {
        Printf(Tee::PriError, "HULK license verification failed\n");
    }
    else
    {
        Printf(Tee::PriLow, "HULK license verification succeeded\n");
    }
    return rc;
}

RC HulkLicenseVerifier::ParseHulkXmlCert(ParserArgs &parserArgs, const string& hulkXmlCert)
{
    RC rc;
    XML_Parser parser = XML_ParserCreate(NULL);
    if (!parser)
    {
        return RC::SOFTWARE_ERROR;
    }

    XML_SetUserData(parser, (void *) &parserArgs);
    XML_SetStartElementHandler(parser, HulkLicenseVerifier::StartElementHandler);

    // buffer to store lines of HULK xml license
    vector<char> hulkXmlBuff;
    CHECK_RC(Utility::ReadPossiblyEncryptedFile(hulkXmlCert, &hulkXmlBuff, NULL));
    MASSERT(hulkXmlBuff.size() <= INT32_MAX);

    if (!XML_Parse(parser, hulkXmlBuff.data(), static_cast<int>(hulkXmlBuff.size()), true))
    {
        Printf(Tee::PriError, "Parse error at line %d: %s\n",
                static_cast<int>(XML_GetLwrrentLineNumber(parser)),
                XML_ErrorString(XML_GetErrorCode(parser)));
        XML_ParserFree(parser);
        return RC::ILWALID_HULK_LICENSE;
    }
    XML_ParserFree(parser);

    return rc;
}