/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "trace_parser.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include "trace_loader.h"
#include "trace_player_engine.h"
#include "gpu/utility/surf2d.h"

namespace
{
    class LineReader
    {
        public:
            LineReader() = default;
            RC Load(MfgTracePlayer::Loader& loader, const string& filename);
            RC LoadEncrypted(MfgTracePlayer::Loader& loader, const string& filename);
            RC ReadLine(string* pLine);
            unsigned GetLwrrentLine() const { return m_Line; }

        private:
            unsigned m_Line   = ~0U;
            size_t   m_BufPos = ~0U;
            string   m_Buffer;
    };

    RC LineReader::Load(MfgTracePlayer::Loader& loader, const string& filename)
    {
        MASSERT(m_Buffer.empty());
        m_Line   = 0;
        m_BufPos = 0;
        return loader.LoadFile(filename, &m_Buffer);
    }

    RC LineReader::LoadEncrypted(MfgTracePlayer::Loader& loader, const string& filename)
    {
        MASSERT(m_Buffer.empty());
        m_Line   = 0;
        m_BufPos = 0;
        return loader.LoadEncryptedFile(filename, &m_Buffer);
    }

    RC LineReader::ReadLine(string* pLine)
    {
        ++m_Line;
        *pLine = "";
        do
        {
            // Find LF
            size_t endPos = m_Buffer.find('\n', m_BufPos);
            if (endPos == string::npos)
            {
                endPos = m_Buffer.size();
            }

            // Copy the data to the output line
            pLine->append(m_Buffer, m_BufPos, endPos-m_BufPos);
            m_BufPos = endPos;

            // Handle end of line
            if (m_BufPos < m_Buffer.size())
            {
                // Skip LF
                ++m_BufPos;

                // Ignore CR
                const size_t last = pLine->size() - 1;
                if (!pLine->empty() && (*pLine)[last] == '\r')
                {
                    pLine->resize(last);
                }

                // Ignore empty lines
                if (!pLine->empty())
                {
                    break;
                }

                // Continue to the next line if this line was empty
                ++m_Line;
            }

        } while (m_BufPos < m_Buffer.size());

        return OK;
    }
}

