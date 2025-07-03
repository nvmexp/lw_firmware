/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/bitflags.h"
#include "core/include/types.h"
#include <vector>

namespace Utility
{
    /// Manages RM handles
    ///
    /// Current layout of handles generated with GenHandle():
    /// * b31..25 - HandleTag
    /// * b24     - RM lib selector
    /// * b23..16 - RM client id
    /// * b15..0  - Index to m_Handles
    ///
    class HandleStorage
    {
        public:
            HandleStorage();

            static constexpr UINT32 Invalid       = ~0U;
            static constexpr UINT32 HandleMask    = Utility::Bitmask<UINT32>(16);
            static constexpr UINT32 HandleTag     = 0b1011111U; // Note: 7 bits
            static constexpr UINT32 UpperBits     = HandleTag << 25;
            static constexpr UINT32 UpperBitsMask = ~Utility::Bitmask<UINT32>(25);

            static bool GeneratedHandle(UINT32 h) { return (h & UpperBitsMask) == UpperBits; }

            bool   Empty() const { return m_NumHandles == 0U; }
            UINT32 NumHandles() const { return m_NumHandles; }
            UINT32 GetParent(UINT32 handle) const;
            UINT32 GenHandle(UINT32 upperBits, UINT32 parent);
            void   RegisterForeignHandle(UINT32 handle, UINT32 parent);
            void   FreeWithChildren(UINT32 handle);
            void   Free(UINT32 handle);

            struct HandleParent
            {
                UINT32 handle;
                UINT32 parent;

                HandleParent(UINT32 h, UINT32 p) : handle(h), parent(p) { }
                HandleParent() = default;
            };

            using const_iterator = vector<HandleParent>::const_iterator;

            const_iterator begin()  const { return m_Handles.cbegin(); }
            const_iterator end()    const { return m_Handles.cend();   }
            const_iterator cbegin() const { return m_Handles.cbegin(); }
            const_iterator cend()   const { return m_Handles.cend();   }

        private:
            vector<HandleParent> m_Handles;
            UINT32               m_NumHandles = 0U;
            UINT32               m_GenIdx     = 0U;
    };
}
