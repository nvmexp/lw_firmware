/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _SPDM_CPPINTFC_H_
#define _SPDM_CPPINTFC_H_

//
// The following types are defined in both libspdm and MODS precompiled header
// files. To WAR collision, we define macros with the typename. This will rename
// the type definition in libspdm, allowing us to use libspdm type. However, when
// these macros are defined, we are prevented from using precompiled header type.
//
#define intn   __intn_libspdm
#define uintn  __uintn_libspdm
#define int64  __int64_libspdm
#define uint64 __uint64_libspdm

#ifndef LIBSPDM_USE_CXX_STATIC_ASSERT
#define LIBSPDM_USE_CXX_STATIC_ASSERT
#endif

//
// Wrap all libspdm header file includes into this file. This allows RMTest
// RMTest consumers to just use this one header, without any knowledge of WAR.
//
extern "C" {
    #include "spdm_common_lib.h"
    #include "spdm_requester_lib.h"
    #include "spdm_responder_lib.h"
    #include "spdm_common_lib_internal.h"
    #include "spdm_requester_lib_internal.h"
    #include "spdm_responder_lib_internal.h"
    #include "spdm_selwred_message_lib_internal.h"
};

#endif // _SPDM_CPPINTFC_H_
