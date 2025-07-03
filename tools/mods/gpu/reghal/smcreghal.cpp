/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "smcreghal.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/reghal/reghal.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "ctrl/ctrl2080.h"

//--------------------------------------------------------------------
//! \brief Constructor
//!
//! The RegHal will not be usable until the Initialize() is called.
//!
//! \param pSubdev     : GpuSubdevice used to read/write registers
//! \param pLwRm       : LwRm client associated with the SMC engine
//! \param swizzId     : Swizz ID associated with the SMC context
//! \param smcEngineId : SMC engine ID associated with the SMC context
//!
SmcRegHal::SmcRegHal(GpuSubdevice * pSubdev, LwRm *pLwRm, UINT32 swizzId, UINT32 smcEngineId)
    : RegHal(pSubdev)
     ,m_pSubdev(pSubdev)
     ,m_pLwRm(pLwRm)
     ,m_SwizzId(swizzId)
     ,m_SmcEngineId(smcEngineId)
{
}

//--------------------------------------------------------------------
UINT32 SmcRegHal::Read32
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    if (IsSmcReg(address, arrayIndexes))
    {
        // SMC register access lwrrently do not support domains as there are no registers that
        // are both per-SMC engine and also require a domain
        MASSERT(domainIndex == 0);
        return ReadPgraphRegister(LookupAddress(address, arrayIndexes));
    }
    return RegHal::Read32(domainIndex, address, arrayIndexes);
}

//--------------------------------------------------------------------
//! \brief Write a register
void SmcRegHal::Write32
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT32            value
)
{
    if (IsSmcReg(address, arrayIndexes))
    {
        // SMC register access lwrrently do not support domains as there are no registers that
        // are both per-SMC engine and also require a domain
        MASSERT(domainIndex == 0);
        WritePgraphRegister(LookupAddress(address, arrayIndexes), value);
        return;
    }
    return RegHal::Write32(domainIndex, space, address, arrayIndexes, value);
}

//--------------------------------------------------------------------
//! \brief Write a register
void SmcRegHal::Write32Sync
(
    UINT32             domainIndex,
    ModsGpuRegAddress  address,
    ArrayIndexes       arrayIndexes,
    UINT32             value
)
{
    if (IsSmcReg(address, arrayIndexes))
    {
        // SMC register access lwrrently do not support domains as there are no registers that
        // are both per-SMC engine and also require a domain
        MASSERT(domainIndex == 0);
        WritePgraphRegister(LookupAddress(address, arrayIndexes), value);
        if (Platform::Hardware != Platform::GetSimulationMode())
        {
            const RC rc = Tasker::PollHw(1000, [&]()->bool
            {
                return (Read32(address, arrayIndexes) == value);
            }, __FUNCTION__);
            if (OK != rc)
            {
                Printf(Tee::PriLow, "SmcRegHal::Write32Sync timed out on readback.\n");
            }
        }
        return;
    }
    return RegHal::Write32Sync(domainIndex, address, arrayIndexes, value);
}

//--------------------------------------------------------------------
//! \brief Write a register field
void SmcRegHal::Write32
(
    UINT32          domainIndex,
    RegSpace        space,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT32          value
)
{
    if (IsSmcReg(field, arrayIndexes))
    {
        // SMC register access lwrrently do not support domains as there are no registers that
        // are both per-SMC engine and also require a domain
        MASSERT(domainIndex == 0);
        UINT32 regValue = Read32(domainIndex, ColwertToAddress(field), arrayIndexes);
        SetField(&regValue, field, value);
        Write32(domainIndex, ColwertToAddress(field), arrayIndexes, regValue);
        return;
    }
    return RegHal::Write32(domainIndex, space, field, arrayIndexes, value);
}

//--------------------------------------------------------------------
//! \brief Write a register field
void SmcRegHal::Write32Sync
(
    UINT32          domainIndex,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT32          value
)
{
    if (IsSmcReg(field, arrayIndexes))
    {
        // SMC register access lwrrently do not support domains as there are no registers that
        // are both per-SMC engine and also require a domain
        MASSERT(domainIndex == 0);
        UINT32 regValue = Read32(domainIndex, ColwertToAddress(field), arrayIndexes);
        SetField(&regValue, field, value);
        Write32Sync(domainIndex, ColwertToAddress(field), arrayIndexes, regValue);
        return;
    }
    return RegHal::Write32Sync(domainIndex, field, arrayIndexes, value);
}

