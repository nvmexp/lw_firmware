/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/hbmtypes.h"
#include "core/include/memlane.h"

#include <tuple>

namespace
{
    static constexpr UINT32 s_DwordBitWidth = 32;
}

namespace Memory
{
    namespace Hbm
    {
        //!
        //! \brief External HBM lane.
        //!
        //! The 'laneBit' field represents an HBM channel lane.
        //!
        struct HbmLane : public Lane
        {
            Hbm::Site site;       //!< HBM site
            Hbm::Channel channel; //!< HBM channel

            HbmLane() : Lane(0, Type::UNKNOWN) {}

            //!
            //! \brief Constructor.
            //!
            //! \param site HBM site.
            //! \param channel HBM channel.
            //! \param channelLaneBit HBM channel lane bit.
            //! \param laneType Type of lane.
            //!
            HbmLane(Hbm::Site site, Hbm::Channel channel, UINT32 channelLaneBit, Type laneType)
                : Lane(channelLaneBit, laneType), site(site), channel(channel)
            {}

            //!
            //! \brief Colwenience constructor for when the lane bit does not
            //! matter.
            //!
            //! \param site HBM site.
            //! \param channel HBM channel.
            //! \param channelDword DWORD in the HBM channel.
            //!
            HbmLane(Hbm::Site site, Hbm::Channel channel, UINT32 channelDword)
                : Lane(channelDword * s_DwordBitWidth, Type::UNKNOWN), site(site), channel(channel)
            {}

            friend bool operator==(const HbmLane& l, const HbmLane& r)
            {
                return (l.site == r.site)
                    && (l.channel == r.channel)
                    && static_cast<const Lane&>(l) == static_cast<const Lane&>(r);
            }
            friend bool operator!=(const HbmLane& l, const HbmLane& r) { return !(l == r); }

            friend bool operator<(const HbmLane& l, const HbmLane& r)
            {
                return std::tie(l.site, l.channel, l.laneBit) <
                    std::tie(r.site, r.channel, r.laneBit);
            }
        };

        //!
        //! \brief A lane's location on the HBM external interface.
        //!
        //! NOTE: This is very similar to HbmLane. It is necessary because there
        //! is no way to derive the DWORD without knowing the subpartition
        //! size. The subparitition size is framebuffer information and is
        //! outside of the scope of HbmLane. HbmLane will store the lane bit,
        //! and this can be used if the DWORD and byte need to be stored
        //! instead.
        //!
        //! \see Memory::Hbm::HbmLane
        //!
        struct HbmLaneDwordLocation
        {
            Site site;        //!< HBM site
            Channel channel;  //!< Channel within the site
            UINT32 dword;     //!< DWORD (32 DQ lanes) within the channel
            UINT32 byte;      //!< Byte within the DWORD
        };
    } // namespace Hbm
} // namespace Memory
