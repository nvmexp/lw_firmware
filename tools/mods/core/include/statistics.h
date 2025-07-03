/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_STATISTICS_H
#define INCLUDED_STATISTICS_H

#include <math.h>

//! Store samples, callwlate min,max,mean
template<class T> class BasicStatistics
{
public:
    BasicStatistics()
        : m_NumSamples(0)
        , m_Sum(0.0)
        , m_Min(0)
        , m_Max(0)
    { }
    int NumSamples() const { return m_NumSamples; }
    T Min() const          { return m_NumSamples ? m_Min : 0; }
    T Max() const          { return m_NumSamples ? m_Max : 0; }
    double Sum() const     { return m_NumSamples ? m_Sum : 0.0; }
    double Mean() const
    {
        if (0 == m_NumSamples)
            return 0.0;
        return m_Sum / m_NumSamples;
    }
    void Reset()
    {
        m_NumSamples = 0;
    }
    void AddSample(const T& x)
    {
        if (0 == m_NumSamples)
        {
            m_Min = m_Max = x;
            m_Sum = static_cast<double>(x);
        }
        else
        {
            m_Sum += static_cast<double>(x);
            if (m_Min > x)
                m_Min = x;
            if (m_Max < x)
                m_Max = x;
        }
        m_NumSamples++;
    }
private:
    int m_NumSamples;
    double m_Sum;
    T m_Min;
    T m_Max;
};

// To Do: add a class here that inherits from BasicStatistics and
// adds variance, standard deviation, and RMS reporting.
#endif // INCLUDED_STATISTICS_H
