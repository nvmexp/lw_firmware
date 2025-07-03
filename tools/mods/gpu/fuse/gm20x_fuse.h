/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDE_GM20X_FUSE_H
#define INCLUDE_GM20X_FUSE_H

#ifndef INCLUDE_GM10X_FUSE_H
#include "gm10x_fuse.h"
#endif

#include "gpu/utility/gm20xfalcon.h"

#include <list>

class GM20xFuse : public GM10xFuse
{
public:
    GM20xFuse(Device *pDevice);
    RC GetFuseWord(UINT32 FuseRow, FLOAT64 TimeoutMs, UINT32 *pRetData) override;

    RC ApplyIffPatches(string chipSku, vector<UINT32>* pRegsToBlow) override;

    RC BlowFuseRows(const vector<UINT32> &NewBitsArray,
                    const vector<UINT32> &RegsToBlow,
                    FuseSource*           pFuseSrc,
                    FLOAT64               TimeoutMs) override;
    void MarkFuseReadDirty() override;

protected:

    string m_FuseReadBinary;
    bool m_FuseReadCodeLoaded;

    // Requested RIR record values
    vector<UINT16> m_RirRecords;

    // Current RIR record values
    vector<UINT16> m_CachedRirRecords;

    // Stores if RIR records need to be modifed
    bool m_ModifyRir = false;

    // RWL changes registers between Pascal and Volta (FUSECTRL -> DEBUGCTRL)
    // so let each GPU store its correct RWL-enable RegHal value here
    ModsGpuRegValue m_RwlVal = MODS_FUSE_FUSECTRL_RWL_ENABLE;

    const static GM20xFalcon::FalconType s_FuseReadFalcon = GM20xFalcon::FalconType::SEC;

    RC DirectFuseRead(UINT32 FuseRow, FLOAT64 TimeoutMs, UINT32 *pRetData);
    RC FalconFuseRead(UINT32 FuseRow, FLOAT64 TimeoutMs, UINT32 *pRetData);

    // Read and cache the RIR records
    RC ReadRIR();

    // Add RIR overrides to be blown after main fusing
    // Also updates m_FuseReg with the RIR overridden values
    RC AddRIRRecords(const FuseUtil::FuseDef& fuseDef, UINT32 valToBlow) override;

    // Disable added RIR records
    // Also updates m_FuseReg with the new values
    RC DisableRIRRecords(const FuseUtil::FuseDef& fuseDef, UINT32 valToBlow) override;

    // Encodes a 16-bit RIR record given the bit number, whether or not to set that
    // bit to 1, and whether or not to disable the RIR record
    virtual UINT32 CreateRIRRecord(UINT32 bit, bool set1 = false, bool disable = false);

    // Given a fuse's FuseLoc list, and a bit index into the fuse,
    // callwlates the absolute bit index (0-6143 in Maxwell)
    // Ex: bit 5 of { row 11: bits 7-20 } = absolute bit 364
    static RC GetAbsoluteBitIndex
    (
        INT32 bitIndex,
        const list<FuseUtil::FuseLoc>& fuseNums,
        UINT32* absIndex
    );

private:
    struct PollRegArgs
    {
        GpuSubdevice * pSubdev;
        UINT32         address;
        UINT32         mask;
        UINT32         value;
    };
    RC PollRegValue( UINT32 address, UINT32 value, UINT32 mask, FLOAT64 timeoutMs);
    static bool PollRegValueFunc(void *pArgs);
    static bool PollGetFuseWordDone(void * pArgs);
    static bool PollFuseReadFalconReady(void * pArgs);

};

#endif

