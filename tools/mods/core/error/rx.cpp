/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// RX - Return code / eXception

#include "core/include/rx.h"

RX RX::OK = RX();

RX::RX() :
    type(),
    msg("")
{}

RX::RX(RC type, const char* msg) :
    type(type),
    msg(msg)
{}

RX::RX(RC type, string msg) :
    type(type),
    msg(msg)
{
}

const string& RX::GetMessage() const
{
    return msg;
}

RX::operator RC() const
{
    return type;
}

bool RX::operator==(RX rhs) const
{
    return type == rhs.type;
}

bool RX::operator!=(RX rhs) const
{
    return !(*this == rhs);
}

StickyRX::StickyRX() :
    RX()
{}

StickyRX::StickyRX(const RX& rx) :
    RX(rx)
{}

StickyRX& StickyRX::operator=(const StickyRX& rhs)
{
    if (*this == RX::OK)
    {
        type = rhs.type;
        msg = rhs.msg;
    }
    return *this;
}
