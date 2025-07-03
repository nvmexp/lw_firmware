/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2010,2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_fb.h"
#include "core/include/framebuf.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/xp.h"
#include "core/include/rc.h"

JsFrameBuffer::JsFrameBuffer()
    : m_pFrameBuffer(NULL), m_pJsFrameBufferObj(NULL)
{
}

void JsFrameBuffer::SetFrameBuffer(FrameBuffer * pFb)
{
    m_pFrameBuffer = pFb;
}

//-----------------------------------------------------------------------------
static void C_JsFrameBuffer_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsFrameBuffer * pJsFrameBuffer;

    //! If a JsFrameBuffer was associated with this object, make
    //! sure to delete it
    pJsFrameBuffer = (JsFrameBuffer *)JS_GetPrivate(cx, obj);
    if (pJsFrameBuffer)
    {
        delete pJsFrameBuffer;
    }
};

//-----------------------------------------------------------------------------
static JSClass JsFrameBuffer_class =
{
    "JsFrameBuffer",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsFrameBuffer_finalize
};

//-----------------------------------------------------------------------------
static SObject JsFrameBuffer_Object
(
    "JsFrameBuffer",
    JsFrameBuffer_class,
    0,
    0,
    "FrameBuffer JS Object"
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! FrameBuffer.
//!
//! \note The JSOBjects created will be freed by the JS GC.
//! \note The JSObject is prototyped from JsFrameBuffer_Object
//! \sa SetFrameBuffer
RC JsFrameBuffer::CreateJSObject(JSContext *cx, JSObject *obj)
{

    //! Only create one JSObject per FrameBuffer
    if (m_pJsFrameBufferObj)
    {
        Printf(Tee::PriLow,
            "A JS Object has already been created for this JsFrameBuffer.\n");
        return OK;
    }

    m_pJsFrameBufferObj = JS_DefineObject(cx,
                                          obj, // GpuSubdevice object
                                          "FrameBuffer", // Property name
                                          &JsFrameBuffer_class,
                                          JsFrameBuffer_Object.GetJSObject(),
                                          JSPROP_READONLY);

    if (!m_pJsFrameBufferObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsFrameBuffer instance into the private area
    //! of the new JSOBject.  This will tie the two together.
    if (JS_SetPrivate(cx, m_pJsFrameBufferObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsFrameBuffer.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsFrameBufferObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the associated FrameBuffer.
//!
//! Return the FrameBuffer that is being wrapped by this JsFrameBuffer.  We
//! should never have a NULL pointer to a FrameBuffer when calling this
//! function.
FrameBuffer * JsFrameBuffer::GetFrameBuffer()
{
    MASSERT(m_pFrameBuffer);

    return m_pFrameBuffer;
}

string JsFrameBuffer::GetName()
{
    return GetFrameBuffer()->GetName();
}

UINT64 JsFrameBuffer::GetGraphicsRamAmount()
{
    return GetFrameBuffer()->GetGraphicsRamAmount();
}

UINT32 JsFrameBuffer::GetL2SliceCount()
{
    return GetFrameBuffer()->GetL2SliceCount();
}

string JsFrameBuffer::PartitionNumberToLetter(UINT32 virtualFbio)
{
    FrameBuffer *pFb = GetFrameBuffer();
    return string(1, pFb->VirtualFbioToLetter(virtualFbio));
}

UINT32 JsFrameBuffer::GetPartitions()
{
    return GetFrameBuffer()->GetFbioCount();
}

UINT32 JsFrameBuffer::GetFbpCount()
{
    return GetFrameBuffer()->GetFbpCount();
}

UINT32 JsFrameBuffer::GetLtcCount()
{
    return GetFrameBuffer()->GetLtcCount();
}

UINT32 JsFrameBuffer::GetL2Caches()
{
    return GetFrameBuffer()->GetLtcCount();
}

UINT32 JsFrameBuffer::GetL2CacheSize()
{
    return GetFrameBuffer()->GetL2CacheSize();
}

UINT32 JsFrameBuffer::GetMaxL2SlicesPerFbp()
{
    return GetFrameBuffer()->GetMaxL2SlicesPerFbp();
}

bool JsFrameBuffer::IsL2SliceValid(UINT32 slice, UINT32 virtualFbp)
{
    return GetFrameBuffer()->IsL2SliceValid(slice, virtualFbp);
}

bool JsFrameBuffer::GetIsEccOn()
{
    return GetFrameBuffer()->IsEccOn();
}

bool JsFrameBuffer::GetIsRowRemappingOn()
{
    return GetFrameBuffer()->IsRowRemappingOn();
}

UINT32 JsFrameBuffer::GetRamProtocol()
{
    return GetFrameBuffer()->GetRamProtocol();
}

string JsFrameBuffer::GetRamProtocolToString()
{
    return GetFrameBuffer()->GetRamProtocolString();
}

UINT32 JsFrameBuffer::GetSubpartitions()
{
    return GetFrameBuffer()->GetSubpartitions();
}

UINT32 JsFrameBuffer::GetChannelsPerFbio()
{
    return GetFrameBuffer()->GetChannelsPerFbio();
}

string JsFrameBuffer::GetVendorName()
{
    return GetFrameBuffer()->GetVendorName();
}

JS_STEST_LWSTOM(JsFrameBuffer,
                Encode,
                4,
                "Encode an RBC address to physical address")
{
    STEST_HEADER(4, 4,
        "Usage: FrameBuffer.Encode(rbcAddress, pageSizeKB, pteKind,"
        " [eccOnAddr, eccOffAddr])\n");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");
    STEST_ARG(0, JsArray,   jsIn);
    STEST_ARG(1, UINT32,    pageSizeKB);
    STEST_ARG(2, UINT32,    pteKind);
    STEST_ARG(3, JSObject*, pReturlwals);

    const FrameBuffer * pFb = pJsFrameBuffer->GetFrameBuffer();
    RC rc;

    FbDecode decode = {0};
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[0], &decode.rank));
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[1], &decode.bank));
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[2], &decode.row));
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[3], &decode.burst));
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[4], &decode.virtualFbio));
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[5], &decode.subpartition));
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[6], &decode.beat));
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[7], &decode.beatOffset));
    C_CHECK_RC(pJavaScript->FromJsval(jsIn[8], &decode.pseudoChannel));

    UINT64 eccOnAddr;
    UINT64 eccOffAddr;
    C_CHECK_RC(pFb->EncodeAddress(decode, pteKind, pageSizeKB,
                                  &eccOnAddr, &eccOffAddr));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, eccOnAddr));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 1, eccOffAddr));
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFrameBuffer,
                Decode,
                2,
                "Decode a framebuffer address into DRAM banks, rows & columns")
{
    STEST_HEADER(3, 4,
        "Usage: FrameBuffer.Decode(address, pageSizeKB, pteKind)\n"
        "or:    FrameBuffer.Decode(address, pageSizeKB, pteKind, [rank, bank,"
        " row, burst, virtualFbio, subpart, beat, beatOffset, pseudoChannel])");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");
    STEST_ARG(0, UINT64, address);
    STEST_ARG(1, UINT32, pageSizeKB);
    STEST_ARG(2, UINT32, pteKind);
    STEST_OPTIONAL_ARG(3, JSObject*, pReturlwals, nullptr);

    RC rc;
    const FrameBuffer *pFb = pJsFrameBuffer->GetFrameBuffer();

    FbDecode decode;
    C_CHECK_RC(pFb->DecodeAddress(&decode, address, pteKind, pageSizeKB));

    if (pReturlwals)
    {
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, decode.rank));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 1, decode.bank));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 2, decode.row));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 3, decode.burst));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 4, decode.virtualFbio));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 5,
                                           decode.subpartition));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 6, decode.beat));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 7, decode.beatOffset));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 8,
                                           decode.pseudoChannel));
    }
    else
    {
        Printf(Tee::PriNormal, "%s\n", pFb->GetDecodeString(decode).c_str());
    }

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFrameBuffer,
                BlacklistPage,
                6,
                "Blacklist a page in the InfoRom")
{
    STEST_HEADER(6, 6,
        "Usage: gpusub.FrameBuffer.BlacklistPage(eccPageLow, eccPageHigh,\n"
        "                                        nonEccPageLow, nonEccPageHigh,\n"
        "                                        blacklistSource, rbcAddress)\n");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");
    STEST_ARG(0, UINT32, eccPageLow);
    STEST_ARG(1, UINT32, eccPageHigh);
    STEST_ARG(2, UINT32, nonEccPageLow);
    STEST_ARG(3, UINT32, nonEccPageHigh);
    STEST_ARG(4, UINT32, blacklistSource);
    STEST_ARG(5, UINT32, rbcAddress);

    RC rc;
    const FrameBuffer *pFb = pJsFrameBuffer->GetFrameBuffer();

    UINT64 eccOnPage = (static_cast<UINT64>(eccPageHigh) << 32) | eccPageLow;
    UINT64 eccOffPage = (static_cast<UINT64>(nonEccPageHigh) << 32) | nonEccPageLow;

    C_CHECK_RC(pFb->BlacklistPage(eccOnPage,
                                  eccOffPage,
                                  static_cast<Memory::BlacklistSource>(blacklistSource),
                                  rbcAddress));

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFrameBuffer,
                RowRemap,
                4,
                "Remap a row in the InfoROM")
{
    STEST_HEADER(4, 4,
                 "Usage: gpusub.FrameBuffer.RowRemap(\n"
                 "              pageShiftedAddrLow, pageShiftedAddrHigh, rowOffset,\n"
                 "              cause)\n");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");
    STEST_ARG(0, UINT32, pageShiftedAddrLow);
    STEST_ARG(1, UINT32, pageShiftedAddrHigh);
    STEST_ARG(2, UINT32, rowOffset);
    STEST_ARG(3, UINT32, cause);

    using MemErrT = Memory::ErrorType;

    RC rc;
    const FrameBuffer* pFb = pJsFrameBuffer->GetFrameBuffer();
    const UINT64 pageShiftedAddr
        = static_cast<UINT64>(pageShiftedAddrHigh) << 32 | pageShiftedAddrLow;
    const UINT64 physicalAddr = (pageShiftedAddr << 12) | rowOffset;

    C_CHECK_RC(pFb->RemapRow(RowRemapper::Request(physicalAddr,
                                                  static_cast<MemErrT>(cause))));

    RETURN_RC(RC::OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFrameBuffer, CheckMaxRemappedRows, 2,
                "Check if the policy for the maximum number of remapped rows "
                "has been violated")
{
    STEST_HEADER(2, 2,
                 "Usage: JsFrameBuffer.CheckMaxRemappedRows(totalRows, numRowsPerBank)\n");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");
    STEST_ARG(0, UINT32, totalRows);
    STEST_ARG(1, UINT32, numRowsPerBank);

    RC rc;

    const FrameBuffer* pFb = pJsFrameBuffer->GetFrameBuffer();

    RowRemapper::MaxRemapsPolicy policy = {};
    policy.totalRows = totalRows;
    policy.numRowsPerBank = numRowsPerBank;

    RETURN_RC(pFb->CheckMaxRemappedRows(policy));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFrameBuffer,
                GetNumInfoRomRemappedRows,
                1,
                "Get the number of remapped rows in the InfoROM")
{
    STEST_HEADER(1, 1,
                 "Usage: gpusub.FrameBuffer.GetNumInfoRomRemappedRows([numRemaps])\n");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");
    STEST_ARG(0, JSObject*, pReturlwals);

    RC rc;
    const FrameBuffer* pFb = pJsFrameBuffer->GetFrameBuffer();

    UINT32 numRows = 0;
    C_CHECK_RC(pFb->GetNumInfoRomRemappedRows(&numRows));

    RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, numRows));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFrameBuffer,
                PrintInfoRomRemappedRows,
                0,
                "Print the details of all remapped rows in the InfoROM")
{
    STEST_HEADER(0, 0,
                 "Usage: FrameBuffer.PrintInfoRomRemappedRows()");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");

    const FrameBuffer* pFb = pJsFrameBuffer->GetFrameBuffer();
    RETURN_RC(pFb->PrintInfoRomRemappedRows());
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsFrameBuffer,
                ClearRemappedRows,
                2,
                "Clear remapped rows in the InfoROM")
{
    STEST_HEADER(2, 2,
                 "Usage: gpusub.FrameBuffer.ClearRemappedRows(\n"
                 "              sourceTypeStr, errorTypeStr)\n");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");
    STEST_ARG(0, string, sourceTypeStr);
    STEST_ARG(1, string, errorTypeStr);

    using RemapSource = RowRemapper::Source;
    using MemErrT = Memory::ErrorType;

    RC rc;
    const FrameBuffer* pFb = pJsFrameBuffer->GetFrameBuffer();
    const string source = Utility::ToLowerCase(sourceTypeStr);
    const string error = Utility::ToLowerCase(errorTypeStr);

    vector<RowRemapper::ClearSelection> selections;

    // Decode source
    //
    if (source == "all")
    {
        selections.emplace_back(RemapSource::FACTORY, MemErrT::NONE);
        selections.emplace_back(RemapSource::FIELD, MemErrT::NONE);
        selections.emplace_back(RemapSource::MODS, MemErrT::NONE);
    }
    else if (source == Utility::ToLowerCase(ToString(RemapSource::FACTORY)))
    {
        selections.emplace_back(RemapSource::FACTORY, MemErrT::NONE);
    }
    else if (source == Utility::ToLowerCase(ToString(RemapSource::FIELD)))
    {
        selections.emplace_back(RemapSource::FIELD, MemErrT::NONE);
    }
    else if (source == Utility::ToLowerCase(ToString(RemapSource::MODS)))
    {
        selections.emplace_back(RemapSource::MODS, MemErrT::NONE);
    }
    else
    {
        Printf(Tee::PriError, "Unknown row remapping source type: '%s'\n",
               sourceTypeStr.c_str());
        RETURN_RC(RC::BAD_PARAMETER);
    }

    // Decode error
    //
    if (error == "all")
    {
        // There is one entry per source in the selections. For each source:
        // - Populate `cause` field of the existing selection with the first
        //   error type.
        // - Add a new selection with the same source type for each additional
        //   error type.
        //
        // Note that all the sources will be in the first N elements of the
        // selections collection (populated from the previous step), where N is
        // the number of unique sources.

        const size_t numSources = selections.size();
        for (UINT32 i = 0; i < numSources; ++i)
        {
            RowRemapper::ClearSelection* pSel = &selections[i];
            pSel->cause = MemErrT::SBE;
            selections.emplace_back(pSel->source, MemErrT::DBE);
        }
    }
    else
    {
        // No need to make new selection entries. Update existing ones.

        MemErrT errorT = MemErrT::NONE;

        if (error == Utility::ToLowerCase(ToString(MemErrT::SBE)))
        {
            errorT = MemErrT::SBE;
        }
        else if (error == Utility::ToLowerCase(ToString(MemErrT::DBE)))
        {
            errorT = MemErrT::DBE;
        }
        else
        {
            Printf(Tee::PriError, "Unknown row remapping error type: '%s'\n",
                   errorTypeStr.c_str());
            RETURN_RC(RC::BAD_PARAMETER);
        }

        const size_t numSources = selections.size();
        for (UINT32 i = 0; i < numSources; ++i)
        {
            selections[i].cause = errorT;
        }
    }

    C_CHECK_RC(pFb->ClearRemappedRows(selections));
    RETURN_RC(RC::OK);
}

