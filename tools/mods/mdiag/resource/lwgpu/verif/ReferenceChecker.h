/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_REFERENCECHECKER_H
#define INCLUDED_REFERENCECHECKER_H

#include <vector>

using namespace std;

class Surface2D;
class CheckDmaBuffer;
class ITestStateReport;

class ReferenceChecker: public ICrcVerifier
{
public:
    virtual void Setup(GpuVerif* verifier);
    virtual TestEnums::TEST_STATUS Check(ICheckInfo* info);

    // Read stored crcs and compare with measured crc. Return true if caller should dump image
    virtual bool Verify
    (
        const char *key, // Key used to find stored crcs
        UINT32 measuredCrc, // Measured crc
        SurfaceInfo* surfInfo,
        UINT32 gpuSubdevice,
        ITestStateReport *report, // File to touch in case of error
        CrcStatus *crcStatus // Result of comparison
    );

    virtual UINT32 CallwlateCrc(const string& checkName, UINT32 subdev,
            const SurfaceAssembler::Rectangle *rect)
    {
        MASSERT(!"ReferenceChecker doesn't support CallwlateCrc.");
        return 0;
    }

private:
    RC ReadCRC(const CheckDmaBuffer *pCheck, UINT32* crc,
               UINT32 gpuSubdevice, vector<UINT08> *pRefVec) const;

    GpuVerif*          m_verifier;
    const ArgReader*    m_params;
    UINT32              m_PrintRefCompMismatchCnt;
};

#endif /* INCLUDED_REFERENCECHECKER_H */