namespace MfgTracePlayer
{
    constexpr Parser::F operator|(Parser::F a, Parser::F b)
    {
        return static_cast<Parser::F>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    constexpr bool operator&(Parser::F a, Parser::F b)
    {
        return !! (static_cast<unsigned>(a) & static_cast<unsigned>(b));
    }
}

MfgTracePlayer::Parser::Parser(Engine& engine)
: m_Loader(engine.GetLoader())
, m_Engine(engine)
{
}

void MfgTracePlayer::Parser::RegisterHandlers()
{
    MASSERT(!m_bHandlersRegistered);
    m_bHandlersRegistered = true;

    using Pser = MfgTracePlayer::Parser;

    m_TokenHandlers["VERSION"]              = &Pser::Version;
    m_TokenHandlers["ALLOC_SURFACE"]        = &Pser::AllocSurface;
    m_TokenHandlers["FREE_SURFACE"]         = &Pser::FreeSurface;
    m_TokenHandlers["ALLOC_VIRTUAL"]        = &Pser::AllocVirtual;
    m_TokenHandlers["FREE_VIRTUAL"]         = &Pser::FreeVirtual;
    m_TokenHandlers["ALLOC_PHYSICAL"]       = &Pser::AllocPhysical;
    m_TokenHandlers["FREE_PHYSICAL"]        = &Pser::FreePhysical;
    m_TokenHandlers["MAP"]                  = &Pser::Map;
    m_TokenHandlers["UNMAP"]                = &Pser::Unmap;
    m_TokenHandlers["CHECK_DYNAMICSURFACE"] = &Pser::CheckDynamicSurface;
    m_TokenHandlers["CHANNEL"]              = &Pser::Channel;
    m_TokenHandlers["SUBCHANNEL"]           = &Pser::SubChannel;
    m_TokenHandlers["CLASS"]                = &Pser::Class;
    m_TokenHandlers["GPENTRY"]              = &Pser::GpEntry;
    m_TokenHandlers["WAIT_FOR_IDLE"]        = &Pser::WaitForIdle;
    m_TokenHandlers["TRACE_OPTIONS"]        = &Pser::Ignore;
    m_TokenHandlers["ESCAPE_WRITE_FILE"]    = &Pser::Ignore;
    m_TokenHandlers["EVENT"]                = &Pser::Event;
    m_TokenHandlers["DISPLAY_IMAGE"]        = &Pser::DisplayImage;

    m_SurfParmHandlers["FILE"]              = { &Pser::SurfFile,           F::hasArg | F::map | F::virt           };
    m_SurfParmHandlers["FILL8"]             = { &Pser::SurfFill8,          F::hasArg | F::map                     };
    m_SurfParmHandlers["SIZE"]              = { &Pser::SurfSize,           F::hasArg | F::map | F::virt | F::phys };
    m_SurfParmHandlers["APERTURE"]          = { &Pser::SurfAperture,       F::hasArg | F::phys                    };
    m_SurfParmHandlers["ACCESS"]            = { &Pser::SurfAccess,         F::hasArg | F::map                     };
    m_SurfParmHandlers["CRC_CHECK"]         = { &Pser::SurfCrcCheck,       F::hasArg | F::map                     };
    m_SurfParmHandlers["CRC_RANGE"]         = { &Pser::SurfCrcRange,       F::hasArg | F::map                     };
    m_SurfParmHandlers["ATTR_OVERRIDE"]     = { &Pser::SurfAttrOverride,   F::hasArg | F::virt | F::phys          };
    m_SurfParmHandlers["ATTR2_OVERRIDE"]    = { &Pser::SurfAttr2Override,  F::hasArg | F::virt | F::phys          };
    m_SurfParmHandlers["TYPE_OVERRIDE"]     = { &Pser::SurfTypeOverride,   F::hasArg | F::virt | F::phys          };
    m_SurfParmHandlers["VIRT_ADDRESS"]      = { &Pser::SurfVirtAddress,    F::hasArg | F::virt                    };
    m_SurfParmHandlers["PHYS_ADDRESS"]      = { &Pser::SurfPhysAddress,    F::hasArg | F::phys                    };
    m_SurfParmHandlers["SPARSE"]            = { &Pser::SurfSparse,         F::virt                                };
    m_SurfParmHandlers["VIRTUAL_ALLOC"]     = { &Pser::SurfVirtualAlloc,   F::hasArg | F::mapOnly                 };
    m_SurfParmHandlers["PHYSICAL_ALLOC"]    = { &Pser::SurfPhysicalAlloc,  F::hasArg | F::mapOnly                 };
    m_SurfParmHandlers["VIRTUAL_OFFSET"]    = { &Pser::SurfVirtualOffset,  F::hasArg | F::mapOnly                 };
    m_SurfParmHandlers["PHYSICAL_OFFSET"]   = { &Pser::SurfPhysicalOffset, F::hasArg | F::mapOnly                 };
}

RC MfgTracePlayer::Parser::Parse(const string& path)
{
    RC rc;

    if (!m_bHandlersRegistered)
    {
        RegisterHandlers();
    }

    CHECK_RC(m_Loader.SetTraceLocation(path));

    CHECK_RC(m_Engine.StartParsing());

    LineReader reader;
    rc = reader.Load(m_Loader, "test.hdr");
    if (rc != RC::OK)
    {
        rc.Clear();
        rc = reader.LoadEncrypted(m_Loader, "test.hde");
        if (rc == RC::FILE_DOES_NOT_EXIST)
        {
            Printf(Tee::PriError, "File \"test.hdr\" not found\n");
        }
        CHECK_RC(rc);
    }

    m_Channels.clear();
    m_SubChannels.clear();
    m_Surfaces.clear();

    Tokens tokens;
    string line;
    for (;;)
    {
        // Read one non-empty line
        CHECK_RC(reader.ReadLine(&line));
        if (line.empty())
        {
            break;
        }

        // Remember current line - for PrintError()
        m_LwrLine = reader.GetLwrrentLine();

        // Strip out comments
        const size_t commentPos = line.find('#');
        if (commentPos != string::npos)
        {
            line.resize(commentPos);
        }

        // Split the line into tokens
        tokens.clear();
        CHECK_RC(Utility::Tokenizer(line, " \t\r\n", &tokens));

        // Ignore empty lines
        if (tokens.empty())
        {
            continue;
        }

        // Find the handler
        const auto handlrIt = m_TokenHandlers.find(tokens[0]);
        if (handlrIt == m_TokenHandlers.end())
        {
            PrintError("Unrecognized or unsupported trace command: %s\n",
                       tokens[0].c_str());
            return RC::BAD_TRACE_DATA;
        }

        // Run the handler
        CHECK_RC((this->*(handlrIt->second))(tokens));
    }

    CHECK_RC(m_Engine.DoneParsing());

    return rc;
}

void MfgTracePlayer::Parser::PrintError(const char* fmt, ...)
{
    const string format = Utility::StrPrintf("Error in trace header, line %u: ", m_LwrLine) + fmt;
    va_list args;
    va_start(args, fmt);
    ModsExterlwAPrintf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL, format.c_str(), args);
    va_end(args);
}

