/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file crccomparison.h
//! \brief File to handle Display CRC comparison of XML format
//!

#ifndef __CRCCOMPARISON_H__
#define __CRCCOMPARISON_H__

#include "xmlnode.h"

class VerifCrc
{
public:
    bool bSkipTimestampMode;
    bool bSkipHwFliplockMode;
    bool bSkipCompositorCRC;
    bool bCompEvenIfPresentIntervalMet ;
    VerifCrc()
    {
        bSkipTimestampMode = false;
        bSkipHwFliplockMode = false;
        bSkipCompositorCRC = false;
        bCompEvenIfPresentIntervalMet = false;
    }
};

class VerifNotifier
{
public:
    bool bSkipControllingChannel;
    bool bSkipPrimaryCrcOutput;
    bool bSkipSecondaryCrcOutput;
    bool bSkipExpectedBufferCollapse;
    bool bSkipWidePipeCrc;
    bool bIgnorePrimaryOverflow;
    bool bIgnoreTagNumber;
    VerifCrc crcField;
    VerifNotifier()
    {
        bSkipControllingChannel = false;
        bSkipPrimaryCrcOutput = false;
        bSkipSecondaryCrcOutput = true;
        bSkipExpectedBufferCollapse = false;
        bSkipWidePipeCrc = false;
        bIgnorePrimaryOverflow = false;
        bIgnoreTagNumber = false;
    }
};

class VerifCrcHeader
{
public:
    bool bSkipChip;
    bool bSkipPlatform;
    VerifCrcHeader()
    {
        bSkipChip = false;
        bSkipPlatform = false;
    }
};

class VerifyCrcTree
{
public:
    VerifCrcHeader crcHeaderField;
    VerifNotifier  crcNotifierField;
};

class CrcComparison
{
public :
    CrcComparison(){}

    RC CompareCrcFiles(const char *filename1,
                       const char *filename2,
                       const char *outputfile,
                       VerifyCrcTree *pCrcFlag = NULL);

    RC DumpCrcToXml(GpuDevice *pGpuDev,
                    const char *filename,
                    void *pCrcSettings,
                    bool CreateNewFile,
                    bool PrintAllCrcs = false);

    static int FileExists(const char *fname,
                    vector<string> *pSearchPath = NULL);

private:

    RC DiffCrcXmlTree(XmlNode *pRootNode1,
                      XmlNode *pRootNode2,
                      const char *outputfile,
                      VerifyCrcTree *pCrcFlag);

    RC IsCrcOptional(XmlNode *pCrcNode,
                     bool *bIsOptional,
                     VerifCrc *pCrcField = NULL);

    RC Crc01XNotifier2Xml(const char *GpuDevID,
                          const char *filename,
                          void *pCrcSettings,
                          bool CreateNewFile,
                          bool PrintAllCrcs);

    RC Crc03XNotifier2Xml(const char *GpuDevID,
                          const char *FileName,
                          void *pCrcSettings,
                          bool CreateNewFile,
                          bool PrintAllCrcs);

    RC AddHeaderToCrcXml(const char *GpuDevID,
                         const char *filename);
};

#endif
