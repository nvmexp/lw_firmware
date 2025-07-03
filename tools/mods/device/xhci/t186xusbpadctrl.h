/* * LWIDIA_COPYRIGHT_BEGIN *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_T186PADCTRL_H
#define INCLUDED_T186PADCTRL_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_XUSBPADCTRL_H
#include "xusbpadctrl.h"
#endif

class T186XusbPadCtrl: public XusbPadCtrl
{

public:
    T186XusbPadCtrl();
    virtual ~T186XusbPadCtrl();

    virtual RC EnableVbusOverride();
    virtual RC DisableDiscDetect(UINT08 Port);
    virtual RC PadInit(UINT08 DeviceModePortIndex, bool IsAdbSafe);
    virtual RC EnablePadTrack(void);
};

#endif
