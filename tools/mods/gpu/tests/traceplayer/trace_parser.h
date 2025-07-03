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

#pragma once

#include "core/include/rc.h"
#include "gpu/utility/surf2d.h"
#include "trace_player_surface_variant.h"

#include <map>
#include <string>
#include <vector>

namespace MfgTracePlayer
{
    class Loader;
    class Engine;

    class Parser
    {
        public:
            explicit Parser(Engine& engine);
            RC Parse(const string& path);

            enum class F
            {
                none    = 0,
                hasArg  = 1 << 0,
                virt    = 1 << 1,
                phys    = 1 << 2,
                map     = 1 << 3,
                mapOnly = 1 << 4,
            };

        private:
            typedef vector<string> Tokens;

            /// Sets up all command and surface parameters handlers.
            void RegisterHandlers();
            /// Prints an error message preceded by trace header line.
            void PrintError(const char* fmt, ...);
            /// Extracts a UINT32 from a string.
            RC GetValue(const string& token, UINT32* pValue, int base);
            /// Extracts a UINT64 from a string.
            RC GetValue(const string& token, UINT64* pValue, int base);
            /// Extracts a color format from a string.
            RC GetColorFormat(const string& token, ColorUtils::Format* pColorFormat);
            /// Extracts a surface layout from a string.
            RC GetLayout(const string& token, Surface2D::Layout* pLayout);

            typedef RC (Parser::*CmdHandler)     (const Tokens&);
            typedef RC (Parser::*SurfParmHandler)(const Tokens&, UINT32, Surface2D*, const string&);

            struct SurfParmDesc
            {
                SurfParmHandler handler;
                F               flags;
            };

            typedef map<string, CmdHandler>   TokenHandlers;
            typedef map<string, SurfParmDesc> SurfParmHandlers;
            typedef map<string, UINT32>       IdentifierMap;

            using Variant = MfgTracePlayer::Variant;

            // Trace method handlers
            RC Ignore               (const Tokens& tokens);
            RC Version              (const Tokens& tokens);
            RC AllocSurfaceInner    (const Tokens& tokens, Variant variant);
            RC FreeSurfaceInner     (const Tokens& tokens, Variant variant);
            RC AllocSurface         (const Tokens& tokens);
            RC FreeSurface          (const Tokens& tokens);
            RC AllocVirtual         (const Tokens& tokens);
            RC FreeVirtual          (const Tokens& tokens);
            RC AllocPhysical        (const Tokens& tokens);
            RC FreePhysical         (const Tokens& tokens);
            RC Map                  (const Tokens& tokens);
            RC Unmap                (const Tokens& tokens);
            RC CheckDynamicSurface  (const Tokens& tokens);
            RC Channel              (const Tokens& tokens);
            RC SubChannel           (const Tokens& tokens);
            RC Class                (const Tokens& tokens);
            RC GpEntry              (const Tokens& tokens);
            RC WaitForIdle          (const Tokens& tokens);
            RC Event                (const Tokens& tokens);
            RC DisplayImage         (const Tokens& tokens);

            // Surface parameter handlers
            RC SurfFile             (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfFill8            (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfSize             (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfAperture         (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfAccess           (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfCrcCheck         (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfCrcRange         (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfAttrOverride     (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfAttr2Override    (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfTypeOverride     (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfVirtAddress      (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfPhysAddress      (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfSparse           (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfVirtualAlloc     (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfPhysicalAlloc    (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfVirtualOffset    (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);
            RC SurfPhysicalOffset   (const Tokens& tokens, UINT32 surfIdx, Surface2D* pSurf, const string& value);

            Loader&          m_Loader;
            Engine&          m_Engine;
            TokenHandlers    m_TokenHandlers;
            SurfParmHandlers m_SurfParmHandlers;
            bool             m_bHandlersRegistered = false;
            IdentifierMap    m_Channels;
            IdentifierMap    m_SubChannels;
            IdentifierMap    m_Surfaces;
            IdentifierMap    m_Events;
            unsigned         m_LwrLine             = ~0U;
    };
}