//--------------------------------------------------------------------
bool SmcRegHal::IsSmcReg(ModsGpuRegAddress address, ArrayIndexes regIndexes) const
{
    const UINT32 addr = LookupAddress(address, regIndexes);
    const bool bSmcReg = m_pSubdev->IsPerSMCEngineReg(addr);

    // SMC reghal lwrrently only knows how to handle pgraph registers on a per SMC basis
    MASSERT(!bSmcReg || m_pSubdev->IsPgraphRegister(addr));

    return bSmcReg;
}

//--------------------------------------------------------------------
UINT32 SmcRegHal::ReadPgraphRegister(UINT32 offset) const
{
    MASSERT(m_pSubdev->IsPgraphRegister(offset));

    LW2080_CTRL_GPU_REG_OP regReadOp = { 0 };
    LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;

    memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));
    regReadOp.regOp = LW2080_CTRL_GPU_REG_OP_READ_32;
    regReadOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
    regReadOp.regOffset = offset;
    regReadOp.regValueHi = 0;
    regReadOp.regValueLo = 0;
    regReadOp.regAndNMaskHi = 0;
    regReadOp.regAndNMaskLo = ~0;

    params.regOpCount = 1;
    params.regOps = LW_PTR_TO_LwP64(&regReadOp);
    m_pSubdev->SetGrRouteInfo(&(params.grRouteInfo), m_pSubdev->GetSmcEngineIdx());
    const RC rc = m_pLwRm->ControlBySubdevice(m_pSubdev,
                                 LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                                 &params,
                                 sizeof(params));

    if (rc != OK)
    {
        Printf(Tee::PriError,
               "Could not access register: %s. Offset= 0x%x, smcEngineId=%d, swizzleId=%d",
                rc.Message(),
                offset,
                m_SmcEngineId,
                m_SwizzId);
        MASSERT(!"See previous message!");
        return 0;
    }

    return regReadOp.regValueLo;
}

//--------------------------------------------------------------------
void SmcRegHal::WritePgraphRegister(UINT32 offset, UINT32 data) const
{
    MASSERT(m_pSubdev->IsPgraphRegister(offset));

    LW2080_CTRL_GPU_REG_OP regWriteOp = { 0 };
    LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;

    memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));
    regWriteOp.regOp = LW2080_CTRL_GPU_REG_OP_WRITE_32;
    regWriteOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
    regWriteOp.regOffset = offset;
    regWriteOp.regValueHi = 0;
    regWriteOp.regValueLo = data;
    regWriteOp.regAndNMaskHi = 0;
    regWriteOp.regAndNMaskLo = ~0;

    params.regOpCount = 1;
    params.regOps = LW_PTR_TO_LwP64(&regWriteOp);
    m_pSubdev->SetGrRouteInfo(&(params.grRouteInfo), m_pSubdev->GetSmcEngineIdx());
    const RC rc = m_pLwRm->ControlBySubdevice(m_pSubdev,
                                 LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                                 &params,
                                 sizeof(params));

    if (rc != OK)
    {
        Printf(Tee::PriError,
               "Could not access register: %s. Offset= 0x%x, smcEngineId=%d, swizzId=%d",
                rc.Message(),
                offset,
                m_SmcEngineId,
                m_SwizzId);
        MASSERT(!"See previous message!");
    }
}

struct PollRegArgs
{
    const SmcRegHal*    pSmcRegHal;
    ModsGpuRegAddress   address;
    UINT32              value;
    UINT32              mask;
};

static bool PollRegValueFunc(void *pArgs)
{
    PollRegArgs *pollArgs = (PollRegArgs*)(pArgs);
    ModsGpuRegAddress address = pollArgs->address;
    UINT32 value   = pollArgs->value;
    UINT32 mask    = pollArgs->mask;

    UINT32 readValue = pollArgs->pSmcRegHal->Read32(address);

    return ((value & mask) == (readValue & mask));
}

RC SmcRegHal::PollRegValue
(
    ModsGpuRegAddress address,
    UINT32 value,
    UINT32 mask,
    FLOAT64 timeoutMs
) const
{
    PollRegArgs pollArgs;
    pollArgs.pSmcRegHal = this;
    pollArgs.address = address;
    pollArgs.value = value;
    pollArgs.mask = mask;
    return POLLWRAP_HW(PollRegValueFunc, &pollArgs, timeoutMs);
}
