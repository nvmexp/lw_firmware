/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/fuse/encoding/iffencoder.h"
#include "gpu/fuse/fusetypes.h"

class AD10xIffEncoder : public IffEncoder
{
public:
    RC DecodeIffRecord
    (
        const shared_ptr<IffRecord>& record,
        string &output,
        bool* pIsValid
    ) override;

protected:
    RC CheckIfRecordValid(const shared_ptr<IffRecord> pRecord) override;
};
