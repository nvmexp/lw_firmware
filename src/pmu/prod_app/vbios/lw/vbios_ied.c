/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   vbios_ied.c.c
 * @brief  VBIOS common functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_bar0.h"
#include "pmu_objvbios.h"
#include "vbios/vbios_opcodes.h"
#include "config/g_vbios_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define PCI_EXP_ROM_SIGNATURE       0xaa55
#define PCI_EXP_ROM_SIGNATURE_LW    0x4e56 // "VN" in word format
#define IS_VALID_PCI_ROM_SIG(sig)   (((sig) == PCI_EXP_ROM_SIGNATURE) || \
                                     ((sig) == PCI_EXP_ROM_SIGNATURE_LW))

// PMU-RISCV-BUILD-HACKS! : Temp. changes to unblock uCode build.
//                          These will be fixed/removed ultimately
#ifndef TO_STR
#define TO_STR(fmt) #fmt
#endif

/* --------------------- Private functions Prototypes ----------------------- */
static FLCN_STATUS  s_vbiosParseUnpackStructure(const char *format, LwU8 *pPackedData,
    LwU32 *pUnpackedData, LwU32 maxUnpackedSize, LwU32 *pUnpackedSize);
static FLCN_STATUS  s_vbiosIedFetchUnpack(VBIOS_IED_CONTEXT *pCtx, LwU32 *pData,
    LwU32 maxUnpackedSize, const char *format);
static LwBool       s_vbiosRegConditionCheck(LwU32 addr, LwU32 mask, LwU32 compare);
static FLCN_STATUS  s_vbiosGetDataTableEntry(VBIOS_IED_CONTEXT *pCtx, LwU8  entryIdx,
    LwU8 objectSize, const char *objectFormat, LwU32 *pObject, LwU32 maxUnpackedSize);

/* -------------------------- Public  functions ----------------------------- */

/*!
 * @brief       Execute VBIOS IED script
 *
*  @param[in]  pVbiosDesc             Flacon memory decriptor for the VBIOS image in FB
 * @param[in]  scriptOffset           Script offset in VBIOS image
 * @param[in]  conditionTableOffset   Condition table offset in VBIOS image
 * @param[in]  dpPortNum              DP port

 * @return     FLCN_STATUS FLCN_OK is script is exelwted
 */
FLCN_STATUS
vbiosIedExelwteScript
(
    RM_FLCN_MEM_DESC *pVbiosDesc,
    LwU16             scriptOffset,
    LwU16             conditionTableOffset,
    LwU8              dpPortNum,
    LwU8              orIndex,
    LwU8              linkIndex
)
{
    VBIOS_IED_CONTEXT ctx;

    ctx.bCompleted           = LW_FALSE;
    ctx.bCondition           = LW_TRUE;
    ctx.pVbiosDesc           = pVbiosDesc;
    ctx.nOffsetLwrrent       = scriptOffset;
    ctx.conditionTableOffset = conditionTableOffset;
    ctx.dpPortNum            = dpPortNum;
    ctx.orIndex              = orIndex;
    ctx.linkIndex            = linkIndex;

    return vbiosIedExelwteScriptSub(&ctx);
}

/*!
 * @brief      Execute VBIOS IED script from table value
 *
 * @param[in]  pVbiosDesc             Flacon memory decriptor for the VBIOS image in FB
 * @param[in]  scriptOffset           Script offset in VBIOS image
 * @param[in]  conditionTableOffset   Condition table offset in VBIOS image
 * @param[in]  dpPortNum              DP port
 * @param[in]  orIndex                OR index
 * @param[in]  linkIndex              DP link index
 * @param[in]  data                   table value to match
 * @param[in]  dataSizeByte           size of the table value to match
 *
 * @return     FLCN_OK if script is exelwted
 */
