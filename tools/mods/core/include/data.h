/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#ifndef INCLUDED_DATA_H
#define INCLUDED_DATA_H

#include "core/include/rc.h"
#include <vector>

namespace VectorData
{

    void Print( vector<UINT08> * pData,  Tee::Priority Pri = Tee::PriNormal, UINT08 BytePerLine = 16 );
    void Print( vector<UINT16> * pData,  Tee::Priority Pri = Tee::PriNormal, UINT08 WordPerLine = 8 );
    void Print( vector<UINT32> * pData,  Tee::Priority Pri = Tee::PriNormal, UINT08 DWordPerLine = 8 );
    void Print( vector<void *> * pData,  Tee::Priority Pri = Tee::PriNormal, UINT08 DWordPerLine = 8 );

    inline void Print(vector<UINT08>* pData, Tee::PriDebugStub, UINT08 bytePerLine = 16)
    {
        Print(pData, Tee::PriSecret, bytePerLine);
    }
    inline void Print(vector<UINT16>* pData, Tee::PriDebugStub, UINT08 wordPerLine = 8)
    {
        Print(pData, Tee::PriSecret, wordPerLine);
    }
    inline void Print(vector<UINT32>* pData, Tee::PriDebugStub, UINT08 dWordPerLine = 8)
    {
        Print(pData, Tee::PriSecret, dWordPerLine);
    }
    inline void Print(vector<void*>* pData, Tee::PriDebugStub, UINT08 dWordPerLine = 8)
    {
        Print(pData, Tee::PriSecret, dWordPerLine);
    }

    RC Compare( vector<UINT08>* pData, vector<UINT08>* pExpData );
    RC Compare( vector<UINT16>* pData, vector<UINT16>* pExpData );
    RC Compare( vector<UINT32>* pData, vector<UINT32>* pExpData );

    RC ComparePattern( vector<UINT08>* pData, vector<UINT08>* pPat );
    RC ComparePattern( vector<UINT16>* pData, vector<UINT16>* pPat );
    RC ComparePattern( vector<UINT32>* pData, vector<UINT32>* pPat );

};

#endif
