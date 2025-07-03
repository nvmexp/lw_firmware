/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef STDOUTSINK_H__
#define STDOUTSINK_H__

#include "types.h"
#include "datasink.h"
#include "rc.h"

/**
 * @class( StdoutSink )
 *
 * Print Tee's stream to the standard output.
 */

class StdoutSink : public DataSink
{
    public:
        StdoutSink();

        RC Enable();
        RC Disable();
        bool IsEnabled() { return m_IsEnabled; }

    protected:
        // DataSink override.
        void DoPrint(const char* str, size_t size, Tee::ScreenPrintState Sps) override;
        bool DoPrintBinary(const UINT08* data, size_t size) override;

    private:
        bool   m_IsEnabled;
};

#endif // !STDOUTSINK_H__