RC MfgTracePlayer::Parser::GetValue(const string& token, UINT32* pValue, int base)
{
    char* endptr = nullptr;
    const UINT64 value = Utility::Strtoull(token.c_str(), &endptr, base);
    if (static_cast<size_t>(endptr - token.c_str()) != token.size() ||
        value == ~0ULL || value > ~0U)
    {
        PrintError("Value \"%s\" does not colwert to a 32-bit unsigned integer\n",
                   token.c_str());
        return RC::BAD_TRACE_DATA;
    }
    *pValue = static_cast<UINT32>(value);
    return OK;
}

RC MfgTracePlayer::Parser::GetValue(const string& token, UINT64* pValue, int base)
{
    char* endptr = nullptr;
    const UINT64 value = Utility::Strtoull(token.c_str(), &endptr, base);
    if (static_cast<size_t>(endptr - token.c_str()) != token.size() ||
        value == ~0ULL)
    {
        PrintError("Value \"%s\" does not colwert to a 64-bit unsigned integer\n",
                   token.c_str());
        return RC::BAD_TRACE_DATA;
    }
    *pValue = value;
    return OK;
}

RC MfgTracePlayer::Parser::GetColorFormat(const string& token, ColorUtils::Format* pColorFormat)
{
    *pColorFormat = ColorUtils::StringToFormat(token);
    if (*pColorFormat == ColorUtils::LWFMT_NONE)
    {
        return RC::BAD_TRACE_DATA;
    }
    return OK;
}

RC MfgTracePlayer::Parser::GetLayout(const string& token, Surface2D::Layout* pLayout)
{
    if (token == "PITCH")
    {
        *pLayout = Surface2D::Pitch;
    }
    else if (token == "BLOCKLINEAR")
    {
        *pLayout = Surface2D::BlockLinear;
    }
    else
    {
        PrintError("Value \"%s\" does not describe surface layout\n",
                   token.c_str());
        return RC::BAD_TRACE_DATA;
    }
    return OK;
}

RC MfgTracePlayer::Parser::Ignore(const Tokens& tokens)
{
    MASSERT(!tokens.empty());
    Printf(Tee::PriDebug, "Ignored trace command: %s\n", tokens[0].c_str());
    return OK;
}

