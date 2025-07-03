/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/script.h"
#include "gpu/include/pcicfg.h"

// Re-evaluate if this becomes different in future chips
#include "maxwell/gm107/dev_lw_pcfg_xve_addendum.h"
#include "maxwell/gm107/dev_lw_pcfg_xve1_addendum.h"
//------------------------------------------------------------------------------
/*static*/
// For GPU devices
UINT32 PciCfgSpace::s_XveRegWriteMap[] = LW_PCFG_XVE_REGISTER_WR_MAP;

// For Azailia devices
UINT32 PciCfgSpace::s_Xve1RegWriteMap[] = LW_PCFG_XVE1_REGISTER_WR_MAP;
//------------------------------------------------------------------------------
/*static*/
bool PciCfgSpace::PollCfgSpaceReady(void* param)
{
    PciCfgSpace* const pCfgSpace = static_cast<PciCfgSpace*>(param);
    UINT32 ReadData = ~0U;
    Platform::PciRead32(
            pCfgSpace->PciDomainNumber(),
            pCfgSpace->PciBusNumber(),
            pCfgSpace->PciDeviceNumber(),
            pCfgSpace->PciFunctionNumber(),
            0,
            &ReadData);
    if (pCfgSpace->IsSaved())
    {
        return (ReadData & 0xFFFF) == pCfgSpace->SavedVendorId();
    }
    return ReadData != ~0U;
}

//------------------------------------------------------------------------------
/*static*/
bool PciCfgSpace::PollCfgSpaceNotReady(void* param)
{
    return ! PollCfgSpaceReady(param);
}

