/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef VC1STREAMINFO_H
#define VC1STREAMINFO_H

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <cstddef>

#include "core/include/rc.h"
#include "core/include/types.h"

#include "vc1syntax.h"

namespace VC1
{

class EntryPointLayerCopyHelper
{
public:
    EntryPointLayerCopyHelper()
      : m_entryPointLayer(&m_sequenceLayer)
    {}

    EntryPointLayerCopyHelper(const EntryPointLayerCopyHelper& rhs)
      : m_sequenceLayer(rhs.m_sequenceLayer)
      , m_entryPointLayer(rhs.m_entryPointLayer)
    {
        m_entryPointLayer.SetSeqLayerParamsSrc(&m_sequenceLayer);
    }

    EntryPointLayerCopyHelper& operator =(const EntryPointLayerCopyHelper &rhs)
    {
        if (this != &rhs)
        {
            m_sequenceLayer = rhs.m_sequenceLayer;
            m_entryPointLayer = rhs.m_entryPointLayer;
            m_entryPointLayer.SetSeqLayerParamsSrc(&m_sequenceLayer);
        }

        return *this;
    }

protected:
    SequenceLayer   m_sequenceLayer;
    EntryPointLayer m_entryPointLayer;
};

class Parser : public EntryPointLayerCopyHelper
{
public:
    Parser()
    {}

    RC ParseStream(const UINT08 *buf, size_t bufSize);
};

} // namespace VC1

#endif // VC1STREAMINFO_H
