/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTCHIP_H_
#define UCTCHIP_H_

#include "gpu/tests/rm/utility/rmtstring.h"
#include "core/include/gpu.h"
#include "uctexception.h"

extern bool m_PstateSupported;

namespace uct
{
    //!
    //! \brief      Hierarchy of chip-specific CTP file name suffixes
    //! \see        PlatformSuffix
    //!
    //! \details    This class is not intended to be used directly.
    //!             Use 'PlatformSuffix' instead.
    //!
    //!             For example, on gf119, this list contains:
    //!             - .gf119
    //!             - .gf11x
    //!             - .gf1xx
    //!             - .gfxxx
    //!             - .fermi
    //!             - (empty)
    //!
    struct ChipSuffix: public rmt::StringList
    {
        //! \brief      Determine the suffix list
        inline void initialize(Gpu::LwDeviceId devid)
        {
            clear();                // Start with an empty list
            append(devid);          // Add the chip-specific suffixes
        }

        //! \brief      Append the suffix list
        void append(Gpu::LwDeviceId devid);
    };

    //!
    //! \brief      Hierarchy of chip-specific and platform-specific CTP file suffixes
    //! \see        CtpFileReader
    //!
    //! \details    This hierarchy is used to determine the hierarchy of files
    //!             when a CTP file is specified on the command line or in an
    //!             'include' directive.
    //!
    //!             Possible platforms are:
    //!             - emu       Emulation
    //!             - hw        Silicon Hardware
    //!             - rtl       RTL Similation
    //!             - cmod      c-Model Similation
    //!             - amod      A-Model Similation
    //!
    //!             For example, on gf119 emulation, this list contains:
    //!             - .emu.gf119
    //!             - .emu.gf11x
    //!             - .emu.gf1xx
    //!             - .emu.gfxxx
    //!             - .emu.fermi
    //!             - .emu
    //!             - .gf119
    //!             - .gf11x
    //!             - .gf1xx
    //!             - .gfxxx
    //!             - .fermi
    //!             - (empty)
    //!
    //!             Therefore, "sanity.ctp" on gf119 emulation resolves to:
    //!             - sanity.emu.gf119.ctp
    //!             - sanity.emu.gf11x.ctp
    //!             - sanity.emu.gf1xx.ctp
    //!             - sanity.emu.gfxxx.ctp
    //!             - sanity.emu.fermi.ctp
    //!             - sanity.emu.ctp
    //!             - sanity.gf119.ctp
    //!             - sanity.gf11x.ctp
    //!             - sanity.gf1xx.ctp
    //!             - sanity.gfxxx.ctp
    //!             - sanity.fermi.ctp
    //!             - sanity.ctp
    //!
    //!             On duplicate settings, topmost (i.e. most specific) of the
    //!             chain take precedence.  The logic to search for these files
    //!             is contained in 'CtpFileReader' and its member classes.
    //!
    struct PlatformSuffix: public ChipSuffix
    {
        //! \brief      Determine the suffix list
        void initialize(GpuSubdevice *pSubdevice);
    };

    //!
    //! \brief      Chip- or platform-specific context
    //!
    //! \details    These data are passed to the 'Field::apply' functions.
    //!
    struct ChipContext
    {
        //! \brief      Supported programmable clock domains
        FreqDomainBitmap supportedFreqDomainSet;

        //! \brief      Construction
        inline ChipContext()
        {
            supportedFreqDomainSet = DomainBitmap_None;
        }

        //!
        //! \brief      Query for supported programmable clock (frequency) domains
        //!
        //! \post       this->supportedFreqDomainSet specifies supported domains
        ///!            per LW2080_CTRL_CLK_DOMAIN_xxx.
        //!
        //! \param[in]  pSubdevice   Target chip under test
        //!
        //! \return     List of exceptions (if any)
        //!
        ExceptionList initialize(GpuSubdevice *pSubdevice, rmt::String exclude);

        //! \brief      Domain names suffixed with periods
        inline rmt::String supportedNameList() const
        {
            return uct::DomainNameMap::freq.nameList(supportedFreqDomainSet);
        }
    };

}

#endif /* UCTCHIP_H_ */

