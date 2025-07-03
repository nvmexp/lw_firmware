/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  lwswitchexport.cpp
 * @brief Dummy function linked by MODS so that the symbols are exported
 *
 */

#include "lwswitch_user_api.h"
#include <stdarg.h>

void CallAllLwSwitch(va_list args)
{
    // LwSwitch User API
    lwswitch_api_get_devices(NULL);
    lwswitch_api_create_device(NULL, NULL);
    lwswitch_api_free_device(NULL);
    lwswitch_api_create_event(NULL, NULL, 0, NULL);
    lwswitch_api_free_event(NULL);
    lwswitch_api_control(NULL, 0, NULL, 0);
    lwswitch_api_wait_events(NULL, 0, 0);
    lwswitch_api_get_event_info(NULL, NULL);
    lwswitch_api_get_signaled_events(NULL, NULL, NULL);
    lwswitch_api_get_kernel_driver_version(NULL);
    lwswitch_api_acquire_capability(NULL, 0);
}
