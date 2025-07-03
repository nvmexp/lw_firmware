/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_TRACE_PLAYER_CRC_H
#define INCLUDED_TRACE_PLAYER_CRC_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

namespace MfgTracePlayer
{
    class Loader;

    UINT32 CalcCRC(const void* buf, UINT32 size);

    class CRCChain
    {
        public:
            CRCChain() : m_Chain(nullptr) { }
            RC Load(Loader& loader, UINT32 classId);
            RC GetCrc(const string& key, const string& suffix, UINT32* pCrc) const;

        private:
            typedef const char*const* Chain;
            static RC GetChainForClass(UINT32 classId, Chain* pChain);

            typedef map<string,UINT32> Crcs;
            Chain m_Chain;
            Crcs m_Crcs;
    };
}

#endif // ! INCLUDED_TRACE_PLAYER_CRC_H
