/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_XUSBPADCTRL_H
#define INCLUDED_XUSBPADCTRL_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#define USB2_PAD_MUX_OWNER_XUSB 1
#define USB2_PAD_MUX_OWNER_UART 2

class XusbPadCtrl
{
public:
    XusbPadCtrl();
    virtual ~XusbPadCtrl();
    virtual RC EnableVbusOverride();
    RC RegWr(UINT32 Offset, UINT32 Data);
    RC RegRd(UINT32 Offset, UINT32 *pData);
    virtual RC DisableDiscDetect(UINT08 port);
    virtual RC PadInit(UINT08 DeviceModePortIndex, bool IsAdbSafe, bool IsForceHS = false);
    virtual RC ForceSsSpd(UINT08 Gen = 0) {return RC::UNSUPPORTED_FUNCTION;}
    virtual RC EnablePadTrack(void);
    virtual RC ForcePortFS(UINT32 Port);
    virtual RC SetupUsb2PadMux(UINT08 Port, UINT08 Owned);
    virtual RC EnableVbus(bool IsAdbPort = false);
    virtual RC DisableVbus();
    virtual RC DevModeEnableVbus();
    virtual RC DevModeDisableVbus();
    virtual RC SetProdSetting() {return OK;};
};

namespace XusbPadCtrlMgr
{
    XusbPadCtrl * GetPadCtrl(bool IsNotCreate = false);
    void Cleanup();
    RC RegWr(UINT32 Offset, UINT32 Data);
    RC RegRd(UINT32 Offset, UINT32 *pData);
    RC EnableVbusOverride();
    RC DisableDiscDetect(UINT08 Port);
    RC EnableVbus(bool IsAdbPort = false);
    RC DisableVbus();
    RC DevModeEnableVbus();
    RC DevModeDisableVbus();
};

#endif

