/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_libif.h"
#include "lwlink_errors.h"
#include <cstdlib>

RC LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(INT32 lwlstatus)
{
    switch (abs(lwlstatus))
    {
        case LWL_SUCCESS:
            return OK;
        case LWL_BAD_ARGS:
            return RC::LWRM_ILWALID_ARGUMENT;
        case LWL_NO_MEM:
            return RC::LWRM_INSUFFICIENT_RESOURCES;
        case LWL_NOT_FOUND:
            return RC::LWRM_NOT_FOUND;
        case LWL_INITIALIZATION_PARTIAL_FAILURE:
            return RC::LWRM_INITIALIZATION_PARTIAL_FAILURE;
        case LWL_INITIALIZATION_TOTAL_FAILURE:
            return RC::LWRM_INITIALIZATION_TOTAL_FAILURE;
        case LWL_PCI_ERROR:
            return RC::LWRM_PCI_ERROR;
        case LWL_ERR_GENERIC:
            return RC::LWRM_ERROR;
        case LWL_ERR_ILWALID_STATE:
            return RC::LWRM_ILWALID_STATE;
        case LWL_UNBOUND_DEVICE:
            return RC::LWRM_UNBOUND_DEVICE;
        case LWL_MORE_PROCESSING_REQUIRED:
            return RC::LWRM_MORE_PROCESSING_REQUIRED;
        case LWL_IO_ERROR:
            return RC::LWRM_IO_ERROR;
        case LWL_ERR_STATE_IN_USE:
            return RC::LWRM_IN_USE;
        case LWL_ERR_NOT_SUPPORTED:
            return RC::LWRM_NOT_SUPPORTED;
        case LWL_ERR_NOT_IMPLEMENTED:
            return RC::LWRM_NOT_IMPLEMENTED;
        default:
            Printf(Tee::PriError,
                   "Could not translate Lwlink library error code 0x%X to a MODS error code\n",
                   abs(lwlstatus));
            break;
    }
    return RC::LWRM_ERROR;
}