//-----------------------------------------------------------------------------
// FrameBuffer JS Properties
CLASS_PROP_READONLY(JsFrameBuffer, Name, string, "");
CLASS_PROP_READONLY(JsFrameBuffer, GraphicsRamAmount, UINT64,
                    "Physical framebuffer size");
CLASS_PROP_READONLY(JsFrameBuffer, Partitions, UINT32,
                    "Number of enabled FB units");
CLASS_PROP_READONLY(JsFrameBuffer, FbpCount, UINT32,
                    "Number of enabled FBP units");
CLASS_PROP_READONLY(JsFrameBuffer, LtcCount, UINT32,
                    "Number of enabled L2 caches");
CLASS_PROP_READONLY(JsFrameBuffer, L2Caches, UINT32,
                    "Number of enabled L2 partitions");
CLASS_PROP_READONLY(JsFrameBuffer, L2CacheSize, UINT32,
                    "Size of L2 cache in bytes");
CLASS_PROP_READONLY(JsFrameBuffer, MaxL2SlicesPerFbp, UINT32,
                    "Max Number of L2 slices per FBP");
CLASS_PROP_READONLY(JsFrameBuffer, Subpartitions, UINT32,
                    "Number of subpartitions per FBIO");
CLASS_PROP_READONLY(JsFrameBuffer, ChannelsPerFbio, UINT32,
                    "Number of channels per FBIO");
