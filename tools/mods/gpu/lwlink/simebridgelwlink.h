/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "simlwlink.h"

//--------------------------------------------------------------------
//! \brief Implementation of LwLink used for simulated Ebridge devices
//!
class SimEbridgeLwLink : public SimLwLink
{
protected:
    SimEbridgeLwLink() {}
    virtual ~SimEbridgeLwLink() {}

    RC DoSetFabricApertures
    (
        const vector<FabricAperture> &fas
    ) override { return RC::UNSUPPORTED_FUNCTION; }
};
