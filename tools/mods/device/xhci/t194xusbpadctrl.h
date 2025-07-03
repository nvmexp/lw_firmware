/* * LWIDIA_COPYRIGHT_BEGIN *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_T194XUSBPADCTRL_H
#define INCLUDED_T194XUSBPADCTRL_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_XUSBPADCTRL_H
#include "xusbpadctrl.h"
#endif

#define XUSB_HS_PORT_NUM    4

class T194XusbPadCtrl: public XusbPadCtrl
{
public:
    T194XusbPadCtrl();
    virtual ~T194XusbPadCtrl();

    //if DeviceModePortIndex = XUSB_HS_PORT_NUM, means all port are for host mode
    virtual RC PadInit(UINT08 DeviceModePortIndex, bool IsAdbSafe, bool IsForceHS = false);
    virtual RC EnablePadTrack(void);
    virtual RC DisableDiscDetect(UINT08 Port);
    virtual RC SetupUsb2PadMux(UINT08 Port, UINT08 Owned);
    virtual RC ForceSsSpd(UINT08 Gen = 0);  // by default force to Gen1
    virtual RC DevModeEnableVbus() override;
    virtual RC DevModeDisableVbus() override;
    RC SetProdSetting() override;
};

#endif
