/* * LWIDIA_COPYRIGHT_BEGIN *
 * Copyright 2017-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_T210PADCTRL_H
#define INCLUDED_T210PADCTRL_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_XUSBPADCTRL_H
#include "xusbpadctrl.h"
#endif

class T210XusbPadCtrl: public XusbPadCtrl
{

public:
    T210XusbPadCtrl();
    virtual ~T210XusbPadCtrl();

    virtual RC DisableDiscDetect(UINT08 Port);
};

#endif
