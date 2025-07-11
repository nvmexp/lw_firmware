/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

//
// LWE (tandrewprinz): Including LWPU copyright, as file contains LWPU modifications.
// Tabs have been replaced with spaces.
//

#include "spdm_common_lib_internal.h"

/**
  This function initializes the session info.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The SPDM session ID.
**/
void spdm_session_info_init(IN spdm_context_t *spdm_context,
                IN spdm_session_info_t *session_info,
                IN uint32 session_id, IN boolean use_psk)
{
    spdm_session_type_t session_type;
    uint32 capabilities_flag;

    capabilities_flag = spdm_context->connection_info.capability.flags &
                spdm_context->local_context.capability.flags;
    switch (capabilities_flag &
        (SPDM_GET_CAPABILITIES_REQUEST_FLAGS_ENCRYPT_CAP |
         SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MAC_CAP)) {
    case 0:
        session_type = SPDM_SESSION_TYPE_NONE;
        break;
    case (SPDM_GET_CAPABILITIES_REQUEST_FLAGS_ENCRYPT_CAP |
          SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MAC_CAP):
        session_type = SPDM_SESSION_TYPE_ENC_MAC;
        break;
    case SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MAC_CAP:
        session_type = SPDM_SESSION_TYPE_MAC_ONLY;
        break;
    default:
        ASSERT(FALSE);
        session_type = SPDM_SESSION_TYPE_MAX;
        break;
    }

    zero_mem(session_info,
         OFFSET_OF(spdm_session_info_t, selwred_message_context));
    spdm_selwred_message_init_context(
        session_info->selwred_message_context);
    session_info->session_id = session_id;
    session_info->use_psk = use_psk;
    spdm_selwred_message_set_use_psk(session_info->selwred_message_context,
                     use_psk);
    spdm_selwred_message_set_session_type(
        session_info->selwred_message_context, session_type);
    spdm_selwred_message_set_algorithms(
        session_info->selwred_message_context,
        spdm_context->connection_info.algorithm.bash_hash_algo,
        spdm_context->connection_info.algorithm.dhe_named_group,
        spdm_context->connection_info.algorithm.aead_cipher_suite,
        spdm_context->connection_info.algorithm.key_schedule);
    spdm_selwred_message_set_psk_hint(
        session_info->selwred_message_context,
        spdm_context->local_context.psk_hint,
        spdm_context->local_context.psk_hint_size);
//
// LWE (tandrewprinz) LW_REFACTOR_TRANSCRIPT: Change initial sizes to
// custom expected size.
//
    session_info->session_transcript.message_k.max_buffer_size =
        sizeof(session_info->session_transcript.message_k.buffer);
    session_info->session_transcript.message_f.max_buffer_size =
        sizeof(session_info->session_transcript.message_f.buffer);
}

/**
  This function gets the session info via session ID.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The SPDM session ID.

  @return session info.
**/
void *spdm_get_session_info_via_session_id(IN void *context,
                       IN uint32 session_id)
{
    spdm_context_t *spdm_context;
    spdm_session_info_t *session_info;
    uintn index;

    if (session_id == ILWALID_SESSION_ID) {
        DEBUG((DEBUG_ERROR,
               "spdm_get_session_info_via_session_id - Invalid session_id\n"));
        ASSERT(FALSE);
        return NULL;
    }

    spdm_context = context;

    session_info = spdm_context->session_info;
    for (index = 0; index < MAX_SPDM_SESSION_COUNT; index++) {
        if (session_info[index].session_id == session_id) {
            return &session_info[index];
        }
    }

    DEBUG((DEBUG_ERROR,
           "spdm_get_session_info_via_session_id - not found session_id\n"));
    return NULL;
}

/**
  This function gets the selwred message context via session ID.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The SPDM session ID.

  @return selwred message context.
**/
void *spdm_get_selwred_message_context_via_session_id(IN void *spdm_context,
                              IN uint32 session_id)
{
    spdm_session_info_t *session_info;

    session_info =
        spdm_get_session_info_via_session_id(spdm_context, session_id);
    if (session_info == NULL) {
        return NULL;
    } else {
        return session_info->selwred_message_context;
    }
}

/**
  This function gets the selwred message context via session ID.

  @param  spdm_session_info              A pointer to the SPDM context.

  @return selwred message context.
**/
void *
spdm_get_selwred_message_context_via_session_info(IN void *spdm_session_info)
{
    spdm_session_info_t *session_info;

    session_info = spdm_session_info;
    if (session_info == NULL) {
        return NULL;
    } else {
        return session_info->selwred_message_context;
    }
}

