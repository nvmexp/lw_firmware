/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azafmt.h"
#include "core/include/massert.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaFmt"

AzaliaFormat::TYPE AzaliaFormat::ToType(UINT32 intType)
{
    switch (intType)
    {
        case 0x0: return TYPE_PCM; break;
        case 0x1: return TYPE_NONPCM; break;
        default:
            Printf(Tee::PriError, "Invalid type number (%d)!\n", intType);
            MASSERT(0);
            return TYPE_PCM;
    }
}

AzaliaFormat::SIZE AzaliaFormat::ToSize(UINT32 intSize)
{
    switch (intSize)
    {
        case 8:  return(SIZE_8);   break;
        case 16: return(SIZE_16); break;
        case 20: return(SIZE_20); break;
        case 24: return(SIZE_24); break;
        case 32: return(SIZE_32); break;
        default:
            Printf(Tee::PriError, "Unknown stream size (%d)!\n", intSize);
            MASSERT(0);
            return(SIZE_16);
    }
}

AzaliaFormat::RATE AzaliaFormat::ToRate(UINT32 intRate)
{
    switch (intRate)
    {
        case 8000:   return RATE_8000;   break;
        case 11025:  return RATE_11025;  break;
        case 16000:  return RATE_16000;  break;
        case 22050:  return RATE_22050;  break;
        case 24000:  return RATE_24000;  break;
        case 32000:  return RATE_32000;  break;
        case 44100:  return RATE_44100;  break;
        case 48000:  return RATE_48000;  break;
        case 88200:  return RATE_88200;  break;
        case 96000:  return RATE_96000;  break;
        case 176400: return RATE_176400; break;
        case 192000: return RATE_192000; break;
        case 384000: return RATE_384000; break;
        default:
            Printf(Tee::PriError, "Unknown SampleRate (%d)!\n",intRate);
            MASSERT(0);
            return RATE_48000;
    }
}

AzaliaFormat::CHANNELS AzaliaFormat::ToChannels(UINT32 intChannels)
{
    switch (intChannels)
    {
        case  1: return CHANNELS_1;  break;
        case  2: return CHANNELS_2;  break;
        case  3: return CHANNELS_3;  break;
        case  4: return CHANNELS_4;  break;
        case  5: return CHANNELS_5;  break;
        case  6: return CHANNELS_6;  break;
        case  7: return CHANNELS_7;  break;
        case  8: return CHANNELS_8;  break;
        case  9: return CHANNELS_9;  break;
        case 10: return CHANNELS_10; break;
        case 11: return CHANNELS_11; break;
        case 12: return CHANNELS_12; break;
        case 13: return CHANNELS_13; break;
        case 14: return CHANNELS_14; break;
        case 15: return CHANNELS_15; break;
        case 16: return CHANNELS_16; break;
        default:
            Printf(Tee::PriError, "Invalid channel number (%d)!\n", intChannels);
            MASSERT(0);
            return CHANNELS_2;
    }
}

AzaliaFormat::AzaliaFormat()
{
    m_Type = TYPE_PCM;
    m_Size = SIZE_8;
    m_Rate = RATE_8000;
    m_Channels = CHANNELS_1;
}

AzaliaFormat::~AzaliaFormat()
{
}

AzaliaFormat::AzaliaFormat(const AzaliaFormat* format)
{
    MASSERT(format != 0);
    m_Type = format->GetType();
    m_Size = format->GetSize();
    m_Rate = format->GetRate();
    m_Channels = format->GetChannels();
}

AzaliaFormat::AzaliaFormat(AzaliaFormat::TYPE type, AzaliaFormat::SIZE size,
                           AzaliaFormat::RATE rate, AzaliaFormat::CHANNELS channels)
{
    SetType(type);
    SetSize(size);
    SetRate(rate);
    SetChannels(channels);
}

void AzaliaFormat::SetType(AzaliaFormat::TYPE type)
{
    switch (type)
    {
        case TYPE_PCM:
        case TYPE_NONPCM:
            m_Type = type;
            break;
        default:
            Printf(Tee::PriError, "unknown type %d\n", type);
            MASSERT(0);
    }
}

AzaliaFormat::TYPE AzaliaFormat::GetType() const
{
    return m_Type;
}

void AzaliaFormat::SetSize(AzaliaFormat::SIZE size)
{
    switch (size)
    {
        case SIZE_8:
        case SIZE_16:
        case SIZE_20:
        case SIZE_24:
        case SIZE_32:
            m_Size = size;
            break;
        default:
            Printf(Tee::PriError, "unknown size %d\n", size);
            MASSERT(0);
    }
}

AzaliaFormat::SIZE AzaliaFormat::GetSize() const
{
    return m_Size;
}

UINT32 AzaliaFormat::GetSizeInBytes() const
{
    switch (m_Size)
    {
        case SIZE_8:
            return 1;
            break;
        case SIZE_16:
            return 2;
            break;
        case SIZE_20:
        case SIZE_24:
        case SIZE_32:
            return 4;
            break;
        default:
            Printf(Tee::PriError, "Unknown size!\n");
            MASSERT(0);
            return 4;
    }
}

