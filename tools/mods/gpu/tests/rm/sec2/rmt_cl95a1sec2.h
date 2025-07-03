/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RMT_CL95A1SEC2_H
#define RMT_CL95A1SEC2_H

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl95a1.h" // LW95A1_TSEC

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "gpu/utility/sec2rtossub.h"

#include "tsec_drv.h"
#include "core/include/memcheck.h"

#define HDCP_KNOWN_RECEIVERS 2
#define HCLASS(x)    LW95A1_HDCP_##x
#define ERROR      RC::SOFTWARE_ERROR

extern LwU8 g_dcpKey[];
extern LwU8 g_certificates[HDCP_KNOWN_RECEIVERS][HCLASS(SIZE_CERT_RX_8)];
extern LwU8 g_srm[];
extern LwU32 g_srmSize;
extern LwU32 g_dcpSize;
extern LwU8 g_lPrime[][HCLASS(SIZE_LPRIME_8)];
extern LwU8 g_hPrime[][HCLASS(SIZE_HPRIME_8)];
extern LwU8 g_rrx[][8];
extern LwU8 g_bIsRepeater[];
extern LwU8 g_content[];
extern LwU8 g_contentEnc[];
extern LwU32 g_contentSize;
extern LwU8 g_eKm[][HCLASS(SIZE_E_KM_8)];
extern LwU8 g_eKs[][HCLASS(SIZE_E_KS_8)];
extern LwU8 g_rn[][HCLASS(SIZE_RN_8)];
extern LwU8 g_rtx[][HCLASS(SIZE_RTX_8)];

typedef struct _Sec2MemDesc
{
    Surface2D    surf2d;
    LwU32        size;
    LwRm::Handle hSysMem;
    LwRm::Handle hSysMemDma;
    LwRm::Handle hDynSysMem;
    LwU64        gpuAddrSysMem;
    LwU32       *pCpuAddrSysMem;
} Sec2MemDesc;

/*!
 * HDCP receiver structure
 */
typedef struct _HdcpReceiver
{
    LwU32       index; // Index into known receivers
    LwU32       sessionID;
    LwU8        bIsRepeater;
    LwU64       rtx;
    LwU64       rrx;
    LwU32       hdcpVer;
    LwU8        bRecvPreComputeSupport;
    LwU64       hprime[LW95A1_HDCP_SIZE_HPRIME_64];
    LwU64       lprime[LW95A1_HDCP_SIZE_LPRIME_64];
    Sec2MemDesc m_certMemDesc;
}HdcpReceiver;
class Class95a1Sec2VprTest: public RmTest
{
public:
    Class95a1Sec2VprTest();
    virtual ~Class95a1Sec2VprTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    RC Sec2RtosVprTestCmd(LwU32 startAddr, LwU32 size);
    RC AllocateMem(Sec2MemDesc *, LwU32);
    RC FreeMem(Sec2MemDesc*);
    RC SendSemaMethod();
    RC PollSemaMethod();
    RC StartMethodStream(const char*, LwU32);
    RC EndMethodStream(const char*);
    RC AllocateVprMemdesc(Sec2MemDesc*, LwU32, LwU64, LwU64);
    RC AllocateProtected(Sec2MemDesc*);
    RC VerifyProtected(Sec2MemDesc*);

    Sec2MemDesc m_resMemDesc;

private:

    Sec2MemDesc m_semMemDesc;
    Sec2MemDesc m_methodMemDesc;
    Sec2Rtos     *m_pSec2Rtos;

    LwRm::Handle m_hObj;
    FLOAT64      m_TimeoutMs;

    GpuSubdevice *m_pSubdev;

    LwU64        GetPhysicalAddress(const Surface& surf, LwU64 offset);
    RC           DestroyProtected();
};

#define VPR_REG_RD32(addr)      ( m_pSubdev->RegRd32(addr) )
#define VPR_REG_WR32(addr, val) ( m_pSubdev->RegWr32(addr,val) )

#endif // RMT_CL95A1SEC2_H
