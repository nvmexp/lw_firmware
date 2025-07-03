/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "azasttm.h"

#include "device/azalia/azafmt.h"
#include "device/azalia/azadmae.h"

#define RANDOM_STRM_NUMBER 0xff
#define RANDOM_NONZERO_STRM_NUMBER 0xfe

namespace StreamProperties
{
    // Notes on Routing Behavior
    // Those who write and use tests should be aware of how the routing
    // algorithm works. At the present time (November 2007), there are
    // two layers to how routing works: choosing which streams to route
    // first, and then choosing how to route the channels within the stream.
    // It's important to note that no attempt is made at finding an
    // "optimal" routing--this is entirely a first-fit algorithm. With that
    // said, here is how routing works at the stream level:
    //   1) Route INPUT streams with a reservation
    //   2) Route OUTPUT streams with a reservation
    //   3) Route INPUT streams without a reservation
    //   4) Route OUTPUT streams without a reservation
    // Within routing a particular stream, here is how channel
    // routing works:
    //   1) A stream must be routed entirely to one codec. You cannot
    //      split a stream across multiple codecs.
    //   2) If there is a reservation for the stream, that will determine
    //      which codec is used. Otherwise, the first codec with a
    //      valid set of routes will be used.
    //   3) Channel pairs are routed in a strictly increasing manner. If
    //      channels 4-5 are routed, then channels 0-3 also must be routed.
    //      This is not a spec'd requirement, but it sure makes our lives easier.
    //   4) If the routing condition is MUST_ROUTE_ALL_CHANNELS, then
    //      routes must be found for each pair of channels, otherwise fail.
    //   5) If MUST_ROUTE_AT_LEAST_ONE_CHANNEL_PAIR, then one pair of
    //      channels will be guaranteed to be routed, and subsequent
    //      pairs will be routed if possible.
    //   6) If MUST_ROUTE_ONLY_ONE_CHANNEL_PAIR, then only a single pair
    //      will be routed. No attempt will be made to route the other pairs.
    //   7) If MAY_BE_UNROUTED then as many pairs as possible will be routed,
    //      but that may result in 0 pairs being routed, or only some of
    //      the pairs being routed.
    //   8) If MUST_BE_UNROUTED then no attempt will be made to find
    //      a route for any of the channels.
    // Once again note that no attempt is made to optimize this behavior. If
    // you have an 8-channel input stream and specify that all of the channels
    // may or must be routed, that may decrease the chances that an output
    // stream will be routed later on! (Of course, the way around this is
    // to use reservations and/or MUST_ROUTE_ONLY_ONE_CHANNEL_PAIR.)
    enum RoutingRequirementsType
    {
        MUST_ROUTE_ALL_CHANNELS              = 0,
        MUST_ROUTE_AT_LEAST_ONE_CHANNEL_PAIR = 1,
        MUST_ROUTE_ONLY_ONE_CHANNEL_PAIR     = 2,
        MAY_BE_UNROUTED                      = 3,
        MUST_BE_UNROUTED                     = 4
    };

    struct strmProp
    {
        AzaliaFormat::CHANNELS channels        = AzaliaFormat::CHANNELS_2;
        AzaliaFormat::SIZE size                = AzaliaFormat::SIZE_16;
        AzaliaFormat::RATE rate                = AzaliaFormat::RATE_48000;
        AzaliaFormat::TYPE type                = AzaliaFormat::TYPE_PCM;
        AzaliaDmaEngine::TRAFFIC_PRIORITY tp   = AzaliaDmaEngine::TP_HIGH;
        UINT32 nBDLEs                          = 0;
        UINT32 minBlocks                       = 0;
        UINT32 maxBlocks                       = 0;
        bool sync                              = false;
        FLOAT32 outputAttenuation              = 0;
        UINT32 maxHarmonics                    = 0;
        UINT32 clickThreshPercent              = 100;
        UINT32 smoothThreshPercent             = 100;
        bool randomData                        = false;
        AzaliaDmaEngine::STRIPE_LINES sdoLines = AzaliaDmaEngine::SL_ONE;
        UINT32 codingType                      = 0;
        RoutingRequirementsType routingRequirements = StreamProperties::MUST_ROUTE_ALL_CHANNELS;
        struct
        {
            bool reserved                      = false;
            UINT32 codec                       = 0;
            UINT32 pin                         = 0;
        } reservation;

        UINT32 streamNumber                    = RANDOM_STRM_NUMBER;
        UINT32 iocSpace                        = 0;

        bool allowIocs                         = false;
        bool intEn                             = true;
        bool fifoEn                            = true;
        bool descEn                            = false;
        bool iocEn                             = false;
        bool formatSubstOk                     = false;
        bool testDescErrors                    = false;
        bool testFifoErrors                    = false;
        bool testPauseResume                   = false;
        bool truncStream                       = false;     // for overcommitted SDO test
        bool failIfNoData                      = true;
        bool testOutStrmBuffMon                = false;
        string filename;

        void Reset();
    };
}