UINT32 AzaliaFormat::GetSizeInBits() const
{
    switch (m_Size)
    {
        case SIZE_8:  return 8;  break;
        case SIZE_16: return 16; break;
        case SIZE_20: return 20; break;
        case SIZE_24: return 24; break;
        case SIZE_32: return 32; break;
        default:
            Printf(Tee::PriError, "Unknown size!\n");
            MASSERT(0);
            return 32;
    }
}

void AzaliaFormat::SetRate(AzaliaFormat::RATE rate)
{
    switch (rate)
    {
        case RATE_8000:
        case RATE_11025:
        case RATE_16000:
        case RATE_22050:
        case RATE_24000:
        case RATE_32000:
        case RATE_44100:
        case RATE_48000:
        case RATE_88200:
        case RATE_96000:
        case RATE_176400:
        case RATE_192000:
        case RATE_384000:
            m_Rate = rate;
            break;
        default:
            Printf(Tee::PriError, "unknown rate %d!\n", rate);
            MASSERT(0);
    }
}

AzaliaFormat::RATE AzaliaFormat::GetRate() const
{
    return m_Rate;
}

UINT32 AzaliaFormat::GetRateAsSamples() const
{
    return ((UINT32) m_Rate);
}

AzaliaFormat::RATEBASE AzaliaFormat::GetRateBase() const
{
    switch (m_Rate)
    {
        case RATE_8000:
        case RATE_16000:
        case RATE_32000:
        case RATE_24000:
        case RATE_48000:
        case RATE_96000:
        case RATE_192000:
        case RATE_384000:
            return RATE_BASE_48;
            break;
        case RATE_11025:
        case RATE_22050:
        case RATE_44100:
        case RATE_88200:
        case RATE_176400:
            return RATE_BASE_44;
            break;
        default:
            Printf(Tee::PriError, "unknown rate %d!\n", m_Rate);
            MASSERT(0);
            return RATE_BASE_48;
    }
}

AzaliaFormat::RATEMULT AzaliaFormat::GetRateMult() const
{
    switch (m_Rate)
    {
        case RATE_8000:
        case RATE_11025:
        case RATE_16000:
        case RATE_22050:
        case RATE_24000:
        case RATE_44100:
        case RATE_48000:
            return RATE_MULT_X1;
            break;
        case RATE_32000:
        case RATE_88200:
        case RATE_96000:
            return RATE_MULT_X2;
            break;
        case RATE_176400:
        case RATE_192000:
            return RATE_MULT_X4;
            break;
        case RATE_384000:
            return RATE_MULT_X8;
            break;
        default:
            Printf(Tee::PriError, "unknown rate %d\n", m_Rate);
            MASSERT(0);
            return RATE_MULT_X1;
    }
}

UINT32 AzaliaFormat::GetRateMultAsInt() const
{
    switch (GetRateMult())
    {
        case RATE_MULT_X2: return 2; break;
        case RATE_MULT_X3: return 3; break;
        case RATE_MULT_X4: return 4; break;
        case RATE_MULT_X8: return 8; break;
        case RATE_MULT_X1: return 1; break;
        default:
            Printf(Tee::PriError, "unknown rate mult!\n");
            MASSERT(0);
            return(1);
    }
}

AzaliaFormat::RATEDIV AzaliaFormat::GetRateDiv() const
{
    switch (m_Rate)
    {
        case RATE_8000:
            return RATE_DIV_X6;
            break;
        case RATE_11025:
            return RATE_DIV_X4;
            break;
        case RATE_16000:
        case RATE_32000:
            return RATE_DIV_X3;
            break;
        case RATE_22050:
        case RATE_24000:
            return RATE_DIV_X2;
            break;
        case RATE_44100:
        case RATE_48000:
        case RATE_88200:
        case RATE_96000:
        case RATE_176400:
        case RATE_192000:
        case RATE_384000:
            return RATE_DIV_X1;
            break;
        default:
            Printf(Tee::PriError, "unknown rate %d\n", m_Rate);
            MASSERT(0);
            return RATE_DIV_X1;
    }
}

void AzaliaFormat::SetChannels(AzaliaFormat::CHANNELS channels)
{
    switch (channels)
    {
        case CHANNELS_1:
        case CHANNELS_2:
        case CHANNELS_3:
        case CHANNELS_4:
        case CHANNELS_5:
        case CHANNELS_6:
        case CHANNELS_7:
        case CHANNELS_8:
        case CHANNELS_9:
        case CHANNELS_10:
        case CHANNELS_11:
        case CHANNELS_12:
        case CHANNELS_13:
        case CHANNELS_14:
        case CHANNELS_15:
        case CHANNELS_16:
            m_Channels = channels;
            break;
        default:
            Printf(Tee::PriError, "unknown channels %d\n", channels);
            MASSERT(0);
    }
}

AzaliaFormat::CHANNELS AzaliaFormat::GetChannels() const
{
    return m_Channels;
}

UINT32 AzaliaFormat::GetChannelsAsInt() const
{
    return (m_Channels + 1);
}

void AzaliaFormat::PrintInfo(Tee::Priority pri)
{
    Printf(pri, "Format: type=0x%x (%s), size=0x%x (%d), rate=0x%x (%d), channels=0x%x (%d)\n",
           m_Type, ((m_Type == TYPE_PCM) ? "PCM" : "NON_PCM"),
           m_Size, GetSizeInBits(), m_Rate, m_Rate, m_Channels, (m_Channels + 1));
}

