/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef H26XPARSER_H
#define H26XPARSER_H

namespace H26X
{

template <class T>
struct UnusedPred
{
    bool operator()(const typename T::value_type &e) const
    {
        return e.IsOutput() && !e.IsUsedForReference();
    }
};

template <class T>
struct IsOutputPred
{
    typedef const typename T::value_type argument_type;
    typedef bool result_type;

    bool operator()(const typename T::value_type &e) const
    {
        return e.IsOutput();
    }
};

}

#endif