FLCN_STATUS
vbiosIedExelwteScriptTable
(
    RM_FLCN_MEM_DESC    *pVbiosDesc,
    LwU16                scriptOffset,
    LwU16                conditionTableOffset,
    LwU8                 dpPortNum,
    LwU8                 orIndex,
    LwU8                 linkIndex,
    LwU32                data,
    LwU32                dataSizeByte,
    IED_TABLE_COMPARISON comparison
)
{
    VBIOS_IED_CONTEXT ctx;
    LwU8              tableData;
    LwU8              tableData0;
    LwU8              tableData1 = 0;
    LwU8              scriptOffset0;
    LwU8              scriptOffset1;
    LwU8              lastData = 0;
    LwU8              entries = 0;
    LwBool            bFound = LW_FALSE;
    FLCN_STATUS       status;

    ctx.bCompleted           = LW_FALSE;
    ctx.bCondition           = LW_TRUE;
    ctx.pVbiosDesc           = pVbiosDesc;
    ctx.nOffsetLwrrent       = scriptOffset;
    ctx.conditionTableOffset = conditionTableOffset;
    ctx.dpPortNum            = dpPortNum;
    ctx.orIndex              = orIndex;
    ctx.linkIndex            = linkIndex;

    // Scan for frequency
    do
    {
        // Because we don't know the size, extract one entry at a time
        status = vbiosIedFetch(&ctx, &tableData0);
        if (status != FLCN_OK)
        {
            return status;
        }
        if (dataSizeByte > 1)
        {
            status = vbiosIedFetch(&ctx, &tableData1);
            if (status != FLCN_OK)
            {
                return status;
            }
        }
        tableData = tableData0 | (((LwU16)tableData1) << 8);

        if (((comparison == IED_TABLE_COMPARISON_EQ) && (data == tableData)) ||
            ((comparison == IED_TABLE_COMPARISON_GE) && (data >= tableData)))
        {
            status = vbiosIedFetch(&ctx, &scriptOffset0);
            if (status != FLCN_OK)
            {
                return status;
            }
            status = vbiosIedFetch(&ctx, &scriptOffset1);
            if (status != FLCN_OK)
            {
                return status;
            }
            bFound = LW_TRUE;
        }
        else if ((entries > 0) && (tableData >= lastData))
        {
            // We didn't find a match yet, we saw at least two values,
            // and the frequencies are not decreasing
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
        else
        {
            lastData = tableData;
            entries++;
            ctx.nOffsetLwrrent += 2;
        }
    } while (!bFound);

    ctx.nOffsetLwrrent = scriptOffset0 | (((LwU16)scriptOffset1) << 8);

    return vbiosIedExelwteScriptSub(&ctx);
}


/*!
 * @brief      Execute VBIOS script
 *
 * @param[in]  pCtx  devinit engine context
 *
 * @return     FLCN_OK if operation succeeds
 */
FLCN_STATUS
vbiosIedExelwteScriptSub
(
    VBIOS_IED_CONTEXT *pCtx
)
{
    INIT_OPCODE_HDR hdr;
    LwU32           i;
    FLCN_STATUS     status = FLCN_OK;

    const VBIOS_IED_OPCODE_MAP0 vbiosIedOpcodeMapTable0[] =
    {
        INIT_OPCODES(OPCODE_MAP0)
    };

    const VBIOS_IED_OPCODE_MAP1 vbiosIedOpcodeMapTable1[] =
    {
        INIT_OPCODES(OPCODE_MAP1)
    };

    const FmtStruct fmtTable =
    {
        FMT_STRINGS(FMT_INIT)
    };

    while (!pCtx->bCompleted)
    {
        status = vbiosIedFetch(pCtx, &pCtx->nOpcode);
        if (status != FLCN_OK)
        {
            return status;
        }

        for (i = 0; i < LW_ARRAY_ELEMENTS(vbiosIedOpcodeMapTable0); i++)
        {
            if (vbiosIedOpcodeMapTable0[i].opcode == pCtx->nOpcode)
            {
                status = s_vbiosIedFetchUnpack(pCtx, (LwU32*)&hdr, sizeof(hdr),
                    (char*)&fmtTable + vbiosIedOpcodeMapTable0[i].hdrFmt);
                if (status != FLCN_OK)
                {
                    return status;
                }

                status = vbiosIedOpcodeMapTable1[i].pFunc(pCtx, &hdr);
                if (status != FLCN_OK)
                {
                    return status;
                }
                break;
            }
        }
        if (i == LW_ARRAY_ELEMENTS(vbiosIedOpcodeMapTable0))
        {
            status = FLCN_ERR_ILWALID_STATE;
            return status;
            //
            // If end up here, it means the opcode is either wrong, or
            // it is a valid opcode and we need to implement it.
            // If the  unknown opcode is garbage, see what is the previous
            // opcode being parsed: its function might have messed up with the
            // instruction pointer.
            //
        }
    }

    return status;
}


/*!
 * @brief       Fetch byte from VBIOS script
 *
 * @param[in]   pCtx     devinit engine context
 * @param[out]  pData    data
 *
 * @return     FLCN_OK if operation succeeds
 */
FLCN_STATUS
vbiosIedFetch
(
    VBIOS_IED_CONTEXT *pCtx,
    LwU8              *pData
)
{
    FLCN_STATUS status;
    LwU32  data32;
    LwU32  alignedOffset;
    LwU32  shift;

    // Offset and size need to be 32-bit aligned - 4 bytes.
    alignedOffset = LW_ALIGN_DOWN(pCtx->nOffsetLwrrent, 4);
    shift = (pCtx->nOffsetLwrrent - alignedOffset) * 8;
    status = dmaRead(&data32, pCtx->pVbiosDesc, alignedOffset, sizeof(data32));
    if (status != FLCN_OK)
    {
        return status;
    }

    pCtx->nOffsetLwrrent++;

    *pData = (LwU8)((data32 >> shift) & 0xFF);
    return FLCN_OK;
}


/*!
 * @brief       check an IED script condition
 *
 * @param[in]   pCtx             devinit engine context
 * @param[in]   nCondition       condition index
 * @param[out]  pResult          result of the condition, LW_TRUE or LW_FALSE
 *
 * @return      FLCN_OK if we could check the condition
 */
FLCN_STATUS
vbiosIedCheckCondition
(
    VBIOS_IED_CONTEXT *pCtx,
    LwU8               nCondition,
    LwBool            *pResult
)
{
    VBIOS_IED_CONDITION_ENTRY  conditionEntry = { 0 };
    FLCN_STATUS                status;

    status = s_vbiosGetDataTableEntry(
        pCtx,
        nCondition,
        VBIOS_IED_CONDITION_ENTRY_SIZE,
        TO_STR(VBIOS_IED_CONDITION_ENTRY_FMT),
        (LwU32*)&conditionEntry,
        sizeof(VBIOS_IED_CONDITION_ENTRY)
    );
    if (status != FLCN_OK)
    {
        return status;
    }
    *pResult = s_vbiosRegConditionCheck(conditionEntry.condAddress,
                                       conditionEntry.condMask,
                                       conditionEntry.condCompare);
    return status;
}

/*!
 * @brief       Fetch block of data from VBIOS script.
 *
 * @param[in]   pCtx             devinit engine context
 * @param[out]  pData            buffer returned to caller
 * @param[in]   maxUnpackedSize  size of the buffer
 *
 * @return      FLCN_OK
 *      If operation succeeds.
 * @return      FLCN_ERR_ILWALID_STATE
 *      If requested data fetch exceeds max size.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
vbiosIedFetchBlock
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData,
    LwU32              maxUnpackedSize
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       offsetAligned;
    LwU32       sizeAligned;
    LwU8        pData8[VBIOS_MAX_BLOCK_SIZE_ALIGNED_BYTE];

    // We need to align down the offset -4 bytes- and align up the size - 4 bytes.
    offsetAligned = LW_ALIGN_DOWN(pCtx->nOffsetLwrrent, 4);
    sizeAligned = LW_ALIGN_UP(pCtx->nOffsetLwrrent - offsetAligned + maxUnpackedSize, 4);

    // Make sure that pData8[] is enough.
    if (sizeAligned > VBIOS_MAX_BLOCK_SIZE_ALIGNED_BYTE)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto vbiosIedFetchBlock_exit;
    }

    status = dmaRead(pData8, pCtx->pVbiosDesc, offsetAligned, sizeAligned);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto vbiosIedFetchBlock_exit;
    }

    memcpy(pData, pData8 + pCtx->nOffsetLwrrent - offsetAligned, maxUnpackedSize);

    pCtx->nOffsetLwrrent += maxUnpackedSize;

vbiosIedFetchBlock_exit:
    return status;
}

/* --------------------- Private functions ----------------------- */

/*!
 * @brief       Fetch complete command header from VBIOS script
 *
 * @param[in]   pCtx             devinit engine context
 * @param[out]  pData            buffer returned to caller
 * @param[in]   maxUnpackedSize  size of the buffer
 * @param[in]   format           header format

 * @return      FLCN_OK if operation succeeds
 */
static FLCN_STATUS
s_vbiosIedFetchUnpack
(
    VBIOS_IED_CONTEXT *pCtx,
    LwU32             *pData,
    LwU32              maxUnpackedSize,
    const char        *format
)
{
    LwU32       offsetAligned;
    LwU32       sizeAligned;
    LwU32       unpackedSize = 0;
    FLCN_STATUS status = FLCN_OK;
    LwU8        pVbiosPtr[VBIOS_MAX_HEADER_SIZE_ALIGNED_BYTE];

    // We need to align down the offset -4 bytes- and align up the size - 4 bytes
    offsetAligned = LW_ALIGN_DOWN(pCtx->nOffsetLwrrent, 4);
    sizeAligned = LW_ALIGN_UP(pCtx->nOffsetLwrrent - offsetAligned + maxUnpackedSize, 4);

    // Make sure that pVbiosPtr[] is enough
    if (sizeAligned > VBIOS_MAX_HEADER_SIZE_ALIGNED_BYTE)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    status = dmaRead(pVbiosPtr, pCtx->pVbiosDesc, offsetAligned, sizeAligned);
    if (status != FLCN_OK)
    {
        return status;
    }

    status = s_vbiosParseUnpackStructure(format, pVbiosPtr + pCtx->nOffsetLwrrent - offsetAligned, pData, maxUnpackedSize, &unpackedSize);

    pCtx->nOffsetLwrrent += unpackedSize;

    return status;
}


/*!
 * @brief       Interpret command header from VBIOS script
 *
 * @param[in]   format           header format
 * @param[in]   pPackedData      packed data
 * @param[out]  pUnpackedData    unpacked data
 * @param[in]   maxUnpackedSize  size of the unpacked data buffer
 * @param[out]  pUnpackedSize    unpacked data size

 * @return      FLCN_OK if operation succeeds
 */
static FLCN_STATUS
s_vbiosParseUnpackStructure
(
    const char *format,
    LwU8       *pPackedData,
    LwU32      *pUnpackedData,
    LwU32       maxUnpackedSize,
    LwU32      *pUnpackedSize
)
{
    LwU8  *pPackedDataOrg = pPackedData;
    LwU32  count;
    LwU8   size;
    char   fmt;
    LwBool bCopy = ((pPackedData != NULL) && (pUnpackedData != NULL));
    LwU32 *pUnpackedDataEnd = NULL;
    LwU32  numUnpackedBytes = 0;

    if (pUnpackedData != NULL)
    {
        pUnpackedDataEnd = (LwU32*)((LwU8*)pUnpackedData + maxUnpackedSize);
    }

    while ((fmt = *format++))
    {
        count = 0;
        while ((fmt >= '0') && (fmt <= '9'))
        {
            count *= 10;
            count += fmt - '0';
            fmt = *format++;
        }
        if (count == 0)
        {
            count = 1;
        }

        size = 0;
        switch (fmt)
        {
            case 'b':
            case 's':
                size = 1;
                break;
            case 'w':
                size = 2;
                break;
            case 'd':
                size = 4;
                break;
            case 'q':
                size = 8;
                break;
            default:
                return LW_FALSE;
        }

        // return if unpacking the packed data will ever go beyond maxUnpackedSize
        if (bCopy)
        {
            if (maxUnpackedSize % 4)
            {
                return FLCN_ERR_ILWALID_ARGUMENT;
            }

            numUnpackedBytes = count * ((size + (sizeof(LwU32) - 1)) & (~(sizeof(LwU32) - 1)));

            if ((LwU32*)((LwU8*)pUnpackedData + numUnpackedBytes) > pUnpackedDataEnd)
            {
                return FLCN_ERR_ILWALID_ARGUMENT;
            }
        }

        while (count--)
        {
            if (bCopy)
            {
                // This assignment requires that pUnpackedData be dword aligned
                *pUnpackedData = 0;
                memcpy((LwU8*)pUnpackedData, pPackedData, size);

                switch (fmt)
                {
                    case 's':
                    {
                        //
                        // Move the signed byte into a temporary variable before
                        // sign extending to prevent a OVERLAPPING_COPY issue
                        // in Coverity.
                        //
                        LwS8 s8 = *((LwS8*)pUnpackedData);
                        *((LwS32*)pUnpackedData) = (LwS32)s8;
                        break;
                    }
                    case 'q':
                        pUnpackedData++;    // Extra field for 64-bit
                        break;
                }

                pUnpackedData++;
            }

            pPackedData += size;
        }
    }

    if (pUnpackedSize != NULL)
    {
        *pUnpackedSize = pPackedData - pPackedDataOrg;
    }

    return FLCN_OK;
}


/*!
 * @brief       get table data from VBIOS script
 *
 * @param[in]   pCtx             devinit engine context
 *  @param[in]   entryIdx         entry index
 * @param[in]   objectSize       object size
 * @param[in]   objectFormat     object format
 * @param[out]  pObject          object
 * @param[in]   maxUnpackedSize  maximum object size
 *
 * @return      FLCN_OK if operation succeeds
 */
static FLCN_STATUS
s_vbiosGetDataTableEntry
(
    VBIOS_IED_CONTEXT *pCtx,
    LwU8               entryIdx,
    LwU8               objectSize,
    const char        *objectFormat,
    LwU32             *pObject,
    LwU32              maxUnpackedSize

)
{
    LwU32       offsetAligned, offset;
    LwU32       sizeAligned;
    FLCN_STATUS status = FLCN_OK;
    LwU8        dataTableDmemPtr[VBIOS_MAX_HEADER_SIZE_ALIGNED_BYTE];

    offset = pCtx->conditionTableOffset + entryIdx * objectSize;
    // We need to align down the offset -4 bytes- and align up the size - 4 bytes
    offsetAligned = LW_ALIGN_DOWN(offset, 4);
    sizeAligned = LW_ALIGN_UP(offset - offsetAligned + maxUnpackedSize, 4);

    // Make sure that pVbiosPtr[] is enough
    if (sizeAligned > VBIOS_MAX_HEADER_SIZE_ALIGNED_BYTE)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    status = dmaRead(dataTableDmemPtr, pCtx->pVbiosDesc, offset - offsetAligned, sizeAligned);
    if (status != FLCN_OK)
    {
        return status;
    }

    status = s_vbiosParseUnpackStructure(objectFormat, dataTableDmemPtr + offset - offsetAligned, pObject, maxUnpackedSize, NULL);

    return status;
}

/*!
 * @brief       check a register value for condition
 *
 * @param[in]   addr             register address
 * @param[in]   mask             register mask
 * @param[in]   compare          the value to be compared
 *
 * @return      LW_TRUE the register value equals compare, LW_FALSE otherwise
 */
static LwBool
s_vbiosRegConditionCheck(LwU32 addr, LwU32 mask, LwU32 compare)
{
    LwU32 val;

    val = REG_RD32(BAR0, addr);
    val &= mask;
    return (val == compare);
}
