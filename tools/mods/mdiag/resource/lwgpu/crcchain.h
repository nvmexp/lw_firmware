/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file
//! \brief Utility class to build and manage crc chain in verification
//!        Surface in test can specify crc chain through class
//!        Each test has a default crc chain for those surfaces
//!        which don't specify crc chain

#ifndef _CRCCHAIN_H_
#define _CRCCHAIN_H_

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif

class ArgReader;
class GpuSubdevice;

typedef list<string> CrcChain;

class CrcChainManager
{
public:
    CrcChainManager(UINT32 defaultClass);
    virtual ~CrcChainManager() {}

    enum
    {
        UNSPECIFIED_CLASSNUM = 0xFFFFFFFF
    };

    virtual CrcChain& GetCrcChain(UINT32 classnum = UNSPECIFIED_CLASSNUM) = 0;
protected:
    UINT32 m_DefaultCrcChainClass;
};

class Trace3DCrcChain: public CrcChainManager
{
public:
    Trace3DCrcChain(const ArgReader* params,
        GpuSubdevice* pSubdev, UINT32 defaultClass);
    virtual ~Trace3DCrcChain() {}

    virtual CrcChain& GetCrcChain(UINT32 classnum = UNSPECIFIED_CLASSNUM);

private:
    RC SetupAmodelCrcChain(UINT32 classnum);
    RC SetupPerfsimCrcChain(UINT32 classnum);
    RC SetupNonAmodelCrcChain(UINT32 classnum);
    RC SetupCrcChain(UINT32 classnum);
    bool RawImageMode();

    // class to crc chain map
    map<UINT32, CrcChain> m_Class2CrcChain;

    const ArgReader* m_Params;
    GpuSubdevice* m_Subdev;
};
#endif
