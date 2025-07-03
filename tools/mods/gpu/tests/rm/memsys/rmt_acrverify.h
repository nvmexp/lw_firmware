/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _ACR_VERIFY_H_
#define _ACR_VERIFY_H_

#include "rmlsfm.h"

class AcrVerify : public RmTest
{
public:
    AcrVerify();
    virtual ~AcrVerify();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC LsFalconStatus(LwU32, LwU32 *);
    RC VerifyAcrSetup();
    RC VerifyFalconStatus (LwU32 falconId, LwBool bIncludeOnDemandFlcns);

    bool IsAcrDisabled();

private:
    const char *GetFalconName(LwU32);
    LwU32 falconRegisterBase[LSF_FALCON_ID_END];

    RC GetMMUSetup(LwU32 regionId, LwU32 *pReadMask, LwU32* pWriteMask,
                    LwU64 *pStartAddr, LwU64 *pEndAddr);
    RC GetFalconStatus (LwU32 falconId, LwU32 *pStatus);
};

//
// Local register read and write alias. WriteReg is private function of AcrVerify
// class
//
#define ACR_REG_RD32(addr)      ( m_pSubdevice->RegRd32(addr) )
#define ACR_REG_WR32(addr, val) ( m_pSubdevice->RegWr32(addr,val) )

// Security levels of falcons
#define NS_MODE     0
#define LS_MODE     1
#define HS_MODE     2

//
// Falcon's UCODE status is stored in the SCTL register. Since we have to deal
// with many falcons copying the bitfield of UCODE level
//
#define LW_FALCON_SCTL_UCODE_LEVEL                         5:4
#define LW_FALCON_SCTL_UCODE_LEVEL_P0                      0x00000000
#define LW_FALCON_SCTL_UCODE_LEVEL_P1                      0x00000001
#define LW_FALCON_SCTL_UCODE_LEVEL_P2                      0x00000002
#define LW_FALCON_SCTL_UCODE_LEVEL_P3                      0x00000003

#define LW_ENGINE_UCODE_LEVEL_0 0
#define LW_ENGINE_UCODE_LEVEL_1 1
#define LW_ENGINE_UCODE_LEVEL_2 2
#define LW_ENGINE_UCODE_LEVEL_3 3

#endif