CLASS_PROP_READONLY(JsFrameBuffer, RamProtocol, UINT32,
                    "Physical Ram Protocol");
CLASS_PROP_READONLY(JsFrameBuffer, RamProtocolToString, string,
                    "Physical Ram Protocol");
CLASS_PROP_READONLY(JsFrameBuffer, L2SliceCount, UINT32,
                    "Number of L2 Slices on chip");
CLASS_PROP_READONLY(JsFrameBuffer, VendorName, string,
                    "Memory vendor name");
CLASS_PROP_READONLY(JsFrameBuffer, IsEccOn, bool,
                    "True if ECC is on");
CLASS_PROP_READONLY(JsFrameBuffer, IsRowRemappingOn, bool,
                    "True if row remapping is on");

JS_SMETHOD_BIND_ONE_ARG(JsFrameBuffer, PartitionNumberToLetter,
        virtualFbio, UINT32,
        "Get the FBIO letter (as string) for this virtual FBIO");

JS_SMETHOD_LWSTOM(JsFrameBuffer, IsL2SliceValid, 2,
                  "return true if the specificed slice is valid for the specified Fbp")
{
    STEST_HEADER(2, 2, "Usage: GpuSubdevice.FrameBuffer.IsL2SliceValid(slice, fbp)");
    STEST_PRIVATE(JsFrameBuffer, pJsFrameBuffer, "JsFrameBuffer");
    STEST_ARG(0, UINT32, slice);
    STEST_ARG(1, UINT32, fbp);

    if (pJavaScript->ToJsval(pJsFrameBuffer->IsL2SliceValid(slice, fbp), pReturlwalue) != OK)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_CLASS(FrameBufferConst);
static SObject FrameBufferConst_Object
(
    "FrameBufferConst",
    FrameBufferConstClass,
    0,
    0,
    "FrameBufferConst JS Object"
);

PROP_CONST(FrameBufferConst, RmMaxBlacklistPages,
           LW2080_CTRL_FB_OFFLINED_PAGES_MAX_PAGES);
PROP_CONST(FrameBufferConst, RAM_UNKNOWN, FrameBuffer::RamUnknown);
PROP_CONST(FrameBufferConst, RAM_SDRAM,   FrameBuffer::RamSdram  );
PROP_CONST(FrameBufferConst, RAM_DDR1,    FrameBuffer::RamDDR1   );
PROP_CONST(FrameBufferConst, RAM_DDR2,    FrameBuffer::RamDDR2   );
PROP_CONST(FrameBufferConst, RAM_DDR3,    FrameBuffer::RamDDR3   );
PROP_CONST(FrameBufferConst, RAM_DDR4,    FrameBuffer::RamSDDR4  );
PROP_CONST(FrameBufferConst, RAM_GDDR2,   FrameBuffer::RamGDDR2  );
PROP_CONST(FrameBufferConst, RAM_GDDR3,   FrameBuffer::RamGDDR3  );
PROP_CONST(FrameBufferConst, RAM_GDDR4,   FrameBuffer::RamGDDR4  );
PROP_CONST(FrameBufferConst, RAM_GDDR5,   FrameBuffer::RamGDDR5  );
PROP_CONST(FrameBufferConst, RAM_GDDR5X,  FrameBuffer::RamGDDR5X );
PROP_CONST(FrameBufferConst, RAM_GDDR6,   FrameBuffer::RamGDDR6  );
PROP_CONST(FrameBufferConst, RAM_GDDR6X,  FrameBuffer::RamGDDR6X );
PROP_CONST(FrameBufferConst, RAM_LPDDR2,  FrameBuffer::RamLPDDR2 );
PROP_CONST(FrameBufferConst, RAM_LPDDR3,  FrameBuffer::RamLPDDR3 );
PROP_CONST(FrameBufferConst, RAM_LPDDR4,  FrameBuffer::RamLPDDR4 );
PROP_CONST(FrameBufferConst, RAM_LPDDR5,  FrameBuffer::RamLPDDR5 );
PROP_CONST(FrameBufferConst, RAM_HBM1,    FrameBuffer::RamHBM1   );
PROP_CONST(FrameBufferConst, RAM_HBM2,    FrameBuffer::RamHBM2   );
PROP_CONST(FrameBufferConst, RAM_HBM3,    FrameBuffer::RamHBM3   );
