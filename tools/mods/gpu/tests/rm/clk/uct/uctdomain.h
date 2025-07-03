/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2017, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTDOMAIN_H_
#define UCTDOMAIN_H_

#include "gpu/tests/rm/utility/rmtstring.h"
#include "ctrl/ctrl2080/ctrl2080volt.h"

//
// UCT prints rely on a mechanism wherein DomainNameMap::volt has domain IDs
// such that domain ID = BIT(index) and it also maps the names accordingly.
// For example:
//     ("lwvdd",           CLK_UCT_VOLT_DOMAIN_LOGIC   )
// CLK_UCT_VOLT_DOMAIN_LOGIC = BIT(0) hence voltDomain = 0 is considered as
// DOMAIN_LOGIC with name "lwvdd".
//
#define CLK_UCT_VOLT_DOMAIN_LOGIC (BIT(LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC - 1))     // 0x1
#define CLK_UCT_VOLT_DOMAIN_SRAM  (BIT(LW2080_CTRL_VOLT_VOLT_DOMAIN_SRAM - 1))      // 0x2
#define CLK_UCT_VOLT_DOMAIN_MSVDD (BIT(LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD - 1))    // 0x4

//
// Helper macro to translate volt domain from UCT defines
// @ref CLK_UCT_VOLT_DOMAIN_<xyz> to LW2080 defines @ref
// LW2080_CTRL_VOLT_VOLT_DOMAIN_<xyz>.
//
#define ClkVoltDomainUctTo2080(_voltDomain) (BIT_IDX_32(_voltDomain) + 1)

namespace uct
{

    //! \brief      Domain Index
    typedef unsigned short Domain;

    //! \brief      Maximum number of possible domains
    static const Domain Domain_Count = 32;

    //! \brief      Bitmapped Domains
    typedef UINT32 DomainBitmap;

    //! \brief      No domains
    static const DomainBitmap DomainBitmap_None = 0x00000000;

    //! \brief      Highest ordered domain
    static const DomainBitmap DomainBitmap_Last = 0x00000001 << (Domain_Count - 1);

    //! \brief      All domains
    static const DomainBitmap DomainBitmap_All  = 0xffffffff;

    //! \brief      Clock domain index into bitmap
    typedef Domain FreqDomain;                              // unsigned short

    //! \brief      Bitmap of clock domains per LW2080_CTRL_CLK_DOMAIN_xxx
    typedef DomainBitmap FreqDomainBitmap;                  // UINT32

    //! \brief      Maximum number of possible clock domains
    static const Domain FreqDomain_Count = Domain_Count;

    //! \brief      Voltage domain index into bitmap
    typedef Domain VoltDomain;                              // unsigned short

    //! \brief      Bitmap of voltage domains per LW2080_CTRL_PERF_VOLTAGE_DOMAINS_xxx
    typedef DomainBitmap VoltDomainBitmap;                  // UINT32

    //! \brief      Maximum number of possible voltage domains
    static const Domain VoltDomain_Count = Domain_Count;

    //! \brief      Map from domain names to the bitmap
    struct DomainNameMap: public rmt::StringBitmap32
    {
        //! \brief      Initializer of 'const' objects
        typedef rmt::StringBitmap32::initialize initialize;

        //! \brief      Construction using bitmapped map
        DomainNameMap(const initialize &map): rmt::StringBitmap32(map) { }

        //! \brief      Domain name
        inline rmt::String name(Domain domain) const
        {
            return text(BIT(domain)).defaultTo(rmt::String::hex(BIT(domain), 8));
        }

        //! \brief      Domain names suffixed with periods
        inline rmt::String nameList(DomainBitmap bitmap) const
        {
            return textList(bitmap).concatenate(NULL, ".", ".");
        }

        //! \brief      Clock domains
        static const DomainNameMap freq;

        //! \brief      Voltage domains
        static const DomainNameMap volt;
    };
}

#endif /* UCTDOMAIN_H_ */