/**
  This function assigns a new session ID.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The SPDM session ID.

  @return session info associated with this new session ID.
**/
void *spdm_assign_session_id(IN void *context, IN uint32 session_id,
                 IN boolean use_psk)
{
    spdm_context_t *spdm_context;
    spdm_session_info_t *session_info;
    uintn index;

    spdm_context = context;

    if (session_id == ILWALID_SESSION_ID) {
        DEBUG((DEBUG_ERROR,
               "spdm_assign_session_id - Invalid session_id\n"));
        ASSERT(FALSE);
        return NULL;
    }

    session_info = spdm_context->session_info;

    for (index = 0; index < MAX_SPDM_SESSION_COUNT; index++) {
        if (session_info[index].session_id == session_id) {
            DEBUG((DEBUG_ERROR,
                   "spdm_assign_session_id - Duplicated session_id\n"));
            ASSERT(FALSE);
            return NULL;
        }
    }

    for (index = 0; index < MAX_SPDM_SESSION_COUNT; index++) {
        if (session_info[index].session_id == ILWALID_SESSION_ID) {
            spdm_session_info_init(spdm_context,
                           &session_info[index], session_id,
                           use_psk);
            spdm_context->latest_session_id = session_id;
            return &session_info[index];
        }
    }

    DEBUG((DEBUG_ERROR, "spdm_assign_session_id - MAX session_id\n"));
    return NULL;
}

/**
  This function allocates half of session ID for a requester.

  @param  spdm_context                  A pointer to the SPDM context.

  @return half of session ID for a requester.
**/
uint16 spdm_allocate_req_session_id(IN spdm_context_t *spdm_context)
{
    uint16 req_session_id;
    spdm_session_info_t *session_info;
    uintn index;

    session_info = spdm_context->session_info;
    for (index = 0; index < MAX_SPDM_SESSION_COUNT; index++) {
        if ((session_info[index].session_id & 0xFFFF0000) ==
            (ILWALID_SESSION_ID & 0xFFFF0000)) {
            req_session_id = (uint16)(0xFFFF - index);
            return req_session_id;
        }
    }

    DEBUG((DEBUG_ERROR, "spdm_allocate_req_session_id - MAX session_id\n"));
    return (ILWALID_SESSION_ID & 0xFFFF0000) >> 16;
}

/**
  This function allocates half of session ID for a responder.

  @param  spdm_context                  A pointer to the SPDM context.

  @return half of session ID for a responder.
**/
uint16 spdm_allocate_rsp_session_id(IN spdm_context_t *spdm_context)
{
    uint16 rsp_session_id;
    spdm_session_info_t *session_info;
    uintn index;

    session_info = spdm_context->session_info;
    for (index = 0; index < MAX_SPDM_SESSION_COUNT; index++) {
        if ((session_info[index].session_id & 0xFFFF) ==
            (ILWALID_SESSION_ID & 0xFFFF)) {
            rsp_session_id = (uint16)(0xFFFF - index);
            return rsp_session_id;
        }
    }

    DEBUG((DEBUG_ERROR, "spdm_allocate_rsp_session_id - MAX session_id\n"));
    return (ILWALID_SESSION_ID & 0xFFFF);
}

/**
  This function frees a session ID.

  @param  spdm_context                  A pointer to the SPDM context.
  @param  session_id                    The SPDM session ID.

  @return freed session info assicated with this session ID.
**/
void *spdm_free_session_id(IN void *context, IN uint32 session_id)
{
    spdm_context_t *spdm_context;
    spdm_session_info_t *session_info;
    uintn index;

    spdm_context = context;

    if (session_id == ILWALID_SESSION_ID) {
        DEBUG((DEBUG_ERROR,
               "spdm_free_session_id - Invalid session_id\n"));
        ASSERT(FALSE);
        return NULL;
    }

    session_info = spdm_context->session_info;
    for (index = 0; index < MAX_SPDM_SESSION_COUNT; index++) {
        if (session_info[index].session_id == session_id) {
            spdm_session_info_init(spdm_context,
                           &session_info[index],
                           ILWALID_SESSION_ID, FALSE);
            return &session_info[index];
        }
    }

    DEBUG((DEBUG_ERROR, "spdm_free_session_id - MAX session_id\n"));
    ASSERT(FALSE);
    return NULL;
}
