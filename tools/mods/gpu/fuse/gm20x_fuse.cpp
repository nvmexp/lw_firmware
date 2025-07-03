/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gm20x_fuse.h"

#include "core/include/crypto.h"
#include "core/include/fileholder.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/tests/testconf.h"
#include "device/interface/pcie.h"
#include "fusesrc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/scopereg.h"
#include "maxwell/gm204/dev_bus.h"
#include "maxwell/gm204/dev_fuse.h"

//-----------------------------------------------------------------------------
GM20xFuse::GM20xFuse(Device* pSubdev)
:
 GM10xFuse(pSubdev),
 m_FuseReadBinary("fuseread_gm206.bin"),
 m_FuseReadCodeLoaded(false)
{

    m_FuseSpec.NumOfFuseReg  = 192;
    m_FuseSpec.NumKFuseWords = 192;
    m_FuseSpec.FuselessStart = 736;     //ramRepairFuseBlockBegin
    m_FuseSpec.FuselessEnd   = 6111;    //ramRepairFuseBlockEnd - 32

    GpuSubdevice *pGpuSubdev = (GpuSubdevice*)GetDevice();
    switch (pGpuSubdev->DeviceId())
    {
        case Gpu::GM200:
            m_GPUSuffix = "GM200";
            break;

        case Gpu::GM204:
            m_GPUSuffix = "GM204";
            break;

        case Gpu::GM206:
            m_GPUSuffix = "GM206";
            break;

        default:
            m_GPUSuffix = "UNKNOWN";
            break;
    }
}

