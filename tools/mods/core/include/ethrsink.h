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

// Ethernet sink.

#ifndef INCLUDED_ETHRSINK_H
#define INCLUDED_ETHRSINK_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef INCLUDED_SOCKMODS_H
#include "sockmods.h"
#endif

#ifndef DATASINK_H__
#include "datasink.h"
#endif

/**
 * @class(EthernetSink)
 *
 * Print Tee's stream to a TCP socket.
 */

class EthernetSink  : public DataSink
{
public:
    EthernetSink();
    ~EthernetSink();

    RC Open(void);
    RC Close(void);

protected:
    // DataSink override.
    void DoPrint(const char* str, size_t strLen, Tee::ScreenPrintState sps) override;

private:
    SocketMods* m_pSocket;
    bool m_Connected;
};

#endif // ! INCLUDED_ETHRSINK_H

