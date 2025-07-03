/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libspdm copyright, as file is LW-authored uses on libspdm code.

/*!
 * @file   lw_callbacks.c
 * @brief  Dispatch functions to call any SEC2-application specific callbacks
 *         upon certain connection or session state.
 */

/* ------------------------- Application Includes --------------------------- */
#include "spdm_responder_lib_internal.h"
#include "lw_device_secret.h"

/* ------------------------- Public Functions ------------------------------- */
/**
    Notify a given callback function upon a change in connection state.

    @note This callback has no way to directly return failure, keep
    this in mind when implementing.
    
    @param spdm_context      A pointer to the SPDM context.
    @param connection_state  Indicate the SPDM connection state.
**/
void spdm_dispatch_connection_state_callback
(
    IN void                    *spdm_context,
    IN spdm_connection_state_t  connection_state
)
{
    // No connection state callbacks for now.
    return;
}

/**
    Notify a given callback function upon a change in session state.

    @note This callback has no way to directly return failure, keep
    this in mind when implementing.

    @param spdm_context   A pointer to the SPDM context.
    @param session_id     The session_id of a session.
    @param session_state  The state of a session.
**/
void spdm_dispatch_session_state_callback
(
    IN void                 *spdm_context,
    IN uint32                session_id,
    IN spdm_session_state_t  session_state
)
{
    switch (session_state)
    {
        case SPDM_SESSION_STATE_ESTABLISHED:
            spdm_device_secret_derive_apm_sk(spdm_context, session_id);
            break;
        default:
            break;
    }

    return;
}
