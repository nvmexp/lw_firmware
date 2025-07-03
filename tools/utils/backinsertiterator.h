/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef BACKINSERTITERATOR_H
#define BACKINSERTITERATOR_H

#include <vector>

#include "core/include/types.h"

//! BackInsertIterator provides an output iterator that inserts values to a
//! std::vector. The difference from the std::back_insert_iterator is that this
//! class allows access to the dereferenced value.
class BackInsertIterator
{
public:
    explicit BackInsertIterator(vector<UINT08> &v)
      : m_data(v)
      , m_itPos(0)
      , m_lwrValue(0)
      , m_needSaving(false)
    {}

    ~BackInsertIterator()
    {
        if (m_needSaving)
        {
            SaveLwrValue();
        }
    }

    UINT08& operator *()
    {
        m_needSaving = true;
        return m_lwrValue;
    }

    BackInsertIterator& operator++()
    {
        if (m_needSaving)
        {
            SaveLwrValue();
            m_needSaving = false;
        }
        ++m_itPos;

        return *this;
    }

private:
    void SaveLwrValue()
    {
        if (m_data.size() <= m_itPos)
        {
            m_data.resize(m_itPos + 1);
        }
        m_data[m_itPos] = m_lwrValue;
    }

    vector<UINT08> &m_data;
    size_t          m_itPos;
    mutable UINT08  m_lwrValue;
    mutable bool    m_needSaving;
};

#endif
