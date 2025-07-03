/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  lwlinkexport.cpp
 * @brief Dummy function linked by MODS so that the symbols are exported
 *
 */

#include "lwlink.h"
#include "lwlink_os.h"
#include "lwlink_user_api.h"
#include <stdarg.h>

void CallAllLwLink(va_list args)
{
    //
    // Temporary change to so that symbols that RM calls (through the MODS
    // exelwtable) dont get optimized out.  Final fix will be to compile the
    // lwlink library into librm.so instead of linking directly with MODS
    //

    // LWLink lib mgmt functions
    lwlink_lib_is_initialized();
    lwlink_lib_is_device_list_empty();

    // LWLink registration functions
    lwlink_lib_register_device(NULL);
    lwlink_lib_unregister_device(NULL);
    lwlink_lib_register_link(NULL, NULL);
    lwlink_lib_unregister_link(NULL);

    // LWLink link mgmt functions
    lwlink_lib_is_link_list_empty(NULL);
    lwlink_lib_get_link(NULL, 0, NULL);
    lwlink_core_check_link_state(NULL, 0);
    lwlink_core_check_tx_sublink_state(NULL, 0);
    lwlink_core_check_rx_sublink_state(NULL, 0);
    lwlink_core_poll_link_state(NULL, 0, 0);
    lwlink_core_poll_sublink_state(NULL, 0, 0, NULL, 0, 0, 0);
    lwlink_core_poll_tx_sublink_state(NULL, 0, 0, 0);
    lwlink_core_poll_rx_sublink_state(NULL, 0, 0, 0);
    lwlink_lib_set_link_master(NULL);
    lwlink_lib_get_link_master(NULL, NULL);

    // LWLink conn mgmt functions
    lwlink_core_get_intranode_conn(NULL, NULL);
    lwlink_core_get_internode_conn(NULL, NULL);
    lwlink_core_add_intranode_conn(NULL, NULL);
    lwlink_core_add_internode_conn(NULL, NULL);
    lwlink_core_remove_intranode_conn(NULL);
    lwlink_core_remove_internode_conn(NULL);
    lwlink_core_check_intranode_conn_state(NULL, 0);
    lwlink_core_copy_intranode_conn_info(NULL, NULL);
    lwlink_core_copy_internode_conn_info(NULL, NULL);

    // LWLink topology discovery functions
    lwlink_lib_get_remote_conn_info(NULL, NULL);
    lwlink_lib_discover_and_get_remote_conn_info(NULL, NULL, 0);
    lwlink_core_discover_and_get_remote_end(NULL, NULL, 0);

    // LWLink init functions
    lwlink_core_init_links_from_off_to_swcfg(NULL, 0, 0);
    lwlink_lib_reinit_link_from_off_to_swcfg(NULL, 0);

    // LWLink training functions
    lwlink_lib_train_links_from_swcfg_to_active(NULL, 0, 0);
    lwlink_core_train_intranode_conns_from_swcfg_to_active_ALT(NULL, 0, 0);
    lwlink_core_train_intranode_conns_from_swcfg_to_active_legacy(NULL, 0, 0);
    lwlink_core_train_internode_conns_from_swcfg_to_active(NULL, 0, 0);
    lwlink_core_train_internode_conn_sublink_from_safe_to_hs(NULL, 0);
    lwlink_lib_train_links_from_L2_to_active(NULL, 0, 0);
    lwlink_core_train_intranode_conns_from_from_L2_to_active(NULL, 0, 0);
    lwlink_lib_retrain_link_from_swcfg_to_active(NULL, 0);
    lwlink_lib_save_training_seeds(NULL, 0);
    lwlink_lib_copy_training_seeds(NULL, NULL);
    lwlink_core_train_check_link_ready_ALI(NULL,0);
    lwlink_lib_check_training_complete(NULL,0);
    lwlink_core_train_intranode_conns_from_swcfg_to_active_non_ALI(NULL,0,0);


    // LWLink shutdown functions
    lwlink_lib_powerdown_links_from_active_to_L2(NULL, 0, 0);
    lwlink_core_powerdown_intranode_conns_from_active_to_L2(NULL, 0, 0);
    lwlink_lib_powerdown_links_from_active_to_off(NULL, 0, 0);
    lwlink_core_powerdown_intranode_conns_from_active_to_off(NULL, 0, 0);
    lwlink_lib_powerdown_links_from_active_to_swcfg(NULL, 0, 0);
    lwlink_core_powerdown_intranode_conns_from_active_to_swcfg(NULL, 0, 0);
    lwlink_lib_reset_links(NULL, 0, 0);
    lwlink_core_reset_intranode_conns(NULL, 0, 0);

    // LWLink logger functions
    lwlink_core_print_link_state(NULL);
    lwlink_core_print_intranode_conn(NULL);

    // LWLink lock functions
    lwlink_lib_top_lock_alloc();
    lwlink_lib_top_lock_free();
    lwlink_lib_link_lock_alloc(NULL);
    lwlink_lib_link_lock_free(NULL);
    lwlink_lib_top_lock_acquire();
    lwlink_lib_top_lock_release();
    lwlink_lib_link_locks_acquire(NULL, 0);
    lwlink_lib_link_locks_release(NULL, 0);

    // LWLink Core Lib Utility functions
    lwlink_print(NULL, 0, NULL, 0, NULL);
    lwlink_malloc(0);
    lwlink_free(NULL);
    lwlink_strcpy(NULL, NULL);
    lwlink_strcmp(NULL, NULL);
    lwlink_memRd32(NULL);
    lwlink_memWr32(NULL, 0);
    lwlink_memRd64(NULL);
    lwlink_memWr64(NULL, 0);
    lwlink_strlen(NULL);
    lwlink_snprintf(NULL, 0, NULL);
    lwlink_memset(NULL, 0, 0);
    lwlink_memcpy(NULL, NULL, 0);
    lwlink_assert(0);
    lwlink_sleep(0);
    lwlink_acquireLock(NULL);
    lwlink_releaseLock(NULL);
    lwlink_freeLock(NULL);
    lwlink_isLockOwner(NULL);

    // LwLink User API
    lwlink_api_init();
    lwlink_api_create_session(NULL);
    lwlink_api_free_session(NULL);
    lwlink_api_control(NULL, 0, NULL, 0);
    lwlink_api_session_acquire_capability(NULL, 0);
    lwlink_api_register_inband(0);
    lwlink_api_unregister_inband(0);
    lwlink_api_inband_wait_event(0);

#if !defined(TEGRA_MODS) && defined(LWCPU_AARCH64) && defined(LW_MODS)
    // Tegrashim functions
    tegrashim_lib_load(0, 0);
    tegrashim_lib_unload();
    tegrashim_lib_set_link_speed(0);
    tegrashim_lib_init_endpoint();
#endif

}
