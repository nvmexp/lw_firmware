/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file pm_rpt.cpp
//! \brief Performance Monitor report information class
//!
//! Body for the Performance Monitor report information class
//!

#include <string.h>

#include "pm_rpt.h"

PMReportInfo::PMReportInfo() :
    m_use_trace_trigger(false),
    m_wait_for_interrupt(false)
{
}

PMReportInfo::PMReportInfo(const PMReportInfo &info)
{
    m_use_trace_trigger = info.m_use_trace_trigger;
    m_wait_for_interrupt = info.m_wait_for_interrupt;
}

bool PMReportInfo::Init(const char *pm_report_option)
{
    bool valid_option = true;

    if (strcmp(pm_report_option, "STANDARD") == 0)
    {
        m_use_trace_trigger = true;
        m_wait_for_interrupt = false;
    }
    else if (strcmp(pm_report_option, "INTERRUPT") == 0)
    {
        m_use_trace_trigger = true;
        m_wait_for_interrupt = true;
    }
    else if (strcmp(pm_report_option, "TEST") == 0)
    {
        m_use_trace_trigger = false;
        m_wait_for_interrupt = false;
    }
    else
    {
        valid_option = false;
    }

    return valid_option;
}
