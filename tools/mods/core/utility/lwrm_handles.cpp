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

#include "core/include/lwrm_handles.h"
#include "core/include/massert.h"
#include <algorithm>

Utility::HandleStorage::HandleStorage()
{
    m_Handles.reserve(1024);
}

UINT32 Utility::HandleStorage::GetParent(UINT32 handle) const
{
    if (GeneratedHandle(handle))
    {
        const UINT32 idx = handle & HandleMask;

        if (idx < static_cast<UINT32>(m_Handles.size()))
        {
            const auto h = m_Handles[idx];
            if (h.parent != Invalid)
            {
                MASSERT(h.handle == handle);
                return h.parent;
            }
            MASSERT(h.handle == Invalid);
        }
    }
    else
    {
        for (const auto h: m_Handles)
        {
            if (h.handle == handle)
            {
                return h.parent;
            }
        }
    }

    return Invalid;
}

UINT32 Utility::HandleStorage::GenHandle(UINT32 upperBits, UINT32 parent)
{
    MASSERT(GeneratedHandle(upperBits));

    const UINT32 size = static_cast<UINT32>(m_Handles.size());

    if (m_NumHandles < size)
    {
        UINT32       idx      = m_GenIdx % size;
        const UINT32 end      = idx;
        UINT32       foundIdx = Invalid;

        do
        {
            if (m_Handles[idx].handle == Invalid)
            {
                foundIdx = idx;
                m_GenIdx = idx + 1;
                break;
            }

            ++idx;

            if (idx == size)
                idx = 0;
        }
        while (idx != end);

        MASSERT(foundIdx != Invalid);

        const UINT32 h = upperBits | foundIdx;

        m_Handles[foundIdx] = { h, parent };
        ++m_NumHandles;

        return h;
    }

    const UINT32 h = upperBits | static_cast<UINT32>(m_Handles.size());
    m_Handles.emplace_back(h, parent);
    ++m_NumHandles;
    return h;
}

void Utility::HandleStorage::RegisterForeignHandle(UINT32 handle, UINT32 parent)
{
    MASSERT(!GeneratedHandle(handle));
    MASSERT(GetParent(handle) == Invalid);
    m_Handles.emplace_back(handle, parent);
    ++m_NumHandles;
}

void Utility::HandleStorage::FreeWithChildren(UINT32 handle)
{
    const UINT32 size = static_cast<UINT32>(m_Handles.size());

    for (UINT32 i = 0; i < size; i++)
    {
        const auto h = m_Handles[i];
        if (h.parent == handle)
        {
            FreeWithChildren(h.handle);
        }
    }

    Free(handle);
}

void Utility::HandleStorage::Free(UINT32 handle)
{
    if (GeneratedHandle(handle))
    {
        const UINT32 idx = handle & HandleMask;
        MASSERT(idx < m_Handles.size());
        MASSERT(m_Handles[idx].handle == handle);
        m_Handles[idx] = { Invalid, Invalid };
    }
    else
    {
        const auto it = find_if(m_Handles.begin(), m_Handles.end(),
                                [=](const auto& h) -> bool { return h.handle == handle; });
        MASSERT(it != m_Handles.end());
        *it = { Invalid, Invalid };
    }
    --m_NumHandles;

    // Compact handle table
    UINT32 newSize = static_cast<UINT32>(m_Handles.size());
    while (newSize > 0 && m_Handles[newSize - 1].handle == Invalid)
    {
        --newSize;
    }
    if (newSize < m_Handles.size())
    {
        m_Handles.resize(newSize);
    }
}