//-----------------------------------------------------------------------------
RC GM20xFuse::GetFuseWord(UINT32 FuseRow, FLOAT64 TimeoutMs, UINT32 *pRetData)
{
    MASSERT(pRetData);

    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();

    // Only GM206 and up will have fuse read selwred, so GM200 and GM204 can
    // use the old method (updated for GM20x registers).
    // Non-silicon runs can also use the old method
    if (pSubdev->IsEmOrSim() ||
        pSubdev->DeviceId() == Gpu::GM200 ||
        pSubdev->DeviceId() == Gpu::GM204)
    {
        return DirectFuseRead(FuseRow, TimeoutMs, pRetData);
    }

    return FalconFuseRead(FuseRow, TimeoutMs, pRetData);
}
//-----------------------------------------------------------------------------
RC GM20xFuse::DirectFuseRead(UINT32 fuseRow, FLOAT64 TimeoutMs, UINT32* pRetData)
{
    MASSERT(pRetData);
    GpuSubdevice* pSubdev = (GpuSubdevice*)GetDevice();
    RegHal& regs = pSubdev->Regs();
    ScopedRegister FuseCtrl(pSubdev, MODS_FUSE_FUSECTRL);

    // set the Row of the fuse
    regs.Write32(MODS_FUSE_FUSEADDR_VLDFLD, fuseRow);
    // set fuse control to 'read'
    regs.Write32(MODS_FUSE_FUSECTRL_CMD_READ);

    // wait for state to become idle
    RC rc = POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs);

    *pRetData = regs.Read32(MODS_FUSE_FUSERDATA);

    return rc;
}
//-----------------------------------------------------------------------------
RC GM20xFuse::FalconFuseRead(UINT32 fuseRow, FLOAT64 TimeoutMs, UINT32* pRetData)
{
    MASSERT(pRetData);

    RC rc;
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();   

    GM20xFalcon falcon(pSubdev, s_FuseReadFalcon);

    // GM206 has FUSECTRL and FUSERDATA selwred, so we have
    // to use a falcon to read the fuse data off the GPU.
    if (!m_FuseReadCodeLoaded)
    {
        CHECK_RC(falcon.Reset());
        CHECK_RC(falcon.LoadBinary(m_FuseReadBinary, FalconUCode::UCodeDescType::LEGACY_V1));
        // Fuse read doesn't need any other input, so start it right away
        CHECK_RC(falcon.Start(false));
        CHECK_RC(POLLWRAP_HW(PollFuseReadFalconReady, &falcon, TimeoutMs));

        UINT32 status;
        CHECK_RC(falcon.ReadMailbox(0, &status));
        if (status != 0x900D290) // g00D 2 g0
        {
            Printf(Tee::PriError, "Fuse read error\n");
            Printf(Tee::PriDebug, "Status:0x%08x", status);
            return RC::SOFTWARE_ERROR;
        }
        m_FuseReadCodeLoaded = true;
    }

    CHECK_RC(falcon.WriteMailbox(1, fuseRow));
    CHECK_RC(falcon.WriteMailbox(0, 0x909E7));

    // wait for state to become idle
    CHECK_RC(POLLWRAP_HW(PollGetFuseWordDone, &falcon, TimeoutMs));
    CHECK_RC(falcon.ReadMailbox(1, pRetData));

    return rc;
}
//-----------------------------------------------------------------------------
RC GM20xFuse::ApplyIffPatches(string chipSku, vector<UINT32>* pRegsToBlow)
{
    RC rc;
    const FuseUtil::SkuConfig* pConfig = GetSkuSpecFromName(chipSku);

    if (pConfig == nullptr)
    {
        if (!chipSku.empty())
        {
            Printf(GetPrintPriority(), "Chip sku: %s not in XML\n", chipSku.c_str());
        }
        return OK;
    }

    if (pConfig->iffPatches.size() == 0)
    {
        Printf(GetPrintPriority(), "No IFF rows found for SKU %s\n", chipSku.c_str());
        return OK;
    }

    vector<UINT32> fuseBlock;
    CHECK_RC(GetCachedFuseReg(&fuseBlock));
    MASSERT(pRegsToBlow->size() == fuseBlock.size());

    // Find how many rows have been used by fuseless and column fuses
    // Also find the first of the existing IFF records (if any)

    // Assume all column-based fuses are used and start at fuseless fuses
    UINT32 usedRows = m_FuseSpec.FuselessStart / REPLACE_RECORD_SIZE;
    UINT32 firstIff = static_cast<UINT32>(pRegsToBlow->size());
    for (UINT32 i = m_FuseSpec.FuselessStart / REPLACE_RECORD_SIZE;
        i < m_FuseSpec.NumOfFuseReg; i++)
    {
        UINT32 row = (*pRegsToBlow)[i] | fuseBlock[i];

        // Anything at or past an IFF record is IFF space
        if ((row & (1 << CHAINID_SIZE)) != 0)
        {
            firstIff = i;
            break;
        }

        if (row != 0)
        {
            usedRows = i + 1;
        }
    }

    // Group the needed IFF rows for simplicity
    list<FuseUtil::IFFRecord> iffRows;

    for (auto& patch : pConfig->iffPatches)
    {
        iffRows.insert(iffRows.end(),
            patch.rows.begin(), patch.rows.end());
    }

    // Some IFF commands are 2 or 3 rows in length and need to be conselwtive. Find
    // the command size by iterating through in exelwtion order (reverse of fusing)
    list<UINT32> commandSizes;
    for (auto iffRecord = iffRows.rbegin(); iffRecord != iffRows.rend(); iffRecord++)
    {
        // The command type is specified in bits 4:0
        UINT32 opCode = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD, _OP, iffRecord->data);
        switch (opCode)
        {
            case IFF_OPCODE(MODIFY_DW):
                commandSizes.push_front(3);
                iffRecord++;
                MASSERT(iffRecord != iffRows.rend());
                iffRecord++;
                MASSERT(iffRecord != iffRows.rend());
                break;
            case IFF_OPCODE(WRITE_DW):
                commandSizes.push_front(2);
                iffRecord++;
                MASSERT(iffRecord != iffRows.rend());
                break;
            case IFF_OPCODE(SET_FIELD):
                commandSizes.push_front(1);
                break;
            default:
                Printf(Tee::PriError, "Malformed IFF Patch: 0x%08x\n", iffRecord->data);
                Printf(Tee::PriError, "Trying to blow Null/Ilwalidated/Unknown IFF\n");
                return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    // IFF rows go at the end of the fuse block. If it's possible to colwert
    // some of the existing rows, then do that, otherwise ilwalidate the rows
    // that don't match the IFF patches to burn, and add them at the end.
    //
    // Note: IFF commands can't change order, so we have to find a match for
    // command N before N+1. Even if command N+1 is already in the fuses,
    // if command N hasn't been written yet, we have to ilwalidate those rows.
    const UINT32 ilwalidatedRow = 0xffffffff;

    UINT32 newRows = 0;
    UINT32 lwrrentRow = static_cast<UINT32>(fuseBlock.size()) - 1;

    auto iffRow = iffRows.begin();
    auto cmdSize = commandSizes.begin();
    while (iffRow != iffRows.end() && lwrrentRow > 0)
    {
        bool hasOneToZero = false;

        // Check for 1 -> 0 transistions
        auto temp = iffRow;
        for (UINT32 i = 0; i < *cmdSize; i++, temp++)
        {
            // Fuse burning doesn't support ATE in IFF yet
            if (temp->dontCare)
            {
                Printf(Tee::PriError, "IFF fuse burning doesn't support the ATE: option!\n");
                return RC::FUSE_VALUE_OUT_OF_RANGE;
            }
            if ((~temp->data & fuseBlock[lwrrentRow - i]) != 0)
            {
                hasOneToZero = true;
                break;
            }
        }

        if (hasOneToZero)
        {
            // Ilwalidate this row and try the next one(s)
            // If the row to ilwalidate is 0, we don't need to change it
            // Keep it in case the IFFs change again and we can reuse this row
            if (fuseBlock[lwrrentRow] != 0)
            {
                (*pRegsToBlow)[lwrrentRow] = ilwalidatedRow;
            }
            lwrrentRow--;
            newRows++;
            continue;
        }

        for (UINT32 i = 0; i < *cmdSize; i++)
        {
            (*pRegsToBlow)[lwrrentRow--] = iffRow->data;
            newRows++;
            iffRow++;
        }
        cmdSize++;
    }

    // Make sure we finished and didn't overlap into fuseless fuses (this is a
    // very unlikely scenario, but it would cause major issues if it happened)
    UINT32 rowsNeeded = newRows + usedRows;
    if (iffRow != iffRows.end() || rowsNeeded > fuseBlock.size())
    {
        Printf(Tee::PriError, "Too many fuse records!\n");
        Printf(Tee::PriError, "Needed: %u;  Available: %u\n",
            rowsNeeded, static_cast<UINT32>(fuseBlock.size()));
        return RC::SOFTWARE_ERROR;
    }

    // Lastly, ilwalidate any extra IFF rows that we don't want anymore
    for (UINT32 i = firstIff; i < fuseBlock.size() - newRows; i++)
    {
        // Don't blow rows if they're blank or if they're already ilwalidated
        if (fuseBlock[i] != 0 && fuseBlock[i] != ilwalidatedRow)
        {
            (*pRegsToBlow)[i] = ilwalidatedRow;
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC GM20xFuse::ReadRIR()
{
    if (!m_CachedRirRecords.empty())
    {
        return OK;
    }

    RC rc;
    GpuSubdevice* pSubdev = static_cast<GpuSubdevice*>(GetDevice());
    TestConfiguration lwrrTestConfig;
    CHECK_RC(lwrrTestConfig.InitFromJs());
    FLOAT64 timeoutMs = lwrrTestConfig.TimeoutMs();

    ScopedRegister fuseCtrl(pSubdev, m_RwlVal);

    for (UINT32 i = 0; i < 4; i++)
    {
        UINT32 rirRow;
        CHECK_RC(DirectFuseRead(i, timeoutMs, &rirRow));
        m_CachedRirRecords.push_back(static_cast<UINT16>(rirRow & 0xffff));
        m_CachedRirRecords.push_back(static_cast<UINT16>(rirRow >> 16));
    }

    m_RirRecords = m_CachedRirRecords;
    return rc;
}

//-----------------------------------------------------------------------------
RC GM20xFuse::AddRIRRecords
(
    const FuseUtil::FuseDef& fuseDef,
    UINT32 valToBlow
)
{
    RC rc;
    if (m_NeedRir.count(fuseDef.Name) == 0)
    {
        return OK;
    }

    CHECK_RC(CacheFuseReg());
    CHECK_RC(ReadRIR());

    UINT32 oldVal = ExtractFuseVal(&fuseDef, ALL_MATCH);
    UINT32 bitsToRir = oldVal & ~valToBlow;
    INT32 bitToFlip = -1;
    while ((bitToFlip = Utility::BitScanForward(bitsToRir)) != -1)
    {
        auto blankRow = find(m_RirRecords.begin(), m_RirRecords.end(), 0);
        if (blankRow == m_RirRecords.end())
        {
            Printf(Tee::PriError, "No remaining RIR records to write to!\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }

        UINT32 rirIndex = 0;
        CHECK_RC(GetAbsoluteBitIndex(bitToFlip, fuseDef.FuseNum, &rirIndex));
        *blankRow = CreateRIRRecord(rirIndex);
        m_FuseReg[rirIndex / 32] &= ~(1 << (rirIndex % 32));
        m_ModifyRir = true;

        if (!fuseDef.RedundantFuse.empty())
        {
            blankRow = find(m_RirRecords.begin(), m_RirRecords.end(), 0);
            if (blankRow == m_RirRecords.end())
            {
                Printf(Tee::PriError, "No remaining RIR records to write to!\n");
                return RC::FUSE_VALUE_OUT_OF_RANGE;
            }

            CHECK_RC(GetAbsoluteBitIndex(bitToFlip, fuseDef.RedundantFuse, &rirIndex));
            *blankRow = CreateRIRRecord(rirIndex);
            m_FuseReg[rirIndex / 32] &= ~(1 << (rirIndex % 32));
            m_ModifyRir = true;
        }
        bitsToRir ^= 1 << bitToFlip;
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC GM20xFuse::DisableRIRRecords
(
    const FuseUtil::FuseDef& fuseDef,
    UINT32 valToBlow
)
{
    RC rc;
    if (m_PossibleUndoRir.count(fuseDef.Name) == 0)
    {
        return OK;
    }

    CHECK_RC(CacheFuseReg());
    CHECK_RC(ReadRIR());

    UINT32 oldVal   = ExtractFuseVal(&fuseDef, ALL_MATCH);
    UINT32 bits0to1 = ~oldVal & valToBlow;
    INT32 bitToUndo = -1;
    while ((bitToUndo = Utility::BitScanForward(bits0to1)) != -1)
    {
        UINT32 rirIndex = 0;

        CHECK_RC(GetAbsoluteBitIndex(bitToUndo, fuseDef.FuseNum, &rirIndex));
        // Find if there is an RIR record for the same bit setting it to 0
        UINT32 rirRecord  = CreateRIRRecord(rirIndex);
        auto rowToDisable = find(m_RirRecords.begin(), m_RirRecords.end(), rirRecord);
        if (rowToDisable != m_RirRecords.end())
        {
            // Replace it with a disabled version of the RIR record
            *rowToDisable = CreateRIRRecord(rirIndex, false, true);
            m_FuseReg[rirIndex / 32] |= (1 << (rirIndex % 32));
            m_ModifyRir = true;
        }

        if (!fuseDef.RedundantFuse.empty())
        {
            CHECK_RC(GetAbsoluteBitIndex(bitToUndo, fuseDef.RedundantFuse, &rirIndex));
            rirRecord = CreateRIRRecord(rirIndex);
            rowToDisable = find(m_RirRecords.begin(), m_RirRecords.end(), rirRecord);
            if (rowToDisable != m_RirRecords.end())
            {
                *rowToDisable = CreateRIRRecord(rirIndex, false, true);
                m_FuseReg[rirIndex / 32] |= (1 << (rirIndex % 32));
                m_ModifyRir = true;
            }
        }

        bits0to1 ^= 1 << bitToUndo;
    }
    return OK;
}

//-----------------------------------------------------------------------------
// Given the index of a bit in a fuse and the fuse's location, returns the
// absolute index (GM20x: 0 - 6143) of that bit
RC GM20xFuse::GetAbsoluteBitIndex
(
    INT32 bitIndex,
    const list<FuseUtil::FuseLoc>& fuseNums,
    UINT32* absIndex
)
{
    UINT32 startingBits = 0;
    for (const auto& fuseNum : fuseNums)
    {
        UINT32 numBits = fuseNum.Msb - fuseNum.Lsb + 1;
        if (startingBits + numBits < static_cast<UINT32>(bitIndex))
        {
            startingBits += numBits;
            continue;
        }

        UINT32 bitInRow = fuseNum.Lsb + bitIndex - startingBits;
        *absIndex = bitInRow + fuseNum.Number * 32;
        return OK;
    }

    // If we've gotten here, the fuse JSON's FuseNumbers
    // don't match the fuse descriptions.
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
UINT32 GM20xFuse::CreateRIRRecord(UINT32 bit, bool set1, bool disable)
{
    // Max of 13 address bits
    MASSERT((bit & 0xffffe000) == 0);

    // Bit assignments for a 16 bit RIR record
    // From TSMC Electrical Fuse Datasheet (TEF16FGL192X32HD18_PHENRM_C140218)
    // 15:      Disable
    // 14-2:    Addr[12-0]
    // 1:       Data
    // 0:       Enable
    UINT16 newRecord = 1;
    newRecord |= (set1 ? 1 : 0) << 1;
    newRecord |= bit << 2;
    newRecord |= (disable ? 1 : 0) << 15;

    return newRecord;
}

//-----------------------------------------------------------------------------
RC GM20xFuse::BlowFuseRows
(
    const vector<UINT32> &NewBitsArray,
    const vector<UINT32> &RegsToBlow,
    FuseSource*           pFuseSrc,
    FLOAT64               TimeoutMs
)
{
    RC rc;
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    RegHal& regs = pSubdev->Regs();
    // make fuse visible
    ScopedRegister FuseCtrl(pSubdev, MODS_FUSE_FUSECTRL);

    regs.Write32(MODS_FUSE_FUSECTRL_CMD_SENSE_CTRL);

    CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));

    SetFuseVisibility(true);
    bool FuseSrcHigh = false;

    for (UINT32 i = 0; i < m_FuseSpec.NumOfFuseReg; i++)
    {
        if (Utility::CountBits(NewBitsArray[i]) == 0)
        {
            // don't bother attempting to blow this row if there's no new bits
            continue;
        }

        // wait until state is idle
        CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));

        // set the Row of the fuse
        regs.Write32(MODS_FUSE_FUSEADDR, i);

        // write the new fuse bits in
        regs.Write32(MODS_FUSE_FUSEWDATA, NewBitsArray[i]);

        if (!FuseSrcHigh)
        {
            // Flick FUSE_SRC to high
            CHECK_RC(pFuseSrc->Toggle(FuseSource::TO_HIGH));
            Tasker::Sleep(m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_GPIO_HIGH]);
            FuseSrcHigh = true;
        }

        // issue the Write command
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_WRITE);
        CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));
    }

    // RIR fusing
    if (!m_ModifyRir)
    {
        return rc;
    }

    regs.Write32(m_RwlVal);
    for (UINT32 i = 0; i < m_CachedRirRecords.size() / 2; i++)
    {
        if (!FuseSrcHigh)
        {
            // Flick FUSE_SRC to high
            CHECK_RC(pFuseSrc->Toggle(FuseSource::TO_HIGH));
            Tasker::Sleep(m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_GPIO_HIGH]);
            FuseSrcHigh = true;
        }

        UINT32 newRir = m_RirRecords[2 * i];
        newRir |= m_RirRecords[2 * i + 1] << 16;
        UINT32 oldRir = m_CachedRirRecords[2 * i];
        oldRir |= m_CachedRirRecords[2 * i + 1] << 16;
        if (newRir == oldRir)
        {
            continue;
        }
        UINT32 valToBlow = newRir & ~oldRir;
        regs.Write32(MODS_FUSE_FUSEADDR_VLDFLD, i);
        regs.Write32(MODS_FUSE_FUSEWDATA, valToBlow);
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_WRITE);
        CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));

        regs.Write32(MODS_FUSE_FUSEADDR_VLDFLD, i);
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_READ);
        CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));
        UINT32 valBlown = regs.Read32(MODS_FUSE_FUSERDATA);

        if (valBlown != newRir)
        {
            Printf(Tee::PriError, "RIR row %d blew incorrectly! Expected %x Actual %x\n",
                i, newRir, valBlown);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
void GM20xFuse::MarkFuseReadDirty()
{
    GM10xFuse::MarkFuseReadDirty();
    m_FuseReadCodeLoaded = false;
}

//-----------------------------------------------------------------------------
RC GM20xFuse::PollRegValue( UINT32 address, UINT32 value, UINT32 mask, FLOAT64 timeoutMs)
{
    PollRegArgs pollArgs;
    pollArgs.pSubdev = (GpuSubdevice*)GetDevice();
    pollArgs.address = address;
    pollArgs.value   = value;
    pollArgs.mask    = mask;
    return POLLWRAP_HW(PollRegValueFunc, &pollArgs, timeoutMs);
}

//-----------------------------------------------------------------------------
bool GM20xFuse::PollRegValueFunc(void *pArgs)
{
    PollRegArgs *pollArgs = (PollRegArgs*)(pArgs);
    UINT32 address = pollArgs->address;
    UINT32 value   = pollArgs->value;
    UINT32 mask    = pollArgs->mask;

    UINT32 temp = pollArgs->pSubdev->RegRd32(address);
    temp |= mask;
    pollArgs->pSubdev->RegWr32(address, temp);
    UINT32 readValue = pollArgs->pSubdev->RegRd32(address);

    return (value  == (readValue & mask));
}

//-----------------------------------------------------------------------------
bool GM20xFuse::PollGetFuseWordDone(void * pArgs)
{
    GM20xFalcon *pFalcon = (GM20xFalcon *)(pArgs);
    UINT32 status = 0;
    pFalcon->ReadMailbox(0, &status);
    return status == 0x155E7; // IS SET
}

//-----------------------------------------------------------------------------
bool GM20xFuse::PollFuseReadFalconReady(void * pArgs)
{
    GM20xFalcon *pFalcon = (GM20xFalcon *)(pArgs);
    UINT32 status = 0;
    pFalcon->ReadMailbox(0, &status);
    // A valid status is either 0X900D290 (good to go) or BADx (bad + reason #)
    return status == 0x900D290 || (status & 0xFFF0) == 0xBAD0;
}
