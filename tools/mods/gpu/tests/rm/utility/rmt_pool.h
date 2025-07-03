/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _RMT_POOL_H
#define _RMT_POOL_H

#include "core/include/utility.h"

namespace rmt
{
    //! Generic pool party class.
    template<class T>
    class Pool
    {
    public:
        bool insert(T item)
        {
            if (m_map.find(item) != m_map.end())
            {
                return false;
            }
            m_map[item] = m_vec.size();
            m_vec.push_back(item);
            return true;
        }

        bool erase(T item)
        {
            typename map<T, size_t>::iterator it = m_map.find(item);
            if (it == m_map.end())
            {
                return false;
            }
            if (it->second < (m_vec.size() - 1))
            {
                m_map[m_vec.back()] = it->second;
                m_vec[it->second] = m_vec.back();
            }
            m_vec.pop_back();
            m_map.erase(it);
            return true;
        }

        T getRandom(Random *pRNG) const
        {
            MASSERT(!m_vec.empty());
            return m_vec[(size_t)pRNG->GetRandom64(0, m_vec.size() - 1)];
        }

        T getAny() const
        {
            MASSERT(!m_vec.empty());
            return m_vec.front();
        }

        size_t size() const
        {
            return m_vec.size();
        }

        bool empty() const
        {
            return m_vec.empty();
        }

    private:
        vector<T>      m_vec;
        map<T, size_t> m_map;
    };
}

#endif
