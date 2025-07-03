/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/fpicker.h"
#include "core/include/rc.h"
#include "core/include/jscript.h"
#include "gputest.h"
#include "core/include/callback.h"
#include "gpu/js/fpk_comm.h"

class FPickerTest: public GpuTest
{
public:
    FPickerTest();
    virtual ~FPickerTest();

    virtual bool IsSupported();
    virtual RC SetDefaultsForPicker(UINT32 idx);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    Callbacks *GetCallbacks(UINT32 idx);
    RC Restart();
    RC PickAll();

private:
    Callbacks m_ValidateCallbacks[FPK_FPK_NUM_PICKERS];
};