//------------------------------------------------------------------------------
RC PciCfgSpace::Initialize(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{
    RC rc;
    m_Domain = domain;
    m_Bus = bus;
    m_Device = device;
    m_Function = function;
    CHECK_RC(UseXveRegMap(function));
    m_Initialized = true;
    return OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpace::UseXveRegMap(UINT32 xveMap)
{
    MASSERT((xveMap == 0) || (xveMap == 1));
    if (xveMap == static_cast<UINT32>(FUNC_GPU)) // GPU's map
    {
        m_XveRegMap = PciCfgSpace::s_XveRegWriteMap;
        m_XveRegMapLength = sizeof(s_XveRegWriteMap)/sizeof(s_XveRegWriteMap[0]);
    }
    else if(xveMap == static_cast<UINT32>(FUNC_AZALIA)) // Azalia's map
    {
        m_XveRegMap = PciCfgSpace::s_Xve1RegWriteMap;
        m_XveRegMapLength = sizeof(s_Xve1RegWriteMap)/sizeof(s_Xve1RegWriteMap[0]);
    }
    else
    {
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}
//------------------------------------------------------------------------------
/* virtual */ RC PciCfgSpace::Save()
{
    RC rc;
    if (IsSaved() || !m_Initialized)
    {
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function, 0, &m_SavedVendorId));
    
    for (UINT32 mapIndex = 0; mapIndex < m_XveRegMapLength; mapIndex++)
    {
        for (UINT32 maskIndex = 0; maskIndex < sizeof(UINT32) * 8; maskIndex++)
        {
            UINT32 mask = 1 << maskIndex;

            if ((m_XveRegMap[mapIndex] & mask) == 0)
            {
                continue;
            }

            UINT32 regIndex = (mapIndex * sizeof(UINT32) * 8) + maskIndex;
            CHECK_RC(SaveEntry(regIndex * sizeof(UINT32)));
        }
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Restore the registers that were saved by Save()
//!
RC PciCfgSpace::Restore()
{
    RC rc;

    for (vector<SavedEntry>::iterator pEntry = m_SavedPciCfgSpace.begin();
         pEntry != m_SavedPciCfgSpace.end(); ++pEntry)
    {
        // Skip restoring specified registers
        if (m_SkipRestoreRegs.count(pEntry->reg))
        {
            continue;
        }

        // Either write the register or do a read-modify-write,
        // depending on whether we're restoring all of the bits or
        // just some of them.
        if (pEntry->mask == 0xffffffff)
        {
            CHECK_RC(Platform::PciWrite32(m_Domain, m_Bus, m_Device, m_Function,
                                          pEntry->reg, pEntry->value));
        }
        else
        {
            UINT32 value = 0;
            CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                         pEntry->reg, &value));
            value = (value & ~pEntry->mask) | (pEntry->value & pEntry->mask);
            CHECK_RC(Platform::PciWrite32(m_Domain, m_Bus, m_Device, m_Function,
                                          pEntry->reg, value));
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Called by Save() to save one register in the PCIe config space.
//!
RC PciCfgSpace::SaveEntry(UINT32 reg, UINT32 mask)
{
    RC rc;
    SavedEntry newEntry;

    newEntry.reg = reg;
    CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                 reg, &newEntry.value));
    newEntry.mask = mask;
    m_SavedPciCfgSpace.push_back(newEntry);
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpace::CheckCfgSpaceReady(FLOAT64 TimeoutMs)
{
    RC rc;

    rc = POLLWRAP_HW(PollCfgSpaceReady, this,TimeoutMs);

    return rc;

}

//------------------------------------------------------------------------------
RC PciCfgSpace::CheckCfgSpaceNotReady(FLOAT64 TimeoutMs)
{
    RC rc;

    rc = POLLWRAP_HW(PollCfgSpaceNotReady, this,TimeoutMs);

    return rc;

}

//------------------------------------------------------------------------------
// JS Interface
static JSBool C_PciCfgSpace_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    const char usage[] =
        "Usage: PciCfgSpace(domain, bus, device, function)\n";

    JavaScriptPtr pJs;
    UINT32  domain;
    UINT32  bus;
    UINT32  device;
    UINT32  function;

    if ((argc != 4) ||
        (OK != pJs->FromJsval(argv[0], &domain)) ||
        (OK != pJs->FromJsval(argv[1], &bus)) ||
        (OK != pJs->FromJsval(argv[2], &device)) ||
        (OK != pJs->FromJsval(argv[3], &function)))
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    // Create the private data
    //
    PciCfgSpace *pPciCfgSpace = new PciCfgSpace();
    MASSERT(pPciCfgSpace);
    pPciCfgSpace->Initialize(domain,bus,device,function);

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);

    return JS_SetPrivate(cx, obj, pPciCfgSpace);
};

//-----------------------------------------------------------------------------
static void C_PciCfgSpace_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    //! If a PciCfgSpace was associated with this object, make sure to delete it
    PciCfgSpace * pPciCfgSpace = (PciCfgSpace *)JS_GetPrivate(cx, obj);
    if (pPciCfgSpace)
    {
        delete pPciCfgSpace;
    }
};

//-----------------------------------------------------------------------------
static JSClass PciCfgSpace_class =
{
    "PciCfgSpace",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_PciCfgSpace_finalize
};

//-----------------------------------------------------------------------------
static SObject PciCfgSpace_Object
(
    "PciCfgSpace",
    PciCfgSpace_class,
    0,
    0,
    "PciCfgSpace JS Object",
    C_PciCfgSpace_constructor
);

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(PciCfgSpace,
                Save,
                0,
                "Save the PciCfgSpace data")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext,
                "Invalid arguments."
                " Usage: PciCfgSpace.Save().");
        return JS_FALSE;
    }

    //Get the PciCfgSpace and initialize it.
    PciCfgSpace *pPciCfgSpace = JS_GET_PRIVATE(PciCfgSpace, pContext,pObject, "PciCfgSpace");
    if (pPciCfgSpace != nullptr)
    {
        RETURN_RC(pPciCfgSpace->Save());
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(PciCfgSpace,
                Restore,
                0,
                "Restore the PciCfgSpace data")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext,
                "Invalid arguments."
                " Usage: PciCfgSpace.Restore().");
        return JS_FALSE;
    }

    //Get the PciCfgSpace and initialize it.
    PciCfgSpace *pPciCfgSpace = JS_GET_PRIVATE(PciCfgSpace, pContext,pObject, "PciCfgSpace");
    if (pPciCfgSpace != nullptr)
    {
        RETURN_RC(pPciCfgSpace->Restore());
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(PciCfgSpace,
                CheckCfgSpaceReady,
                1,
                "Check to see if the PciCfgSpace is valid")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    FLOAT32 timeout;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &timeout)))
    {
        JS_ReportError(pContext,
                "Invalid arguments."
                " Usage: PciCfgSpace.CheckCfgSpaceReady(timeoutMs).");
        return JS_FALSE;
    }

    //Get the PciCfgSpace and initialize it.
    PciCfgSpace *pPciCfgSpace = JS_GET_PRIVATE(PciCfgSpace, pContext,pObject, "PciCfgSpace");
    if (pPciCfgSpace != nullptr)
    {
        RETURN_RC(pPciCfgSpace->CheckCfgSpaceReady((FLOAT64)timeout));
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(PciCfgSpace,
                CheckCfgSpaceNotReady,
                1,
                "Check to see if the PciCfgSpace is valid")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    FLOAT32 timeout;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &timeout)))
    {
        JS_ReportError(pContext,
                "Invalid arguments."
                " Usage: PciCfgSpace.CheckCfgSpaceNotReady(timeoutMs).");
        return JS_FALSE;
    }

    //Get the PciCfgSpace and initialize it.
    PciCfgSpace *pPciCfgSpace = JS_GET_PRIVATE(PciCfgSpace, pContext,pObject, "PciCfgSpace");
    if (pPciCfgSpace != nullptr)
    {
        RETURN_RC(pPciCfgSpace->CheckCfgSpaceNotReady((FLOAT64)timeout));
    }
    return JS_FALSE;
}

