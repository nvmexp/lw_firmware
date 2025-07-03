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

namespace Security
{
    //!
    //! \brief Security unlock level.
    //!
    //! Defines what level of access a user has.
    //!
    enum class UnlockLevel
    {
        UNKNOWN                 //!< Unknown
        ,NONE                   //!< No access
        ,BYPASS_EXPIRED         //!< Expired bypass file
        ,BYPASS_UNEXPIRED       //!< Active bypass file
        ,LWIDIA_NETWORK         //!< On the LWPU network
        ,LWIDIA_NETWORK_BYPASS  //!< On the LWPU network with bypass
    };

    //!
    //! \brief Setup security unlock level for the run.
    //!
    void InitSelwrityUnlockLevel(UnlockLevel selwrityUnlockLevel);

    //!
    //! \brief Get the current security unlock level.
    //!
    UnlockLevel GetUnlockLevel();

    //!
    //! \brief True if on the LWPU intranet, false otherwise.
    //!
    bool OnLwidiaIntranet();

    //!
    //! \brief True if user has permission to see internal information, false
    //! otherwise.
    //!
    //! Examples of internal information:
    //! - HULK
    //! - GPU permissions (PLM)
    //!
    bool CanDisplayInternalInfo();
}
