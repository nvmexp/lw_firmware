/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDE_GP10X_FUSE_H
#define INCLUDE_GP10X_FUSE_H

#ifndef INCLUDE_GM20X_FUSE_H
#include "gm20x_fuse.h"
#endif

class GP10xFuse : public GM20xFuse
{
public:
    GP10xFuse(Device *pDevice);
protected:
    RC LatchFuseToOptReg(FLOAT64 TimeoutMs) override;
    bool IsFusePrivSelwrityEnabled() override;
    RC GetSelwreUcode(bool isPMUBinaryEncrypted, vector<UINT32>* pBinary);
    RC EnableSelwreFuseblow(bool isPMUBinaryEncrypted) override;
    virtual RC CheckFuseblowEnabled();

    // Encodes a 16-bit RIR record given the bit number, whether or not to set that
    // bit to 1, and whether or not to disable the RIR record
    UINT32 CreateRIRRecord(UINT32 bit, bool set1, bool disable) override;
};

#endif

