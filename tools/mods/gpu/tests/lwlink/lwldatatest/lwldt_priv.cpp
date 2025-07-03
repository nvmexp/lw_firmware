/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwldt_priv.h"
#include "core/include/massert.h"
#include "core/include/platform.h"

//------------------------------------------------------------------------------
//! \brief Colwert a transfer direction to a string
//!
//! \return String representing the transfer direction
string LwLinkDataTestHelper::TransferDirStr(TransferDir td)
{
    switch (td)
    {
        case TD_IN_ONLY:
            return "I";
        case TD_OUT_ONLY:
            return "O";
        case TD_IN_OUT:
            return "I/O";
        default:
            break;
    }
    return "Unk";
}

//------------------------------------------------------------------------------
//! \brief Colwert a transfer direction mask to a string
//!
//! \return String representing the transfer direction mask
string LwLinkDataTestHelper::TransferDirMaskStr(UINT32 tdMask)
{
    string tdMaskStr = "ALL";
    if (tdMask != TD_ALL)
    {
        tdMaskStr = "";
        string joinStr = "";
        if (tdMask & TD_IN_ONLY)
        {
            tdMaskStr += TransferDirStr(TD_IN_ONLY);
            joinStr = ",";
        }
        if (tdMask & TD_OUT_ONLY)
        {
            tdMaskStr += joinStr + TransferDirStr(TD_OUT_ONLY);
            joinStr = ",";
        }
        if (tdMask & TD_IN_OUT)
        {
            tdMaskStr += joinStr + TransferDirStr(TD_IN_OUT);
        }
    }
    return tdMaskStr;
}

//------------------------------------------------------------------------------
//! \brief Colwert a transfer type to a string
//!
//! \return String representing the transfer type
string LwLinkDataTestHelper::TransferTypeStr(TransferType tt)
{
    switch (tt)
    {
        case TT_LOOPBACK:
            return "Loopback";
        case TT_P2P:
            return "P2P";
        case TT_SYSMEM:
            return "Sysmem";
        case TT_SYSMEM_LOOPBACK:
            return "Sysmem Loopback";
        case TT_ILWALID:
            return "Ilw";
        default:
            break;
    }
    return "Unknown";
}
//------------------------------------------------------------------------------
//! \brief Colwert a transfer type mask to a string
//!
//! \return String representing the transfer type mask
string LwLinkDataTestHelper::TransferTypeMaskStr(UINT32 ttMask)
{
    string ttMaskStr = "ALL";
    if (ttMask != TT_ALL)
    {
        ttMaskStr = "";
        string joinStr = "";
        if (ttMask & TT_P2P)
        {
            ttMaskStr += joinStr + TransferTypeStr(TT_P2P);
            joinStr = ",";
        }
        if (ttMask & TT_SYSMEM)
        {
            ttMaskStr += joinStr + TransferTypeStr(TT_SYSMEM);
            joinStr = ",";
        }
        if (ttMask & TT_LOOPBACK)
        {
            ttMaskStr += joinStr + TransferTypeStr(TT_LOOPBACK);
            joinStr = ",";
        }
    }
    return ttMaskStr;
}
