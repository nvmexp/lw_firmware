/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

namespace Smlwtil
{
    struct SmcEngine
    {
        bool useAllGpcs;
        bool useWithSriov;
        UINT32 gpcCount;
    };
    struct Partition
    {
        UINT32 partitionFlag;
        vector<SmcEngine> smcEngineData;
    };

    RC ParsePartitionsStr
    (
        string config,
        vector<Partition>* pPartitionData
    );
}
