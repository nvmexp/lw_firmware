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
// RX is a compromise between the traditional error handling method, RC,
// and oft-used method, std::exception.
// Since C++ has no restrictions on what is thrown/caught, RX instances are
// thrown to maintain RC support while including exception-like messages

#pragma once
#ifndef INCLUDED_RX_H
#define INCLUDED_RX_H

#include <string>

#include "rc.h"

using std::string;

class RX
{
    protected:
        RC type;
        string msg;

    public:
        static RX OK;

        RX();
        RX(RC type, const char* msg);
        RX(RC type, const string msg);
        virtual ~RX() {}

        const string& GetMessage() const;

        operator RC() const;

        bool operator==(RX rhs) const;
        bool operator!=(RX rhs) const;
};

class StickyRX : public RX
{
    public:
        StickyRX();
        StickyRX(const RX& rx);
        StickyRX& operator=(const StickyRX& rhs);
        virtual ~StickyRX() {}
};

#endif