RC MfgTracePlayer::Parser::Version(const Tokens& tokens)
{
    if (tokens.size() != 2 || tokens[1] != "5")
    {
        PrintError("Unsupported trace version - %s\n",
                   tokens.size() < 2 ? "0" : tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }
    return OK;
}

namespace
{
    const char* CommandFromVariant(MfgTracePlayer::Variant variant)
    {
        using Variant = MfgTracePlayer::Variant;

        switch (variant)
        {
            case Variant::All:      return "ALLOC_SURFACE";
            case Variant::Virtual:  return "ALLOC_VIRTUAL";
            case Variant::Physical: return "ALLOC_PHYSICAL";
            case Variant::Map:      return "MAP";
        }

        return nullptr;
    }
}

RC MfgTracePlayer::Parser::AllocSurfaceInner(const Tokens& tokens, Variant variant)
{
    if (tokens.size() < 2)
    {
        PrintError("Invalid %s parameters\n", CommandFromVariant(variant));
        return RC::BAD_TRACE_DATA;
    }

    if (m_Surfaces.find(tokens[1]) != m_Surfaces.end())
    {
        PrintError("Duplicate allocation name \"%s\"\n", tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    F allowedFlags = F::none;
    if (variant & Variant::Virtual)
    {
        allowedFlags = allowedFlags | F::virt;
    }
    if (variant & Variant::Physical)
    {
        allowedFlags = allowedFlags | F::phys;
    }
    if (variant & Variant::Map)
    {
        allowedFlags = allowedFlags | F::map;
    }
    if (variant == Variant::Map)
    {
        allowedFlags = allowedFlags | F::mapOnly;
    }

    RC rc;

    UINT32 index = ~0U;
    m_Engine.CreateSurface(tokens[1], &index, variant);
    m_Surfaces[tokens[1]] = index;
    const string emptyParam;

    for (UINT32 i = 2; i < tokens.size(); i += 2)
    {
        const string& param    = tokens[i];
        const auto    handlrIt = m_SurfParmHandlers.find(param);
        if ((handlrIt != m_SurfParmHandlers.end()) && (handlrIt->second.flags & allowedFlags))
        {
            const bool hasArg = handlrIt->second.flags & F::hasArg;
            if ((i + 1 >= tokens.size()) && hasArg)
            {
                PrintError("Missing value for parameter %s\n", param.c_str());
                return RC::BAD_TRACE_DATA;
            }
            const string& value = hasArg ? tokens[i + 1] : emptyParam;

            StickyRC handlerRC;
            m_Engine.ForEachSurface2D(index, [&](Surface2D* pSurf)
            {
                MASSERT(pSurf);

                handlerRC = (this->*(handlrIt->second.handler))(tokens, index, pSurf, value);
            });

            CHECK_RC(handlerRC);
        }
        else
        {
            PrintError("Unsupported %s parameter - \"%s\"\n",
                       CommandFromVariant(variant), param.c_str());
            return RC::BAD_TRACE_DATA;
        }
    }

    m_Engine.AddAllocSurface(index);

    return rc;
}

RC MfgTracePlayer::Parser::FreeSurfaceInner(const Tokens& tokens, Variant variant)
{
    if (tokens.size() != 2)
    {
        PrintError("Invalid %s parameters\n", CommandFromVariant(variant));
        return RC::BAD_TRACE_DATA;
    }

    const auto it = m_Surfaces.find(tokens[1]);
    if (it == m_Surfaces.end())
    {
        PrintError("Allocation name \"%s\" not found\n", tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    m_Engine.AddFreeSurface(it->second);
    return OK;
}

RC MfgTracePlayer::Parser::AllocSurface(const Tokens& tokens)
{
    return AllocSurfaceInner(tokens, Variant::All);
}

RC MfgTracePlayer::Parser::FreeSurface(const Tokens& tokens)
{
    return FreeSurfaceInner(tokens, Variant::All);
}

RC MfgTracePlayer::Parser::AllocVirtual(const Tokens& tokens)
{
    return AllocSurfaceInner(tokens, Variant::Virtual);
}

RC MfgTracePlayer::Parser::FreeVirtual(const Tokens& tokens)
{
    return FreeSurfaceInner(tokens, Variant::Virtual);
}

RC MfgTracePlayer::Parser::AllocPhysical(const Tokens& tokens)
{
    return AllocSurfaceInner(tokens, Variant::Physical);
}

RC MfgTracePlayer::Parser::FreePhysical(const Tokens& tokens)
{
    return FreeSurfaceInner(tokens, Variant::Physical);
}

RC MfgTracePlayer::Parser::Map(const Tokens& tokens)
{
    return AllocSurfaceInner(tokens, Variant::Map);
}

RC MfgTracePlayer::Parser::Unmap(const Tokens& tokens)
{
    return FreeSurfaceInner(tokens, Variant::Map);
}

RC MfgTracePlayer::Parser::CheckDynamicSurface(const Tokens& tokens)
{
    if (tokens.size() < 2)
    {
        PrintError("Invalid CHECK_DYNAMICSURFACE trace command\n");
        return RC::BAD_TRACE_DATA;
    }

    string suffix;

    for (UINT32 i=2; i < tokens.size(); i++)
    {
        if (tokens[i] == "WAIT_FOR_IDLE") // ignore, we idle all channels before CRC anyway
        {
            i++;
        }
        else if (i == 2)
        {
            suffix = tokens[i];
        }
        else
        {
            PrintError("Unsupported CHECK_DYNAMICSURFACE parameter - %s\n",
                       tokens[i].c_str());
            return RC::BAD_TRACE_DATA;
        }
    }

    const auto it = m_Surfaces.find(tokens[1]);
    if (it == m_Surfaces.end())
    {
        PrintError("Surface \"%s\" not found\n", tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (!m_Engine.HasCrc(it->second))
    {
        PrintError("Surface \"%s\" has no CRC file defined\n", tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (!m_Engine.HasMap(it->second))
    {
        PrintError("Surface \"%s\" has not been created with ALLOC_SURFACE or MAP\n",
                   tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    return m_Engine.AddCheckSurface(it->second, suffix);
}

RC MfgTracePlayer::Parser::Channel(const Tokens& tokens)
{
    if (tokens.size() != 2)
    {
        PrintError("Unsupported CHANNEL parameters\n");
        return RC::BAD_TRACE_DATA;
    }

    if (m_Channels.find(tokens[1]) != m_Channels.end())
    {
        PrintError("Channel \"%s\" oclwrred twice in trace\n",
                   tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    UINT32 index = ~0U;
    m_Engine.CreateChannel(tokens[1], &index);
    m_Channels[tokens[1]] = index;

    return OK;
}

RC MfgTracePlayer::Parser::SubChannel(const Tokens& tokens)
{
    if (tokens.size() != 5 || tokens[4] != "USE_TRACE_SUBCHNUM")
    {
        PrintError("Unsupported SUBCHANNEL parameters\n");
        return RC::BAD_TRACE_DATA;
    }

    if (m_SubChannels.find(tokens[1]) != m_SubChannels.end())
    {
        PrintError("Subchannel \"%s\" oclwrred twice in trace\n",
                   tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    RC rc;

    UINT32 subch = ~0U;
    CHECK_RC(GetValue(tokens[3], &subch, 10));

    const auto it = m_Channels.find(tokens[2]);
    if (it == m_Channels.end())
    {
        PrintError("Channel \"%s\" not found\n", tokens[2].c_str());
        return RC::BAD_TRACE_DATA;
    }

    UINT32 index = ~0U;
    m_Engine.CreateSubChannel(tokens[1], it->second, subch, &index);
    m_SubChannels[tokens[1]] = index;

    return rc;
}

RC MfgTracePlayer::Parser::Class(const Tokens& tokens)
{
    if (tokens.size() != 3)
    {
        PrintError("Unsupported CLASS parameters\n");
        return RC::BAD_TRACE_DATA;
    }

    const auto it = m_SubChannels.find(tokens[1]);
    if (it == m_SubChannels.end())
    {
        PrintError("Subchannel \"%s\" not found\n", tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    return m_Engine.SetSubChannelClass(it->second, tokens[2]);
}

RC MfgTracePlayer::Parser::GpEntry(const Tokens& tokens)
{
    if (tokens.size() != 5)
    {
        PrintError("Unsupported GPENTRY parameters\n");
        return RC::BAD_TRACE_DATA;
    }

    const auto chIt = m_Channels.find(tokens[1]);
    if (chIt == m_Channels.end())
    {
        PrintError("Channel \"%s\" not found\n", tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    const auto surfIt = m_Surfaces.find(tokens[2]);
    if (surfIt == m_Surfaces.end())
    {
        PrintError("Surface \"%s\" not found\n", tokens[2].c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (!m_Engine.HasMap(surfIt->second))
    {
        PrintError("Surface \"%s\" has not been created with ALLOC_SURFACE or MAP\n",
                   tokens[2].c_str());
        return RC::BAD_TRACE_DATA;
    }

    RC rc;
    UINT32 offset = ~0U;
    UINT32 size   = ~0U;
    CHECK_RC(GetValue(tokens[3], &offset, 0));
    CHECK_RC(GetValue(tokens[4], &size,   0));

    m_Engine.AddGpEntry(chIt->second, surfIt->second, offset, size);
    return rc;
}

RC MfgTracePlayer::Parser::WaitForIdle(const Tokens& tokens)
{
    if (tokens.size() < 2)
    {
        PrintError("Invalid WAIT_FOR_IDLE command parameters\n");
        return RC::BAD_TRACE_DATA;
    }

    vector<UINT32> chIds;
    chIds.reserve(tokens.size() - 1);

    for (size_t i = 1; i < tokens.size(); i++)
    {
        const auto it = m_Channels.find(tokens[i]);
        if (it == m_Channels.end())
        {
            PrintError("Channel \"%s\" not found\n", tokens[i].c_str());
            return RC::BAD_TRACE_DATA;
        }

        chIds.push_back(it->second);
    }

    m_Engine.AddWaitForIdle(move(chIds));
    return OK;
}

RC MfgTracePlayer::Parser::Event(const Tokens& tokens)
{
    if (tokens.size() != 2)
    {
        PrintError("Invalid EVENT command parameters\n");
        return RC::BAD_TRACE_DATA;
    }

    auto it = m_Events.find(tokens[1]);
    if (it == m_Events.end())
    {
        UINT32 index = ~0U;
        m_Engine.CreateEvent(tokens[1], &index);
        it = m_Events.insert(make_pair(tokens[1], index)).first;
    }

    m_Engine.AddEvent(it->second);
    return OK;
}

RC MfgTracePlayer::Parser::DisplayImage(const Tokens& tokens)
{
    if (tokens.size() < 7 || tokens.size() > 10)
    {
        PrintError("Invalid DISPLAY_IMAGE command parameters\n");
        return RC::BAD_TRACE_DATA;
    }

    const auto surfIt = m_Surfaces.find(tokens[1]);
    if (surfIt == m_Surfaces.end())
    {
        PrintError("Surface \"%s\" not found\n", tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (!m_Engine.HasMap(surfIt->second))
    {
        PrintError("Surface \"%s\" has not been created with ALLOC_SURFACE or MAP\n",
                   tokens[1].c_str());
        return RC::BAD_TRACE_DATA;
    }

    RC rc;
    UINT32             offset         = ~0U;
    ColorUtils::Format colorFormat    = ColorUtils::LWFMT_NONE;
    Surface2D::Layout  layout         = Surface2D::Pitch;
    UINT32             width          = ~0U;
    UINT32             height         = ~0U;
    UINT32             pitch          = 0;
    UINT32             logBlockHeight = 0;
    UINT32             numBlocksWidth = 0;
    UINT32             aaSamples      = 1;

    CHECK_RC(GetValue      (tokens[2], &offset,      0));
    CHECK_RC(GetColorFormat(tokens[3], &colorFormat   ));
    CHECK_RC(GetLayout     (tokens[4], &layout        ));
    CHECK_RC(GetValue      (tokens[5], &width,       0));
    CHECK_RC(GetValue      (tokens[6], &height,      0));
    if (layout == Surface2D::Pitch)
    {
        if (tokens.size() < 8 || tokens.size() > 9)
        {
            PrintError("Invalid DISPLAY_IMAGE command parameters\n");
            return RC::BAD_TRACE_DATA;
        }
        CHECK_RC(GetValue(tokens[7], &pitch, 0));
        if (tokens.size() > 8)
            CHECK_RC(GetValue(tokens[8], &aaSamples, 0));
    }
    else
    {
        logBlockHeight = 4;
        numBlocksWidth = (width-1) / 16 + 1; // width/16 aligned up
        if (tokens.size() > 7)
            CHECK_RC(GetValue(tokens[7], &logBlockHeight, 0));
        if (tokens.size() > 8)
            CHECK_RC(GetValue(tokens[8], &numBlocksWidth, 0));
        if (tokens.size() > 9)
            CHECK_RC(GetValue(tokens[9], &aaSamples, 0));
    }

    m_Engine.AddDisplayImage(surfIt->second, offset, colorFormat, layout,
                             width, height, pitch, logBlockHeight, numBlocksWidth,
                             aaSamples);
    return rc;
}

RC MfgTracePlayer::Parser::SurfFile
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    m_Engine.SetSurfaceFile(surfIdx, param);
    return OK;
}

RC MfgTracePlayer::Parser::SurfFill8
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT32 value = ~0U;
    CHECK_RC(GetValue(param, &value, 0));

    m_Engine.SetSurfaceFill(surfIdx, value);
    return OK;
}

RC MfgTracePlayer::Parser::SurfSize
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT64 size = ~0U;
    CHECK_RC(GetValue(param, &size, 0));

    pSurf->SetArrayPitch(size);
    return rc;
}

RC MfgTracePlayer::Parser::SurfAperture
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    if (param == "VIDEO")
    {
        pSurf->SetLocation(Memory::Fb);
    }
    else if (param == "COHERENT_SYSMEM")
    {
        pSurf->SetLocation(Memory::Coherent);
    }
    else if (param == "NONCOHERENT_SYSMEM")
    {
        pSurf->SetLocation(Memory::NonCoherent);
    }
    else
    {
        PrintError("Invalid APERTURE parameter - \"%s\"\n", param.c_str());
        return RC::BAD_TRACE_DATA;
    }
    return OK;
}

RC MfgTracePlayer::Parser::SurfAccess
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    if (param == "READ_WRITE")
    {
        pSurf->SetProtect(Memory::ReadWrite);
    }
    else if (param == "READ_ONLY")
    {
        pSurf->SetProtect(Memory::Readable);
    }
    else if (param == "WRITE_ONLY")
    {
        pSurf->SetProtect(Memory::Writeable);
    }
    else
    {
        PrintError("Invalid ACCESS parameter - \"%s\"\n", param.c_str());
        return RC::BAD_TRACE_DATA;
    }
    return OK;
}

RC MfgTracePlayer::Parser::SurfCrcCheck
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    m_Engine.SetSurfaceCrc(surfIdx, param);
    return OK;
}

RC MfgTracePlayer::Parser::SurfCrcRange
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    vector<string> dims;
    RC rc;
    CHECK_RC(Utility::Tokenizer(param, ":", &dims));

    if (dims.size() != 2)
    {
        PrintError("Invalid CRC_RANGE - %s\n", param.c_str());
        return RC::BAD_TRACE_DATA;
    }

    UINT32 begin = 0;
    UINT32 end   = 0;
    CHECK_RC(GetValue(dims[0], &begin, 10));
    CHECK_RC(GetValue(dims[1], &end,   10));

    if (begin >= end)
    {
        PrintError("Invalid CRC_RANGE - %s\n", param.c_str());
        return RC::BAD_TRACE_DATA;
    }

    m_Engine.SetSurfaceCrcRange(surfIdx, begin, end);
    return OK;
}

RC MfgTracePlayer::Parser::SurfAttrOverride
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT32 attr = ~0U;
    CHECK_RC(GetValue(param, &attr, 0));

    m_Engine.ConfigFromAttr(surfIdx, attr);
    return OK;
}

RC MfgTracePlayer::Parser::SurfAttr2Override
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT32 attr = ~0U;
    CHECK_RC(GetValue(param, &attr, 0));

    m_Engine.ConfigFromAttr2(surfIdx, attr);
    return OK;
}

RC MfgTracePlayer::Parser::SurfTypeOverride
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT32 type = ~0U;
    CHECK_RC(GetValue(param, &type, 0));

    pSurf->SetType(type);
    return rc;
}

RC MfgTracePlayer::Parser::SurfVirtAddress
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT64 addr = ~0U;
    CHECK_RC(GetValue(param, &addr, 0));

    pSurf->SetGpuVirtAddrHint(addr);
    return rc;
}

RC MfgTracePlayer::Parser::SurfPhysAddress
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT64 addr = ~0U;
    CHECK_RC(GetValue(param, &addr, 0));

    pSurf->SetGpuPhysAddrHint(addr);
    return rc;
}

RC MfgTracePlayer::Parser::SurfSparse
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    pSurf->SetIsSparse(true);
    return RC::OK;
}

RC MfgTracePlayer::Parser::SurfVirtualAlloc
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    const auto surfIt = m_Surfaces.find(param);
    if (surfIt == m_Surfaces.end())
    {
        PrintError("Surface \"%s\" not found\n", param.c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (! m_Engine.HasVirt(surfIt->second))
    {
        PrintError("Surface \"%s\" was not declared with ALLOC_SURFACE or ALLOC_VIRTUAL\n", param.c_str());
        return RC::BAD_TRACE_DATA;
    }

    m_Engine.SetVirtSurface(surfIdx, surfIt->second);
    return RC::OK;
}

RC MfgTracePlayer::Parser::SurfPhysicalAlloc
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    const auto surfIt = m_Surfaces.find(param);
    if (surfIt == m_Surfaces.end())
    {
        PrintError("Surface \"%s\" not found\n", param.c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (m_Engine.HasVirt(surfIt->second) ||
        m_Engine.HasMap(surfIt->second) ||
        ! m_Engine.HasPhys(surfIt->second))
    {
        PrintError("Surface \"%s\" was not declared with ALLOC_PHYSICAL\n", param.c_str());
        return RC::BAD_TRACE_DATA;
    }

    m_Engine.SetPhysSurface(surfIdx, surfIt->second);
    return RC::OK;
}

RC MfgTracePlayer::Parser::SurfVirtualOffset
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT64 offs = ~0U;
    CHECK_RC(GetValue(param, &offs, 0));

    m_Engine.SetSurfaceVirtOffs(surfIdx, offs);
    return rc;
}

RC MfgTracePlayer::Parser::SurfPhysicalOffset
(
    const Tokens& tokens,
    UINT32        surfIdx,
    Surface2D*    pSurf,
    const string& param
)
{
    RC rc;
    UINT64 offs = ~0U;
    CHECK_RC(GetValue(param, &offs, 0));

    m_Engine.SetSurfacePhysOffs(surfIdx, offs);
    return rc;
}
