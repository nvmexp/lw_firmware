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
//! \file pm_rpt.h
//! \brief Performance Monitor report information class
//!
//! Header for the Performance Monitor report information class
//!

#ifndef __PM_RPT_H__
#define __PM_RPT_H__

class PMReportInfo
{
public:
    PMReportInfo();
    PMReportInfo(const PMReportInfo &info);
    bool Init(const char *pm_report_option);

    bool UseTraceTrigger() { return m_use_trace_trigger; };
    bool WaitForInterrupt() { return m_wait_for_interrupt; };
    void ClearWaitForInterrupt() { m_wait_for_interrupt = false; };

private:
    bool m_use_trace_trigger;
    bool m_wait_for_interrupt;
};

#endif
