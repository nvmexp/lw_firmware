/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTRESULT_H_
#define UCTRESULT_H_

#include "gpu/tests/rm/utility/rmtstring.h"
#include "uctpstate.h"
#include "uctexception.h"
#include "ucttrialspec.h"

namespace uct
{
    //!
    //! \brief      Result of a trial interation
    //!
    struct Result
    {
        //! \brief      What is requested from Resman
        FullPState      target;

        //!
        //! \brief      What Resman chose instead
        //! \see        UniversalClockTest::VerifyClockProgramming
        //! \see        UniversalClockTest::ConfigClocks
        //!
        //! \details    Depending on the setting of 'rmapi', this data may come from
        //!             LW2080_CTRL_CMD_CLK_CONFIG_INFO_V2 or LW2080_CTRL_CMD_CLK_GET_INFO.
        //!
        //! \todo       To avoid confusion, we should probably have two separate members:
        //!             'actual' which comes from either LW2080_CTRL_CMD_CLK_CONFIG_INFO_V2
        //!             or the analogous LW2080_CTRL_CMD_CLK_SET_INFO; and 'physical'
        //!             (or something) which comes from LW2080_CTRL_CMD_CLK_GET_INFO.
        //!             Either may have zero frequencies if not applicable, but should
        //!             otherwise be equal.
        PState          actual;

        //! \brief      Physical measurements
        PState          measured;

        //! \brief      Frequency or voltage as text
        static rmt::String valueString(UINT32 value);

        //!
        //! \brief      Delta value as text
        //!
        //! \param[in]  a   Quantity being compared
        //! \param[in]  b   Base quantity
        //!
        static rmt::String deltaString(UINT32 a, UINT32 b);

        //!
        //! \brief      Text representation
        //!
        //! \todo       Add support for 'source' members.
        //!
        rmt::String reportString(bool bTargetOnly) const;

        //!
        //! \brief      Check to see if result.actual and result.measured matches that
        //!             indicated by result.target. This check is augmented by 'tolerance';
        //!             values only need to lie within the tolerance.
        //!
        //! \param[in]  tolerance       Tolerance setting from the trial specification
        //! \param[in]  bCheckMeasured  Indicator of whether need to check result.measured
        //! \param[in]  bCheckSource    Indicator of whether need to check clock source
        //!
        ExceptionList checkResult(const TrialSpec::ToleranceSetting tolerance, bool bCheckMeasured, bool bCheckSource, UINT32 clkDomainDvcoMinMask);
    };
};

#endif      // UCTRESULT_H_

