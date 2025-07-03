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

#pragma once

#include "datasink.h"
#include "serial.h"

/**
 * @class( SerialSink )
 *
 * Print Tee's stream to the serial port.
 */

class SerialSink : public DataSink
{
    public:
        SerialSink() = default;
        virtual ~SerialSink() = default;

        RC Initialize();
        RC Uninitialize();
        bool IsInitialized();
        RC SetBaud(UINT32 Baud);
        UINT32 GetBaud();
        RC SetPort(UINT32 Port);
        UINT32 GetPort();

        const char* GetFilterString() const { return m_Filter.c_str(); }
        void SetFilterString(const char* filter) { m_Filter = filter; }

    protected:
        // DataSink override.
        void DoPrint(const char* str, size_t strLen, Tee::ScreenPrintState sps) override;

    private:

        string  m_Filter;
        UINT32  m_Port = 1;
        Serial* m_pCom = nullptr;
};
