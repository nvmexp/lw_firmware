/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef UTF_STATUS_SHIM_H
#define UTF_STATUS_SHIM_H

/*
 * UTF is meant to work with both liblwriscv and libfsp. The former uses
 * LWRV_STATUS return codes, and the latter uses error_t. This file defines
 * RET_TYPE and retval_to_error_t() which the UTF test will use to make them
 * compatible with both liblwriscv and libfsp.
 */

#if defined(UTF_USE_LIBLWRISCV)
#include <lwriscv/status.h>
#define RET_TYPE LWRV_STATUS

/*
 * Define the errors that would be in common-errors.h on the libfsp side.
 * We use these in our tests so that we have a consistent set of error codes
 * and we can reuse the same test code when testing liblwriscv and libfsp
 */
typedef enum
{
    E_SUCCESS,
    E_ILWALID_PARAM,
    E_BUSY,
    E_NOTSUPPORTED,
    E_TIMEOUT,
    E_PERM,
    E_FAULT,
    E_DT_BUS_INFO_NOT_FOUND,
    E_NO_ACCESS,
    E_ILWALID_STATE
} error_t;

static error_t retval_to_error_t(LWRV_STATUS status)
{
    switch(status)
    {
        case LWRV_OK:
            return E_SUCCESS;
        case LWRV_ERR_ILWALID_ARGUMENT:
            return E_ILWALID_PARAM;
        case LWRV_ERR_INSUFFICIENT_RESOURCES:
            return E_FAULT;
        case LWRV_ERR_OUT_OF_RANGE:
            return E_ILWALID_PARAM;
        case LWRV_ERR_DMA_NACK:
            return E_BUSY;
        case LWRV_ERR_GENERIC:
            return E_FAULT;
        default:
            UT_FAIL("unknown return code");
    }
}

#elif defined(UTF_USE_LIBFSP)
#define RET_TYPE error_t
#define retval_to_error_t(status) (status)

#else
#error UTF_USE_LIBLWRISCV or UTF_USE_LIBFSP must be defined
#endif

#endif //UTF_STATUS_SHIM_H
