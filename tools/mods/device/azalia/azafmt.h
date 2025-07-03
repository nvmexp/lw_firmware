/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZAFMT_H
#define INCLUDED_AZAFMT_H

//!
//! \file azafmt.h
//! \brief The interface of the AzaliaFormat class.
//!

#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif
#ifndef INCLUDE_TYPES_H
#include "core/include/types.h"
#endif

class AzaliaFormat
{
public:
    //--------------------------------------------------------------------------
    //              Public Enums, Structs, and Constants
    //--------------------------------------------------------------------------

    // Note: the enum values below match the bit locations for fields
    // of the controller stream format register. Don't arbitrarily
    // change the values!
    //
    // Also, although these values are public, DO NOT use them as
    // simple integers. Instead, use the colwersion functions in
    // this class. This will avoid bugs in the future, in the
    // event that these values ever change in the HD spec.

    //! \brief Possible sample sizes.
    //!
    //! Note that these sample sizes are always contained in 8, 16, or
    //! 32 bit containers.
    enum SIZE
    {
        SIZE_8  = 0x0, // has 16-bit alignment restrictions
        SIZE_16 = 0x1,
        SIZE_20 = 0x2,
        SIZE_24 = 0x3,
        SIZE_32 = 0x4
    };

    //! \brief Possible sample rates.
    //!
    //! Note that each rate can be defined according to a tuple
    //! of base rate, rate multiplier, and rate divider.
    enum RATE
    {
        RATE_8000   = 8000,
        RATE_11025  = 11025,
        RATE_16000  = 16000,
        RATE_22050  = 22050,
        RATE_24000  = 24000,
        RATE_32000  = 32000,
        RATE_44100  = 44100,
        RATE_48000  = 48000,
        RATE_88200  = 88200,
        RATE_96000  = 96000,
        RATE_176400 = 176400,
        RATE_192000 = 192000,
        RATE_384000 = 384000
    };

    //! \brief Base rates, as defined by the HD spec.
    enum RATEBASE
    {
        RATE_BASE_48 = 0x0,
        RATE_BASE_44 = 0x1
    };

    //! \brief Rate multipliers, as defined by the HD spec.
    enum RATEMULT
    {
        RATE_MULT_X1 = 0x0,
        RATE_MULT_X2 = 0x1,
        RATE_MULT_X3 = 0x2,
        RATE_MULT_X4 = 0x3,
        RATE_MULT_X8 = 0x4
    };

    //! \brief Rate dividers, as defined by the HD spec.
    enum RATEDIV
    {
        RATE_DIV_X1 = 0x0,
        RATE_DIV_X2 = 0x1,
        RATE_DIV_X3 = 0x2,
        RATE_DIV_X4 = 0x3,
        RATE_DIV_X5 = 0x4,
        RATE_DIV_X6 = 0x5,
        RATE_DIV_X7 = 0x6,
        RATE_DIV_X8 = 0x7
    };

    //! \brief The number of channels in a stream.
    enum CHANNELS
    {
        CHANNELS_1  = 0x0,
        CHANNELS_2  = 0x1,
        CHANNELS_3  = 0x2,
        CHANNELS_4  = 0x3,
        CHANNELS_5  = 0x4,
        CHANNELS_6  = 0x5,
        CHANNELS_7  = 0x6,
        CHANNELS_8  = 0x7,
        CHANNELS_9  = 0x8,
        CHANNELS_10 = 0x9,
        CHANNELS_11 = 0xA,
        CHANNELS_12 = 0xB,
        CHANNELS_13 = 0xC,
        CHANNELS_14 = 0xD,
        CHANNELS_15 = 0xE,
        CHANNELS_16 = 0xF
    };

    //! \brief The stream type. Most streams will be PCM.
    enum TYPE
    {
        TYPE_PCM     = 0x0,
        TYPE_NONPCM  = 0x1
    };

    //--------------------------------------------------------------------------
    //       Public Methods
    //--------------------------------------------------------------------------

    //! \brief Colwert an integer value to the equivalent type enum value.
    static AzaliaFormat::TYPE ToType(UINT32 intType);
    //! \brief Colwert an integer value to the equivalent size enum value.
    static AzaliaFormat::SIZE ToSize(UINT32 intSize);
    //! \brief Colwert an integer value to the equivalent rate enum value.
    static AzaliaFormat::RATE ToRate(UINT32 intRate);
    //! \brief Colwert an integer value to the equivalent channel enum value.
    static AzaliaFormat::CHANNELS ToChannels(UINT32 intChannels);

    //! \brief Construct with the default format.
    AzaliaFormat();
    //! \brief Destruct.
    ~AzaliaFormat();
    //! \brief Copy construct.
    AzaliaFormat(const AzaliaFormat* format);
    //! \brief Construct with the specified format.
    AzaliaFormat(AzaliaFormat::TYPE type, AzaliaFormat::SIZE size,
                 AzaliaFormat::RATE rate, AzaliaFormat::CHANNELS channels);

    void SetType(AzaliaFormat::TYPE type);
    AzaliaFormat::TYPE GetType() const;

    void SetSize(AzaliaFormat::SIZE size);
    AzaliaFormat::SIZE GetSize() const;
    UINT32 GetSizeInBytes() const;
    UINT32 GetSizeInBits() const;

    void SetRate(AzaliaFormat::RATE rate);
    AzaliaFormat::RATE GetRate() const;
    UINT32 GetRateAsSamples() const;
    AzaliaFormat::RATEBASE GetRateBase() const;
    AzaliaFormat::RATEMULT GetRateMult() const;
    UINT32 GetRateMultAsInt() const;
    AzaliaFormat::RATEDIV GetRateDiv() const;

    void SetChannels(AzaliaFormat::CHANNELS channels);
    AzaliaFormat::CHANNELS GetChannels() const;
    UINT32 GetChannelsAsInt() const;
    void PrintInfo(Tee::Priority pri);
private:

    AzaliaFormat::TYPE m_Type;
    AzaliaFormat::SIZE m_Size;
    AzaliaFormat::RATE m_Rate;
    AzaliaFormat::CHANNELS m_Channels;
};

#endif // INCLUDED_AZAFMT_H

