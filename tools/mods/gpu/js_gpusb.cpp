/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_gpusb.h"

#include "class/cl90e7.h" // GF100_SUBDEVICE_INFOROM
#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/deprecat.h"
#include "core/include/fileholder.h"
#include "core/include/js_uint64.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/mgrmgr.h"
#include "core/include/modsdrv.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/registry.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "core/include/xp.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl90e7.h"
#include "device/interface/gpio.h"
#include "device/interface/js/js_gpio.h"
#include "device/interface/pcie.h"
#include "gpu/framebuf/js_fb.h"
#include "gpu/fuse/js_fuse.h"
#include "gpu/include/boarddef.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpuism.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpupm.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/hbmimpl.h"
#include "gpu/perf/js_avfs.h"
#include "gpu/perf/js_perf.h"
#include "gpu/perf/js_pmu.h"
#include "gpu/perf/js_power.h"
#include "gpu/perf/js_therm.h"
#include "gpu/perf/js_volt.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perfmon/js_perfmon.h"
#include "gpu/reghal/js_reghal.h"
#include "gpu/repair/row_remapper/row_remapper.h"
#include "gpu/tempctrl/tempctrl.h"
#include "gpu/utility/ecccount.h"
#include "gpu/utility/edccount.h"
#include "gpu/utility/hbmdev.h"
#include "gpu/utility/js_testdevice.h"
#include "gpu/utility/pcie/pexdev.h"
#include "js_gpudv.h"
#include "jsapi.h"

#include <set>
#include <algorithm>

namespace
{
    set<JsGpuSubdevice *> s_JsGpuSubdevices;
};

// Named properties
map<string, pair<JSPropertyOp, JSPropertyOp> > JsGpuSubdevice::s_NamedProperties;

// Invalid Get/Set property functions for use with named properties that are
// not implemented in a particular GPU
P_(JsGpuSubdevice_Get_IlwalidProp);
P_(JsGpuSubdevice_Set_IlwalidProp);

JS_CLASS(Dummy);
//------------------------------------------------------------------------------
//! \brief Named property class that automatically registers the property and
//!        its associated get/set function at runtime
class JsGpusubdeviceNamedProperty
{
public:
    JsGpusubdeviceNamedProperty(const char *Name,
                                JSPropertyOp GetMethod,
                                JSPropertyOp SetMethod);
};
JsGpusubdeviceNamedProperty::JsGpusubdeviceNamedProperty
(
    const char *Name,
    JSPropertyOp GetMethod,
    JSPropertyOp SetMethod
)
{
    JsGpuSubdevice::RegisterNamedProperty(Name, GetMethod, SetMethod);
}

JsGpuSubdevice::JsGpuSubdevice()
    : m_pGpuSubdevice(nullptr)
     ,m_pJsGpuSubdevObj(nullptr)
     ,m_pJsFb(nullptr)
     ,m_pJsTherm(nullptr)
     ,m_pJsPerf(nullptr)
     ,m_pJsRegHal(nullptr)
     ,m_pJsPower(nullptr)
     ,m_pJsGpio(nullptr)
     ,m_pJsPerfmon(nullptr)
     ,m_pJsPmu(nullptr)
#if defined(INCLUDE_DGPU)
     ,m_pJsFuse(nullptr)
     ,m_pJsAvfs(nullptr)
     ,m_pJsVolt3x(nullptr)
#endif
{
    s_JsGpuSubdevices.insert(this);
}

JsGpuSubdevice::~JsGpuSubdevice()
{
    s_JsGpuSubdevices.erase(this);
}

void JsGpuSubdevice::SetGpuSubdevice(GpuSubdevice * pSubdev)
{
    m_pGpuSubdevice = pSubdev;
}

//-----------------------------------------------------------------------------
//! \brief Constructor for a GpuSubdevice object.
//!
//! This ctor takes in two arguments that determine which Gpu subdevice
//! the user wants access to.  It then creates a JsGuSubdevice wrapper
//! and associates it with the given GpuSubdevice.
//!
//! \note This function is only called from JS when a GpuSubdevice object
//! is created in JS (i.e. sb = new GpuSubdevice(0,0)).
static JSBool C_JsGpuSubdevice_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    const char usage[] = "Usage: GpuSubdevice(GpuDeviceInst, GpuSubdeviceInst) or GpuSubdevice(testdevice)";

    if (argc > 2)
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJs;
    GpuSubdevice *pGpuSubdev = nullptr;

    if (argc == 1)
    {
        JSObject* devObj = nullptr;
        if ((OK != pJs->FromJsval(argv[0], &devObj)))
        {
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }

        JsTestDevice* pJsTestDevice = nullptr;
        if ((pJsTestDevice = JS_GET_PRIVATE(JsTestDevice, cx, devObj, "TestDevice")) == nullptr)
        {
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }

        TestDevicePtr pTestDevice = pJsTestDevice->GetTestDevice();
        if (pTestDevice.get() == nullptr)
        {
            JS_ReportWarning(cx, "Invalid test device object");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }

        pGpuSubdev = pTestDevice->GetInterface<GpuSubdevice>();
        if (!pGpuSubdev)
        {
            JS_ReportWarning(cx, "Test device is not a GPU");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
    }
    else
    {
        UINT32 gpuDevInst, gpuSubdevInst;
        Device *pGpuDev = nullptr;

        if ((OK != pJs->FromJsval(argv[0], &gpuDevInst)) ||
            (OK != pJs->FromJsval(argv[1], &gpuSubdevInst)))
        {
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }

        if (!DevMgrMgr::d_GraphDevMgr)
        {
            Printf(Tee::PriError, "JsGpuSubdevice constructor called "
                                  "before Gpu.Initialize\n");
            return JS_FALSE;
        }

        //! First try and retrieve the specified GpuDevice
        if (DevMgrMgr::d_GraphDevMgr->GetDevice(gpuDevInst, &pGpuDev) != OK)
        {
            JS_ReportWarning(cx, "Invalid Gpu device index");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }

        //! Now try and retrieve the GpuSubdevice given the GpuDevice we got
        //! above.
        pGpuSubdev = ((GpuDevice *)pGpuDev)->GetSubdevice(gpuSubdevInst);

        if (!pGpuSubdev)
        {
            JS_ReportWarning(cx, "Invalid Gpu Subdevice index");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
    }

    //! Create a JsGpuSubdevice wrapper and associate it with the
    //! given GpuSubdevice.
    JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
    MASSERT(pJsGpuSubdevice);
    pJsGpuSubdevice->SetGpuSubdevice(pGpuSubdev);

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);
    if (OK != pJsGpuSubdevice->CreateNamedProperties(cx, obj))
    {
        JS_ReportError(cx, "Unable to create init properties");
        return JS_FALSE;
    }

    //! Create a JsFrameBuffer object along with this JsGpuSubdevice object
    JsFrameBuffer *pJsFb = new JsFrameBuffer();
    MASSERT(pJsFb);
    pJsFb->SetFrameBuffer(pGpuSubdev->GetFB());
    if (pJsFb->CreateJSObject(cx, obj) != OK)
    {
        delete pJsFb;
        JS_ReportWarning(cx, "Unable to create JsFrameBuffer JSObject");
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }
    pJsGpuSubdevice->SetJsFb(pJsFb);

    // Make sure we have a Thermal object available
    if (pGpuSubdev->GetThermal())
    {
        JsThermal *pJsTherm = new JsThermal();
        MASSERT(pJsTherm);
        pJsTherm->SetThermal(pGpuSubdev->GetThermal());
        if (pJsTherm->CreateJSObject(cx, obj) != OK)
        {
            delete pJsTherm;
            JS_ReportWarning(cx, "Unable to create JsThermal JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsTherm(pJsTherm);
    }

    // Make sure we have a Perf object available
    if (pGpuSubdev->GetPerf())
    {
        JsPerf *pJsPerf = new JsPerf();
        MASSERT(pJsPerf);
        pJsPerf->SetPerfObj(pGpuSubdev->GetPerf());
        if (pJsPerf->CreateJSObject(cx, obj) != OK)
        {
            delete pJsPerf;
            JS_ReportWarning(cx, "Unable to create JsPerf JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsPerf(pJsPerf);
    }

    // Make sure we have a RegHal object available
    JsRegHal *pJsRegHal = new JsRegHal(&(pGpuSubdev->Regs()));
    MASSERT(pJsRegHal);
    if (pJsRegHal->CreateJSObject(cx, obj) != OK)
    {
        delete pJsRegHal;
        JS_ReportWarning(cx, "Unable to create JsRegHal JSObject");
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }
    pJsGpuSubdevice->SetJsRegHal(pJsRegHal);

    // Make sure we have a Power object available
    if (pGpuSubdev->GetPower())
    {
        JsPower *pJsPower = new JsPower();
        MASSERT(pJsPower);
        pJsPower->SetPowerObj(pGpuSubdev->GetPower());
        if (pJsPower->CreateJSObject(cx, obj) != OK)
        {
            delete pJsPower;
            JS_ReportWarning(cx, "Unable to create JsPower JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsPower(pJsPower);
    }

    if (pGpuSubdev->SupportsInterface<Gpio>())
    {
        JsGpio* pJsGpio = new JsGpio(pGpuSubdev->GetInterface<Gpio>());
        MASSERT(pJsGpio);
        if (pJsGpio->CreateJSObject(cx, obj) != RC::OK)
        {
            delete pJsGpio;
            JS_ReportWarning(cx, "Unable to create JsGpio JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsGpio(pJsGpio);
    }

    // Make sure we have a Perfmon object available
    GpuPerfmon* pPerfmon;
    if (OK == pGpuSubdev->GetPerfmon(&pPerfmon))
    {
        JsPerfmon *pJsPerfmon = new JsPerfmon();
        MASSERT(pJsPerfmon);
        pJsPerfmon->SetPerfmonObj(pPerfmon);
        if (pJsPerfmon->CreateJSObject(cx, obj) != OK)
        {
            delete pJsPerfmon;
            JS_ReportWarning(cx, "Unable to create JsPerfmon JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsPerfmon(pJsPerfmon);
    }

    // Make sure we have a Pmu object available
    PMU *pPmu;
    if (OK == pGpuSubdev->GetPmu(&pPmu))
    {
        JsPmu *pJsPmu = new JsPmu();
        MASSERT(pJsPmu);
        pJsPmu->SetPmuObj(pPmu);
        if (pJsPmu->CreateJSObject(cx, obj) != OK)
        {
            delete pJsPmu;
            JS_ReportWarning(cx, "Unable to create JsPmu JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsPmu(pJsPmu);
    }

#if defined(INCLUDE_DGPU)
    // Make sure we have a Fuse object available
    if (pGpuSubdev->GetFuse())
    {
        JsFuse *pJsFuse = new JsFuse();
        pJsFuse->SetFuseObj(pGpuSubdev->GetFuse());
        if (pJsFuse->CreateJSObject(cx, obj) != OK)
        {
            delete pJsFuse;
            JS_ReportWarning(cx, "Unable to create JsFuse JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsFuse(pJsFuse);
    }

    if (pGpuSubdev->GetAvfs())
    {
        JsAvfs *pJsAvfs = new JsAvfs();
        MASSERT(pJsAvfs);
        pJsAvfs->SetAvfs(pGpuSubdev->GetAvfs());
        if (OK != pJsAvfs->CreateJSObject(cx, obj))
        {
            delete pJsAvfs;
            JS_ReportWarning(cx, "Unable to create JsAvfs JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsAvfs(pJsAvfs);
    }

    if (pGpuSubdev->GetVolt3x())
    {
        JsVolt3x *pJsVolt = new JsVolt3x();
        MASSERT(pJsVolt);
        pJsVolt->SetVolt3x(pGpuSubdev->GetVolt3x());
        if (OK != pJsVolt->CreateJSObject(cx, obj))
        {
            delete pJsVolt;
            JS_ReportWarning(cx, "Unable to create JsVolt3x JSObject");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pJsGpuSubdevice->SetJsVolt(pJsVolt);
    }

#endif

    //! Finally tie the JsGpuSubdevice wrapper to the new object
    return JS_SetPrivate(cx, obj, pJsGpuSubdevice);
};

//-----------------------------------------------------------------------------
static void C_JsGpuSubdevice_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsGpuSubdevice * pJsGpuSubdevice;

    //! If a JsGpuSubdevice was associated with this object, make
    //! sure to delete it
    pJsGpuSubdevice = (JsGpuSubdevice *)JS_GetPrivate(cx, obj);
    if (pJsGpuSubdevice)
    {
        delete pJsGpuSubdevice;
    }
};

//-----------------------------------------------------------------------------
static JSClass JsGpuSubdevice_class =
{
    "GpuSubdevice",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsGpuSubdevice_finalize
};

//-----------------------------------------------------------------------------
static SObject JsGpuSubdevice_Object
(
    "GpuSubdevice",
    JsGpuSubdevice_class,
    0,
    0,
    "GpuSubdevice JS Object",
    C_JsGpuSubdevice_constructor
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! GpuSubdevice.
//!
//! \note The JSOBjects created will be freed by the JS GC.
//! \note The JSObject is prototyped from JsGpuSubdevice_Object
//! \sa SetGpuSubdevice
RC JsGpuSubdevice::CreateJSObject(JSContext *cx, JSObject *obj)
{
    RC rc;

    //! Only create one JSObject per GpuSubdevice
    if (m_pJsGpuSubdevObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsGpuSubdevice.\n");
        return OK;
    }

    m_pJsGpuSubdevObj = JS_DefineObject(cx,
                                        obj, // GpuTest object
                                        "BoundGpuSubdevice", // Property name
                                        &JsGpuSubdevice_class,
                                        JsGpuSubdevice_Object.GetJSObject(),
                                        JSPROP_READONLY);

    if (!m_pJsGpuSubdevObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsGpuSubdevice instance into the private area
    //! of the new JSOBject.  This will tie the two together.
    if (JS_SetPrivate(cx, m_pJsGpuSubdevObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsGpuSubdevice.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsGpuSubdevObj, "Help", &C_Global_Help, 1, 0);
    if (OK != CreateNamedProperties(cx, m_pJsGpuSubdevObj))
    {
        Printf(Tee::PriError, "Unable to create init properties");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    //! Create a JsFrameBuffer object along with this JsGpuSubdevice object
    m_pJsFb = new JsFrameBuffer();
    MASSERT(m_pJsFb);
    m_pJsFb->SetFrameBuffer(GetGpuSubdevice()->GetFB());
    CHECK_RC(m_pJsFb->CreateJSObject(cx, m_pJsGpuSubdevObj));

    //! Create a JsThermal object and attach it to JsGpuSubdevice
    // Make sure we have a Thermal object available
    if (GetGpuSubdevice()->GetThermal())
    {
        m_pJsTherm = new JsThermal();
        MASSERT(m_pJsTherm);
        m_pJsTherm->SetThermal(GetGpuSubdevice()->GetThermal());
        CHECK_RC(m_pJsTherm->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }

    //! Create a JsPerf object and attach it to JsGpuSubdevice
    // Make sure we have a Perf object available
    if (GetGpuSubdevice()->GetPerf())
    {
        m_pJsPerf = new JsPerf();
        MASSERT(m_pJsPerf);
        m_pJsPerf->SetPerfObj(GetGpuSubdevice()->GetPerf());
        CHECK_RC(m_pJsPerf->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }

    // Make sure we have a RegHal object available
    m_pJsRegHal = new JsRegHal(&(GetGpuSubdevice()->Regs()));
    MASSERT(m_pJsRegHal);
    CHECK_RC(m_pJsRegHal->CreateJSObject(cx, m_pJsGpuSubdevObj));

    //! Create a JsPower object and attach it to JsGpuSubdevice
    // Make sure we have a Power object available
    if (GetGpuSubdevice()->GetPower())
    {
        m_pJsPower = new JsPower();
        MASSERT(m_pJsPower);
        m_pJsPower->SetPowerObj(GetGpuSubdevice()->GetPower());
        CHECK_RC(m_pJsPower->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }

    //! Create a JsGpio object and attach it to JsGpuSubdevice
    // Make sure we have a Perf object available
    if (GetGpuSubdevice()->SupportsInterface<Gpio>())
    {
        m_pJsGpio = new JsGpio(GetGpuSubdevice()->GetInterface<Gpio>());
        MASSERT(m_pJsGpio);
        CHECK_RC(m_pJsGpio->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }

    //! Create a JsPerfmon object and attach it to JsGpuSubdevice
    // Make sure we have a Perfmon object available
    GpuPerfmon* pPerfmon;
    if (OK == GetGpuSubdevice()->GetPerfmon(&pPerfmon))
    {
        m_pJsPerfmon = new JsPerfmon();
        MASSERT(m_pJsPerfmon);
        m_pJsPerfmon->SetPerfmonObj(pPerfmon);
        CHECK_RC(m_pJsPerfmon->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }

    //! Create a JsPmu object and attach it to JsGpuSubdevice
    // Make sure we have a PMU object available
    PMU *pPmu;
    if (OK == GetGpuSubdevice()->GetPmu(&pPmu))
    {
        m_pJsPmu = new JsPmu();
        MASSERT(m_pJsPmu);
        m_pJsPmu->SetPmuObj(pPmu);
        CHECK_RC(m_pJsPmu->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }

#if defined(INCLUDE_DGPU)
    //! Create a JsFuse object and attach it to JsGpuSubdevice
    // Make sure we have a Fuse object available
    if (GetGpuSubdevice()->GetFuse())
    {
        m_pJsFuse = new JsFuse();
        MASSERT(m_pJsFuse);
        m_pJsFuse->SetFuseObj(GetGpuSubdevice()->GetFuse());
        CHECK_RC(m_pJsFuse->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }

    if (GetGpuSubdevice()->GetAvfs())
    {
        m_pJsAvfs = new JsAvfs();
        MASSERT(m_pJsAvfs);
        m_pJsAvfs->SetAvfs(GetGpuSubdevice()->GetAvfs());
        CHECK_RC(m_pJsAvfs->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }

    if (GetGpuSubdevice()->GetVolt3x())
    {
        m_pJsVolt3x = new JsVolt3x();
        MASSERT(m_pJsVolt3x);
        m_pJsVolt3x->SetVolt3x(GetGpuSubdevice()->GetVolt3x());
        CHECK_RC(m_pJsVolt3x->CreateJSObject(cx, m_pJsGpuSubdevObj));
    }
#endif

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Static function to update all outstanding JsGpuSubdevice object sub
//! object pointers when resuming from standby.
//!
//! \param pResumeSubdev : The gpusubdevice object that is being resumed
/* static */ void JsGpuSubdevice::Resume(GpuSubdevice *pResumeSubdev)
{
    set<JsGpuSubdevice *>::iterator ppJsGpuSub = s_JsGpuSubdevices.begin();

    for ( ; ppJsGpuSub != s_JsGpuSubdevices.end(); ppJsGpuSub++)
    {
        JsGpuSubdevice *pJsGpuSub = *ppJsGpuSub;

        MASSERT(pJsGpuSub);

        GpuSubdevice *pSubdev = pJsGpuSub->GetGpuSubdevice();
        if (pResumeSubdev != pSubdev)
            continue;

        if (pJsGpuSub->m_pJsFb)
            pJsGpuSub->m_pJsFb->SetFrameBuffer(pSubdev->GetFB());

        if (pJsGpuSub->m_pJsTherm)
            pJsGpuSub->m_pJsTherm->SetThermal(pSubdev->GetThermal());

        if (pJsGpuSub->m_pJsPerf)
            pJsGpuSub->m_pJsPerf->SetPerfObj(pSubdev->GetPerf());

        if (pJsGpuSub->m_pJsPower)
            pJsGpuSub->m_pJsPower->SetPowerObj(pSubdev->GetPower());

        if (pJsGpuSub->m_pJsGpio)
            pJsGpuSub->m_pJsGpio->SetGpio(pSubdev->GetInterface<Gpio>());

        if (pJsGpuSub->m_pJsPerfmon)
        {
            GpuPerfmon* pPerfmon;
            if (OK == pSubdev->GetPerfmon(&pPerfmon))
                pJsGpuSub->m_pJsPerfmon->SetPerfmonObj(pPerfmon);
        }

        if (pJsGpuSub->m_pJsPmu)
        {
            PMU *pPmu;
            if (OK == pSubdev->GetPmu(&pPmu))
                pJsGpuSub->m_pJsPmu->SetPmuObj(pPmu);
        }

#if defined(INCLUDE_DGPU)
        if (pJsGpuSub->m_pJsFuse)
            pJsGpuSub->m_pJsFuse->SetFuseObj(pSubdev->GetFuse());

        if (pJsGpuSub->m_pJsAvfs)
            pJsGpuSub->m_pJsAvfs->SetAvfs(pSubdev->GetAvfs());

        if (pJsGpuSub->m_pJsVolt3x)
            pJsGpuSub->m_pJsVolt3x->SetVolt3x(pSubdev->GetVolt3x());
#endif
    }
}

//------------------------------------------------------------------------------
//! \brief Return the associated GpuSubdevice.
//!
//! Return the GpuSubdevice that is being wrapped by this JsGpuSubdevice.  We
//! should never have a NULL pointer to a GpuSubdevice when calling this
//! function.
GpuSubdevice * JsGpuSubdevice::GetGpuSubdevice()
{
    MASSERT(m_pGpuSubdevice);

    return m_pGpuSubdevice;
}

UINT32 JsGpuSubdevice::GetDevInst()
{
    return GetGpuSubdevice()->DevInst();
}

UINT32 JsGpuSubdevice::GetDeviceId()
{
    return GetGpuSubdevice()->DeviceId();
}

string JsGpuSubdevice::GetDeviceName()
{
    return GetGpuSubdevice()->DeviceName();
}
RC JsGpuSubdevice::GetChipSkuNumber(string* pVal)
{
    return GetGpuSubdevice()->GetChipSkuNumber(pVal);
}
RC JsGpuSubdevice::GetChipSkuModifier(string* pVal)
{
    return GetGpuSubdevice()->GetChipSkuModifier(pVal);
}
RC JsGpuSubdevice::GetChipSkuNumberModifier(string* pVal)
{
    return GetGpuSubdevice()->GetChipSkuNumberModifier(pVal);
}
string JsGpuSubdevice::GetBoardNumber()
{
    return GetGpuSubdevice()->GetBoardNumber();
}
string JsGpuSubdevice::GetBoardSkuNumber()
{
    return GetGpuSubdevice()->GetBoardSkuNumber();
}
string JsGpuSubdevice::GetCDPN()
{
    return GetGpuSubdevice()->GetCDPN();
}
bool JsGpuSubdevice::GetIsPrimary()
{
    return GetGpuSubdevice()->IsPrimary();
}
string JsGpuSubdevice::GetBootInfo()
{
    return GetGpuSubdevice()->GetBootInfoString();
}
UINT32 JsGpuSubdevice::GetPciDeviceId()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->DeviceId();
}
UINT32 JsGpuSubdevice::GetChipArchId()
{
    return GetGpuSubdevice()->ChipArchId();
}
UINT32 JsGpuSubdevice::GetSubdeviceInst()
{
    return GetGpuSubdevice()->GetSubdeviceInst();
}
UINT32 JsGpuSubdevice::GetGpuInst()
{
    return GetGpuSubdevice()->GetGpuInst();
}
UINT64 JsGpuSubdevice::GetFbHeapSize()
{
    return GetGpuSubdevice()->FbHeapSize();
}
UINT64 JsGpuSubdevice::GetFramebufferBarSize()
{
    return GetGpuSubdevice()->FramebufferBarSize();
}
bool JsGpuSubdevice::HasBug(UINT32 bugNum)
{
    return GetGpuSubdevice()->HasBug(bugNum);
}
bool JsGpuSubdevice::HasFeature(UINT32 feature)
{
    return GetGpuSubdevice()->HasFeature((Device::Feature)feature);
}
bool JsGpuSubdevice::IsFeatureEnabled(UINT32 feature)
{
    return GetGpuSubdevice()->IsFeatureEnabled((Device::Feature)feature);
}
bool JsGpuSubdevice::IsSOC()
{
    return GetGpuSubdevice()->IsSOC();
}
UINT32 JsGpuSubdevice::GetBusType()
{
    return GetGpuSubdevice()->BusType();
}
bool JsGpuSubdevice::HasDomain(UINT32 domain)
{
    return GetGpuSubdevice()->HasDomain((Gpu::ClkDomain)domain);
}
bool JsGpuSubdevice::HasGpcRg()
{
    return GetGpuSubdevice()->HasGpcRg();
}
bool JsGpuSubdevice::CanClkDomainBeProgrammed(UINT32 domain)
{
    return GetGpuSubdevice()->CanClkDomainBeProgrammed((Gpu::ClkDomain)domain);
}
UINT32 JsGpuSubdevice::GetPcieLinkSpeed()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->GetLinkSpeed(Pci::SpeedUnknown);
}
UINT32 JsGpuSubdevice::GetPcieSpeedCapability()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->LinkSpeedCapability();
}
UINT32 JsGpuSubdevice::GetPcieLinkWidth()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->GetLinkWidth();
}
UINT32 JsGpuSubdevice::GetMemoryIndex()
{
    return GetGpuSubdevice()->MemoryIndex();
}
UINT32 JsGpuSubdevice::GetRamConfigStrap()
{
    return GetGpuSubdevice()->RamConfigStrap();
}
UINT32 JsGpuSubdevice::GetTvEncoderStrap()
{
    return GetGpuSubdevice()->TvEncoderStrap();
}
UINT32 JsGpuSubdevice::GetIo3GioConfigPadStrap()
{
    return GetGpuSubdevice()->Io3GioConfigPadStrap();
}
UINT32 JsGpuSubdevice::GetUserStrapVal()
{
    return GetGpuSubdevice()->UserStrapVal();
}
RC JsGpuSubdevice::GetNamedProp(string Name, UINT32 *pValue)
{
    RC rc;
    GpuSubdevice::NamedProperty *pNamedProp;
    CHECK_RC(GetGpuSubdevice()->GetNamedProperty(Name, &pNamedProp));
    return pNamedProp->Get(pValue);
}
RC JsGpuSubdevice::GetNamedProp(string Name, bool *pValue)
{
    RC rc;
    GpuSubdevice::NamedProperty *pNamedProp;
    CHECK_RC(GetGpuSubdevice()->GetNamedProperty(Name, &pNamedProp));
    return pNamedProp->Get(pValue);
}
RC JsGpuSubdevice::GetNamedProp(string Name, string *pValue)
{
    RC rc;
    GpuSubdevice::NamedProperty *pNamedProp;
    CHECK_RC(GetGpuSubdevice()->GetNamedProperty(Name, &pNamedProp));
    return pNamedProp->Get(pValue);
}
RC JsGpuSubdevice::GetNamedProp(string Name, JsArray *pArray)
{
    RC rc;
    vector<UINT32> values;
    JavaScriptPtr pJavaScript;
    GpuSubdevice::NamedProperty *pNamedProp;

    CHECK_RC(GetGpuSubdevice()->GetNamedProperty(Name, &pNamedProp));
    CHECK_RC(pNamedProp->Get(&values));

    rc = pJavaScript->ToJsArray(values, pArray);
    if (rc != OK)
    {
        Printf(Tee::PriNormal, "Can't init property %s\n", Name.c_str());
    }
    return rc;
}
RC JsGpuSubdevice::SetNamedProp(string Name, UINT32 Value)
{
    RC rc;
    GpuSubdevice::NamedProperty *pNamedProp;
    CHECK_RC(GetGpuSubdevice()->GetNamedProperty(Name, &pNamedProp));
    return pNamedProp->Set(Value);
}
RC JsGpuSubdevice::SetNamedProp(string Name, bool Value)
{
    RC rc;
    GpuSubdevice::NamedProperty *pNamedProp;
    CHECK_RC(GetGpuSubdevice()->GetNamedProperty(Name, &pNamedProp));
    return pNamedProp->Set(Value);
}
RC JsGpuSubdevice::SetNamedProp(string Name, const string &Value)
{
    RC rc;
    GpuSubdevice::NamedProperty *pNamedProp;
    CHECK_RC(GetGpuSubdevice()->GetNamedProperty(Name, &pNamedProp));
    return pNamedProp->Set(Value);
}
RC JsGpuSubdevice::SetNamedProp(string Name, JsArray *pArray)
{
    RC rc;
    vector<UINT32> values;
    JavaScriptPtr pJavaScript;
    GpuSubdevice::NamedProperty *pNamedProp;

    CHECK_RC(GetGpuSubdevice()->GetNamedProperty(Name, &pNamedProp));

    rc = pJavaScript->FromJsArray(*pArray, &values);
    if (rc != OK)
    {
        Printf(Tee::PriNormal, "Can't init property %s\n", Name.c_str());
        return rc;
    }

    return pNamedProp->Set(values);
}
bool JsGpuSubdevice::IsMfgModsSmcEnabled()
{
    // We can only query partition info if MfgMods SMC is enabled
    return GetGpuSubdevice()->IsSMCEnabled() && GetGpuSubdevice()->UsingMfgModsSMC();
}

//------------------------------------------------------------------------------
//! \brief Dynamically create the named properties at runtime.  If a named
//!        property is defined in JSGpuSubdevice, but not present in the
//!        actual GpuSubdevice then a property is still created, but the get
//!        and set functions point to a JS interface function which will cause
//!        an error
RC JsGpuSubdevice::CreateNamedProperties(JSContext *cx, JSObject *obj)
{
    set<string> propertyNames;
    map<string, pair<JSPropertyOp, JSPropertyOp> >::iterator pNamedProp;
    JSPropertyOp GetMethod;
    JSPropertyOp SetMethod;

    GetGpuSubdevice()->GetNamedPropertyList(&propertyNames);

    for (pNamedProp = s_NamedProperties.begin();
         pNamedProp != s_NamedProperties.end(); pNamedProp++)
    {
        if (propertyNames.count(pNamedProp->first))
        {
            GetMethod = pNamedProp->second.first;
            SetMethod = pNamedProp->second.second;
        }
        else
        {
            static set<string> complained;
            if (complained.find(pNamedProp->first) == complained.end())
            {
                Printf(Tee::PriDebug,
                       "JS Named Property %s not present in GpuSubdevice\n",
                       pNamedProp->first.c_str());
                complained.insert(pNamedProp->first);
            }
            GetMethod = JsGpuSubdevice_Get_IlwalidProp;
            SetMethod = JsGpuSubdevice_Set_IlwalidProp;
        }

        if (JS_FALSE == JS_DefineProperty(cx, obj,
                          pNamedProp->first.c_str(),
                          INT_TO_JSVAL(0),
                          GetMethod,
                          SetMethod,
                          0))
        {
            Printf(Tee::PriError, "Unable to create named property %s",
                   pNamedProp->first.c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}
//------------------------------------------------------------------------------
//! \brief Register a named property that will be created at runtime
/* static */ void JsGpuSubdevice::RegisterNamedProperty
(
    const char * Name,
    JSPropertyOp GetMethod,
    JSPropertyOp SetMethod
)
{
    if (s_NamedProperties.count(string(Name)))
    {
        Printf(Tee::PriWarn,
               "JsGpuSubdevice init property %s already registered\n", Name);
        return;
    }
    pair<JSPropertyOp, JSPropertyOp> ops;
    ops.first = GetMethod;
    ops.second = SetMethod;

    s_NamedProperties[string(Name)] = ops;
}
JSObject * JsGpuSubdevice::GetParentDevice()
{
    JavaScriptPtr pJavaScript;
    GpuDevice * pGpuDevice = GetGpuSubdevice()->GetParentDevice();
    if (!pGpuDevice)
        return NULL;

    JSContext *pContext;
    if (OK != pJavaScript->GetContext(&pContext))
    {
        MASSERT(!"JsGpuSubdevice::GetParentDevice : Unable to get a JS Context");
        return NULL;
    }

    // Create a JsGpuDevice object to return to JS
    // Note:  This will be cleaned up by JS Garbage Collector
    JsGpuDevice * pJsGpuDevice = new JsGpuDevice();
    MASSERT(pJsGpuDevice);
    pJsGpuDevice->SetGpuDevice(pGpuDevice);

    pJsGpuDevice->CreateJSObject(pContext,
                                 pJavaScript->GetGlobalObject());

    return pJsGpuDevice->GetJSObject();
}

//------------------------------------------------------------------------------
string JsGpuSubdevice::GetPdiString()
{
    UINT64 pdi;
    if (OK != GetGpuSubdevice()->GetPdi(&pdi))
    {
        return "";
    }
    return Utility::StrPrintf("0x%016llx", pdi);
}

//------------------------------------------------------------------------------
// This can be ilwoked before RM is initialized, but it is not implemented for all chips.
string JsGpuSubdevice::GetRawEcidString()
{
    vector<UINT32> ecid;
    if (OK != GetGpuSubdevice()->GetRawEcid(&ecid, false))
    {
        return "";
    }
    return Utility::FormatRawEcid(ecid);
}
//------------------------------------------------------------------------------
// This can be ilwoked before RM is initialized, but it is not implemented for all chips.
// Note: For Pascal+ GPUs we need the fields in the ECID reversed before requesting
//       the unlock keys from the Green Hill Server. Otherwise the keys are incorrect.
string JsGpuSubdevice::GetGHSRawEcidString()
{
    vector<UINT32> Ecids;
    if (OK != GetGpuSubdevice()->GetRawEcid(&Ecids, true)) // reverse the fields
    {
        return "";
    }
    return Utility::FormatRawEcid(Ecids);
}

//------------------------------------------------------------------------------
// This can be ilwoked before RM is initialized, but it is not implemented for all chips.
string JsGpuSubdevice::GetEcidString()
{
    string Ecid;
    if (OK != GetGpuSubdevice()->GetEcid(&Ecid))
    {
        return "";
    }
    return Ecid;
}

//------------------------------------------------------------------------------
// This can be ilwoked after RM has been initialized.
string JsGpuSubdevice::GetChipIdString()
{
    vector<UINT32> Ecids;
    if (OK != GetGpuSubdevice()->ChipId(&Ecids))
    {
        return GetGpuSubdevice()->IsSOC() ? "" : "0";
    }
    return Utility::FormatRawEcid(Ecids);
}

//------------------------------------------------------------------------------
string JsGpuSubdevice::GetUUIDString()
{
    return GetGpuSubdevice()->GetUUIDString();
}

//------------------------------------------------------------------------------
string JsGpuSubdevice::GetChipIdCooked()
{
    return GetGpuSubdevice()->ChipIdCooked();
}

//------------------------------------------------------------------------------
RC JsGpuSubdevice::GetRevision(UINT32* pVal)
{
    return GetGpuSubdevice()->GetChipRevision(pVal);
}
//------------------------------------------------------------------------------
RC JsGpuSubdevice::GetPrivateRevision(UINT32* pVal)
{
    return GetGpuSubdevice()->GetChipPrivateRevision(pVal);
}
//------------------------------------------------------------------------------
string JsGpuSubdevice::GetRevisionString()
{
    char rev[10];
    char SuffixChar = '\0';

    UINT32 revision = 0;
    GetRevision(&revision);
    sprintf(rev, "%02x%c", revision, SuffixChar);

    return rev;
}
//------------------------------------------------------------------------------
string JsGpuSubdevice::GetPrivateRevisionString()
{
    char rev[10];

    UINT32 privateRev = 0;
    GetPrivateRevision(&privateRev);
    sprintf(rev, "%06x", privateRev);

    return rev;
}

UINT64 JsGpuSubdevice::GetMemoryClock()
{
    GpuSubdevice * pSubdevice = GetGpuSubdevice();
    UINT64 ActualPllHz;
    UINT64 TargetPllHz;
    UINT64 SlowedHz;
    Gpu::ClkSrc src;

    if( OK != pSubdevice->GetClock(Gpu::ClkM, &ActualPllHz, &TargetPllHz, &SlowedHz, &src ) )
    {
       Printf(Tee::PriNormal,
              "JsGpuSubdevice: Error Getting MemoryClock for %d.%d",
              pSubdevice->GetParentDevice()->GetDeviceInst(),
              pSubdevice->GetSubdeviceInst());
       return 0;
    }

    return ActualPllHz;
}

RC JsGpuSubdevice::SetMemoryClock(UINT64 Hz)
{
    return GetGpuSubdevice()->SetClock(Gpu::ClkM, Hz);
}

//------------------------------------------------------------------------------
UINT32 JsGpuSubdevice::GetGpuLocId()
{
    return GetGpuSubdevice()->GetGpuLocId();
}

//------------------------------------------------------------------------------
RC JsGpuSubdevice::SetGpuLocId(UINT32 locId)
{
    return GetGpuSubdevice()->SetGpuLocId(locId);
}

//------------------------------------------------------------------------------
bool JsGpuSubdevice::GetVerboseJtag()
{
    GpuSubdevice * pSubdevice = GetGpuSubdevice();

    return pSubdevice->GetVerboseJtag();
}

//------------------------------------------------------------------------------
RC JsGpuSubdevice::SetVerboseJtag(bool enable)
{
    return GetGpuSubdevice()->SetVerboseJtag(enable);
}

//------------------------------------------------------------------------------
string JsGpuSubdevice::GetGpuIdentStr()
{
    return GetGpuSubdevice()->GpuIdentStr();
}

UINT64 JsGpuSubdevice::GetTargetMemoryClock()
{
    GpuSubdevice * pSubdevice = GetGpuSubdevice();
    UINT64 ActualPllHz;
    UINT64 TargetPllHz;
    UINT64 SlowedHz;
    Gpu::ClkSrc src;

    if( OK != pSubdevice->GetClock(Gpu::ClkM, &ActualPllHz, &TargetPllHz, &SlowedHz, &src ) )
    {
        Printf(Tee::PriNormal,
               "JsGpuSubdevice: Error Getting TargetMemoryClock for %d.%d",
               pSubdevice->GetParentDevice()->GetDeviceInst(),
               pSubdevice->GetSubdeviceInst());
        return 0;
    }

    return TargetPllHz;
}

UINT32 JsGpuSubdevice::GetSubsystemVendorId()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->SubsystemVendorId();
}

UINT32 JsGpuSubdevice::GetSubsystemDeviceId()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->SubsystemDeviceId();
}

UINT32 JsGpuSubdevice::GetBoardId()
{
    return GetGpuSubdevice()->BoardId();
}

PHYSADDR JsGpuSubdevice::GetPhysLwBase()
{
    return GetGpuSubdevice()->GetPhysLwBase();
}

PHYSADDR JsGpuSubdevice::GetPhysFbBase()
{
    return GetGpuSubdevice()->GetPhysFbBase();
}

PHYSADDR JsGpuSubdevice::GetPhysInstBase()
{
    return GetGpuSubdevice()->GetPhysInstBase();
}

UINT32 JsGpuSubdevice::GetIrq()
{
    return GetGpuSubdevice()->GetIrq();
}

UINT32 JsGpuSubdevice::GetRopCount()
{
    return GetGpuSubdevice()->RopCount();
}

UINT32 JsGpuSubdevice::GetShaderCount()
{
    return GetGpuSubdevice()->ShaderCount();
}

UINT32 JsGpuSubdevice::GetUGpuCount()
{
    return Utility::CountBits(GetUGpuMask());

}

UINT32 JsGpuSubdevice::GetUGpuMask()
{
    return GetGpuSubdevice()->GetUGpuMask();
}

UINT32 JsGpuSubdevice::GetVpeCount()
{
    return GetGpuSubdevice()->VpeCount();
}

UINT32 JsGpuSubdevice::GetFrameBufferUnitCount()
{
    return GetGpuSubdevice()->GetFrameBufferUnitCount();
}

UINT32 JsGpuSubdevice::GetFrameBufferIOCount()
{
    return GetGpuSubdevice()->GetFrameBufferIOCount();
}
UINT32 JsGpuSubdevice::GetGpcCount()
{
    return GetGpuSubdevice()->GetGpcCount();
}

UINT32 JsGpuSubdevice::GetMaxGpcCount()
{
    return GetGpuSubdevice()->GetMaxGpcCount();
}

UINT32 JsGpuSubdevice::GetTpcCount()
{
    return GetGpuSubdevice()->GetTpcCount();
}

UINT32 JsGpuSubdevice::GetMaxTpcCount()
{
    return GetGpuSubdevice()->GetMaxTpcCount();
}

UINT32 JsGpuSubdevice::GetMaxFbpCount()
{
    return GetGpuSubdevice()->GetMaxFbpCount();
}

UINT32 JsGpuSubdevice::GetGpcMask()
{
    return GetGpuSubdevice()->GetFsImpl()->GpcMask();
}

UINT32 JsGpuSubdevice::GetTpcMask()
{
    return GetGpuSubdevice()->GetFsImpl()->TpcMask();
}

UINT32 JsGpuSubdevice::GetTpcMaskOnGpc(UINT32 GpcNum)
{
    return GetGpuSubdevice()->GetFsImpl()->TpcMask(GpcNum);
}

bool JsGpuSubdevice::IsGpcGraphicsCapable(UINT32 hwGpcNum)
{
    return GetGpuSubdevice()->IsGpcGraphicsCapable(hwGpcNum);
}

UINT32 JsGpuSubdevice::GetGraphicsTpcMaskOnGpc(UINT32 hwGpcNum)
{
    return GetGpuSubdevice()->GetGraphicsTpcMaskOnGpc(hwGpcNum);
}

bool JsGpuSubdevice::SupportsReconfigTpcMaskOnGpc(UINT32 gpcNum)
{
    return GetGpuSubdevice()->SupportsReconfigTpcMaskOnGpc(gpcNum);
}

UINT32 JsGpuSubdevice::GetReconfigTpcMaskOnGpc(UINT32 GpcNum)
{
    return GetGpuSubdevice()->GetReconfigTpcMaskOnGpc(GpcNum);
}

UINT32 JsGpuSubdevice::GetPesMaskOnGpc(UINT32 GpcNum)
{
    return GetGpuSubdevice()->GetFsImpl()->PesMask(GpcNum);
}

bool JsGpuSubdevice::SupportsRopMaskOnGpc(UINT32 gpcNum)
{
    return GetGpuSubdevice()->GetFsImpl()->SupportsRopMask(gpcNum);
}

UINT32 JsGpuSubdevice::GetRopMaskOnGpc(UINT32 GpcNum)
{
    return GetGpuSubdevice()->GetFsImpl()->RopMask(GpcNum);
}

bool JsGpuSubdevice::SupportsCpcMaskOnGpc(UINT32 gpcNum)
{
    return GetGpuSubdevice()->GetFsImpl()->SupportsCpcMask(gpcNum);
}

UINT32 JsGpuSubdevice::GetCpcMaskOnGpc(UINT32 GpcNum)
{
    return GetGpuSubdevice()->GetFsImpl()->CpcMask(GpcNum);
}

UINT32 JsGpuSubdevice::GetFbMask()
{
    return GetGpuSubdevice()->GetFsImpl()->FbMask();
}

UINT32 JsGpuSubdevice::GetFbpMask()
{
    return GetGpuSubdevice()->GetFsImpl()->FbpMask();
}

UINT32 JsGpuSubdevice::GetFbpaMask()
{
    return GetGpuSubdevice()->GetFsImpl()->FbpaMask();
}

UINT32 JsGpuSubdevice::GetL2Mask()
{
    return GetGpuSubdevice()->GetFsImpl()->L2Mask();
}

UINT32 JsGpuSubdevice::GetL2MaskOnFbp(UINT32 FbpNum)
{
    return GetGpuSubdevice()->GetFsImpl()->L2Mask(FbpNum);
}

bool JsGpuSubdevice::SupportsL2SliceMaskOnFbp(UINT32 fbpNum)
{
    return GetGpuSubdevice()->GetFsImpl()->SupportsL2SliceMask(fbpNum);
}

UINT32 JsGpuSubdevice::GetL2SliceMaskOnFbp(UINT32 FbpNum)
{
    return GetGpuSubdevice()->GetFsImpl()->L2SliceMask(FbpNum);
}

UINT32 JsGpuSubdevice::GetFbioMask()
{
    return GetGpuSubdevice()->GetFsImpl()->FbioMask();
}

UINT32 JsGpuSubdevice::GetFbioShiftMask()
{
    return GetGpuSubdevice()->GetFsImpl()->FbioshiftMask();
}
UINT32 JsGpuSubdevice::GetLwencMask()
{
    return GetGpuSubdevice()->GetFsImpl()->LwencMask();
}
UINT32 JsGpuSubdevice::GetLwdecMask()
{
    return GetGpuSubdevice()->GetFsImpl()->LwdecMask();
}
UINT32 JsGpuSubdevice::GetLwjpgMask()
{
    return GetGpuSubdevice()->GetFsImpl()->LwjpgMask();
}
UINT32 JsGpuSubdevice::GetPceMask()
{
    return GetGpuSubdevice()->GetFsImpl()->PceMask();
}
UINT32 JsGpuSubdevice::GetMaxPceCount()
{
    return GetGpuSubdevice()->GetMaxPceCount();
}
UINT32 JsGpuSubdevice::GetOfaMask()
{
    return GetGpuSubdevice()->GetFsImpl()->OfaMask();
}
UINT32 JsGpuSubdevice::GetSyspipeMask()
{
    return GetGpuSubdevice()->GetFsImpl()->SyspipeMask();
}

UINT32 JsGpuSubdevice::GetSmMask()
{
    return GetGpuSubdevice()->GetFsImpl()->SmMask();
}

UINT32 JsGpuSubdevice::GetSmMaskOnTpc(UINT32 GpcNum, UINT32 TpcNum)
{
    return GetGpuSubdevice()->GetFsImpl()->SmMask(GpcNum, TpcNum);
}

UINT32 JsGpuSubdevice::GetXpMask()
{
    return GetGpuSubdevice()->GetFsImpl()->XpMask();
}

UINT32 JsGpuSubdevice::GetMevpMask()
{
    return GetGpuSubdevice()->GetFsImpl()->MevpMask();
}

bool JsGpuSubdevice::GetHasVbiosModsFlagA()
{
    return GetGpuSubdevice()->HasVbiosModsFlagA();
}

bool JsGpuSubdevice::GetHasVbiosModsFlagB()
{
    return GetGpuSubdevice()->HasVbiosModsFlagB();
}

UINT32 JsGpuSubdevice::GetNumAteSpeedo()
{
    return GetGpuSubdevice()->NumAteSpeedo();
}

bool JsGpuSubdevice::GetIsMXMBoard()
{
    return GetGpuSubdevice()->IsMXMBoard();
}

UINT32 JsGpuSubdevice::GetMXMVersion()
{
    return GetGpuSubdevice()->MxmVersion();
}

UINT32 JsGpuSubdevice::GetHQualType()
{
    return GetGpuSubdevice()->HQualType();
}

bool JsGpuSubdevice::GetIsEmulation()
{
    return GetGpuSubdevice()->IsEmulation();
}

bool JsGpuSubdevice::GetIsEmOrSim()
{
    return GetGpuSubdevice()->IsEmOrSim();
}

bool JsGpuSubdevice::GetIsDFPGA()
{
    return GetGpuSubdevice()->IsDFPGA();
}

UINT32 JsGpuSubdevice::GetPciDomainNumber()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->DomainNumber();
}

UINT32 JsGpuSubdevice::GetPciBusNumber()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->BusNumber();
}

UINT32 JsGpuSubdevice::GetPciDeviceNumber()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->DeviceNumber();
}

UINT32 JsGpuSubdevice::GetPciFunctionNumber()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->FunctionNumber();
}

string JsGpuSubdevice::GetRomPartner()
{
    return GetGpuSubdevice()->GetRomPartner();
}
string JsGpuSubdevice::GetRomProjectId()
{
    return GetGpuSubdevice()->GetRomProjectId();
}

string JsGpuSubdevice::GetRomVersion()
{
    return GetGpuSubdevice()->GetRomVersion();
}

string JsGpuSubdevice::GetRomType()
{
    return GetGpuSubdevice()->GetRomType();
}

UINT32 JsGpuSubdevice::GetRomTypeEnum()
{
    return GetGpuSubdevice()->GetRomTypeEnum();
}

UINT32 JsGpuSubdevice::GetRomTimestamp()
{
    return GetGpuSubdevice()->GetRomTimestamp();
}

UINT32 JsGpuSubdevice::GetRomExpiration()
{
    return GetGpuSubdevice()->GetRomExpiration();
}

UINT32 JsGpuSubdevice::GetRomSelwrityStatus()
{
    return GetGpuSubdevice()->GetRomSelwrityStatus();
}

string JsGpuSubdevice::GetRomOemVendor()
{
    return GetGpuSubdevice()->GetRomOemVendor();
}

string JsGpuSubdevice::GetInfoRomVersion()
{
    return GetGpuSubdevice()->GetInfoRomVersion();
}

string JsGpuSubdevice::GetBoardSerialNumber()
{
    return GetGpuSubdevice()->GetBoardSerialNumber();
}

string JsGpuSubdevice::GetBoardMarketingName()
{
    return GetGpuSubdevice()->GetBoardMarketingName();
}

string JsGpuSubdevice::GetBoardProjectSerialNumber()
{
    return GetGpuSubdevice()->GetBoardProjectSerialNumber();
}

UINT32 JsGpuSubdevice::GetFoundry()
{
    return GetGpuSubdevice()->Foundry();
}

UINT32 JsGpuSubdevice::GetAspmState()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->GetAspmState();
}

UINT32 JsGpuSubdevice::GetAspmCyaState()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->GetAspmCyaState();
}

UINT32 JsGpuSubdevice::GetL2CacheSize()
{
    return GetGpuSubdevice()->GetFB()->GetL2CacheSize();
}

UINT32 JsGpuSubdevice::GetL2CacheSizePerUGpu(UINT32 uGpu)
{
    return GetGpuSubdevice()->GetL2CacheSizePerUGpu(uGpu);
}

UINT32 JsGpuSubdevice::GetMaxL2SlicesPerFbp()
{
    return GetGpuSubdevice()->GetFB()->GetMaxL2SlicesPerFbp();
}

UINT32 JsGpuSubdevice::GetMaxCeCount()
{
    return GetGpuSubdevice()->GetMaxCeCount();
}

bool JsGpuSubdevice::GetIsASLMCapable()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->IsASLMCapable();
}

RC JsGpuSubdevice::SetPexAerEnable(bool bEnable)
{
    GetGpuSubdevice()->GetInterface<Pcie>()->EnableAer(bEnable);
    return OK;
}

bool JsGpuSubdevice::GetPexAerEnable()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->AerEnabled();
}

string JsGpuSubdevice::GetFoundryString()
{
    switch (GetGpuSubdevice()->Foundry())
    {
        case LW2080_CTRL_GPU_INFO_FOUNDRY_TSMC: return "TSMC";
        case LW2080_CTRL_GPU_INFO_FOUNDRY_UMC: return "UMC";
        case LW2080_CTRL_GPU_INFO_FOUNDRY_IBM: return "IBM";
        case LW2080_CTRL_GPU_INFO_FOUNDRY_SMIC: return "SMIC";
        case LW2080_CTRL_GPU_INFO_FOUNDRY_CHARTERED: return "Chartered";
        case LW2080_CTRL_GPU_INFO_FOUNDRY_TOSHIBA: return "Toshiba";
        case LW2080_CTRL_GPU_INFO_FOUNDRY_SONY: return "Sony";
        case LW2080_CTRL_GPU_INFO_FOUNDRY_SAMSUNG: return "Samsung"; // aka SEC
        default: return "Unknown";
    }
}

bool JsGpuSubdevice::GetHasAspmL1Substates()
{
    return GetGpuSubdevice()->GetInterface<Pcie>()->HasAspmL1Substates();
}

bool JsGpuSubdevice::GetCanL2BeDisabled()
{
    return GetGpuSubdevice()->CanL2BeDisabled();
}

bool JsGpuSubdevice::GetIsEccFuseEnabled()
{
    return GetGpuSubdevice()->IsEccFuseEnabled();
}

string JsGpuSubdevice::GetVbiosImageHash()
{
    vector<UINT08> values;
    string s;

    if (OK != GetGpuSubdevice()->GetVBiosImageHash(&values))
        return s;

    for (unsigned int i = 0; i < values.size(); i++)
    {
        char c[3] = {0,0,0};
        sprintf(c,"%02X",values[i]);
        s.append(c,2);
    }

    return s;
}

RC JsGpuSubdevice::DumpCieData()
{
    return GetGpuSubdevice()->DumpCieData();
}
JS_STEST_BIND_NO_ARGS(JsGpuSubdevice, DumpCieData, "Dump LED calibration Data");

RC JsGpuSubdevice::RunFpfFub()
{
    return GetGpuSubdevice()->RunFpfFub();
}

RC JsGpuSubdevice::RunFpfFubUseCase(UINT32 useCase)
{
    return GetGpuSubdevice()->RunFpfFubUseCase(useCase);
}

RC JsGpuSubdevice::RunRirFrb()
{
    return GetGpuSubdevice()->RunRirFrb();
}

//------------------------------------------------------------------------------
bool JsGpuSubdevice::GetSkipAzaliaInit()
{
    return GetGpuSubdevice()->GetSkipAzaliaInit();
}

//------------------------------------------------------------------------------
RC JsGpuSubdevice::SetSkipAzaliaInit(bool bSkip)
{
    GetGpuSubdevice()->SetSkipAzaliaInit(bSkip);
    return RC::OK;
}

string JsGpuSubdevice::GetGspFwImg()
{
    return GetGpuSubdevice()->GetGspFwImg();
}

RC JsGpuSubdevice::SetGspFwImg(string gspFwImg)
{
    GetGpuSubdevice()->SetGspFwImg(gspFwImg);
    return RC::OK;
}

string JsGpuSubdevice::GetGspLogFwImg()
{
    return GetGpuSubdevice()->GetGspLogFwImg();
}

RC JsGpuSubdevice::SetGspLogFwImg(string gspLogFwImg)
{
    GetGpuSubdevice()->SetGspLogFwImg(gspLogFwImg);
    return RC::OK;
}

string JsGpuSubdevice::GetChipSelwrityFuse()
{
    RegHal& regs = GetGpuSubdevice()->Regs();
    if (!regs.IsSupported(MODS_FUSE_OPT_SELWRE_PMU_DEBUG_DIS))
    {
        return "";
    }
    if (!regs.HasReadAccess(MODS_FUSE_OPT_SELWRE_PMU_DEBUG_DIS))
    {
        Printf(Tee::PriWarn, "Cannot read chip security fuse\n");
        return "";
    }

    bool isProdFused = regs.Test32(MODS_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_DATA_YES);
    if (isProdFused)
    {
        return "Production";
    }
    else
    {
        return "Debug";
    }
}

UINT32 JsGpuSubdevice::GetSmbusAddress()
{
    return GetGpuSubdevice()->GetSmbusAddress();
}

string JsGpuSubdevice::GetPackageStr()
{
    return GetGpuSubdevice()->GetPackageStr();
}

bool JsGpuSubdevice::IsChipSltFused()
{
    bool bIsFused = false;
    GetGpuSubdevice()->IsSltFused(&bIsFused);
    return bIsFused;
}

//------------------------------------------------------------------------------
bool JsGpuSubdevice::GetUseRegOps()
{
    return GetGpuSubdevice()->GetUseRegOps();
}

//------------------------------------------------------------------------------
RC JsGpuSubdevice::SetUseRegOps(bool bUseRegOps)
{
    return GetGpuSubdevice()->SetUseRegOps(bUseRegOps);
}

bool JsGpuSubdevice::GetIsMIGEnabled()
{
    return GetGpuSubdevice()->IsPersistentSMCEnabled();
}

bool JsGpuSubdevice::GetSkipEccErrCtrInit()
{
    return GetGpuSubdevice()->GetSkipEccErrCtrInit();
}

RC JsGpuSubdevice::SetSkipEccErrCtrInit(bool shouldSkip)
{
    GetGpuSubdevice()->SetSkipEccErrCtrInit(shouldSkip);
    return RC::OK;
}

JS_STEST_BIND_NO_ARGS(JsGpuSubdevice, RunFpfFub, "Load and execute the FUB for FPF");
JS_STEST_BIND_NO_ARGS(JsGpuSubdevice, RunRirFrb, "Load and execute RIR binary");
JS_STEST_BIND_ONE_ARG(JsGpuSubdevice, RunFpfFubUseCase, useCase, UINT32,
                      "Load and execute the FUB for FPF for a given use case");

//-----------------------------------------------------------------------------
// GpuSubdevice JS Methods

// Make register and framebuffer read/write callable from JavaScript:
GPUSUBDEVICE_READFUNC( RegRd08, "Read 8bits from register." );
GPUSUBDEVICE_READFUNC( RegRd16, "Read 16bits from register." );
GPUSUBDEVICE_READFUNC( RegRd32, "Read 32bits from register." );
GPUSUBDEVICE_WRITEFUNC( RegWr08, "Write 8bits to register." );
GPUSUBDEVICE_WRITEFUNC( RegWr16, "Write 16bits to register." );
GPUSUBDEVICE_WRITEFUNC( RegWr32, "Write 32bits to register." );

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                        SetCriticalFbRanges,
                        1,
                        "Critical Fb Range")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.SetCriticalFbRanges(Array)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JsArray, InnerArray);
    RC rc;

    if(InnerArray.size() > 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        pSubdev->ClearCriticalFbRanges();

        for (UINT32 i = 0; i < InnerArray.size(); i++)
        {
            JSObject * pRegMaskObj;
            UINT64 Start = 0;
            UINT64 Length = 0;

            C_CHECK_RC(pJavaScript->FromJsval(InnerArray[i], &pRegMaskObj));
            C_CHECK_RC(pJavaScript->GetProperty(pRegMaskObj, "Start", &Start));
            C_CHECK_RC(pJavaScript->GetProperty(pRegMaskObj,"Length", &Length));
            pSubdev->AddCriticalFbRange(Start, Length);
        }
    }

    RETURN_RC(OK);
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                GetGpuPcieErrors,
                1,
                "Read Gpu PCIE link errors on both rcv and trx ends")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetGpuPcieErrors(Array)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pArrayToReturn);
    RC rc;

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    Pci::PcieErrorsMask LocErrors = Pci::NO_ERROR;
    Pci::PcieErrorsMask HostErrors = Pci::NO_ERROR;
    C_CHECK_RC(pSubdev->GetInterface<Pcie>()->GetPolledErrors(&LocErrors, &HostErrors));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, (UINT32)LocErrors));
    RETURN_RC(pJavaScript->SetElement(pArrayToReturn, 1, (UINT32)HostErrors));
}

//-----------------------------------------------------------------------------
JS_CLASS(SubdevConst);
static SObject SubdevConst_Object
(
    "SubdevConst",
    SubdevConstClass,
    0,
    0,
    "SubdevConst JS Object"
);
// These have moved to JsPexDev, but need to keep them here for backwards
// compatibility with external scripts
PROP_CONST(SubdevConst, CORR_ID, PexErrorCounts::CORR_ID);
PROP_CONST(SubdevConst, NON_FATAL_ID, PexErrorCounts::NON_FATAL_ID);
PROP_CONST(SubdevConst, FATAL_ID, PexErrorCounts::FATAL_ID);
PROP_CONST(SubdevConst, UNSUP_REQ_ID, PexErrorCounts::UNSUP_REQ_ID);
PROP_CONST(SubdevConst, DETAILED_LINEERROR_ID, PexErrorCounts::DETAILED_LINEERROR_ID);
PROP_CONST(SubdevConst, DETAILED_CRC_ID, PexErrorCounts::DETAILED_CRC_ID);
PROP_CONST(SubdevConst, DETAILED_NAKS_R_ID, PexErrorCounts::DETAILED_NAKS_R_ID);
PROP_CONST(SubdevConst, DETAILED_NAKS_S_ID, PexErrorCounts::DETAILED_NAKS_S_ID);
PROP_CONST(SubdevConst, DETAILED_FAILEDL0SEXITS_ID, PexErrorCounts::DETAILED_FAILEDL0SEXITS_ID);

PROP_CONST(SubdevConst, GPU_XMIT_L0S_ID, JsGpuSubdevice::GPU_XMIT_L0S_ID);
PROP_CONST(SubdevConst, UPSTREAM_XMIT_L0S_ID, JsGpuSubdevice::UPSTREAM_XMIT_L0S_ID);
PROP_CONST(SubdevConst, L1_ID, JsGpuSubdevice::L1_ID);
PROP_CONST(SubdevConst, L1P_ID, JsGpuSubdevice::L1P_ID);
PROP_CONST(SubdevConst, DEEP_L1_ID, JsGpuSubdevice::DEEP_L1_ID);
PROP_CONST(SubdevConst, L1_1_SS_ID, JsGpuSubdevice::L1_1_SS_ID);
PROP_CONST(SubdevConst, L1_2_SS_ID, JsGpuSubdevice::L1_2_SS_ID);
PROP_CONST(SubdevConst, ECC_UNIT_FBPA, LW2080_CTRL_GPU_ECC_UNIT_FBPA);
PROP_CONST(SubdevConst, ECC_UNIT_L2, LW2080_CTRL_GPU_ECC_UNIT_L2);
PROP_CONST(SubdevConst, ECC_UNIT_L1, LW2080_CTRL_GPU_ECC_UNIT_L1);
PROP_CONST(SubdevConst, ECC_UNIT_SM, LW2080_CTRL_GPU_ECC_UNIT_SM);
PROP_CONST(SubdevConst, ECC_UNIT_SM_L1_DATA, LW2080_CTRL_GPU_ECC_UNIT_SM_L1_DATA);
PROP_CONST(SubdevConst, ECC_UNIT_SM_L1_TAG, LW2080_CTRL_GPU_ECC_UNIT_SM_L1_TAG);
PROP_CONST(SubdevConst, ECC_UNIT_SM_CBU, LW2080_CTRL_GPU_ECC_UNIT_SM_CBU);
PROP_CONST(SubdevConst, ECC_UNIT_SHM, LW2080_CTRL_GPU_ECC_UNIT_SHM);
PROP_CONST(SubdevConst, ECC_UNIT_TEX, LW2080_CTRL_GPU_ECC_UNIT_TEX);
PROP_CONST(SubdevConst, ECC_UNIT_SM_ICACHE, LW2080_CTRL_GPU_ECC_UNIT_SM_ICACHE);
PROP_CONST(SubdevConst, ECC_UNIT_GCC, LW2080_CTRL_GPU_ECC_UNIT_GCC);
PROP_CONST(SubdevConst, ECC_UNIT_GPCMMU, LW2080_CTRL_GPU_ECC_UNIT_GPCMMU);
PROP_CONST(SubdevConst, ECC_UNIT_HUBMMU_L2TLB, LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_L2TLB);
PROP_CONST(SubdevConst, ECC_UNIT_HUBMMU_HUBTLB, LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_HUBTLB);
PROP_CONST(SubdevConst, ECC_UNIT_HUBMMU_FILLUNIT, LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_FILLUNIT);
PROP_CONST(SubdevConst, ECC_UNIT_GPCCS, LW2080_CTRL_GPU_ECC_UNIT_GPCCS);
PROP_CONST(SubdevConst, ECC_UNIT_FECS, LW2080_CTRL_GPU_ECC_UNIT_FECS);
PROP_CONST(SubdevConst, ECC_UNIT_PMU, LW2080_CTRL_GPU_ECC_UNIT_PMU);
PROP_CONST(SubdevConst, ECC_UNIT_PCIE_REORDER, ECC_UNIT_PCIE_REORDER);
PROP_CONST(SubdevConst, ECC_UNIT_PCIE_P2P, ECC_UNIT_PCIE_P2PREQ);
PROP_CONST(SubdevConst, ECC_SUPPORTED_IDX, 0);
PROP_CONST(SubdevConst, ECC_ENABLED_MASK_IDX, 1);
PROP_CONST(SubdevConst, ECC_CONFIG_DISABLED, LW2080_CTRL_GPU_ECC_CONFIGURATION_DISABLED);
PROP_CONST(SubdevConst, ECC_CONFIG_ENABLED, LW2080_CTRL_GPU_ECC_CONFIGURATION_ENABLED);
PROP_CONST(SubdevConst, L2_DISABLED, GpuSubdevice::L2_DISABLED);
PROP_CONST(SubdevConst, L2_ENABLED, GpuSubdevice::L2_ENABLED);
PROP_CONST(SubdevConst, EDC_SUPPORTED_IDX, 0);
PROP_CONST(SubdevConst, EDC_ENABLED_MASK_IDX, 1);
PROP_CONST(SubdevConst, EDC_UNIT_RD, 0);
PROP_CONST(SubdevConst, EDC_UNIT_WR, 1);
PROP_CONST(SubdevConst, EDC_UNIT_REPLAY, 2);
PROP_CONST(SubdevConst, VBIOS_OK, GpuSubdevice::VBIOS_OK);
PROP_CONST(SubdevConst, VBIOS_ERROR, GpuSubdevice::VBIOS_ERROR);
PROP_CONST(SubdevConst, VBIOS_WARNING, GpuSubdevice::VBIOS_WARNING);
PROP_CONST(SubdevConst, VBIOSTYPE_ILWALID, LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_ILWALID);
PROP_CONST(SubdevConst, VBIOSTYPE_UNSIGNED, LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_UNSIGNED);
PROP_CONST(SubdevConst, VBIOSTYPE_LWIDIA_DEBUG, LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_DEBUG);
PROP_CONST(SubdevConst, VBIOSTYPE_LWIDIA_RELEASE, LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_RELEASE);
PROP_CONST(SubdevConst, VBIOSTYPE_LWIDIA_AE_DEBUG, LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_AE_DEBUG);
PROP_CONST(SubdevConst, VBIOSTYPE_PARTNER_DEBUG, LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_PARTNER_DEBUG);
PROP_CONST(SubdevConst, VBIOSTYPE_PARTNER, LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_PARTNER);
PROP_CONST(SubdevConst, VBIOSSEC_OK, LW2080_CTRL_BIOS_INFO_STATUS_OK);
PROP_CONST(SubdevConst, VBIOSSEC_ILWALID, LW2080_CTRL_BIOS_INFO_STATUS_ILWALID);
PROP_CONST(SubdevConst, VBIOSSEC_EXPIRED, LW2080_CTRL_BIOS_INFO_STATUS_EXPIRED);
PROP_CONST(SubdevConst, VBIOSSEC_DEVID_MISMATCH, LW2080_CTRL_BIOS_INFO_STATUS_DEVID_MISMATCH);
PROP_CONST(SubdevConst, VBIOSSEC_SIG_VERIFICATION_FAILURE, LW2080_CTRL_BIOS_INFO_STATUS_SIG_VERIFICATION_FAILURE);
PROP_CONST(SubdevConst, ISM_CTRL_TRIGGER_off, Ism::LW_ISM_CTRL_TRIGGER_off);
PROP_CONST(SubdevConst, ISM_CTRL_TRIGGER_pri, Ism::LW_ISM_CTRL_TRIGGER_pri);
PROP_CONST(SubdevConst, ISM_CTRL_TRIGGER_pmm, Ism::LW_ISM_CTRL_TRIGGER_pmm);
PROP_CONST(SubdevConst, ISM_CTRL_TRIGGER_now, Ism::LW_ISM_CTRL_TRIGGER_now);
PROP_CONST(SubdevConst, ISM_KEEP_ACTIVE, Ism::KEEP_ACTIVE);
PROP_CONST(SubdevConst, ISM_HOLD_AUTO,   IsmSpeedo::HOLD_AUTO);
PROP_CONST(SubdevConst, ISM_HOLD_MANUAL, IsmSpeedo::HOLD_MANUAL);
PROP_CONST(SubdevConst, ISM_HOLD_OSC,    IsmSpeedo::HOLD_OSC);
PROP_CONST(SubdevConst, PPC_TCE_BYPASS_DEFAULT,   MODSDRV_PPC_TCE_BYPASS_DEFAULT);
PROP_CONST(SubdevConst, PPC_TCE_BYPASS_FORCE_ON,  MODSDRV_PPC_TCE_BYPASS_FORCE_ON);
PROP_CONST(SubdevConst, PPC_TCE_BYPASS_FORCE_OFF, MODSDRV_PPC_TCE_BYPASS_FORCE_OFF);

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetPcieErrorCounts,
                3,
                "Read PEX error counters")
{
    STEST_HEADER(2, 3,
            "var LocErrors = new Array;\n"
            "var HostErrors = new Array;\n"
            "var Subdev = new GpuSubdev(0,0);\n"
            "Subdev.GetPcieErrorCounts(LocErrors, HostErrors, CountType)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetLocErrors);
    STEST_ARG(1, JSObject*, pRetHostErrors);
    STEST_OPTIONAL_ARG(2, UINT32, temp, PexDevice::PEX_COUNTER_ALL);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    PexErrorCounts LocErrors;
    PexErrorCounts HostErrors;
    C_CHECK_RC(pSubdev->GetInterface<Pcie>()->GetErrorCounts(&LocErrors,
                                                  &HostErrors,
                            static_cast<PexDevice::PexCounterType>(temp)));
    C_CHECK_RC(JsPexDev::PexErrorCountsToJs(pRetLocErrors, &LocErrors));
    RETURN_RC(JsPexDev::PexErrorCountsToJs(pRetHostErrors, &HostErrors));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                ResetPcieErrorCounters,
                0,
                "Reset PEX error counters")
{
    STEST_HEADER(0, 0, "Usage: Subdev.ResetPcieErrorCounters()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pSubdev->GetInterface<Pcie>()->ResetErrorCounters());
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                ForcePcieErrorCountersToZero,
                1,
                "Force PCIE error counters to report zero")
{
    STEST_HEADER(1, 1, "Usage: Subdev.ForcePcieErrorCountersToZero(true/false)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, bool, ForceZero);

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    pGpuPcie->SetForceCountersToZero(ForceZero);
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GetPcieLanes,
                  1,
                  "Get PCIE Rx and Tx lanes")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetPcieLanes(Array)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pArrayToReturn);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    UINT32 RxLanes, TxLanes;

    C_CHECK_RC(pSubdev->GetInterface<Pcie>()->GetPcieLanes(&RxLanes, &TxLanes));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, RxLanes));
    RETURN_RC(pJavaScript->SetElement(pArrayToReturn, 1, TxLanes));

}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GetPcieDetectedLanes,
                  1,
                  "Get PCIE detected lanes")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetPcieDetectedLanes(Lanes)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    UINT32 Lanes;

    C_CHECK_RC(pSubdev->GetInterface<Pcie>()->GetPcieDetectedLanes(&Lanes));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, Lanes));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetPcieLinkSpeed,
                1,
                "Set PCIE Link Speed")
{
    STEST_HEADER(1, 1, "Usage: Subdev.SetPcieLinkSpeed(2500/5000/8000/16000)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, NewSpeed);

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    RETURN_RC(pGpuPcie->SetLinkSpeed((Pci::PcieLinkSpeed)NewSpeed));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetPcieLinkWidth,
                1,
                "Set PCIE Link Width")
{
    STEST_HEADER(1, 1, "Usage: Subdev.SetPcieLinkWidth(1/2/4/8/16)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, Width);

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pSubdev->GetInterface<Pcie>()->SetLinkWidth(Width));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetLinkWidths,
                1,
                "Read Gpu link width on both rcv and trx ends")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetLinkWidths(Array)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pArrayToReturn);

    static Deprecation depr("GpuSubdevice.GetLinkWidths", "3/10/2017",
                            "GetLinkWidths is deprecated. Please use GetPcieLinkWidths.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    UINT32 LocWidth, HostWidth;
    C_CHECK_RC(pSubdev->GetInterface<Pcie>()->GetLinkWidths(&LocWidth, &HostWidth));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, LocWidth));
    RETURN_RC(pJavaScript->SetElement(pArrayToReturn, 1, HostWidth));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetPcieLinkWidths,
                1,
                "Read Gpu link width on both rcv and trx ends")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetPcieLinkWidths(Array)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pArrayToReturn);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    UINT32 LocWidth, HostWidth;
    C_CHECK_RC(pSubdev->GetInterface<Pcie>()->GetLinkWidths(&LocWidth, &HostWidth));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, LocWidth));
    RETURN_RC(pJavaScript->SetElement(pArrayToReturn, 1, HostWidth));
}

P_( JsGpuSubdevice_Get_UpStreamPexDev);
static SProperty UpStreamPexDev
(
    JsGpuSubdevice_Object,
    "UpStreamPexDev",
    0,
    0,
    JsGpuSubdevice_Get_UpStreamPexDev,
    0,
    JSPROP_READONLY,
    "acquire the PexDev above - null if nothing"
);
P_( JsGpuSubdevice_Get_UpStreamPexDev )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        PexDevice* pPexDev;
        if ((pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev) != OK) ||
            !pPexDev)
        {
// NEED to get PciRead/Write to work on Mac
#ifndef LW_MACINTOSH
#endif
            *pValue = JSVAL_NULL;
            return JS_TRUE;
        }

        JsPexDev * pJsPexDev = new JsPexDev();
        MASSERT(pJsPexDev);
        pJsPexDev->SetPexDev(pPexDev);
        if (pJsPexDev->CreateJSObject(pContext, pObject) != OK)
        {
            delete pJsPexDev;
            JS_ReportError(pContext, "Unable to create JsPexDev JSObject");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }

        if (pJs->ToJsval(pJsPexDev->GetJSObject(), pValue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_( JsGpuSubdevice_Get_UpStreamIndex );
static SProperty UpStreamIndex
(
    JsGpuSubdevice_Object,
    "UpStreamIndex",
    0,
    0,
    JsGpuSubdevice_Get_UpStreamIndex,
    0,
    JSPROP_READONLY,
    "returns upstream device's port number at which the Gpu is connected"
);
P_( JsGpuSubdevice_Get_UpStreamIndex )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        PexDevice* pPexDev;
        UINT32 PortId = 0;
        pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &PortId);

        if (pJs->ToJsval(PortId, pValue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
P_( JsGpuSubdevice_Get_L2Mode );
P_( JsGpuSubdevice_Set_L2Mode );
static SProperty L2Mode
(
    JsGpuSubdevice_Object,
    "L2Mode",
    0,
    0,
    JsGpuSubdevice_Get_L2Mode,
    JsGpuSubdevice_Set_L2Mode,
    0,
    "returns upstream device's port number at which the Gpu is connected"
);
P_( JsGpuSubdevice_Get_L2Mode )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);
    JavaScriptPtr pJs;
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        GpuSubdevice::L2Mode Mode;
        if (OK != pSubdev->GetL2Mode(&Mode))
        {
            JS_ReportError(pContext, "GpuSubdev::GetL2Mode failed");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }
        if (pJs->ToJsval((UINT32)Mode, pValue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}
P_( JsGpuSubdevice_Set_L2Mode )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);
    JavaScriptPtr pJs;
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        UINT32 Mode;
        if (OK != pJs->FromJsval(*pValue, &Mode))
        {
            JS_ReportError(pContext, "Can't read L2Mode from JS");
            return JS_FALSE;
        }
        if (OK != pSubdev->SetL2Mode((GpuSubdevice::L2Mode)Mode))
        {
            JS_ReportError(pContext, "GpuSubdev::SetL2Mode failed");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }
        return JS_TRUE;
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                RegWr32Multiple,
                1,
                "Write multiple registers quickly.")
{
    STEST_HEADER(1, 1,
            "Usage: GpuSubdevice.RegWr32Multiple([reg, val, reg, val, ...])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, vector<UINT32>, RegValPairs);

    // Must have even number of entries
    if ((RegValPairs.size() % 2) != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    size_t Size = RegValPairs.size();
    for (UINT32 j = 0; j < Size; j += 2)
    {
        pJsGpuSubdevice->GetGpuSubdevice()->RegWr32(RegValPairs[j],
                                                    RegValPairs[j + 1]);
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  EarlyRegWr,
                  1,
                  "Write early registers.")
{
    STEST_HEADER(1, 1,
            "Usage: GpuSubdevice.EarlyRegWr([reg, val, reg, val, ...])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JsArray, ArrayOfJsVals);
    RC rc;

    JsArray PairOfJsVals;
    size_t Size = ArrayOfJsVals.size();

    for(size_t i = 0; i < Size; ++i)
    {
        string reg;
        UINT32 value;
        C_CHECK_RC(pJavaScript->FromJsval(ArrayOfJsVals[i], &PairOfJsVals));
        C_CHECK_RC(pJavaScript->FromJsval(PairOfJsVals[0], &reg));
        C_CHECK_RC(pJavaScript->FromJsval(PairOfJsVals[1], &value));

        pJsGpuSubdevice->GetGpuSubdevice()->SetEarlyReg(reg, value);
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetIntThermCalibration,
                2,
                "Sets the internal thermal calibration constants")
{
    STEST_HEADER(2, 2,
            "Usage: GpuSubdevice.SetIntThermCalibration(ValA, ValB)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, ValueA);
    STEST_ARG(1, UINT32, ValueB);

    RETURN_RC(pJsGpuSubdevice->GetGpuSubdevice()->SetIntThermCalibration(ValueA, ValueB));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetAspmState,
                1,
                "Set ASPM setting on GPU.")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.SetAspmState(ASPM)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, AspmState);

    GpuSubdevice * pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    RETURN_RC(pGpuPcie->SetAspmState((Pci::ASPMState)AspmState));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetClock,
                4,
                "Set a GPU clock domain's frequency.")
{
    STEST_HEADER(2, 4, "Usage: GpuSubdevice.SetClock(ClkWhich, NewTargetHz, [PStateNum], [Flags])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, WhichClk);
    STEST_ARG(1, UINT64, TargetHz);
    STEST_OPTIONAL_ARG(2, INT32, PStateNum, -1);
    STEST_OPTIONAL_ARG(3, UINT32, Flags, 0);

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    if (PStateNum < 0)
    {
        // Pstate is not passed to the function
        // Derive it internally
        RETURN_RC(pGpuSubdevice->SetClock((Gpu::ClkDomain)WhichClk, TargetHz));
    }
    // Pstate is explicitly passed
    RETURN_RC(pGpuSubdevice->SetClock((Gpu::ClkDomain)WhichClk,
                                      TargetHz, PStateNum, Flags));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetClock,
                2,
                "Read the [ActualPllHz, TargetPllHz, SlowedHz, Src] of one of the ClkXX clocks")
{
    STEST_HEADER(2, 2, "Usage: GpuSubdevice.GetClock(ClkWhich, [actualPllHz,targetPllHz,slowedHz,src])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, WhichClk);
    STEST_ARG(1, JSObject*, pReturlwals);

    RC rc;
    Gpu::ClkSrc Src;
    UINT64 ActualPllHz;
    UINT64 TargetPllHz;
    UINT64 SlowedHz;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetClock((Gpu::ClkDomain)WhichClk,
                                       &ActualPllHz,
                                       &TargetPllHz,
                                       &SlowedHz,
                                       &Src));

    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, ActualPllHz));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 1, TargetPllHz));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 2, SlowedHz));
    RETURN_RC( pJavaScript->SetElement(pReturlwals, 3, (UINT32)Src));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                MeasureClock,
                3,
                "Measure the real frequency of any particular PLL available for a clock by using clock counters")
{
    STEST_HEADER(3, 3, "Usage: GpuSubdevice.MeasureClock(ClkWhich, PllIndex, RetVal)");
    Printf(Tee::PriNormal,
           "Warning: This API has been deprecated in favor of the MeasureClock "
           "found in GpuSubdevice.Perf (MeasureClock, MeasurePllClock, "
           "MeasureClockPartitions)\n");

    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, WhichClk);
    STEST_ARG(1, UINT32, PllIndex);
    STEST_ARG(2, JSObject*, pReturlwal);

    RC rc;
    UINT64 FreqHz;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetPerf()->MeasurePllClock(
        (Gpu::ClkDomain)WhichClk, PllIndex, &FreqHz));
    RETURN_RC(pJavaScript->SetElement(pReturlwal, 0, FreqHz));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetPllCount,
                2,
                "Get the PLL count of a particular clock domain")
{
    STEST_HEADER(2, 2, "Usage: GpuSubdevice.MeasureClock(ClkWhich, RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, WhichClk);
    STEST_ARG(1, JSObject*, pReturlwal);

    RC rc;
    UINT32 count;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetPllCount((Gpu::ClkDomain)WhichClk,
                                            &count));
    RETURN_RC(pJavaScript->SetElement(pReturlwal, 0, count));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetSysPll,
                1,
                "Set the SysPll to the specified frequency in Hz")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.SetSysPll(targetHz)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT64, targetHz);

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pGpuSubdevice->SetSysPll(targetHz));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetAteSpeedo,
                2,
                "Get speedo number from ATE burnt fuse")
{
    STEST_HEADER(2, 2, "Usage: GpuSubdevice.GetAteSpeedo(ArrayToStoreAllSpeedos, ArrayToStoreVersion)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);
    STEST_ARG(1, JSObject*, pRetVersion);

    RC rc;
    UINT32 Speedo = 0;
    UINT32 Version = 0;
    bool AtLeastOneGetSuccessful = false;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    for (UINT32 i = 0; i < pGpuSubdevice->NumAteSpeedo(); i++)
    {
        if (OK == pGpuSubdevice->GetAteSpeedo(i, &Speedo, &Version))
        {
            AtLeastOneGetSuccessful = true;
        }
        else
        {
            Speedo = 0;
        }
        C_CHECK_RC(pJavaScript->SetElement(pRetVal, i, Speedo));
    }
    if (!AtLeastOneGetSuccessful)
    {
        C_CHECK_RC(RC::UNSUPPORTED_FUNCTION);
    }
    RETURN_RC(pJavaScript->SetElement(pRetVersion, 0, Version));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetAteSpeedoOverride,
                1,
                "Get overridden speedo value from ATE burnt fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetAteSpeedoOverride(ReturlwalueArray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 Speedo;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetAteSpeedoOverride(&Speedo));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, Speedo));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetKappaInfo,
                3,
                "Get the kappa info from the burnt fuse")
{
    STEST_HEADER(2, 3, "Usage: GpuSubdevice.GetKappaInfo(ArrayToStoreValue, ArrayToStoreVersion, ArrayToStoreValid(optional))");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);
    STEST_ARG(1, JSObject*, pRetVersion);
    STEST_OPTIONAL_ARG(2, JSObject*, pRetValid, nullptr);

    RC rc;
    UINT32 kappa = 0;
    UINT32 version = 0;
    INT32 valid = 0;
    GpuSubdevice* pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetKappaInfo(&kappa, &version, &valid));
    C_CHECK_RC(pJavaScript->SetElement(pRetVal, 0, kappa));
    C_CHECK_RC(pJavaScript->SetElement(pRetVersion, 0, version));
    if (pRetValid != NULL)
    {
        C_CHECK_RC(pJavaScript->SetElement(pRetValid, 0, valid));
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                KappaValueToFuse,
                4,
                "Get the kappa Fuse from given kappa value, version and valid")
{
    STEST_HEADER(4, 4, "Usage: GpuSubdevice.KappaValueToFuse(KappaValue, KappaVersion, KappaValid, ArrayToStoreFuseValue");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, FLOAT32, kappaValue);
    STEST_ARG(1, UINT32, version);
    STEST_ARG(2, UINT32, valid);
    STEST_ARG(3, JSObject*, pRetFuseValue);

    RC rc;
    UINT32 fuseValue = 0;
    GpuSubdevice* pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->KappaValueToFuse(kappaValue, version, valid, &fuseValue));
    RETURN_RC(pJavaScript->SetElement(pRetFuseValue, 0, fuseValue));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetAteIddq,
                1,
                "Get Iddq number from ATE burnt fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetAteIddq(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 Iddq    = 0;
    UINT32 Version = 0;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetAteIddq(&Iddq, &Version));
    C_CHECK_RC(pJavaScript->SetElement(pRetVal, 0, Iddq));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 1, Version));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetAteMsvddIddq,
                1,
                "Get MSVDD Iddq number from ATE burnt fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetAteMsvddIddq(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 Iddq    = 0;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetAteMsvddIddq(&Iddq));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, Iddq));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetAteSramIddq,
                1,
                "Get SRAM Iddq number from ATE burnt fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetAteSramIddq(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 Iddq    = 0;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetAteSramIddq(&Iddq));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, Iddq));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetAteRev,
                1,
                "Get ATE revision from OPT fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetAteRev(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 Revision = 0;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetAteRev(&Revision));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, Revision));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetSramBin,
                1,
                "Get ATE SRAM Bin from the burnt fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetSramBin(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    INT32 bin = 0;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetSramBin(&bin));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, bin));
}
//-----------------------------------------------------------------------------
JS_CLASS(IsmConst);
static SObject IsmConst_Object
(
    "IsmConst",
    IsmConstClass,
    0,
    0,
    "IsmConst JS Object"
);
PROP_CONST(IsmConst, COMP,      IsmSpeedo::COMP);
PROP_CONST(IsmConst, BIN,       IsmSpeedo::BIN);
PROP_CONST(IsmConst, METAL,     IsmSpeedo::METAL);
PROP_CONST(IsmConst, MINI,      IsmSpeedo::MINI);
PROP_CONST(IsmConst, TSOSC,     IsmSpeedo::TSOSC);
PROP_CONST(IsmConst, VNSENSE,   IsmSpeedo::VNSENSE);
PROP_CONST(IsmConst, NMEAS,     IsmSpeedo::NMEAS);
PROP_CONST(IsmConst, HOLD,      IsmSpeedo::HOLD);
PROP_CONST(IsmConst, AGING,     IsmSpeedo::AGING);
PROP_CONST(IsmConst, BIN_AGING, IsmSpeedo::BIN_AGING);
PROP_CONST(IsmConst, AGING2,    IsmSpeedo::AGING2);
PROP_CONST(IsmConst, CPM,       IsmSpeedo::CPM);
PROP_CONST(IsmConst, IMON,      IsmSpeedo::IMON);
PROP_CONST(IsmConst, CTRL,      IsmSpeedo::CTRL);
PROP_CONST(IsmConst, ISINK,     IsmSpeedo::ISINK);
PROP_CONST(IsmConst, ALL,       IsmSpeedo::ALL);

PROP_CONST(IsmConst, FLAGS_NONE,        Ism::FLAGS_NONE);
PROP_CONST(IsmConst, KEEP_ACTIVE,       Ism::KEEP_ACTIVE);
PROP_CONST(IsmConst, PRIMARY,           Ism::PRIMARY);
PROP_CONST(IsmConst, STATE1,            Ism::STATE_1);
PROP_CONST(IsmConst, STATE2,            Ism::STATE_2);
PROP_CONST(IsmConst, STATE3,            Ism::STATE_3);
PROP_CONST(IsmConst, STATE4,            Ism::STATE_4);

PROP_CONST(IsmConst, TRIG_OFF,          Ism::LW_ISM_CTRL_TRIGGER_off);
PROP_CONST(IsmConst, TRIG_PRI,          Ism::LW_ISM_CTRL_TRIGGER_pri);
PROP_CONST(IsmConst, TRIG_PMM,          Ism::LW_ISM_CTRL_TRIGGER_pmm);
PROP_CONST(IsmConst, TRIG_NOW,          Ism::LW_ISM_CTRL_TRIGGER_now);

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                TriggerIsmExp,
                0,
                "Force an ISM trigger via priv reg write")
{
    STEST_HEADER(0, 0, "Usage: GpuSubdevice.TriggerIsmExp()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->TriggerISMExperiment());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetIsmClkKhz,
                1,
                "Get ISM clk frequency in KHz")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetIsmClkKHz(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 clk    = 0;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM = NULL;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->GetISMClkFreqKhz(&clk));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, clk));
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                GetNumISMChains,
                0,
                "Get number of ISM chains for this GPU")
{
    STEST_HEADER(0, 0, "Usage: GpuSubdevice.GetNumISMChains(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    RC rc = RC::OK;
    INT32 numChains = 0;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM = NULL;
    if (pGpuSubdevice->GetISM(&pISM) == RC::OK)
    {
        numChains = pISM->GetNumISMChains();
    }
    if (pJavaScript->ToJsval(numChains, pReturlwalue) != RC::OK)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                GetNumISMs,
                1,
                "Get number of specific type of ISMs ")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetNumISMs(ismType)"); //$
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, ismType);

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM = NULL;
    RC rc = pGpuSubdevice->GetISM(&pISM);

    // If rc is not OK, then we will just return zero for the number of ISMs. There is no other
    // reasonable action to take when we get any other error code.
    INT32 numISMs = 0;  // default to zero in case we get an error.
    if (rc == RC::OK)
    {
        numISMs = pISM->GetNumISMs(static_cast<IsmSpeedo::PART_TYPES>(ismType));
    }
    if (pJavaScript->ToJsval(numISMs, pReturlwalue) != RC::OK)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Helper functon for
//
// Copy the info properties from the JS object to a C++ structure.
RC ISMInfoFromJSObject(
    JSObject    * pJsObj
    ,IsmInfo  * pInfo
)
{
    JavaScriptPtr pJs;
    StickyRC rc;
    CHECK_RC_MSG(pJs->GetProperty(pJsObj, "version", &pInfo->version), "Failed to get \"version\" property"); //$
    switch (pInfo->version)
    {
        case 3:
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "spareIn",   &pInfo->spareIn),  "Failed to get \"spareIn\" property");  //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "spareOut",  &pInfo->spareOut), "Failed to get \"spareOut\" property");  //$
        case 2:
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "delaySel",   &pInfo->delaySel), "Failed to get \"delaySel\" property");  //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "analogSel",   &pInfo->analogSel), "Failed to get \"analogSel\" property");  //$
        case 1: //
            //Mandatory properties
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "chainId",   &pInfo->chainId),   "Failed to get \"chainId\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "chiplet",   &pInfo->chiplet),   "Failed to get \"chiplet\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "startBit",  &pInfo->startBit),  "Failed to get \"startBit\" property");  //$
            // oscIdx aka srcSel
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "oscIdx",    &pInfo->oscIdx),    "Failed to get \"oscIdx\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "outDiv",    &pInfo->outDiv),    "Failed to get \"outDiv\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "mode",      &pInfo->mode),      "Failed to get \"mode\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "init",      &pInfo->init),      "Failed to get \"init\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "finit",     &pInfo->finit),     "Failed to get \"finit\" property");     //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "adj",       &pInfo->adj),       "Failed to get \"adj\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "clkKHz",    &pInfo->clkKHz),    "Failed to get \"clkKHz\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "count",     &pInfo->count),     "Failed to get \"count\" property");     //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "flags",     &pInfo->flags),     "Failed to get \"flags\" property");     //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "refClkSel", &pInfo->refClkSel), "Failed to get \"refClkSel\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "duration",  &pInfo->duration),  "Failed to get \"duration\" property");  //$
            break;
        default:
            Printf(Tee::PriError, "Unknown IsmInfo version:%d\n", pInfo->version);
            return RC::BAD_PARAMETER;
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
/*virtual*/
RC IsmInfoToJsObj                       //!< Colwert ISM data to JS object
(
    const vector<UINT32>    &speedos         //!< Measured ring cycles
    , const vector<IsmInfo> &info            //!< ISM info
    , PART_TYPES            SpeedoType       //!< Type of speedo
    , JSContext             *pContext        //!< JS context
    , JSObject              *pReturlwals     //!< JS object pointer
)
{
    RC rc;
    JSObject *pSpeedoVals;
    JSObject *pSpeedoInfo;
    jsval tmpjs;
    JavaScriptPtr pJs;
    if (pJs->GetProperty(pReturlwals, "Values", &pSpeedoVals) == RC::OK &&
        pJs->GetProperty(pReturlwals, "Info", &pSpeedoInfo) == RC::OK)
    {
        // Use new interface:
        // var = x{Info:[{chainId:0, chiplet:0, startBit:0, oscIdx:0, mode:0}, ...], Values:[]};
        for (UINT32 idx = 0; idx < speedos.size(); idx++)
        {
            CHECK_RC(pJs->SetElement(pSpeedoVals, idx, speedos[idx]));

            JSObject *pInfo = JS_NewObject(pContext, &DummyClass, 0, 0);
            switch (info[idx].version)
            { 
                case 3: // fall through
                    CHECK_RC(pJs->SetProperty(pInfo, "spareIn",   info[idx].spareIn));
                    CHECK_RC(pJs->SetProperty(pInfo, "spareOut",  info[idx].spareOut));
                case 2: // fall through
                    CHECK_RC(pJs->SetProperty(pInfo, "delaySel", info[idx].delaySel));
                    CHECK_RC(pJs->SetProperty(pInfo, "analogSel", info[idx].analogSel));
                case 1: 
                    CHECK_RC(pJs->SetProperty(pInfo, "version", info[idx].version));
                    CHECK_RC(pJs->SetProperty(pInfo, "chainId", info[idx].chainId));
                    CHECK_RC(pJs->SetProperty(pInfo, "chiplet", info[idx].chiplet));
                    CHECK_RC(pJs->SetProperty(pInfo, "startBit", info[idx].startBit));
                    CHECK_RC(pJs->SetProperty(pInfo, "oscIdx", info[idx].oscIdx));
                    CHECK_RC(pJs->SetProperty(pInfo, "outDiv", info[idx].outDiv));
                    CHECK_RC(pJs->SetProperty(pInfo, "mode", info[idx].mode));
                    CHECK_RC(pJs->SetProperty(pInfo, "duration", info[idx].duration));
                    CHECK_RC(pJs->SetProperty(pInfo, "clkKHz", info[idx].clkKHz));
                    CHECK_RC(pJs->SetProperty(pInfo, "count", info[idx].count));
                    CHECK_RC(pJs->SetProperty(pInfo, "adj", info[idx].adj));
                    CHECK_RC(pJs->SetProperty(pInfo, "init", info[idx].init));
                    CHECK_RC(pJs->SetProperty(pInfo, "finit", info[idx].finit));
                    CHECK_RC(pJs->SetProperty(pInfo, "flags", info[idx].flags));
                    CHECK_RC(pJs->SetProperty(pInfo, "roEnable", info[idx].roEnable));
                    CHECK_RC(pJs->SetProperty(pInfo, "refClkSel", info[idx].refClkSel));
                    CHECK_RC(pJs->SetProperty(pInfo, "dcdEn", info[idx].dcdEn));
                    CHECK_RC(pJs->SetProperty(pInfo, "outSel", info[idx].outSel));
                    CHECK_RC(pJs->SetProperty(pInfo, "pactSel", info[idx].pactSel));
                    CHECK_RC(pJs->SetProperty(pInfo, "nactSel", info[idx].nactSel));
                    CHECK_RC(pJs->SetProperty(pInfo, "srcClkSel", info[idx].srcClkSel)); // all
                    CHECK_RC(pJs->SetProperty(pInfo, "dcdCount", info[idx].dcdCount)); // v3 & v4
                    break;
                default:
                    Printf(Tee::PriError, "Unknown version:%d\n", info[idx].version);
                    return RC::SOFTWARE_ERROR;
            }
            CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
            CHECK_RC(pJs->SetElement(pSpeedoInfo, idx, tmpjs));
        }
    }
    else
    {
        // Use legacy interface. var x = new JsArray
        for (UINT32 idx = 0; idx < speedos.size(); idx++)
        {
            CHECK_RC(pJs->SetElement(pReturlwals, idx, speedos[idx]));
        }
    }
    return rc;
}


//------------------------------------------------------------------------------
// This is the helper function for all of the new ReadISM* APIs that will replace all of the
// ReadSpeedo* APIs. This helper will read all of the test parameters from a JS object instead
// of trying to parse the parameter list. The JS object is version controlled so anytime
// new fields are added to the object we have to bump the version and update the
// ISMInfoFromJSObject() to read in the new fields.
// Note: We are moving to using JS objects to identify the parameters because new versions of
//       these ISM always have additional fields to programm and trying to keep a backward
//       compatible argument list is getting too complicated.
static JSBool ReadISMHelper(
    JSContext   * pContext
    ,JSObject   * pObject
    ,uintN        NumArguments
    ,jsval      * pArguments
    ,jsval      * pReturlwalue
    ,const char * usage
    ,PART_TYPES SpeedoType
)
{
    // Check the mandatory arguments.
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pObject      !=0);

    StickyRC rc;
    vector<IsmInfo> IsmSettings;
    JavaScriptPtr pJavaScript;

    JSObject * pReturlwals;
    if (RC::OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
    {
        JS_ReportError(pContext, "Invalid value for %s\n%s", "pReturlwals", usage); //$
        return JS_FALSE;
    }

    JSObject * pJsIsmInfo;
    if (RC::OK != pJavaScript->FromJsval(pArguments[1], &pJsIsmInfo))
    {
        JS_ReportError(pContext, "Invalid value for %s\n%s", "pJsIsmInfo", usage); //$
        return JS_FALSE;
    }
    // pArguments[1] can be either a JsObject or JsArray of JsObjects
    if (!JS_IsArrayObject(pContext, pJsIsmInfo))
    {   // Its not an array of objects. So verify the single object can be used on all ISMs.
        IsmSettings.resize(1);
        C_CHECK_RC(ISMInfoFromJSObject(pJsIsmInfo, &IsmSettings[0]));
    }
    else
    {   // Work with the array of IsmInfo objects
        JsArray JsIsmInfoArray;
        pJavaScript->FromJsval(pArguments[1], &JsIsmInfoArray);
        IsmSettings.resize(JsIsmInfoArray.size());
        for (UINT32 i = 0; i < JsIsmInfoArray.size(); i++)
        {
            C_CHECK_RC(pJavaScript->FromJsval(JsIsmInfoArray[i], &pJsIsmInfo));
            C_CHECK_RC(ISMInfoFromJSObject(pJsIsmInfo, &IsmSettings[i]));
        }
    }

    if (IsmSettings[0].duration == 0)
    {
        JS_ReportError(pContext, "%s\nDuration can't be zero!\n", usage); //$
        RETURN_RC(RC::BAD_PARAMETER);
    }

    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, "GpuSubdevice")) != 0)
    {
        GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
        vector<UINT32> Speedos;
        vector<IsmInfo> Info;
        C_CHECK_RC(pGpuSubdevice->ReadSpeedometers(&Speedos,
                                                   &Info,
                                                   SpeedoType,
                                                   &IsmSettings,
                                                   IsmSettings[0].duration));
        // Copy the results to the JS object.
        C_CHECK_RC(IsmInfoToJsObj(Speedos, Info, SpeedoType, pContext, pReturlwals));
        RETURN_RC(rc);
    }
    return JS_TRUE;
}

//-------------------------------------------------------------------------------------------------
// Read the BIN ISMs from the GPU.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadBinISMs,
                2,
                "Read the ring-oscillator counts for each BIN ISM.")
{
    STEST_HEADER(2, 2,
        "Usage: GpuSubdevice.ReadBinISMs(returnArray, JsIsmInfoObject");

    return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue, usage, BIN);
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. Pascal+ supports it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadAgingISMs,
                3, // max args
                "Read the ring-oscillator counts for each ROSC_BIN_AGING ISMs.")
{
    STEST_HEADER(2, // minArgs
                 3, // maxArgs
                 "Usage: GpuSubdevice.ReadAgingISMs(return_array, JsIsmInfoObject, agingType=0");

    RC rc;
    UINT32 agingType = 0;
    if (NumArguments == 3)
    {
        C_CHECK_RC_MSG(pJavaScript->FromJsval(pArguments[2], &agingType), usage);
    }

    switch (agingType)
    {
        case 0:
            // Read LW_ISM_ROSC_bin_aging and LW_ISM_ROSC_bin_aging_v1 chains
            return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue,
                                  usage, AGING);
        case 1:
            // Include BINs
            return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue,
                                  usage, BIN_AGING);
        case 2:
            // Read LW_ISM_aging and LW_ISM_aging_v1 chains
            return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue,
                                  usage, AGING2);
        default:
        {
            Printf(Tee::PriError, "The agingType parameter must be 0-2");
            return RC::BAD_PARAMETER;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadCompISMs,
                2, // max args
                "Read the ring-oscillator counts for each COMP ISM.")
{
    STEST_HEADER(2, // minArgs
                 2, // maxArgs
        "Usage: GpuSubdevice.ReadCompISMs(return_array, JsIsmInfoObject");
    return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue, usage, COMP);
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadMetalISMs,
                2, // max args
                "Read the ring-oscillator counts for each BIN_METAL ISM.")
{
    STEST_HEADER(2, // minArgs
                 2, // maxArgs
        "Usage: GpuSubdevice.ReadMetalISMs(return_array, JsIsmInfoObject)");
    return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue, usage, METAL);
}

//-------------------------------------------------------------------------------------------------
// Read the BIN ISMs from the GPU.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadImonISMs,
                2,
                "Read the ring-oscillator counts for each IMON ISM.")
{
    STEST_HEADER(2, 2,
        "Usage: GpuSubdevice.ReadImonISMs(returnArray, JsIsmInfoObject");

    return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue, usage, IMON);
}

//-----------------------------------------------------------------------------
// Complete routine to setup the mini ISM, program the master controller, & read back the counts.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadFullMiniISMs,
                2, // max args
                "Read the ring-oscillator counts for each MINI ISM.")
{
    STEST_HEADER(2, // minArgs
                 2, // maxArgs
        "Usage: GpuSubdevice.ReadFullMiniISMs(return_array, JsIsmInfoObject)");
    return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue, usage, MINI);
}


//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadTsoscISMs,
                2, // max args
                "Read the ring-oscillator counts for each TSOSC ISM.")
{
    STEST_HEADER(2, 2,
                 "Usage: GpuSubdevice.ReadTsoscISMs(return_array, JsIsmInfoObject) ");
    return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue, usage, TSOSC);
}

//------------------------------------------------------------------------------
// This function is deprecated on Hopper+ GPUs. The new ReadISMHelper() API should be used going
// forward because it supports a JS object to hold all of the parameters that were initially
// passed to this function.
// The ne function is a cleaner and more consice approach.
static JSBool ReadSpeedoHelper(
    JSContext   * pContext
    ,JSObject   * pObject
    ,uintN        NumArgs
    ,jsval      * pArguments
    ,jsval      * pReturlwalue
    ,const char * usage
    ,PART_TYPES SpeedoType
)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    static Deprecation depr("ReadSpeedoHelper", "7/1/2021",
                            "ReadSpeedoHelper is deprecated. Please use ReadISMHelper.\n");

    // Check the arguments.
    StickyRC rc;
    JavaScriptPtr pJs;
    vector<IsmInfo> IsmSettings;
    IsmInfo IsmArgs;
    // Check the arguments.
    JSObject * pReturlwals = 0;
    UINT32 Duration = ~0;
    UINT32 stdArgs = min(14, (int)NumArgs);
    GpuSubdevice * pGpuSubdevice = nullptr;
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, "GpuSubdevice")) != 0)
    {
        pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
        if (pGpuSubdevice->GetParentDevice()->GetFamily() >= GpuDevice::GpuFamily::Hopper)
        {
            Printf(Tee::PriError, "ReadSpeedoHelper() is deprecated. Please use ReadISMHelper()\n"); //$
            return !OK;
        }
    }

    switch (stdArgs){
        default:
           JS_ReportError(pContext, usage);
           RETURN_RC(RC::BAD_PARAMETER);
        case 14: // fall through
            if (SpeedoType == AGING2)
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[13], &IsmArgs.srcClkSel));
            }
            else
            {
                JS_ReportError(pContext, usage);
                RETURN_RC(RC::BAD_PARAMETER);
            }
        case 13: // fall through
            if (SpeedoType == AGING2)
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[12], &IsmArgs.nactSel));
            }
            else
            {
                JS_ReportError(pContext, usage);
                RETURN_RC(RC::BAD_PARAMETER);
            }
        case 12: // fall through
            if (SpeedoType == AGING2)
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[11], &IsmArgs.pactSel));
            }
            else
            {
                JS_ReportError(pContext, usage);
                RETURN_RC(RC::BAD_PARAMETER);
            }
        case 11: // fall through
            if ((SpeedoType == BIN_AGING) || (SpeedoType == AGING) || (SpeedoType == BIN))
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[10], &IsmArgs.outSel));
            }
            else if (SpeedoType != AGING2) // default the value for AGING2
            {
                JS_ReportError(pContext, usage);
                RETURN_RC(RC::BAD_PARAMETER);
            }
        case 10: // fall through
            if ((SpeedoType == BIN_AGING) || (SpeedoType == AGING))
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[9], &IsmArgs.dcdEn));
            }
            else if (SpeedoType == BIN)
            {
                // Ignored
            }
            else if (SpeedoType != AGING2) // default the value for AGING2
            {
                JS_ReportError(pContext, usage);
                RETURN_RC(RC::BAD_PARAMETER);
            }
        case 9:// pArguments[8] is handled by calling routine.
        case 8: // fall through
            if ((SpeedoType == BIN_AGING) || (SpeedoType == AGING))
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[7], &IsmArgs.roEnable));
            }
            else if (SpeedoType == MINI)
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[7], &IsmArgs.refClkSel));
            }
            else if (SpeedoType == BIN)
            {
                // Ignored
            }
            else if (SpeedoType != AGING2) // default the value for AGING2
            {
                JS_ReportError(pContext, usage);
                RETURN_RC(RC::BAD_PARAMETER);
            }
        case 7: // fall through
            C_CHECK_RC(pJs->FromJsval(pArguments[6], &IsmArgs.chainId));
        case 6: // fall through
            C_CHECK_RC(pJs->FromJsval(pArguments[5], &IsmArgs.chiplet));
        case 5: // fall through (process mode/Adj value)
            if( SpeedoType == TSOSC)
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[4], &IsmArgs.adj));
            }
            else
            {
                C_CHECK_RC(pJs->FromJsval(pArguments[4], &IsmArgs.mode));
            }
        case 4: // fall through
            C_CHECK_RC(pJs->FromJsval(pArguments[3], &IsmArgs.outDiv));
        case 3: // fall through
            C_CHECK_RC(pJs->FromJsval(pArguments[2], &Duration));
        case 2: // fall through
            C_CHECK_RC(pJs->FromJsval(pArguments[1], &IsmArgs.oscIdx));
        case 1: // fall through
            C_CHECK_RC(pJs->FromJsval(pArguments[0], &pReturlwals));
            break;
    }

    IsmSettings.push_back(IsmArgs);

    // process a series of optional arguments in the form of:
    // {startBit, oscIdx, outDiv, mode},...
    while(stdArgs+4 <= NumArgs)
    {
        IsmInfo info;
        C_CHECK_RC(pJs->FromJsval(pArguments[stdArgs++], &info.startBit));
        C_CHECK_RC(pJs->FromJsval(pArguments[stdArgs++], &info.oscIdx));
        C_CHECK_RC(pJs->FromJsval(pArguments[stdArgs++], &info.outDiv));
        C_CHECK_RC(pJs->FromJsval(pArguments[stdArgs++], &info.mode));
        IsmSettings.push_back(info);
    }

    if (Duration == 0)
    {
        JS_ReportError(pContext, "%s\nDuration can't be zero!\n",usage);
        RETURN_RC(RC::BAD_PARAMETER);
    }

    vector<UINT32> Speedos;
    vector<IsmInfo> Info;

    C_CHECK_RC(pGpuSubdevice->ReadSpeedometers(&Speedos,
                                               &Info,
                                               SpeedoType,
                                               &IsmSettings,
                                               Duration));

    C_CHECK_RC(IsmInfoToJsObj(Speedos, Info, SpeedoType, pContext, pReturlwals));
    RETURN_RC(rc);
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadBinSpeedometers,
                11,
                "Read the ring-oscillator counts for each BIN ISM.")
{
    STEST_HEADER(1, 11,
        "Usage: GpuSubdevice.ReadBinSpeedometers(return_array "
        "[,OscIdx=-1,Duration=-1,OutDiv=-1,Mode=-1,Chiplet=-1,InstrId=-1, "
        "ROEnable=0, agingType=0, DcdEn=0, OutSel=0])");

    static Deprecation depr("GpuSubdevice.ReadBinSpeedometers", "7/1/2021",
                            "GpuSubdevice.ReadBinSpeedometers is deprecated."
                            " Please use GpuSubdevice.ReadBinISMs.\n");

    return ReadSpeedoHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue, usage, BIN);

}


//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. Pascal+ supports it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadAgingSpeedometers,
                14, // max args
                "Read the ring-oscillator counts for each ROSC_BIN_AGING ISM.")
{
    STEST_HEADER(1, 14,
        "Usage: GpuSubdevice.ReadAgingSpeedometers(return_array, "
        "[OscIdx=-1, Duration=-1, OutDiv=-1, Mode=-1, Chiplet=-1, InstrId=-1, "
        "ROEnable=0, agingType=0, DcdEn=0, OutSel=0, PactSel=0, NactSel=0, SrcClkSel=0])");

    static Deprecation depr("GpuSubdevice.ReadAgingSpeedometers", "7/1/2021",
                            "GpuSubdevice.ReadAgingSpeedometers is deprecated."
                            " Please use GpuSubdevice.ReadAgingISMs.\n");
    UINT32 agingType = 0;
    if (NumArguments >= 9)
    {
        RC rc;
        C_CHECK_RC_MSG(pJavaScript->FromJsval(pArguments[8], &agingType), usage);
    }

    switch (agingType)
    {
        case 0:
            // Read LW_ISM_ROSC_bin_aging and LW_ISM_ROSC_bin_aging_v1 chains
            return (ReadSpeedoHelper(pContext, pObject, NumArguments, pArguments,
                                     pReturlwalue, usage, AGING));
        case 1:
            // Include BINs
            return (ReadSpeedoHelper(pContext, pObject, NumArguments, pArguments,
                                     pReturlwalue, usage, BIN_AGING));
        case 2:
            // Read LW_ISM_aging and LW_ISM_aging_v1 chains
            return (ReadSpeedoHelper(pContext, pObject, NumArguments, pArguments,
                                     pReturlwalue, usage, AGING2));
        default:
        {
            Printf(Tee::PriError, "The value of agingType should be 0 or 1 or 2");
            return RC::BAD_PARAMETER;
        }
    }
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadCompSpeedometers,
                7,// max args
                "Read the ring-oscillator counts for each COMP ISM.")
{
    STEST_HEADER(1, 7,
        "Usage: GpuSubdevice.ReadCompSpeedometers(return_array "
        "[,OscIdx=-1,Duration=-1,OutDiv=-1,Mode=-1,Chiplet=-1,InstrId=-1])");

    static Deprecation depr("GpuSubdevice.ReadCompSpeedometers", "7/1/2021",
                            "GpuSubdevice.ReadCompSpeedometers is deprecated."
                            " Please use GpuSubdevice.ReadCompISMs.\n");
    return ReadSpeedoHelper(pContext,pObject,NumArguments,pArguments
                            ,pReturlwalue,usage,COMP);
}
//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadMetalSpeedometers,
                7, // max args
                "Read the ring-oscillator counts for each BIN_METAL ISM.")
{
    STEST_HEADER(1, 7,
        "Usage: GpuSubdevice.ReadMetalSpeedometers(return_array "
        "[,OscIdx=-1,Duration=-1,OutDiv=-1,Mode=-1,Chiplet=-1,InstrId=-1])");

    static Deprecation depr("GpuSubdevice.ReadMetalSpeedometers", "7/1/2021",
                            "GpuSubdevice.ReadMetalSpeedometers is deprecated."
                            " Please use GpuSubdevice.ReadMetalISMs.\n");

    return ReadSpeedoHelper(pContext,pObject,NumArguments,pArguments
                            ,pReturlwalue,usage,METAL);
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadMiniSpeedometers,
                8, // max args
                "Read the ring-oscillator counts for each MINI ISM.")
{
    STEST_HEADER(1, 8,
        "Usage: GpuSubdevice.ReadMiniSpeedometers(return_array "
        "[, OscIdx=-1, Duration=-1, OutDiv=-1, Mode=-1, Chiplet=-1, InstrId=-1, RefClkSel=-1])");

    static Deprecation depr("GpuSubdevice.ReadMiniSpeedometers", "7/1/2021",
                            "GpuSubdevice.ReadMiniSpeedometers is deprecated."
                            " Please use GpuSubdevice.ReadFullMiniISMs.\n");

    return ReadSpeedoHelper(pContext,pObject,NumArguments,pArguments
                            ,pReturlwalue,usage,MINI);
}
//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadTsoscSpeedometers,
                7,// max args
                "Read the ring-oscillator counts for each TSOSC ISM.")
{
    STEST_HEADER(1, 7,
                 "Usage: GpuSubdevice.ReadTsoscSpeedometers(return_array "
                 "[,CountSel=-1,Duration=-1,OutDiv=-1,Adj=-1,Chiplet=-1,InstrId=-1])");

    static Deprecation depr("GpuSubdevice.ReadTsoscSpeedometers", "7/1/2021",
                            "GpuSubdevice.ReadTsoscSpeedometers is deprecated."
                            " Please use GpuSubdevice.ReadTsoscISMs.\n");

    return ReadSpeedoHelper(pContext,pObject,NumArguments,pArguments
                            ,pReturlwalue,usage,TSOSC);
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupIsmController,
                3,
                "Setup ISM master controller to run an experiment.")
{
    STEST_HEADER(3, 6,
            "Usage: GpuSubdevice.SetupIsmController(dur, delay, trigger [, loopMode=0, haltMode=0, ctrlSrcSel=1])"); //$
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, dur); // length of time to let the experiment run
    STEST_ARG(1, INT32, delay); // delay after trigger before taking samples
    STEST_ARG(2, INT32, trigger); // type of trigger to use
    STEST_OPTIONAL_ARG(3, INT32, loopMode, 0); // new to GP100+
    STEST_OPTIONAL_ARG(4, INT32, haltMode, 0); // new to GP100+
    STEST_OPTIONAL_ARG(5, INT32, ctrlSrcSel, 1); // new to GP100+

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupISMController(dur, delay, trigger, loopMode, haltMode, ctrlSrcSel));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GH100 supports it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupIsmSubController,
                3,
                "Setup all of the ISM sub-controllers to run an experiment.")
{
    STEST_HEADER(3, 6,
            "Usage: GpuSubdevice.SetupIsmSubController(dur, delay, trigger [, loopMode=0, haltMode=0, ctrlSrcSel=1])"); //$
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, dur);                   // length of time to let the experiment run
    STEST_ARG(1, INT32, delay);                 // delay after trigger before taking samples
    STEST_ARG(2, INT32, trigger);               // type of trigger to use
    STEST_OPTIONAL_ARG(3, INT32, loopMode, 0);  // new to GP100+
    STEST_OPTIONAL_ARG(4, INT32, haltMode, 0);  // new to GP100+
    STEST_OPTIONAL_ARG(5, INT32, ctrlSrcSel, 1);  // new to GP100+

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupISMSubController(dur, delay, trigger, loopMode, haltMode, ctrlSrcSel));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GH100 supports it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupIsmSubControllerOnChain,
                3,
                "Setup all of the ISM sub-controllers on a specific chain to run an experiment.")
{
    STEST_HEADER(5, 8,
            "Usage: GpuSubdevice.SetupIsmSubController(chainId, chiplet, dur, delay, trigger [, loopMode=0, haltMode=0, ctrlSrcSel=1])"); //$
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, chainId);
    STEST_ARG(1, INT32, chiplet);
    STEST_ARG(2, INT32, dur);                   // length of time to let the experiment run
    STEST_ARG(3, INT32, delay);                 // delay after trigger before taking samples
    STEST_ARG(4, INT32, trigger);               // type of trigger to use
    STEST_OPTIONAL_ARG(5, INT32, loopMode, 0);  // new to GP100+
    STEST_OPTIONAL_ARG(6, INT32, haltMode, 0);  // new to GP100+
    STEST_OPTIONAL_ARG(7, INT32, ctrlSrcSel, 1);  // new to GP100+

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupISMSubControllerOnChain(chainId, chiplet, dur, delay, trigger,
                                                  loopMode, haltMode, ctrlSrcSel));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                PollISMCtrlComplete,
                1,
                "Poll for ISM master controller complete bit.")
{
    STEST_HEADER(1, 1,
            "Usage: GpuSubdevice.PollIsmExperimentComplete(complete)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM=NULL;
    UINT32 complete = 0;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->PollISMCtrlComplete(&complete));
    RETURN_RC(pJavaScript->SetElement(pReturlwals,0,complete));
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GF117 & Kepler support it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupMiniISMs,
                5,
                "Setup all the MINI ISMs for an experiment.")
{
    STEST_HEADER(5, 7,
        "Usage: GpuSubdevice.SetupMiniISMs(oscIdx, outDiv, mode, init, finit"
        "  [, flags=0, refClkSel=-1])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, oscIdx);
    STEST_ARG(1, INT32, outDiv);
    STEST_ARG(2, INT32, mode);
    STEST_ARG(3, INT32, init);
    STEST_ARG(4, INT32, finit);
    STEST_OPTIONAL_ARG(5, UINT32, chainFlags, Ism::NORMAL_CHAIN);
    STEST_OPTIONAL_ARG(6, INT32, refClkSel, -1);

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupMiniISMs(oscIdx, outDiv, mode, init, finit, refClkSel, chainFlags));
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
// Setup all of the Mini ISM on a specific chain for the next experiment.
// All of the ISMs will have the same oscIdx, outDiv, mode, init & finit values.
// Note: This does not cause the ISMs to start counting. You must call
// SetupIsmController() to start the experiment.
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupMiniISMsOnChain,
                7,
                "Setup all the MINI ISMs on a specific chain for an experiment.")
{
    STEST_HEADER(7, 8,
        "Usage: GpuSubdevice.SetupMiniISMsOnChain(chainId, chiplet, oscIdx, "
        "outDiv, mode, init, finit [, refClkSel=-1])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, chainId);
    STEST_ARG(1, INT32, chiplet);
    STEST_ARG(2, INT32, oscIdx);
    STEST_ARG(3, INT32, outDiv);
    STEST_ARG(4, INT32, mode);
    STEST_ARG(5, INT32, init);
    STEST_ARG(6, INT32, finit);
    STEST_OPTIONAL_ARG(7, INT32, refClkSel, -1);

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupMiniISMsOnChain(
               chainId, chiplet, oscIdx, outDiv, mode, init, finit, refClkSel));
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
// Helper functon for ReadMiniISMs() and ReadMiniISMsOnChain()
//
static RC ISMChainDataToJSVal(
    JSObject * pReturlwals
    ,vector<UINT32> &Speedos
    ,vector<IsmInfo> &Info
    ,jsval      * pReturlwalue
    ,JSContext  * pContext
)
{
    JavaScriptPtr pJs;
    StickyRC rc;
    jsval tmpjs;
    JSObject *pSpeedoVals;
    JSObject *pSpeedoInfo;

    C_CHECK_RC(pJs->GetProperty(pReturlwals, "Values", &pSpeedoVals));
    C_CHECK_RC(pJs->GetProperty(pReturlwals, "Info", &pSpeedoInfo));
    // use new interface:
    // var = x{Info:[{ chainId:0, chiplet:0, startBit:0, oscIdx:0, mode:0, ...}, ...], Values:[] };
    // Append the new values to the existing object.
    for (UINT32 p = 0; p < Speedos.size(); p++)
    {
        C_CHECK_RC(pJs->SetElement(pSpeedoVals, p, Speedos[p]));

        JSObject *pInfo = JS_NewObject(pContext, &DummyClass, 0, 0);
        C_CHECK_RC(pJs->SetProperty(pInfo, "chainId",   Info[p].chainId));
        C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet",   Info[p].chiplet));
        C_CHECK_RC(pJs->SetProperty(pInfo, "startBit",  Info[p].startBit));
        C_CHECK_RC(pJs->SetProperty(pInfo, "oscIdx",    Info[p].oscIdx));
        C_CHECK_RC(pJs->SetProperty(pInfo, "outDiv",    Info[p].outDiv));
        C_CHECK_RC(pJs->SetProperty(pInfo, "mode",      Info[p].mode));
        C_CHECK_RC(pJs->SetProperty(pInfo, "init",      Info[p].init));
        C_CHECK_RC(pJs->SetProperty(pInfo, "finit",     Info[p].finit));
        C_CHECK_RC(pJs->SetProperty(pInfo, "adj",       Info[p].adj));
        C_CHECK_RC(pJs->SetProperty(pInfo, "clkKHz",    Info[p].clkKHz));
        C_CHECK_RC(pJs->SetProperty(pInfo, "count",     Info[p].count));
        C_CHECK_RC(pJs->SetProperty(pInfo, "flags",     Info[p].flags));
        C_CHECK_RC(pJs->SetProperty(pInfo, "refClkSel", Info[p].refClkSel));
        C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
        C_CHECK_RC(pJs->SetElement(pSpeedoInfo, p, tmpjs));
    }
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                ReadMiniISMs,
                2,
                "Read the ring-oscillator counts for each MINI ISM.")
{
    STEST_HEADER(2, 3,
            "Usage: GpuSubdevice.ReadMiniISMs([return_array],[flags])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);
    STEST_ARG(1, UINT32, expFlags);
    STEST_OPTIONAL_ARG(2, UINT32, chainFlags, Ism::NORMAL_CHAIN);

    RC rc;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    vector<UINT32> Speedos;
    vector<IsmInfo> Info;
    GpuIsm * pISM;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->ReadMiniISMs(&Speedos, &Info, chainFlags, expFlags));
    return (ISMChainDataToJSVal(pReturlwals, Speedos, Info, pReturlwalue, pContext));
}

//------------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                ReadMiniISMsOnChain,
                4,
                "Read the ring-oscillator counts for each MINI ISM on a single chain.")
{
    STEST_HEADER(4, 4, "Usage: GpuSubdevice.ReadMiniISMsOnChain([chainId],[chiplet],[return_array],[flags])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, chainId);
    STEST_ARG(1, INT32, chiplet);
    STEST_ARG(2, JSObject*, pReturlwals);
    STEST_ARG(3, UINT32, flags);

    RC rc;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    vector<UINT32> Speedos;
    vector<IsmInfo> Info;
    GpuIsm *pISM;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->ReadMiniISMsOnChain(
            chainId, chiplet, &Speedos,&Info,flags));

    return (ISMChainDataToJSVal(pReturlwals, Speedos, Info,
                pReturlwalue,pContext));
}

//------------------------------------------------------------------------------
// Helper functon for RunNoiseMeasLite() & RunNoiseMeasLiteOnChain()
// Copy the correct NoiseInfo fields from the C++ structure into the JS object.
static RC ISMNoiseDataToJSObject(
    JSObject * pReturlwals
    ,vector<UINT32> &Values // was Speedos
    ,vector<NoiseInfo> &Info // was IsmInfo
    ,jsval      * pReturlwalue
    ,JSContext  * pContext
)
{
    JavaScriptPtr pJs;
    StickyRC rc;
    jsval tmpjs;
    JSObject *pNoiseVals;
    JSObject *pNoiseInfo;

    C_CHECK_RC(pJs->GetProperty(pReturlwals, "Values", &pNoiseVals));
    C_CHECK_RC(pJs->GetProperty(pReturlwals, "Info", &pNoiseInfo));
    // use new interface:
    // var = x{ Info:[{chainId:0, chiplet:0, startBit:0, tap:0, mode:0, ...}, ...], Values:[] };
    // Append the new values to the existing object.
    for (UINT32 p = 0; p < Info.size(); p++)
    {
        JSObject *pInfo = JS_NewObject(pContext, &DummyClass, 0, 0);
        switch (Info[p].version)
        {
            case 2:
                C_CHECK_RC(pJs->SetElement(pNoiseVals, p, Values[p]));

                C_CHECK_RC(pJs->SetProperty(pInfo, "version",  Info[p].version));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chainId",  Info[p].ni2.chainId));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet",  Info[p].ni2.chiplet));
                C_CHECK_RC(pJs->SetProperty(pInfo, "startBit", Info[p].ni2.startBit));
                C_CHECK_RC(pJs->SetProperty(pInfo, "tap",      Info[p].ni2.tap));
                C_CHECK_RC(pJs->SetProperty(pInfo, "mode",     Info[p].ni2.mode));
                C_CHECK_RC(pJs->SetProperty(pInfo, "outsel",   Info[p].ni2.outSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "thresh",   Info[p].ni2.thresh));
                C_CHECK_RC(pJs->SetProperty(pInfo, "triminit", Info[p].ni2.trimInit));
                C_CHECK_RC(pJs->SetProperty(pInfo, "measlen",  Info[p].ni2.measLen));
                C_CHECK_RC(pJs->SetProperty(pInfo, "count",    Info[p].ni2.count));
                C_CHECK_RC(pJs->SetProperty(pInfo, "data",     Info[p].ni2.data));
                C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
                C_CHECK_RC(pJs->SetElement(pNoiseInfo, p, tmpjs));
                break;

            case 3:
                C_CHECK_RC(pJs->SetElement(pNoiseVals, p, Values[p]));

                C_CHECK_RC(pJs->SetProperty(pInfo, "version",      Info[p].version));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chainId",      Info[p].ni3.chainId));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet",      Info[p].ni3.chiplet));
                C_CHECK_RC(pJs->SetProperty(pInfo, "startBit",     Info[p].ni3.startBit));
                C_CHECK_RC(pJs->SetProperty(pInfo, "hmDiv",        Info[p].ni3.hmDiv));
                C_CHECK_RC(pJs->SetProperty(pInfo, "hmFineDlySel", Info[p].ni3.hmFineDlySel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "hmTapDlySel",  Info[p].ni3.hmTapDlySel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "hmTapSel",     Info[p].ni3.hmTapSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "hmTrim",       Info[p].ni3.hmTrim));
                C_CHECK_RC(pJs->SetProperty(pInfo, "lockPt",       Info[p].ni3.lockPt));
                C_CHECK_RC(pJs->SetProperty(pInfo, "mode",         Info[p].ni3.mode));
                C_CHECK_RC(pJs->SetProperty(pInfo, "outSel",       Info[p].ni3.outSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "outClamp",     Info[p].ni3.outClamp));
                C_CHECK_RC(pJs->SetProperty(pInfo, "sat",          Info[p].ni3.sat));
                C_CHECK_RC(pJs->SetProperty(pInfo, "thresh",       Info[p].ni3.thresh));
                C_CHECK_RC(pJs->SetProperty(pInfo, "winLen",       Info[p].ni3.winLen));
                C_CHECK_RC(pJs->SetProperty(pInfo, "hmAvgWin",     Info[p].ni3.hmAvgWin));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fdUpdateOff",  Info[p].ni3.fdUpdateOff));
                C_CHECK_RC(pJs->SetProperty(pInfo, "count",        Info[p].ni3.count));
                C_CHECK_RC(pJs->SetProperty(pInfo, "hmOut",        Info[p].ni3.hmOut));
                C_CHECK_RC(pJs->SetProperty(pInfo, "rdy",          Info[p].ni3.rdy));
                C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
                C_CHECK_RC(pJs->SetElement(pNoiseInfo, p, tmpjs));
                break;

            case 4:
            case 5:
                C_CHECK_RC(pJs->SetElement(pNoiseVals, p, Values[p]));

                C_CHECK_RC(pJs->SetProperty(pInfo, "version",     Info[p].version));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chainId",     Info[p].niLite.chainId));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet",     Info[p].niLite.chiplet));
                C_CHECK_RC(pJs->SetProperty(pInfo, "startBit",    Info[p].niLite.startBit));
                C_CHECK_RC(pJs->SetProperty(pInfo, "trim",        Info[p].niLite.trim));
                C_CHECK_RC(pJs->SetProperty(pInfo, "tap",         Info[p].niLite.tap));
                C_CHECK_RC(pJs->SetProperty(pInfo, "iddq",        Info[p].niLite.iddq));
                C_CHECK_RC(pJs->SetProperty(pInfo, "mode",        Info[p].niLite.mode));
                C_CHECK_RC(pJs->SetProperty(pInfo, "sel",         Info[p].niLite.sel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "div",         Info[p].niLite.div));
                C_CHECK_RC(pJs->SetProperty(pInfo, "divClkIn",    Info[p].niLite.divClkIn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fixedDlySel", Info[p].niLite.fixedDlySel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "count",       Info[p].niLite.count));
                if (Info[p].version == 5)
                    C_CHECK_RC(pJs->SetProperty(pInfo, "refClkSel", Info[p].niLite.refClkSel));
                C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
                C_CHECK_RC(pJs->SetElement(pNoiseInfo, p, tmpjs));
                break;

            case 6:
                // Note: the raw count values are 64bits, so it consumes 2 entries in Values[]
                C_CHECK_RC(pJs->SetElement(pNoiseVals, 2 * p, Values[2 * p]));
                C_CHECK_RC(pJs->SetElement(pNoiseVals, 2 * p + 1, Values[2 * p + 1]));

                C_CHECK_RC(pJs->SetProperty(pInfo, "version",       Info[p].version));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chainId",       Info[p].niLite3.chainId));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet",       Info[p].niLite3.chiplet));
                C_CHECK_RC(pJs->SetProperty(pInfo, "startBit",      Info[p].niLite3.startBit));
                C_CHECK_RC(pJs->SetProperty(pInfo, "trim",          Info[p].niLite3.trim));
                C_CHECK_RC(pJs->SetProperty(pInfo, "tap",           Info[p].niLite3.tap));
                C_CHECK_RC(pJs->SetProperty(pInfo, "iddq",          Info[p].niLite3.iddq));
                C_CHECK_RC(pJs->SetProperty(pInfo, "mode",          Info[p].niLite3.mode));
                C_CHECK_RC(pJs->SetProperty(pInfo, "clkSel",        Info[p].niLite3.clkSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "outSel",        Info[p].niLite3.outSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "div",           Info[p].niLite3.div));
                C_CHECK_RC(pJs->SetProperty(pInfo, "divClkIn",      Info[p].niLite3.divClkIn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fixedDlySel",   Info[p].niLite3.fixedDlySel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "refClkSel",     Info[p].niLite3.refClkSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "jtagOvr",       Info[p].niLite3.jtagOvr));
                C_CHECK_RC(pJs->SetProperty(pInfo, "winLen",        Info[p].niLite3.winLen));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrEn",        Info[p].niLite3.cntrEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrClr",       Info[p].niLite3.cntrClr));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrDir",       Info[p].niLite3.cntrDir));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrToRead",    Info[p].niLite3.cntrToRead));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrOffset",    Info[p].niLite3.cntrOffset));
                C_CHECK_RC(pJs->SetProperty(pInfo, "errEn",         Info[p].niLite3.errEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareIn",       Info[p].niLite3.spareIn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "thresh",        Info[p].niLite3.thresh));
                C_CHECK_RC(pJs->SetProperty(pInfo, "mnMxWinEn",     Info[p].niLite3.mnMxWinEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "mnMxWinDur",    Info[p].niLite3.mnMxWinDur));
                C_CHECK_RC(pJs->SetProperty(pInfo, "threshDir",     Info[p].niLite3.threshDir));
                C_CHECK_RC(pJs->SetProperty(pInfo, "roEn",          Info[p].niLite3.roEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "countLow",      Info[p].niLite3.countLow));
                C_CHECK_RC(pJs->SetProperty(pInfo, "countHi",       Info[p].niLite3.countHi));
                // We are going to try to set count but JS doesn't really support a full 64bits
                // so it may not work. The caller should use the countLow & countHi fields.
                C_CHECK_RC(pJs->SetProperty(pInfo, "count",
                                            (UINT64)((UINT64)Info[p].niLite3.countHi << 32 |
                                            (UINT64)Info[p].niLite3.countLow)));
                C_CHECK_RC(pJs->SetProperty(pInfo, "ready",         Info[p].niLite3.ready));
                C_CHECK_RC(pJs->SetProperty(pInfo, "errOut",        Info[p].niLite3.errOut));
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareOut",      Info[p].niLite3.spareOut));
                C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
                C_CHECK_RC(pJs->SetElement(pNoiseInfo, p, tmpjs));
                break;

            case 7:
                // Note: the raw count values are 64bits, so it consumes 2 entries in Values[]
                C_CHECK_RC(pJs->SetElement(pNoiseVals, 2 * p, Values[2 * p]));
                C_CHECK_RC(pJs->SetElement(pNoiseVals, 2 * p + 1, Values[2 * p + 1]));

                C_CHECK_RC(pJs->SetProperty(pInfo, "version",       Info[p].version));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chainId",       Info[p].niLite4.chainId));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet",       Info[p].niLite4.chiplet));
                C_CHECK_RC(pJs->SetProperty(pInfo, "startBit",      Info[p].niLite4.startBit));
                C_CHECK_RC(pJs->SetProperty(pInfo, "trim",          Info[p].niLite4.trim));
                C_CHECK_RC(pJs->SetProperty(pInfo, "tap",           Info[p].niLite4.tap));
                C_CHECK_RC(pJs->SetProperty(pInfo, "iddq",          Info[p].niLite4.iddq));
                C_CHECK_RC(pJs->SetProperty(pInfo, "mode",          Info[p].niLite4.mode));
                C_CHECK_RC(pJs->SetProperty(pInfo, "clkSel",        Info[p].niLite4.clkSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "outSel",        Info[p].niLite4.outSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "div",           Info[p].niLite4.div));
                C_CHECK_RC(pJs->SetProperty(pInfo, "divClkIn",      Info[p].niLite4.divClkIn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fixedDlySel",   Info[p].niLite4.fixedDlySel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "refClkSel",     Info[p].niLite4.refClkSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "jtagOvr",       Info[p].niLite4.jtagOvr));
                C_CHECK_RC(pJs->SetProperty(pInfo, "winLen",        Info[p].niLite4.winLen));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrEn",        Info[p].niLite4.cntrEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrClr",       Info[p].niLite4.cntrClr));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrDir",       Info[p].niLite4.cntrDir));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrToRead",    Info[p].niLite4.cntrToRead));
                C_CHECK_RC(pJs->SetProperty(pInfo, "cntrOffset",    Info[p].niLite4.cntrOffset));
                C_CHECK_RC(pJs->SetProperty(pInfo, "errEn",         Info[p].niLite4.errEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareIn",       Info[p].niLite4.spareIn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "thresh",        Info[p].niLite4.thresh));
                C_CHECK_RC(pJs->SetProperty(pInfo, "threshDir",     Info[p].niLite4.threshDir));
                C_CHECK_RC(pJs->SetProperty(pInfo, "roEn",          Info[p].niLite4.roEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "countLow",      Info[p].niLite4.countLow));
                C_CHECK_RC(pJs->SetProperty(pInfo, "countHi",       Info[p].niLite4.countHi));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fabRoEn",       Info[p].niLite4.fabRoEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fabRoSBcast",   Info[p].niLite4.fabRoSBcast));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fabRoSwPol",    Info[p].niLite4.fabRoSwPol));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fabRoIdxBcast", Info[p].niLite4.fabRoIdxBcast));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fabRoSel",      Info[p].niLite4.fabRoSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "fabRoSpareIn",  Info[p].niLite4.fabRoSpareIn)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "streamSel",        Info[p].niLite4.streamSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "streamMode",       Info[p].niLite4.streamMode));
                C_CHECK_RC(pJs->SetProperty(pInfo, "selStreamTrigSrc", Info[p].niLite4.selStreamTrigSrc)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "samplePeriod",     Info[p].niLite4.samplePeriod)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "streamTrigSrc",    Info[p].niLite4.streamTrigSrc)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "readyBypass",      Info[p].niLite4.readyBypass)); //$

                // We are going to try to set count but JS doesn't really support a full 64bits
                // so it may not work. The caller should use the countLow & countHi fields.
                C_CHECK_RC(pJs->SetProperty(pInfo, "count",
                                            (UINT64)((UINT64)Info[p].niLite4.countHi << 32 |
                                            (UINT64)Info[p].niLite4.countLow)));
                C_CHECK_RC(pJs->SetProperty(pInfo, "ready",         Info[p].niLite4.ready));
                C_CHECK_RC(pJs->SetProperty(pInfo, "errOut",        Info[p].niLite4.errOut));
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareOut",      Info[p].niLite4.spareOut));
                C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
                C_CHECK_RC(pJs->SetElement(pNoiseInfo, p, tmpjs));
                break;

            case 8: // supports GH100 NMEAS_lite_v8
            case 9: // supports GH100 NMEAS_lite_v9
                C_CHECK_RC(pJs->SetElement(pNoiseVals, 2 * p, Values[2 * p]));
                C_CHECK_RC(pJs->SetElement(pNoiseVals, 2 * p + 1, Values[2 * p + 1]));
                C_CHECK_RC(pJs->SetProperty(pInfo, "version",       Info[p].version));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chainId",       Info[p].niLite8.chainId));
                C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet",       Info[p].niLite8.chiplet));
                C_CHECK_RC(pJs->SetProperty(pInfo, "startBit",      Info[p].niLite8.startBit));
                C_CHECK_RC(pJs->SetProperty(pInfo, "iddq",          Info[p].niLite8.iddq)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "mode",          Info[p].niLite8.mode)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "outSel",        Info[p].niLite8.outSel)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "clkSel",        Info[p].niLite8.clkSel)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "div",           Info[p].niLite8.div)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "clkPhaseIlw",   Info[p].niLite8.clkPhaseIlw)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "divClkIn",      Info[p].niLite8.divClkIn )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "trimInitA",     Info[p].niLite8.trimInitA)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "trimInitB",     Info[p].niLite8.trimInitB)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "trimTypeSel",   Info[p].niLite8.trimTypeSel)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "tapTypeSel",    Info[p].niLite8.tapTypeSel)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "trimInitC",     Info[p].niLite8.trimInitC)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "fixedDlySel",   Info[p].niLite8.fixedDlySel)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "tapSel",        Info[p].niLite8.tapSel)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "jtagOvr",       Info[p].niLite8.jtagOvr)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "threshMax",     Info[p].niLite8.threshMax)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "threshMin",     Info[p].niLite8.threshMin)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "threshEn",      Info[p].niLite8.threshEn)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareIn",       Info[p].niLite8.spareIn)); //$
                if (Info[p].version == 9)
                {
                    C_CHECK_RC(pJs->SetProperty(pInfo, "streamSel",        Info[p].niLite8.streamSel)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "streamMode",       Info[p].niLite8.streamMode)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "selStreamTrigSrc", Info[p].niLite8.selStreamTrigSrc)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "samplePeriod",     Info[p].niLite8.samplePeriod)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "streamTrigSrc",    Info[p].niLite8.streamTrigSrc)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "readyBypass",      Info[p].niLite8.readyBypass)); //$
                }
                if (Info[p].version == 8)
                {
                    C_CHECK_RC(pJs->SetProperty(pInfo, "cntrEn",         Info[p].niLite8.cntrEn)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "winLen",         Info[p].niLite8.winLen)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "cntrClr",        Info[p].niLite8.cntrClr)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "cntrOffsetDir",  Info[p].niLite8.cntrOffsetDir)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "cntrToRead",     Info[p].niLite8.cntrToRead)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "cntrTrimOffset", Info[p].niLite8.cntrTrimOffset)); //$
                }
                C_CHECK_RC(pJs->SetProperty(pInfo, "count",         Info[p].niLite8.count)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "countLow",      Info[p].niLite8.countLow)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "countHi",       Info[p].niLite8.countHi)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "ready",         Info[p].niLite8.ready)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "threshOut",     Info[p].niLite8.threshOut)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "threshMaxOut",  Info[p].niLite8.threshMaxOut)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "threshMinOut",  Info[p].niLite8.threshMinOut)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareOut",      Info[p].niLite8.spareOut)); //$

                C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
                C_CHECK_RC(pJs->SetElement(pNoiseInfo, p, tmpjs));
                break;
        } // end of switch
    } // end of for
    RETURN_RC(rc);
}
//*************************************************************************************************
// Copy the NoiseMeas properties from the JS object to a C++ structure.
static RC ISMNoiseDataFromJSObject(
    JSObject    * pJsObj
    ,NoiseInfo  * pNi
)
{
    JavaScriptPtr pJs;
    StickyRC rc;
    CHECK_RC_MSG(pJs->GetProperty(pJsObj, "version", &pNi->version), "Failed to get \"version\" property"); //$
    switch (pNi->version)
    {
        case 8: // supports GH100 NMEAS_lite_v8, NMEAS_lite_v9
            //Mandatory properties
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "iddq",             &pNi->niLite8.iddq),              "Failed to get \"iddq\" property");             //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "mode",             &pNi->niLite8.mode),              "Failed to get \"mode\" property");             //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "outSel",           &pNi->niLite8.outSel),            "Failed to get \"outSel\" property");           //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "clkSel",           &pNi->niLite8.clkSel),            "Failed to get \"clkSel\" property");           //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "div",              &pNi->niLite8.div),               "Failed to get \"div\" property");              //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "clkPhaseIlw",      &pNi->niLite8.clkPhaseIlw),       "Failed to get \"clkPhaseIlw\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "divClkIn",         &pNi->niLite8.divClkIn),          "Failed to get \"divClkIn\" property");         //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "trimInitA",        &pNi->niLite8.trimInitA),         "Failed to get \"trimInitA\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "trimInitB",        &pNi->niLite8.trimInitB),         "Failed to get \"trimInitB\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "trimTypeSel",      &pNi->niLite8.trimTypeSel),       "Failed to get \"trimTypeSel\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "tapTypeSel",       &pNi->niLite8.tapTypeSel),        "Failed to get \"tapTypeSel\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "trimInitC",        &pNi->niLite8.trimInitC),         "Failed to get \"trimInitC\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fixedDlySel",      &pNi->niLite8.fixedDlySel),       "Failed to get \"fixedDlySel\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "tapSel",           &pNi->niLite8.tapSel),            "Failed to get \"tapSel\" property");           //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "jtagOvr",          &pNi->niLite8.jtagOvr),           "Failed to get \"jtagOvr\" property");          //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "threshMax",        &pNi->niLite8.threshMax),         "Failed to get \"threshMax\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "threshMin",        &pNi->niLite8.threshMin),         "Failed to get \"threshMin\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "threshEn",         &pNi->niLite8.threshEn),          "Failed to get \"threshEn\" property");         //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "spareIn",          &pNi->niLite8.spareIn),           "Failed to get \"spareIn\" property");          //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "streamSel",        &pNi->niLite8.streamSel),         "Failed to get \"streamSel\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "streamMode",       &pNi->niLite8.streamMode ),       "Failed to get \"streamMode\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "selStreamTrigSrc", &pNi->niLite8.selStreamTrigSrc),  "Failed to get \"selStreamTrigSrc\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "samplePeriod",     &pNi->niLite8.samplePeriod),      "Failed to get \"samplePeriod\" property");     //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "streamTrigSrc",    &pNi->niLite8.streamTrigSrc),     "Failed to get \"streamTrigSrc\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "readyBypass",      &pNi->niLite8.readyBypass),       "Failed to get \"readyBypass\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrEn",           &pNi->niLite8.cntrEn),            "Failed to get \"cntrEn\" property");           //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "winLen",           &pNi->niLite8.winLen),            "Failed to get \"winLen\" property");           //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrClr",          &pNi->niLite8.cntrClr),           "Failed to get \"cntrClr\" property");          //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrOffsetDir",    &pNi->niLite8.cntrOffsetDir),     "Failed to get \"cntrOffsetDir\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrToRead",       &pNi->niLite8.cntrToRead),        "Failed to get \"cntrToRead\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrTrimOffset",   &pNi->niLite8.cntrTrimOffset),    "Failed to get \"cntrTrimOffset\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "count",            &pNi->niLite8.count),             "Failed to get \"count\" property");            //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "countLow",         &pNi->niLite8.countLow),          "Failed to get \"countLow\" property");         //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "countHi",          &pNi->niLite8.countHi),           "Failed to get \"countHi\" property");          //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "ready",            &pNi->niLite8.ready),             "Failed to get \"ready\" property");            //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "threshOut",        &pNi->niLite8.threshOut),         "Failed to get \"threshOut\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "threshMaxOut",     &pNi->niLite8.threshMaxOut),      "Failed to get \"threshMaxOut\" property");     //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "threshMinOut",     &pNi->niLite8.threshMinOut),      "Failed to get \"threshMinOut\" property");     //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "spareOut",         &pNi->niLite8.spareOut),          "Failed to get \"spareOut\" property");         //$
            break;
        case 7: // supports GA10x NMEAS_lite_v4, NMEAS_lite_v5, NMEAS_lite_v6, & NMEAS_lite_v7
            //Mandatory properties
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "trim",             &pNi->niLite4.trim),             "Failed to get \"trim\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "tap",              &pNi->niLite4.tap ),             "Failed to get \"tap\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "iddq",             &pNi->niLite4.iddq),             "Failed to get \"iddq\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "mode",             &pNi->niLite4.mode),             "Failed to get \"mode\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "clkSel",           &pNi->niLite4.clkSel),           "Failed to get \"clkSel\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "outSel",           &pNi->niLite4.outSel),           "Failed to get \"outSel\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "div",              &pNi->niLite4.div),              "Failed to get \"div\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "divClkIn",         &pNi->niLite4.divClkIn),         "Failed to get \"divClkIn\" property");  //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fixedDlySel",      &pNi->niLite4.fixedDlySel),      "Failed to get \"fixedDlySel\" property");//$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "jtagOvr",          &pNi->niLite4.jtagOvr),          "Failed to get \"jtagOvr\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "refClkSel",        &pNi->niLite4.refClkSel),        "Failed to get \"refClkSel\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "winLen",           &pNi->niLite4.winLen),           "Failed to get \"winLen\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrEn",           &pNi->niLite4.cntrEn),           "Failed to get \"cntrEn\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrClr",          &pNi->niLite4.cntrClr),          "Failed to get \"cntrClr\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrDir",          &pNi->niLite4.cntrDir),          "Failed to get \"cntrDir\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrToRead",       &pNi->niLite4.cntrToRead),       "Failed to get \"cntrToRead\" property");//$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrOffset",       &pNi->niLite4.cntrOffset),       "Failed to get \"cntrOffset\" property");//$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "errEn",            &pNi->niLite4.errEn),            "Failed to get \"errEn\" property");     //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "spareIn",          &pNi->niLite4.spareIn),          "Failed to get \"spareIn\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "thresh",           &pNi->niLite4.thresh),           "Failed to get \"thresh\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "threshDir",        &pNi->niLite4.threshDir),        "Failed to get \"threshDir\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "roEn",             &pNi->niLite4.roEn),             "Failed to get \"roEn\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fabRoSBcast",      &pNi->niLite4.fabRoSBcast  ),    "Failed to get \"fabRoSBcast\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fabRoIdxBcast",    &pNi->niLite4.fabRoIdxBcast),    "Failed to get \"fabRoIdxBcast\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fabRoSel",         &pNi->niLite4.fabRoSel),         "Failed to get \"fabRoSel\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fabRoEn",          &pNi->niLite4.fabRoEn),          "Failed to get \"fabRoEn\" property");              //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fabRoSwPol",       &pNi->niLite4.fabRoSwPol),       "Failed to get \"fabRoSwPol\" property");           //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fabRoSpareIn",     &pNi->niLite4.fabRoSpareIn),     "Failed to get \"fabRoSpareIn\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "streamSel",        &pNi->niLite4.streamSel),        "Failed to get \"streamSel\" property");            //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "streamMode",       &pNi->niLite4.streamMode),       "Failed to get \"streamMode\" property");           //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "selStreamTrigSrc", &pNi->niLite4.selStreamTrigSrc), "Failed to get \"selStreamTrigSrc\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "samplePeriod",     &pNi->niLite4.samplePeriod),     "Failed to get \"samplePeriod\" property");        //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "streamTrigSrc",    &pNi->niLite4.streamTrigSrc),    "Failed to get \"streamTrigSrc\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "readyBypass",      &pNi->niLite4.readyBypass),      "Failed to get \"readyBypass\" property");        //$

            // TODO: see if these fields need to be programmed on not
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "errOut",          &pNi->niLite4.errOut),            "Failed to get \"errOut\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "spareOut",        &pNi->niLite4.spareOut),          "Failed to get \"spareOut\" property");  //$
            break;

        case 6:
            //Mandatory properties
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "trim",        &pNi->niLite3.trim),        "Failed to get \"trim\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "tap",         &pNi->niLite3.tap ),        "Failed to get \"tap\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "iddq",        &pNi->niLite3.iddq),        "Failed to get \"iddq\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "mode",        &pNi->niLite3.mode),        "Failed to get \"mode\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "clkSel",      &pNi->niLite3.clkSel),      "Failed to get \"clkSel\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "outSel",      &pNi->niLite3.outSel),      "Failed to get \"outSel\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "div",         &pNi->niLite3.div),         "Failed to get \"div\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "divClkIn",    &pNi->niLite3.divClkIn),    "Failed to get \"divClkIn\" property");  //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fixedDlySel", &pNi->niLite3.fixedDlySel), "Failed to get \"fixedDlySel\" property");//$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "jtagOvr",     &pNi->niLite3.jtagOvr),     "Failed to get \"jtagOvr\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "refClkSel",   &pNi->niLite3.refClkSel),   "Failed to get \"refClkSel\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "winLen",      &pNi->niLite3.winLen),      "Failed to get \"winLen\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrEn",      &pNi->niLite3.cntrEn),      "Failed to get \"cntrEn\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrClr",     &pNi->niLite3.cntrClr),     "Failed to get \"cntrClr\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrDir",     &pNi->niLite3.cntrDir),     "Failed to get \"cntrDir\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrToRead",  &pNi->niLite3.cntrToRead),  "Failed to get \"cntrToRead\" property");//$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "cntrOffset",  &pNi->niLite3.cntrOffset),  "Failed to get \"cntrOffset\" property");//$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "errEn",       &pNi->niLite3.errEn),       "Failed to get \"errEn\" property");     //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "spareIn",     &pNi->niLite3.spareIn),     "Failed to get \"spareIn\" property");   //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "thresh",      &pNi->niLite3.thresh),      "Failed to get \"thresh\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "mnMxWinEn",   &pNi->niLite3.mnMxWinEn),   "Failed to get \"mnMxWinEn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "mnMxWinDur",  &pNi->niLite3.mnMxWinDur),  "Failed to get \"mnMxWinDur\" property");//$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "threshDir",   &pNi->niLite3.threshDir),   "Failed to get \"threshDir\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "roEn",        &pNi->niLite3.roEn),        "Failed to get \"roEn\" property");      //$
            // TODO: see if these fields need to be programmed on not
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "errOut",      &pNi->niLite3.errOut),      "Failed to get \"errOut\" property");    //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "spareOut",    &pNi->niLite3.spareOut),    "Failed to get \"spareOut\" property");  //$
            break;
        case 5: // fall through
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "refClkSel",   &pNi->niLite.refClkSel),    "Failed to get \"refClkSel\" property"); //$
        case 4:
            //Mandatory properties
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "trim",        &pNi->niLite.trim),        "Failed to get \"trim\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "tap",         &pNi->niLite.tap),         "Failed to get \"tap\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "iddq",        &pNi->niLite.iddq),        "Failed to get \"iddq\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "mode",        &pNi->niLite.mode),        "Failed to get \"mode\" property");      //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "sel",         &pNi->niLite.sel),         "Failed to get \"sel\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "div",         &pNi->niLite.div),         "Failed to get \"div\" property");       //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "divClkIn",    &pNi->niLite.divClkIn),    "Failed to get \"divClkIn\" property");  //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "fixedDlySel", &pNi->niLite.fixedDlySel), "Failed to get \"fixedDlySel\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsObj, "count",       &pNi->niLite.count),       "Failed to get \"count\" property");     //$
            break;
        default:
            rc = RC::BAD_PARAMETER;
            break;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. Maxwell supports it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupNoiseMeasISMs,
                6,
                "Setup all the NMEAS ISMs for an experiment.")
{
    STEST_HEADER(6 /*minArgs*/, 6 /*maxArgs*/,
        "Usage: GpuSubdevice.SetupNoiseMeasISMs(mode, outsel,tap,thresh,triminit,measlen)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, mode);
    STEST_ARG(1, INT32, outsel);
    STEST_ARG(2, INT32, tap);
    STEST_ARG(3, INT32, thresh);
    STEST_ARG(4, INT32, triminit);
    STEST_ARG(5, INT32, measlen);

    Ism::NoiseMeasParam param;
    param.version = 2;
    param.nm2.tap = tap;
    param.nm2.mode = mode;
    param.nm2.outSel = outsel;
    param.nm2.thresh = thresh;
    param.nm2.trimInit = triminit;
    param.nm2.measLen = measlen;

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupNoiseMeas(param));
    RETURN_RC(rc);
}
//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. Pascal supports it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupNoiseMeas3ISMs,
                13,
                "Setup all the NMEAS_3 ISMs for an experiment.")
{
    STEST_HEADER(14 /*minArgs*/, 14 /*maxArgs*/,
        "Usage: GpuSubdevice.SetupNoiseMeasISMs(hmDiv,hmFinDlySel,hmTapDlySel,hmTapSel"
                 "hmTrim,lockPt,mode,outSel,outClamp,sat,thresh,winLen,hmAvgWin,"
                 "fdUpdateOff)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG( 0, INT32, hmDiv);
    STEST_ARG( 1, INT32, hmFineDlySel);
    STEST_ARG( 2, INT32, hmTapDlySel);
    STEST_ARG( 3, INT32, hmTapSel);
    STEST_ARG( 4, INT32, hmTrim);
    STEST_ARG( 5, INT32, lockPt);
    STEST_ARG( 6, INT32, mode);
    STEST_ARG( 7, INT32, outSel);
    STEST_ARG( 8, INT32, outClamp);
    STEST_ARG( 9, INT32, sat);
    STEST_ARG(10, INT32, thresh);
    STEST_ARG(11, INT32, winLen);
    STEST_ARG(12, INT32, hmAvgWin);
    STEST_ARG(13, INT32, fdUpdateOff);

    Ism::NoiseMeasParam param;
    param.version = 3;
    param.nm3.hmDiv = hmDiv;
    param.nm3.hmFineDlySel = hmFineDlySel;
    param.nm3.hmTapDlySel = hmTapDlySel;
    param.nm3.hmTapSel = hmTapSel;
    param.nm3.hmTrim = hmTrim;
    param.nm3.lockPt = lockPt;
    param.nm3.mode = mode;
    param.nm3.outSel = outSel;
    param.nm3.outClamp = outClamp;
    param.nm3.sat = sat;
    param.nm3.thresh = thresh;
    param.nm3.winLen = winLen;
    param.nm3.hmAvgWin = hmAvgWin;
    param.nm3.fdUpdateOff = fdUpdateOff;

    RC rc;
    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupNoiseMeas(param));
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                RunNoiseMeas,
                2,
                "Run the noise measurement experiment for each NMEAS ISM on all chains.")
{
    STEST_HEADER(2, 2, "Usage: GpuSubdevice.RunNoiseMeas(stage,return_array)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, stage);
    STEST_ARG(1, JSObject*, pReturlwals);

    RC rc;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    vector<UINT32> rawValues;
    vector<IsmSpeedo::NoiseInfo> Info;
    GpuIsm *pISM;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->RunNoiseMeas((Ism::NoiseMeasStage)stage, &rawValues, &Info));
    return(ISMNoiseDataToJSObject(pReturlwals, rawValues, Info, pReturlwalue, pContext));
}

//------------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                RunNoiseMeasOnChain,
                4,
                "Run the noise measurement experiment for each NMEAS ISM on a single chain.")
{
    STEST_HEADER(4, 4, "Usage: GpuSubdevice.RunNoiseMeasOnChain(chainId,chiplet,stage,return_array)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, chainId);
    STEST_ARG(1, INT32, chiplet);
    STEST_ARG(2, INT32, stage);
    STEST_ARG(3, JSObject*, pReturlwals);

    RC rc;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    vector<UINT32> rawValues;
    vector<IsmSpeedo::NoiseInfo> Info;
    GpuIsm *pISM;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->RunNoiseMeasOnChain(
        chainId,
        chiplet,
        (Ism::NoiseMeasStage)stage,
        &rawValues,
        &Info));
    return(ISMNoiseDataToJSObject(pReturlwals, rawValues, Info, pReturlwalue, pContext));
}

//------------------------------------------------------------------------------------------------
// Expected Javascript usage:
//    let noiseInfo3 =
//    {
//        version:6 trim:0, tap:0, iddq:0, mode:0, clkSel:0,   outSel:0,   div:0, divClkIn:0,
//        fixedDlySel:0, jtagOvr:1, refClkSel:0, winLen:0, cntrEn:0, cntrClr:0, cntrDir:0,
//        cntrToRead:0, cntrOffset:0, errEn:0, spareIn:0, thresh:0, mnMxWinEn:0,
//        mnMxWinDur:0 threshDir:0, roEn:0, errOut:0, spareOut:0,
//        // Note these technically don't need to be set but doing it just for completeness
//        // to show all of the accessable fields in this structure. They wil be updated by
//        // ReadNoiseMeasLiteISMs()
//        ready:0, count_low:0, count_hi:0, chiplet:-1, chainId:-1, startBit:-1
//    };
//    let paramVersion = 6;
//    let g = new GpuSubdevice(0, 0);
//    g.SetupNoiseMeasLiteISMs(noiseInfo3);
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupNoiseMeasLiteISMs,
                1, // min number of args
                "Setup all the NMEAS_lite_xx ISMs for an experiment.")
{
    STEST_HEADER(1 /*minArgs*/, 9 /*maxArgs*/,
        "Usage: GpuSubdevice.SetupNoiseMeasLiteISMs(noiseInfo) | "
               "GpuSubdevice.SetupNoiseMeasLiteISMs(trim, tap, iddq, mode, sel, div, "
                "divClkIn, fixedDlySel [, refClkSel])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    JavaScriptPtr pJs;
    RC rc;
    NoiseInfo param = { 0 };
    // Note we are no longer supporting additional descrete arguments. If the user wants additional
    // fields going forward they must use the 1 argument version that passes in a JS object with
    // all the fields specified. For legacy APIs we will support 1, 8 or 9 arguments. Any other
    // number is a failure.
    if (NumArguments == 8 || NumArguments == 9)
    {
        param.version = 4;
        C_CHECK_RC(pJs->FromJsval(pArguments[0], &param.niLite.trim));
        C_CHECK_RC(pJs->FromJsval(pArguments[1], &param.niLite.tap));
        C_CHECK_RC(pJs->FromJsval(pArguments[2], &param.niLite.iddq));
        C_CHECK_RC(pJs->FromJsval(pArguments[3], &param.niLite.mode));
        C_CHECK_RC(pJs->FromJsval(pArguments[4], &param.niLite.sel));
        C_CHECK_RC(pJs->FromJsval(pArguments[5], &param.niLite.div));
        C_CHECK_RC(pJs->FromJsval(pArguments[6], &param.niLite.divClkIn));
        C_CHECK_RC(pJs->FromJsval(pArguments[7], &param.niLite.fixedDlySel));
        if (NumArguments == 9)
        {
            param.version = 5;
            C_CHECK_RC(pJs->FromJsval(pArguments[8], &param.niLite.refClkSel));
        }
    }
    // Load the NoiseInfo struct with the JS values.
    else if (NumArguments == 1)
    {
        JSObject* pJsNoiseInfo;
        C_CHECK_RC(pJs->FromJsval(pArguments[0], &pJsNoiseInfo));
        C_CHECK_RC(ISMNoiseDataFromJSObject(pJsNoiseInfo, &param));
    }
    else
    {
        RETURN_RC(RC::BAD_PARAMETER);
    }

    GpuIsm *pISM;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupNoiseMeasLite(param));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                SetupNoiseMeasLiteISMsOnChain,
                3, // min number of args
                "Setup all the NMEAS_lite_v3 ISMs for an experiment.")
{
    STEST_HEADER(3 /*minArgs*/, 11 /*maxArgs*/,
        "Usage: GpuSubdevice.SetupNoiseMeasLiteISMs(chainId, chiplet, noiseInfo) or"
        "Usage: GpuSubdevice.SetupNoiseMeasLiteISMsOnChain(chainId, chiplet,"
        "trim, tap, iddq, mode, sel, div, divClkIn, fixedDlySel [, refClkSel])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    UINT32 chainId;
    UINT32 chiplet;
    JavaScriptPtr pJs;
    RC rc = RC::OK;
    NoiseInfo param = { 0 };

    // Note we are no longer supporting additional descrete arguments. If the user wants additional
    // fields going forward they must use the 3 argument version that passes in a JS object with
    // all the fields specified. For legacy APIs we will support 3, 10 or 11 arguments. Any other
    // number is a failure.
    if (NumArguments == 10 || NumArguments == 11)
    {
        param.version = 4;
        C_CHECK_RC(pJs->FromJsval(pArguments[0], &chainId));
        C_CHECK_RC(pJs->FromJsval(pArguments[1], &chiplet));
        C_CHECK_RC(pJs->FromJsval(pArguments[2], &param.niLite.trim));
        C_CHECK_RC(pJs->FromJsval(pArguments[3], &param.niLite.tap));
        C_CHECK_RC(pJs->FromJsval(pArguments[4], &param.niLite.iddq));
        C_CHECK_RC(pJs->FromJsval(pArguments[5], &param.niLite.mode));
        C_CHECK_RC(pJs->FromJsval(pArguments[6], &param.niLite.sel));
        C_CHECK_RC(pJs->FromJsval(pArguments[7], &param.niLite.div));
        C_CHECK_RC(pJs->FromJsval(pArguments[8], &param.niLite.divClkIn));
        C_CHECK_RC(pJs->FromJsval(pArguments[9], &param.niLite.fixedDlySel));
        if (NumArguments == 11)
        {
            param.version = 5;
            C_CHECK_RC(pJs->FromJsval(pArguments[10], &param.niLite.refClkSel));
        }
    }
    else if (NumArguments == 3)
    {
        JSObject* pJsNoiseInfo;
        C_CHECK_RC(pJs->FromJsval(pArguments[0], &chainId));
        C_CHECK_RC(pJs->FromJsval(pArguments[1], &chiplet));
        C_CHECK_RC(pJs->FromJsval(pArguments[2], &pJsNoiseInfo));
        // Load the NoiseInfo struct with the JS values.
        C_CHECK_RC(ISMNoiseDataFromJSObject(pJsNoiseInfo, &param));
    }
    else
    {
        RETURN_RC(RC::BAD_PARAMETER);
    }

    GpuIsm *pISM;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->SetupNoiseMeasLiteOnChain(chainId, chiplet, param));
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
// This feature is supported on GV100+ chips.
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  ReadNoiseMeasLiteISMs,
                  1,
                  "Read the ring-oscillator counts for each NMEAS_lite ISMs")
{
    STEST_HEADER(1, 2,
        "Usage: GpuSubdevice.ReadNoiseMeasLiteISMs(return_array[], bPollExperiment=true)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);
    STEST_OPTIONAL_ARG(1, bool, bPollExperiment, true);

    RC rc;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    vector<UINT32> rawValues;
    vector<NoiseInfo> Info;
    GpuIsm *pISM;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->ReadNoiseMeasLite(&rawValues, &Info, bPollExperiment));
    return ISMNoiseDataToJSObject(pReturlwals, rawValues, Info, pReturlwalue, pContext);
}

//------------------------------------------------------------------------------
// This feature is supported on GV100+ chips.
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  ReadNoiseMeasLiteISMsOnChain,
                  3,
                  "Read the ring-oscillator counts for each NMEAS_lite ISM on a single chain.")
{
    STEST_HEADER(3, 4, "Usage: GpuSubdevice.ReadNoiseMeasLiteISMsOnChain(chainId,"
        "chiplet, return_array[], bPollExperiment=true)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, chainId);
    STEST_ARG(1, INT32, chiplet);
    STEST_ARG(2, JSObject*, pReturlwals);
    STEST_OPTIONAL_ARG(3, bool, bPollExperiment, true);

    RC rc;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    vector<UINT32> rawVals;
    vector<NoiseInfo> Info;
    GpuIsm *pISM;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->ReadNoiseMeasLiteOnChain(chainId, chiplet, &rawVals, &Info, bPollExperiment));
    return ISMNoiseDataToJSObject(pReturlwals, rawVals, Info, pReturlwalue, pContext);
}

//*************************************************************************************************
// Copy the ISinkInfo properties from the JS object to a C++ structure.
// The JSObject should have the following properties:
//     let isinkInfo = { version:0, chainId:0, chiplet:0, offset:x, ... }
// See below for a complete list of the isink1 properties.
RC ISMISinkDataFromJSObject(
    JSObject    * pJsISink
    ,ISinkInfo  * pISink
)
{
    JavaScriptPtr pJs;
    StickyRC rc;
    CHECK_RC_MSG(pJs->GetProperty(pJsISink, "version", &pISink->hdr.version), "Failed to get \"version\" property"); //$
    CHECK_RC_MSG(pJs->GetProperty(pJsISink, "chainId", &pISink->hdr.chainId), "Failed to get \"chainId\" property"); //$
    CHECK_RC_MSG(pJs->GetProperty(pJsISink, "chiplet", &pISink->hdr.chiplet), "Failed to get \"chiplet\" property"); //$
    CHECK_RC_MSG(pJs->GetProperty(pJsISink, "offset",  &pISink->hdr.offset), "Failed to get \"offset\" property"); //$
    switch (pISink->hdr.version)
    {
        case 1:
            CHECK_RC_MSG(pJs->GetProperty(pJsISink, "iOff",   &pISink->isink1.iOff),   "Failed to get \".iOff\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsISink, "iSel",   &pISink->isink1.iSel),   "Failed to get \".iSel\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsISink, "selDbg", &pISink->isink1.selDbg), "Failed to get \".selDbg\" property"); //$
            break;
        default:
            Printf(Tee::PriError, "isinkInfo.version:%d has not been implemented\n", pISink->hdr.version); //$
            rc = RC::BAD_PARAMETER;
            break;
    }
    return rc;
}

//------------------------------------------------------------------------------------------------
// Colwert the CURRENT SINK data in the C++ structure to an array JS objects
RC ISMISinkDataToJSObject(
    JSObject * pReturlwals
    ,vector<ISinkInfo> &Info
    ,jsval * pReturlwalue
    ,JSContext * pContext
)
{
    JavaScriptPtr pJs;
    jsval tmpjs;
    StickyRC rc;

    for (UINT32 i = 0; i < Info.size(); i++)
    {
        JSObject *pInfo = JS_NewObject(pContext, &DummyClass, 0, 0);
        C_CHECK_RC(pJs->SetProperty(pInfo, "version", Info[i].hdr.version));
        C_CHECK_RC(pJs->SetProperty(pInfo, "chainId", Info[i].hdr.chainId));
        C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet", Info[i].hdr.chiplet));
        C_CHECK_RC(pJs->SetProperty(pInfo, "offset",  Info[i].hdr.offset));
        switch (Info[i].hdr.version)
        {
            case 1:
                C_CHECK_RC(pJs->SetProperty(pInfo, "iOff",   Info[i].isink1.iOff));
                C_CHECK_RC(pJs->SetProperty(pInfo, "iSel",   Info[i].isink1.iSel));
                C_CHECK_RC(pJs->SetProperty(pInfo, "selDbg", Info[i].isink1.selDbg));
                break;
            default:
                Printf(Tee::PriError, "isinkInfo.version:%d has not been implemented\n",
                       Info[i].hdr.version);
                return(RC::SOFTWARE_ERROR);
        }
        C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
        C_CHECK_RC(pJs->SetElement(pReturlwals, i, tmpjs));
    }
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------------------------
// Write 1 to all CURRENT SINK ISMs on this GPU.
// - pJsISinkInfo is either a JS object or a JsArray of JS objects with all of the required
//   properties (see ISMISinkDataFromJsObjec() for a complete list of the properties.
//   The chainId, chiplet, & offset properties are used to match the ISinks on the GPU. A value of
//   -1 can be used as a wild card to match all ISink propery values. For example the following 
//   values in the pJsISinkInfo object will produce different actions.
//   .chainId   .chiplet    .offset     Action
//   -------    -------     ------      -----------------------------------------------------------
//   -1         -1          -1          Write all ISinks
//   -1         -1          107         Write only ISinks that have an offset value = 107
//   -1         46          -1          Write all ISinks with a chiplet = 46
//   0xB8       -1          -1          Write all ISinks with a chainId = 0xB8
//   0xB8       46          -1          Write all ISinks with a chainId = 0xB8 & chiplet = 46.
//   0xB8       46          172         Write a single ISink with chainId = 0xB8, chiplet = 46,
//                                      and offset = 172.
//  If pJsISinkInfo is a JsArray, then each ISink will be compared aginst the array of JsIsinkInfo
//  objects and the first match will be used to write the ISink. So the more specific filters
//  should be put in the array before the more generic filters.
//  This feature is supported on AD10x+ chips.
JS_STEST_LWSTOM(JsGpuSubdevice,
                WriteISinkISMs,
                1,
                "Write all the LW_ISM_LWRRENT_SINK ISMs on this GPU.")
{
    STEST_HEADER(1 /*minArgs*/, 1 /*maxArgs*/,
        "Usage: GpuSubdevice.WriteISinkISMs(isinkInfoObject or isinkInfoArray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pJsISinkInfo);
    RC rc;

    vector<ISinkInfo> isinkParams(1);

    if (!JS_IsArrayObject(pContext, pJsISinkInfo))
    {
        // It's not an array. So verify the its an object that should be used on all ISinks
        C_CHECK_RC(ISMISinkDataFromJSObject(pJsISinkInfo, &isinkParams[0]));
    }
    else
    {   // Work with the Array
        STEST_ARG(0, JsArray, ISinkInfoArray);
        isinkParams.resize(ISinkInfoArray.size());
        for (UINT32 i = 0; i < isinkParams.size(); i++)
        {
            JSObject * pJsISinkInfo;
            C_CHECK_RC(pJavaScript->FromJsval(ISinkInfoArray[i], &pJsISinkInfo));
            C_CHECK_RC(ISMISinkDataFromJSObject(pJsISinkInfo, &isinkParams[i]));
        }
    }

    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->WriteISinks(isinkParams));
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------------------------
// Read all of the CURRENT SINK ISMs on this GPU and return an array of JSObjects with the ISink
// info.
// This feature is supported on AD10x+ chips.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadISinkISMs,
                2,
                "Read the LWRRENT_SINK ISMs on all chain(s).")
{
    STEST_HEADER(1, 2, "Usage: GpuSubdevice.ReadISinkISMs([return_array], isinkInfo)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);
    STEST_OPTIONAL_ARG(1, JSObject*, pJsISinkInfo, nullptr);

    RC rc;
    vector<ISinkInfo> isinkParams(1);
    if (pJsISinkInfo == nullptr)
    {
        isinkParams[0].hdr.chainId = -1;
        isinkParams[0].hdr.chiplet = -1;
        isinkParams[0].hdr.offset  = -1;
    }
    else
    {
        if (!JS_IsArrayObject(pContext, pJsISinkInfo))
        {
            // It's not an array. So verify the its an object that should be used on all CPMs
            C_CHECK_RC(ISMISinkDataFromJSObject(pJsISinkInfo, &isinkParams[0]));
        }
        else
        {   // Work with the Array
            JsArray ISinkInfoArray;
            rc = pJavaScript->FromJsval(pArguments[1], &ISinkInfoArray);
            if (rc != RC::OK)
            {
                pJavaScript->Throw(pContext, rc, 
                                   Utility::StrPrintf("Invalid value for ISinkInfoArray\n%s", usage)); //$
                return JS_FALSE;
            }
            isinkParams.resize(ISinkInfoArray.size());
            for (UINT32 i = 0; i < isinkParams.size(); i++)
            {
                JSObject * pJsISinkInfo;
                C_CHECK_RC(pJavaScript->FromJsval(ISinkInfoArray[i], &pJsISinkInfo));
                C_CHECK_RC(ISMISinkDataFromJSObject(pJsISinkInfo, &isinkParams[i]));
            }
        }
    }

    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm *pISM;
    C_CHECK_RC(psd->GetISM(&pISM));

    vector<ISinkInfo> isinkResults;
    C_CHECK_RC(pISM->ReadISinks(&isinkResults, isinkParams));
    return (ISMISinkDataToJSObject(pReturlwals, isinkResults, pReturlwalue, pContext));
}

//*************************************************************************************************
// Copy the CpmInfo properties from the JS object to a C++ structure.
// The JSObject should have the following properties:
//     let cpmInfo = { version:0, chainId:0, chiplet:0, offset:x, ... }
// See below for a complete list of the cpm1/cpm2 properties.
static RC ISMCpmDataFromJSObject(
    JSObject  * pJsCpm
    ,CpmInfo  * pCpm
)
{
    JavaScriptPtr pJs;
    StickyRC rc;
    CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "version", &pCpm->hdr.version), "Failed to get \"version\" property"); //$
    CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "chainId", &pCpm->hdr.chainId), "Failed to get \"chainId\" property"); //$
    CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "chiplet", &pCpm->hdr.chiplet), "Failed to get \"chiplet\" property"); //$
    CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "offset",  &pCpm->hdr.offset), "Failed to get \"offset\" property"); //$
    switch (pCpm->hdr.version)
    {
        case 0:
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "enable",             &pCpm->cpm1.enable),             "Failed to get \".enable\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "iddq",               &pCpm->cpm1.iddq),               "Failed to get \".iddq\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "resetFunc",          &pCpm->cpm1.resetFunc),          "Failed to get \".resetFunc\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "resetMask",          &pCpm->cpm1.resetMask),          "Failed to get \".resetMask\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "launchSkew",         &pCpm->cpm1.launchSkew),         "Failed to get \".launchSkew\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "captureSkew",        &pCpm->cpm1.captureSkew),        "Failed to get \".captureSkew\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "tuneDelay",          &pCpm->cpm1.tuneDelay),          "Failed to get \".tuneDelay\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "spareIn",            &pCpm->cpm1.spareIn),            "Failed to get \".spareIn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "resetPath",          &pCpm->cpm1.resetPath),          "Failed to get \".resetPath\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "maskId",             &pCpm->cpm1.maskId),             "Failed to get \".maskId\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "maskEn",             &pCpm->cpm1.maskEn),             "Failed to get \".maskEn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "maskEdge",           &pCpm->cpm1.maskEdge),           "Failed to get \".maskEdge\" property"); //$
            // These properties are read only and not technically required to write the ISM.
            // However we are going to verify they exist because the same JS object is used to read
            // the CPM and we want to give an early failure/warning to the user.
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "treeOut",            &pCpm->cpm1.treeOut),            "Failed to get \".treeOut\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "status0",            &pCpm->cpm1.status0),            "Failed to get \".status0\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "status1",            &pCpm->cpm1.status1),            "Failed to get \".status1\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "status2",            &pCpm->cpm1.status2),            "Failed to get \".status2\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "miscOut",            &pCpm->cpm1.miscOut),            "Failed to get \".miscOut\" property"); //$
            break;
        case 1:
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "enable",             &pCpm->cpm1.enable),             "Failed to get \".enable\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "iddq",               &pCpm->cpm1.iddq),               "Failed to get \".iddq\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "resetFunc",          &pCpm->cpm1.resetFunc),          "Failed to get \".resetFunc\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "resetMask",          &pCpm->cpm1.resetMask),          "Failed to get \".resetMask\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "launchSkew",         &pCpm->cpm1.launchSkew),         "Failed to get \".launchSkew\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "captureSkew",        &pCpm->cpm1.captureSkew),        "Failed to get \".captureSkew\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "spareIn",            &pCpm->cpm1.spareIn),            "Failed to get \".spareIn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "resetPath",          &pCpm->cpm1.resetPath),          "Failed to get \".resetPath\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "maskId",             &pCpm->cpm1.maskId),             "Failed to get \".maskId\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "maskEn",             &pCpm->cpm1.maskEn),             "Failed to get \".maskEn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "maskEdge",           &pCpm->cpm1.maskEdge),           "Failed to get \".maskEdge\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "synthPathConfig",    &pCpm->cpm1.synthPathConfig),    "Failed to get \".synthPathConfig\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "calModeSel",         &pCpm->cpm1.calibrationModeSel), "Failed to get \".calibrationModeSel\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "pgDis",              &pCpm->cpm1.pgDis),              "Failed to get \".pgDis\" property"); //$
            // These properties are read only and not technically required to write the ISM.
            // However we are going to verify they exist because the same JS object is used to read
            // the CPM and we want to give an early failure/warning to the user.
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "treeOut",            &pCpm->cpm1.treeOut),            "Failed to get \".treeOut\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "status0",            &pCpm->cpm1.status0),            "Failed to get \".status0\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "status1",            &pCpm->cpm1.status1),            "Failed to get \".status1\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "status2",            &pCpm->cpm1.status2),            "Failed to get \".status2\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "miscOut",            &pCpm->cpm1.miscOut),            "Failed to get \".miscOut\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "garyCntOut",         &pCpm->cpm1.garyCntOut),         "Failed to get \".garyCntOut\" property"); //$
            break;

        case 2: // Versions 2 & 3 use the same internal structure (.cpm2) however at the hardware level the bit fields are in
        case 3: // different locations. The user is responsible for setting up the .version property correctly.
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "grayCnt",               &pCpm->cpm2.grayCnt),               "Failed to get \".grayCnt\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "macroFailureCnt",       &pCpm->cpm2.macroFailureCnt),       "Failed to get \".macroFailureCnt\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "miscOut",               &pCpm->cpm2.miscOut),               "Failed to get \".miscOut\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "pathTimingStatus",      &pCpm->cpm2.pathTimingStatus),      "Failed to get \".pathTimingStatus\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "hardMacroCtrlOverride", &pCpm->cpm2.hardMacroCtrlOverride), "Failed to get \".hardMacroCtrlOverride\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "hardMacroCtrl",         &pCpm->cpm2.hardMacroCtrl),         "Failed to get \".hardMacroCtrl\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "instanceIdOverride",    &pCpm->cpm2.instanceIdOverride),    "Failed to get \".intsanceIdOverride\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "instanceId",            &pCpm->cpm2.instanceId),            "Failed to get \".instanceId\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "samplingPeriod",        &pCpm->cpm2.samplingPeriod),        "Failed to get \".samplingPeriod\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "skewCharEn",            &pCpm->cpm2.skewCharEn),            "Failed to get \".skewCharEn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "spareIn",               &pCpm->cpm2.spareIn),               "Failed to get \".spareIn\" property"); //$
            break;

        case 4: // The user is required to provide properties for both versions.
        case 5:
            // common properties
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "grayCnt",               &pCpm->cpm4.grayCnt),               "Failed to get \".grayCnt\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "macroFailureCnt",       &pCpm->cpm4.macroFailureCnt),       "Failed to get \".macroFailureCnt\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "miscOut",               &pCpm->cpm4.miscOut),               "Failed to get \".miscOut\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "pathTimingStatus",      &pCpm->cpm4.pathTimingStatus),      "Failed to get \".pathTimingStatus\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "instanceId",            &pCpm->cpm4.instanceId),            "Failed to get \".instanceId\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "samplingPeriod",        &pCpm->cpm4.samplingPeriod),        "Failed to get \".samplingPeriod\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "skewCharEn",            &pCpm->cpm4.skewCharEn),            "Failed to get \".skewCharEn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "spareIn",               &pCpm->cpm4.spareIn),               "Failed to get \".spareIn\" property"); //$
            // version 4 specific
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "hardMacroCtrlOverride", &pCpm->cpm4.hardMacroCtrlOverride), "Failed to get \".hardMacroCtrlOverride\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "hardMacroCtrl",         &pCpm->cpm4.hardMacroCtrl),         "Failed to get \".hardMacroCtrl\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "instanceIdOverride",    &pCpm->cpm4.instanceIdOverride),    "Failed to get \".intsanceIdOverride\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "pgEn",                  &pCpm->cpm4.pgEn),                  "Failed to get \".pgEn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "pgEnOverride",          &pCpm->cpm4.pgEnOverride),          "Failed to get \".pgEnOverride\" property"); //$
            // version 5 specific
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "enable",                &pCpm->cpm4.enable),                "Failed to get \".enable\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "resetFunc",             &pCpm->cpm4.resetFunc),             "Failed to get \".resetFunc\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "resetPath",             &pCpm->cpm4.resetPath),             "Failed to get \".resetPath\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "configData",            &pCpm->cpm4.configData),            "Failed to get \".configData\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "dataTypeId",            &pCpm->cpm4.dataTypeId),            "Failed to get \".dataTypeId\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "dataValid",             &pCpm->cpm4.dataValid),             "Failed to get \".dataValid\" property"); //$
            break;

        case 6: // GH100 .version property correctly.
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "grayCnt",               &pCpm->cpm6.grayCnt),               "Failed to get \".grayCnt\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "macroFailureCnt",       &pCpm->cpm6.macroFailureCnt),       "Failed to get \".macroFailureCnt\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "miscOut",               &pCpm->cpm6.miscOut),               "Failed to get \".miscOut\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "pathTimingStatus",      &pCpm->cpm6.pathTimingStatus),      "Failed to get \".pathTimingStatus\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "instanceId",            &pCpm->cpm6.instanceId),            "Failed to get \".instanceId\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "samplingPeriod",        &pCpm->cpm6.samplingPeriod),        "Failed to get \".samplingPeriod\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "skewCharEn",            &pCpm->cpm6.skewCharEn),            "Failed to get \".skewCharEn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "spareIn",               &pCpm->cpm6.spareIn),               "Failed to get \".spareIn\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "hardMacroCtrl",         &pCpm->cpm6.hardMacroCtrl),         "Failed to get \".hardMacroCtrl\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "hardMacroCtrlOverride", &pCpm->cpm6.hardMacroCtrlOverride), "Failed to get \".hardMacroCtrlOverride\" property"); //$
            CHECK_RC_MSG(pJs->GetProperty(pJsCpm, "instanceIdOverride",    &pCpm->cpm6.instanceIdOverride),    "Failed to get \".instanceIdOverride\" property"); //$
            break;

    default:
            Printf(Tee::PriError, "cpmInfo.version:%d has not been implemented\n", pCpm->hdr.version); //$
            rc = RC::BAD_PARAMETER;
            break;
    }
    return rc;
}

//------------------------------------------------------------------------------------------------
// Colwert the CPM data in the C++ structure to an array JS objects
static RC ISMCpmDataToJSVal(
    JSObject * pReturlwals
    ,vector<CpmInfo> &Info
    ,jsval * pReturlwalue
    ,JSContext * pContext
)
{
    JavaScriptPtr pJs;
    jsval tmpjs;
    StickyRC rc;

    for (UINT32 i = 0; i < Info.size(); i++)
    {
        JSObject *pInfo = JS_NewObject(pContext, &DummyClass, 0, 0);
        C_CHECK_RC(pJs->SetProperty(pInfo, "version", Info[i].hdr.version));
        C_CHECK_RC(pJs->SetProperty(pInfo, "chainId", Info[i].hdr.chainId));
        C_CHECK_RC(pJs->SetProperty(pInfo, "chiplet", Info[i].hdr.chiplet));
        C_CHECK_RC(pJs->SetProperty(pInfo, "offset",  Info[i].hdr.offset));
        switch (Info[i].hdr.version)
        {
            case 0: // fallthrough
                C_CHECK_RC(pJs->SetProperty(pInfo, "tuneDelay",   Info[i].cpm1.tuneDelay));
            case 1:
                C_CHECK_RC(pJs->SetProperty(pInfo, "enable",      Info[i].cpm1.enable));
                C_CHECK_RC(pJs->SetProperty(pInfo, "iddq",        Info[i].cpm1.iddq));
                C_CHECK_RC(pJs->SetProperty(pInfo, "resetFunc",   Info[i].cpm1.resetFunc));
                C_CHECK_RC(pJs->SetProperty(pInfo, "resetMask",   Info[i].cpm1.resetMask));
                C_CHECK_RC(pJs->SetProperty(pInfo, "launchSkew",  Info[i].cpm1.launchSkew));
                C_CHECK_RC(pJs->SetProperty(pInfo, "captureSkew", Info[i].cpm1.captureSkew));
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareIn",     Info[i].cpm1.spareIn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "resetPath",   Info[i].cpm1.resetPath));
                C_CHECK_RC(pJs->SetProperty(pInfo, "maskId",      Info[i].cpm1.maskId));
                C_CHECK_RC(pJs->SetProperty(pInfo, "maskEn",      Info[i].cpm1.maskEn));
                C_CHECK_RC(pJs->SetProperty(pInfo, "maskEdge",    Info[i].cpm1.maskEdge));
                C_CHECK_RC(pJs->SetProperty(pInfo, "treeOut",     Info[i].cpm1.treeOut));
                C_CHECK_RC(pJs->SetProperty(pInfo, "status0",     Info[i].cpm1.status0));
                C_CHECK_RC(pJs->SetProperty(pInfo, "status1",     Info[i].cpm1.status1));
                C_CHECK_RC(pJs->SetProperty(pInfo, "status2",     Info[i].cpm1.status2));
                C_CHECK_RC(pJs->SetProperty(pInfo, "miscOut",     Info[i].cpm1.miscOut));
                if (Info[i].hdr.version == 1)
                {
                    C_CHECK_RC(pJs->SetProperty(pInfo, "synthPathConfig",   Info[i].cpm1.synthPathConfig));    //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "calModeSel",        Info[i].cpm1.calibrationModeSel)); //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "pgDis",             Info[i].cpm1.pgDis));              //$
                    C_CHECK_RC(pJs->SetProperty(pInfo, "garyCntOut",        Info[i].cpm1.garyCntOut));         //$
                }
                break;

            case 2: // versions 2 & 3 use the same internal structure (.cpm2), however the hardware has these
            case 3: // fields in different bit locations.
                C_CHECK_RC(pJs->SetProperty(pInfo, "grayCnt",               Info[i].cpm2.grayCnt              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "macroFailureCnt",       Info[i].cpm2.macroFailureCnt      )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "miscOut",               Info[i].cpm2.miscOut              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "pathTimingStatus",      Info[i].cpm2.pathTimingStatus     )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "hardMacroCtrlOverride", Info[i].cpm2.hardMacroCtrlOverride)); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "hardMacroCtrl",         Info[i].cpm2.hardMacroCtrl        )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "instanceIdOverride",    Info[i].cpm2.instanceIdOverride   )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "instanceId",            Info[i].cpm2.instanceId           )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "samplingPeriod",        Info[i].cpm2.samplingPeriod       )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "skewCharEn",            Info[i].cpm2.skewCharEn           )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareIn",               Info[i].cpm2.spareIn              )); //$
                break;

            case 4: // The version value reflects what JS values get updated.
                C_CHECK_RC(pJs->SetProperty(pInfo, "grayCnt",               Info[i].cpm4.grayCnt                 )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "macroFailureCnt",       Info[i].cpm4.macroFailureCnt         )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "miscOut",               Info[i].cpm4.miscOut                 )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "pathTimingStatus",      Info[i].cpm4.pathTimingStatus        )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "hardMacroCtrlOverride", Info[i].cpm4.hardMacroCtrlOverride   )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "hardMacroCtrl",         Info[i].cpm4.hardMacroCtrl           )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "instanceIdOverride",    Info[i].cpm4.instanceIdOverride      )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "instanceId",            Info[i].cpm4.instanceId              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "pgEn",                  Info[i].cpm4.pgEn                    )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "pgEnOverride",          Info[i].cpm4.pgEnOverride            )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "samplingPeriod",        Info[i].cpm4.samplingPeriod          )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "skewCharEn",            Info[i].cpm4.skewCharEn              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareIn",               Info[i].cpm4.spareIn                 )); //$
                break;

            case 5: // The version value reflects what JS values get updated.
                C_CHECK_RC(pJs->SetProperty(pInfo, "enable",                Info[i].cpm4.enable                  )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "resetFunc",             Info[i].cpm4.resetFunc               )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "resetPath",             Info[i].cpm4.resetPath               )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "configData",            Info[i].cpm4.configData              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "dataTypeId",            Info[i].cpm4.dataTypeId              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "dataValid",             Info[i].cpm4.dataValid               )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "grayCnt",               Info[i].cpm4.grayCnt                 )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "macroFailureCnt",       Info[i].cpm4.macroFailureCnt         )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "miscOut",               Info[i].cpm4.miscOut                 )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "pathTimingStatus",      Info[i].cpm4.pathTimingStatus        )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "instanceId",            Info[i].cpm4.instanceId              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "samplingPeriod",        Info[i].cpm4.samplingPeriod          )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "skewCharEn",            Info[i].cpm4.skewCharEn              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareIn",               Info[i].cpm4.spareIn                 )); //$
                break;

            case 6: // The user is responsible for setting up the .version property correctly.
                C_CHECK_RC(pJs->SetProperty(pInfo, "grayCnt",               Info[i].cpm6.grayCnt                 )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "macroFailureCnt",       Info[i].cpm6.macroFailureCnt         )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "miscOut",               Info[i].cpm6.miscOut                 )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "pathTimingStatus",      Info[i].cpm6.pathTimingStatus        )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "instanceId",            Info[i].cpm6.instanceId              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "samplingPeriod",        Info[i].cpm6.samplingPeriod          )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "skewCharEn",            Info[i].cpm6.skewCharEn              )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "spareIn",               Info[i].cpm6.spareIn                 )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "hardMacroCtrl",         Info[i].cpm6.hardMacroCtrl           )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "hardMacroCtrlOverride", Info[i].cpm6.hardMacroCtrlOverride   )); //$
                C_CHECK_RC(pJs->SetProperty(pInfo, "instanceIdOverride",    Info[i].cpm6.instanceIdOverride      )); //$
                break;
        }
        C_CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
        C_CHECK_RC(pJs->SetElement(pReturlwals, i, tmpjs));
    }
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------------------------
// Write 1 to all CPM ISMs on this GPU.
// - pJsCpmInfo is either a JS object or a JsArray of JS objects with all of the required
//   properties (see ISMCmpDataFromJsObjec() for a complete list of the properties.
//   The chainId, chiplet, & offset properties are used to match the CPMs on the GPU. A value of
//   -1 can be used as a wild card to match all CPM propery values. For example the following
//   values in the pJsCpmInfo object will produce different actions.
//   .chainId   .chiplet    .offset     Action
//   -------    -------     ------      -----------------------------------------------------------
//   -1         -1          -1          Write all CPMs
//   -1         -1          107         Write only CPMs that have an offset value = 107
//   -1         46          -1          Write all CPMs with a chiplet = 46
//   0xB8       -1          -1          Write all CPMs with a chainId = 0xB8
//   0xB8       46          -1          Write all CPMs with a chainId = 0xB8 & chiplet = 46.
//   0xB8       46          172         Write a single CPM with chainId = 0xB8, chiplet = 46,
//                                      and offset = 172.
//  If pJsCpmInfo is a JsArray, then each CPM will be compared aginst the array of JsCpmInfo
//  objects and the first match will be used to write the CPM. So the more specific filters should
//  be put in the array before the more generic filters.
//  This feature is supported on GV100+ chips.
JS_STEST_LWSTOM(JsGpuSubdevice,
               WriteCpmISMs,
               1,
               "Write all the LW_ISM_CPM ISMs on this GPU.")
{
    STEST_HEADER(1 /*minArgs*/, 1 /*maxArgs*/,
        "Usage: GpuSubdevice.WriteCpmISMs(cpmInfoObject or cpmInfoArray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pJsCpmInfo);
    RC rc;

    vector<CpmInfo> cpmParams(1);

    if (!JS_IsArrayObject(pContext, pJsCpmInfo))
    {
        // It's not an array. So verify the its an object that should be used on all CPMs
        C_CHECK_RC(ISMCpmDataFromJSObject(pJsCpmInfo, &cpmParams[0]));
    }
    else
    {   // Work with the Array
        STEST_ARG(0, JsArray, CpmInfoArray);
        cpmParams.resize(CpmInfoArray.size());
        for (UINT32 i = 0; i < cpmParams.size(); i++)
        {
            JSObject * pJsCpmInfo;
            C_CHECK_RC(pJavaScript->FromJsval(CpmInfoArray[i], &pJsCpmInfo));
            C_CHECK_RC(ISMCpmDataFromJSObject(pJsCpmInfo, &cpmParams[i]));
        }
    }

    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));
    C_CHECK_RC(pISM->WriteCPMs(cpmParams));
    RETURN_RC(rc);
}

//-------------------------------------------------------------------------------------------
// Read all of the CPMs on this GPU and return an array of JSObjects with the CPM info.
// This feature is supported on GV100+ chips.
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  ReadCpmISMs,
                  2,
                  "Read the ring-oscillator counts for each CPM ISM on all chain(s).")
{
    STEST_HEADER(1, 2, "Usage: GpuSubdevice.ReadCpmISMs([return_array], cpmInfo)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);
    STEST_OPTIONAL_ARG(1, JSObject*, pJsCpmInfo, nullptr);

    RC rc;
    vector<CpmInfo> cpmParams(1);
    if (pJsCpmInfo == nullptr)
    {
        cpmParams[0].hdr.chainId = -1;
        cpmParams[0].hdr.chiplet = -1;
        cpmParams[0].hdr.offset  = -1;
    }
    else
    {
        if (!JS_IsArrayObject(pContext, pJsCpmInfo))
        {
            // It's not an array. So verify the its an object that should be used on all CPMs
            C_CHECK_RC(ISMCpmDataFromJSObject(pJsCpmInfo, &cpmParams[0]));
        }
        else
        {   // Work with the Array
            JsArray CpmInfoArray;
            if (pJavaScript->FromJsval(pArguments[1], &CpmInfoArray) != RC::OK)
            {
                JS_ReportError(pContext, "Invalid value for CpmInfoArray\n%s", usage); //$
            }
            cpmParams.resize(CpmInfoArray.size());
            for (UINT32 i = 0; i < cpmParams.size(); i++)
            {
                JSObject * pJsCpmInfo;
                C_CHECK_RC(pJavaScript->FromJsval(CpmInfoArray[i], &pJsCpmInfo));
                C_CHECK_RC(ISMCpmDataFromJSObject(pJsCpmInfo, &cpmParams[i]));
            }
        }
    }

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm *pISM;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));

    vector<CpmInfo> cpmResults;
    C_CHECK_RC(pISM->ReadCPMs(&cpmResults, cpmParams));
    return (ISMCpmDataToJSVal(pReturlwals, cpmResults, pReturlwalue, pContext));
}

//------------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetKeepISMActive,
                4,
                "Sets ISM's iddq bit to 0/1 after reads ")
{
    STEST_HEADER(4, 4, "Usage: GpuSubdevice.SetKeepISMActive(instrId,chiplet,offset,activeState)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, instrId);
    STEST_ARG(1, INT32, chiplet);
    STEST_ARG(2, INT32, offset);
    STEST_ARG(3, INT32, activeState);

    RC rc;
    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm *pISM;
    C_CHECK_RC(pGpuSubdevice->GetISM(&pISM));
    C_CHECK_RC(pISM->SetKeepActive(instrId,chiplet,offset,!!activeState));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                UnlockHost2Jtag,
                2,
                "Unlock host to JTAG access")
{
    STEST_HEADER(2, 2,
            "Usage: Subdev.UnlockHost2Jtag(secCfgMask, selwreKeys)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, vector<UINT32>, secCfgMask);
    STEST_ARG(1, vector<UINT32>, selwreKeys);

    RC rc;
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->UnlockHost2Jtag(secCfgMask,selwreKeys));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadHost2Jtag,
                4,
                "Read host to JTAG")
{
    STEST_HEADER(4, 4,
            "Usage: Subdev.ReadHost2Jtag(Chiplet, Inst, NumBits, OutArray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, Chiplet);
    STEST_ARG(1, UINT32, InstId);
    STEST_ARG(2, UINT32, ChainLength);
    STEST_ARG(3, JSObject*, pReturnArray);

    RC rc;
    vector<UINT32>Results;
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->ReadHost2Jtag(Chiplet,
                                                                 InstId,
                                                                 ChainLength,
                                                                 &Results));
    for (UINT32 i = 0; i < Results.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pReturnArray, i, Results[i]));
    }
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadHost2JtagMulti,
                3,
                "Read host to JTAG multi-cluster")
{
    STEST_HEADER(3, 3,
            "Usage: Subdev.ReadHost2JtagMulti(ClusterArray, NumBits, OutArray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JsArray, ArrayOfClusters);
    STEST_ARG(1, UINT32, ChainLength);
    STEST_ARG(2, JSObject*, pReturnArray);

    RC rc;
    vector<UINT32>Results;
    vector<GpuSubdevice::JtagCluster> Clusters;
    GpuSubdevice::JtagCluster jtagCluster(0,0);
    JSObject * clusterJsObj;
    for (size_t i = 0; i < ArrayOfClusters.size(); i++)
    {

        C_CHECK_RC(pJavaScript->FromJsval(ArrayOfClusters[i],&clusterJsObj));
        C_CHECK_RC(pJavaScript->GetProperty(clusterJsObj,"instrId",&jtagCluster.instrId));
        C_CHECK_RC(pJavaScript->GetProperty(clusterJsObj,"chipletId", &jtagCluster.chipletId));
        Clusters.push_back(jtagCluster);

    }

    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->ReadHost2Jtag(Clusters,
                                                                 ChainLength,
                                                                 &Results));
    for (UINT32 i = 0; i < Results.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pReturnArray, i, Results[i]));
    }
    RETURN_RC(rc);
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                WriteHost2Jtag,
                4,
                "Write host to JTAG")
{
    STEST_HEADER(4, 4,
            "Usage: Subdev.WriteHost2Jtag(Chiplet, Inst, NumBits, InArray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, Chiplet);
    STEST_ARG(1, UINT32, InstId);
    STEST_ARG(2, UINT32, ChainLength);
    STEST_ARG(3, vector<UINT32>, InputData);

    RC rc;
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->WriteHost2Jtag(Chiplet,
                                                                  InstId,
                                                                  ChainLength,
                                                                  InputData));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                WriteHost2JtagMulti,
                3,
                "Write host to JTAG multi-cluster")
{
    STEST_HEADER(3, 3,
            "Usage: Subdev.WriteHost2JtagMulti(ClusterArray, NumBits, InArray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JsArray, ArrayOfClusters);
    STEST_ARG(1, UINT32, ChainLength);
    STEST_ARG(2, vector<UINT32>, InputData);

    RC rc;

    vector<GpuSubdevice::JtagCluster> Clusters;
    GpuSubdevice::JtagCluster jtagCluster(0,0);
    JSObject * clusterJsObj;
    for (size_t i = 0; i < ArrayOfClusters.size(); i++)
    {
        C_CHECK_RC(pJavaScript->FromJsval(ArrayOfClusters[i],&clusterJsObj));
        C_CHECK_RC(pJavaScript->GetProperty(clusterJsObj,"instrId",&jtagCluster.instrId));
        C_CHECK_RC(pJavaScript->GetProperty(clusterJsObj,"chipletId", &jtagCluster.chipletId));
        Clusters.push_back(jtagCluster);
    }
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->WriteHost2Jtag(Clusters,
                                                                  ChainLength,
                                                                  InputData));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GpioIsOutput,
                  1,
                  "Check if the GPIO pin is set for output")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdev.IsOutput(PinNum)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, Pin);

    bool IsOutput = pJsGpuSubdevice->GetGpuSubdevice()->GpioIsOutput(Pin);

    if (pJavaScript->ToJsval(IsOutput, pReturlwalue) != OK )
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GpioSetOutput,
                  2,
                  "Set a GPIO pin output level")
{
    STEST_HEADER(2, 2, "Usage: GpuSubdev.GpioSetOutput(PinNum, Level)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, Pin);
    STEST_ARG(1, bool, Level);

    pJsGpuSubdevice->GetGpuSubdevice()->GpioSetOutput(Pin, Level);
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GpioGetOutput,
                  1,
                  "read level of output GPIO pin")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdev.GpioGetOutput(PinNum)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, Pin);

    bool OutputLevel;
    OutputLevel = pJsGpuSubdevice->GetGpuSubdevice()->GpioGetOutput(Pin);

    if (pJavaScript->ToJsval(OutputLevel, pReturlwalue) != OK )
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GpioGetInput,
                  1,
                  "read level of input GPIO pin")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdev.GpioGetInput(PinNum)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, Pin);

    bool InputLevel;
    InputLevel = pJsGpuSubdevice->GetGpuSubdevice()->GpioGetInput(Pin);

    if (pJavaScript->ToJsval(InputLevel, pReturlwalue) != OK )
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GpioSetToOutput,
                2,
                "Set Gpio direction to output")
{
    STEST_HEADER(2, 2, "GpuSubdev.GpioSetToOutput(GpioNum, bToOutput)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, GpioPin);
    STEST_ARG(1, bool, ToOutput);

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    GpuSubdevice::GpioDirection Direction = GpuSubdevice::INPUT;
    if (ToOutput)
    {
        Direction = GpuSubdevice::OUTPUT;
    }
    RETURN_RC(pGpuSubdevice->SetGpioDirection(GpioPin, Direction));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                DetectVbridge,
                1,
                "Detect the video bridge (will only work right in windows).")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.DetectVbridge([result])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);

    RC rc;
    bool result;
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->DetectVbridge(&result));
    RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, result));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetFBCalibrationCount,
                2,
                "Returns the count of FB calibration errors registered so far")
{
    STEST_HEADER(2, 2,
            "Usage: GpuSubdevice.GetFBCalibrationCount([count], resetCount)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);
    STEST_ARG(1, bool, Reset);

    RC rc;
    FbCalibrationLockCount result = {0};

    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->GetFBCalibrationCount(&result, Reset));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, (UINT32)result.driveStrengthRiseCount));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 1, (UINT32)result.driveStrengthFallCount));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 2, (UINT32)result.driveStrengthTermCount));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 3, (UINT32)result.slewStrengthRiseCount));
    RETURN_RC(pJavaScript->SetElement(pReturlwals, 4, (UINT32)result.slewStrengthFallCount));
}

//-----------------------------------------------------------------------------
// GpuSubdevice JS Properties
CLASS_PROP_READONLY(JsGpuSubdevice, DeviceName, string, "");
CLASS_PROP_READONLY(JsGpuSubdevice, ChipSkuNumber, string, "ChipSkuNumber");
CLASS_PROP_READONLY(JsGpuSubdevice, ChipSkuModifier, string, "ChipSkuModifier");
CLASS_PROP_READONLY(JsGpuSubdevice, ChipSkuNumberModifier, string, "ChipSkuNumberModifier");
CLASS_PROP_READONLY(JsGpuSubdevice, BoardNumber, string, "BoardNumber");
CLASS_PROP_READONLY(JsGpuSubdevice, BoardSkuNumber, string, "BoardSkuNumber");
CLASS_PROP_READONLY(JsGpuSubdevice, CDPN, string, "Collaborative Design Project Number");
CLASS_PROP_READONLY(JsGpuSubdevice, DevInst, UINT32, "Test Device instance for graphics subdevice");
CLASS_PROP_READONLY(JsGpuSubdevice, DeviceId, UINT32,
                    "LW DeviceId for graphics subdevice.");
CLASS_PROP_READONLY(JsGpuSubdevice, IsPrimary, bool,
                    "indicates whether the subdevice is a primary VGA adapter or not");
CLASS_PROP_READONLY(JsGpuSubdevice, BootInfo, string,
                    "indicates how the GPU was initialized during boot");
CLASS_PROP_READONLY(JsGpuSubdevice, PciDeviceId, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, ChipArchId, UINT32, "Chip Arch ID");
CLASS_PROP_READONLY(JsGpuSubdevice, SubdeviceInst, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, GpuInst, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, ParentDevice, JSObject *, "");
CLASS_PROP_READONLY(JsGpuSubdevice, FbHeapSize, UINT64,
                    "size of the usable region of the FB");
CLASS_PROP_READONLY(JsGpuSubdevice, FramebufferBarSize, UINT64, "size of the framebuffer bar");
CLASS_PROP_READONLY(JsGpuSubdevice, TargetMemoryClock, UINT64,
                    "last target memory clock frequency (Hz)");
CLASS_PROP_READONLY(JsGpuSubdevice, PdiString, string, "lwpu PDI as a string");
CLASS_PROP_READONLY(JsGpuSubdevice, RawEcidString, string,
                    "lwpu raw chip ID as a string available before RM init.");
CLASS_PROP_READONLY(JsGpuSubdevice, GHSRawEcidString, string,
                    "GHS version of lwpu raw chip ID as a string available before RM init.");
CLASS_PROP_READONLY(JsGpuSubdevice, EcidString, string,
                    "lwpu chip ID as a human-readable string available before RM init.");
CLASS_PROP_READONLY(JsGpuSubdevice, UUIDString, string,
                    "lwpu uuid as a string.");
CLASS_PROP_READONLY(JsGpuSubdevice, ChipIdString, string,
                    "lwpu chip ID as a string.");
CLASS_PROP_READONLY(JsGpuSubdevice, ChipIdCooked, string,
                    "lwpu chip ID as a human-readable string.");
CLASS_PROP_READONLY(JsGpuSubdevice, Revision, UINT32,
                    "lwpu chip revision.");
CLASS_PROP_READONLY(JsGpuSubdevice, RevisionString, string,
                    "lwpu chip revision as a string.");
CLASS_PROP_READONLY(JsGpuSubdevice, PrivateRevision, UINT32,
                    "lwpu chip private revision.");
CLASS_PROP_READONLY(JsGpuSubdevice, PrivateRevisionString, string,
                    "lwpu chip private revision as a string.");
CLASS_PROP_READONLY(JsGpuSubdevice, SubsystemVendorId, UINT32, "SubsystemVendorId");
CLASS_PROP_READONLY(JsGpuSubdevice, SubsystemDeviceId, UINT32, "SubsystemDeviceId");
CLASS_PROP_READONLY(JsGpuSubdevice, BoardId, UINT32, "BoardId");
CLASS_PROP_READONLY(JsGpuSubdevice, PhysLwBase, PHYSADDR,
                    "register physical base address");
CLASS_PROP_READONLY(JsGpuSubdevice, PhysFbBase, PHYSADDR,
                    "frame buffer physical base address");
CLASS_PROP_READONLY(JsGpuSubdevice, PhysInstBase, PHYSADDR,
                    "instance memory physical base address");
CLASS_PROP_READONLY(JsGpuSubdevice, GpcMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, TpcMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, FbMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, FbpMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, FbpaMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, L2Mask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, FbioMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, FbioShiftMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, LwdecMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, LwencMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, LwjpgMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, PceMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, MaxPceCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, OfaMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, SyspipeMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, SmMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, XpMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, MevpMask, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, HasVbiosModsFlagA, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, HasVbiosModsFlagB, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, IsMXMBoard, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, MXMVersion, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, RopCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, ShaderCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, UGpuCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, VpeCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, FrameBufferUnitCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, TpcCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, MaxTpcCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, GpcCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, MaxGpcCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, MaxFbpCount, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, NumAteSpeedo, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, SmbusAddress, UINT32, "");
CLASS_PROP_READONLY(JsGpuSubdevice, HQualType, UINT32,
                    "Hardware Qualification sample type");
CLASS_PROP_READONLY(JsGpuSubdevice, IsEmulation, bool,
                    "This gpu is an fpga emulation");
CLASS_PROP_READONLY(JsGpuSubdevice, IsEmOrSim, bool,
                    "This gpu is an fpga emulation or software simulation");
CLASS_PROP_READONLY(JsGpuSubdevice, IsDFPGA, bool,
                    "This gpu is Display FPGA emulation");
CLASS_PROP_READONLY(JsGpuSubdevice, PciDomainNumber, UINT32,
                    "PCI DomainNumber for current graphics device.");
CLASS_PROP_READONLY(JsGpuSubdevice, PciBusNumber, UINT32,
                    "PCI BusNumber for current graphics device." );
CLASS_PROP_READONLY(JsGpuSubdevice, PciDeviceNumber, UINT32,
                    "PCI DeviceNumber for current graphics device." );
CLASS_PROP_READONLY(JsGpuSubdevice, PciFunctionNumber, UINT32,
                    "PCI FunctionNumber for current graphics device." );
CLASS_PROP_READONLY(JsGpuSubdevice, Irq, UINT32,
                    "Irq this device is hooked to." );
CLASS_PROP_READONLY(JsGpuSubdevice, RomPartner, string,
                    "Get the ROM Partner.");
CLASS_PROP_READONLY(JsGpuSubdevice, RomProjectId, string,
                    "Get the ROM ProjectId.");
CLASS_PROP_READONLY(JsGpuSubdevice, RomVersion, string,
                    "Get the ROM (Bios, EFI, or Fcode) version." );
CLASS_PROP_READONLY(JsGpuSubdevice, RomType, string,
                    "Get the ROM (Bios, EFI, or Fcode) type string." );
CLASS_PROP_READONLY(JsGpuSubdevice, RomTypeEnum, UINT32,
                    "Get the ROM (Bios, EFI, or Fcode) type enum." );
CLASS_PROP_READONLY(JsGpuSubdevice, RomTimestamp, UINT32,
                    "Get the ROM (Bios, EFI, or Fcode) creation UTC timestamp." );
CLASS_PROP_READONLY(JsGpuSubdevice, RomExpiration, UINT32,
                    "Get the ROM (Bios, EFI, or Fcode) expiration UTC timestamp." );
CLASS_PROP_READONLY(JsGpuSubdevice, RomSelwrityStatus, UINT32,
                    "Get the ROM (Bios, EFI, or Fcode) security status." );
CLASS_PROP_READONLY(JsGpuSubdevice, RomOemVendor, string,
                    "Get the ROM (Bios, EFI, or Fcode) vendor." );
CLASS_PROP_READONLY(JsGpuSubdevice, InfoRomVersion, string,
                    "Get the InfoROM version." );
CLASS_PROP_READONLY(JsGpuSubdevice, BoardSerialNumber, string,
                    "Get the Board Serial Number." );
CLASS_PROP_READONLY(JsGpuSubdevice, BoardMarketingName, string,
                    "Get the Board Marketing Name.");
CLASS_PROP_READONLY(JsGpuSubdevice, BoardProjectSerialNumber, string,
                    "Get the Board Project Serial Number." );
CLASS_PROP_READONLY(JsGpuSubdevice, Foundry, UINT32,
                    "Foundry number for current graphics device." );
CLASS_PROP_READONLY(JsGpuSubdevice, FoundryString, string,
                    "Chip's foundry" );
CLASS_PROP_READONLY(JsGpuSubdevice, BusType, UINT32,
                    "Bus Type" );
CLASS_PROP_READONLY(JsGpuSubdevice, PcieLinkSpeed, UINT32,
                    "Pcie link speed" );
CLASS_PROP_READONLY(JsGpuSubdevice, PcieSpeedCapability, UINT32,
                    "Pcie link speed capability of the GPU" );
CLASS_PROP_READONLY(JsGpuSubdevice, PcieLinkWidth, UINT32,
                    "Pcie link width" );
CLASS_PROP_READONLY(JsGpuSubdevice, MemoryIndex, UINT32,
                    "Memory strap for HBM devices");
CLASS_PROP_READONLY(JsGpuSubdevice, RamConfigStrap, UINT32,
                    "Kind of FB memory attached to Gpu" );
CLASS_PROP_READONLY(JsGpuSubdevice, TvEncoderStrap, UINT32,
                    "Desired Tv standard of encoder at boot time" );
CLASS_PROP_READONLY(JsGpuSubdevice, Io3GioConfigPadStrap, UINT32,
                    "reference into LUT for IO3GIO pad configuration" );
CLASS_PROP_READONLY(JsGpuSubdevice, UserStrapVal, UINT32,
                    "value configured on User strap" );
CLASS_PROP_READONLY(JsGpuSubdevice, AspmState, UINT32,
                    "GPU ASPM level enabled" );
CLASS_PROP_READONLY(JsGpuSubdevice, AspmCyaState, UINT32,
                    "GPU ASPM CYA - set per pstate" );
CLASS_PROP_READONLY(JsGpuSubdevice, IsASLMCapable, bool,
                    "Is GPU ASLM Capable" );
CLASS_PROP_READONLY(JsGpuSubdevice, L2CacheSize, UINT32,
                    "Get the size of active L2 cache");
CLASS_PROP_READONLY(JsGpuSubdevice, MaxL2SlicesPerFbp, UINT32,
                    "Get the max number of L2 cache slices per Fbp");
CLASS_PROP_READONLY(JsGpuSubdevice, MaxCeCount, UINT32, "");
CLASS_PROP_READWRITE(JsGpuSubdevice, MemoryClock, UINT64,
                     "Memory clock frequency (Hz)");
CLASS_PROP_READWRITE(JsGpuSubdevice, VerboseJtag, bool,
                     "Verbose Jtag ");
CLASS_PROP_READWRITE(JsGpuSubdevice, PexAerEnable, bool,
                     "PEX Advanced Error Reporting Enable");
CLASS_PROP_READWRITE(JsGpuSubdevice, GpuLocId, UINT32, "GPU Location ID");
CLASS_PROP_READONLY(JsGpuSubdevice, GpuIdentStr, string, "GPU identifier string");
CLASS_PROP_READONLY(JsGpuSubdevice, HasAspmL1Substates, bool, "GPU has ASPM L1 substates");
CLASS_PROP_READONLY(JsGpuSubdevice, IsEccFuseEnabled, bool, "Is ECC fuse blown on this GPU");
CLASS_PROP_READONLY(JsGpuSubdevice, CanL2BeDisabled, bool, "Can L2 be disabled on this GPU");
CLASS_PROP_READONLY(JsGpuSubdevice, VbiosImageHash, string, "Hash of the vbios image");
CLASS_PROP_READWRITE(JsGpuSubdevice, SkipAzaliaInit, bool, "Skip azalia init");
CLASS_PROP_READONLY(JsGpuSubdevice, PackageStr, string, "GPU package string");
CLASS_PROP_READWRITE(JsGpuSubdevice, GspFwImg, string, "GSP firmware image path");
CLASS_PROP_READWRITE(JsGpuSubdevice, GspLogFwImg, string, "GSP logging image path");
CLASS_PROP_READWRITE(JsGpuSubdevice, UseRegOps, bool,
                     "Force use of RM RegOps control call for register access");
CLASS_PROP_READONLY(JsGpuSubdevice, IsMIGEnabled, bool, "MIG enabled in inforom");
CLASS_PROP_READWRITE(JsGpuSubdevice, SkipEccErrCtrInit, bool, "Skip initializing ECC err counter");

// Create a dynamic property that will be added at runtime if the associated
// GpuSubdevice object supports it
#define NAMED_PROP(propName, resulttype, helpmsg)                              \
   P_(JsGpuSubdevice_Get_##propName);                                          \
   P_(JsGpuSubdevice_Set_##propName);                                          \
   P_(JsGpuSubdevice_Get_##propName)                                           \
   {                                                                           \
      MASSERT(pContext != 0);                                                  \
      MASSERT(pValue   != 0);                                                  \
                                                                               \
      JsGpuSubdevice * pJsGpuSb;                                               \
      if ((pJsGpuSb = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, NULL)) != 0) \
      {                                                                        \
          resulttype initProp;                                                 \
          if ((OK != pJsGpuSb->GetNamedProp(#propName, &initProp)) ||          \
              (OK != JavaScriptPtr()->ToJsval(initProp, pValue)))              \
          {                                                                    \
             JS_ReportError(pContext, "Failed to get JsGpuSubdevice." #propName );\
             *pValue = JSVAL_NULL;                                             \
             return JS_FALSE;                                                  \
          }                                                                    \
          return JS_TRUE;                                                      \
      }                                                                        \
      return JS_FALSE;                                                         \
   }                                                                           \
   P_(JsGpuSubdevice_Set_##propName)                                           \
   {                                                                           \
      MASSERT(pContext != 0);                                                  \
      MASSERT(pValue   != 0);                                                  \
                                                                               \
      resulttype Value;                                                        \
      JsGpuSubdevice * pJsGpuSb;                                               \
      if ((pJsGpuSb = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, NULL)) != 0) \
      {                                                                        \
          if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))               \
          {                                                                    \
             JS_ReportError(pContext, "Failed to set JsGpuSubdevice." #propName); \
             return JS_FALSE;                                                  \
          }                                                                    \
                                                                               \
          if(OK != pJsGpuSb->SetNamedProp(#propName, Value))                   \
          {                                                                    \
             JS_ReportError(pContext, "Error Setting " #propName);             \
             *pValue = JSVAL_NULL;                                             \
             return JS_FALSE;                                                  \
          }                                                                    \
          return JS_TRUE;                                                      \
      }                                                                        \
      return JS_FALSE;                                                         \
   }                                                                           \
   static JsGpusubdeviceNamedProperty s_NamedProp##propName                    \
   (                                                                           \
       #propName,                                                              \
       JsGpuSubdevice_Get_##propName,                                          \
       JsGpuSubdevice_Set_##propName                                           \
   )

// Create a dynamic array property that will be added at runtime if the
// associated GpuSubdevice object supports it
#define NAMED_ARRAY_PROP(propName, helpmsg)                                    \
   P_(JsGpuSubdevice_Get_##propName);                                          \
   P_(JsGpuSubdevice_Set_##propName);                                          \
   P_(JsGpuSubdevice_Get_##propName)                                           \
   {                                                                           \
      MASSERT(pContext != 0);                                                  \
      MASSERT(pValue   != 0);                                                  \
                                                                               \
      JsGpuSubdevice * pJsGpuSb;                                               \
      if ((pJsGpuSb = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, NULL)) != 0) \
      {                                                                        \
          JsArray initProp;                                                    \
          if ((OK != pJsGpuSb->GetNamedProp(#propName, &initProp)) ||          \
              (OK != JavaScriptPtr()->ToJsval(&initProp, pValue)))             \
          {                                                                    \
             JS_ReportError(pContext, "Failed to get JsGpuSubdevice." #propName );\
             *pValue = JSVAL_NULL;                                             \
             return JS_FALSE;                                                  \
          }                                                                    \
          return JS_TRUE;                                                      \
      }                                                                        \
      return JS_FALSE;                                                         \
   }                                                                           \
   P_(JsGpuSubdevice_Set_##propName)                                           \
   {                                                                           \
      MASSERT(pContext != 0);                                                  \
      MASSERT(pValue   != 0);                                                  \
                                                                               \
      JsArray jsArray;                                                         \
      JsGpuSubdevice * pJsGpuSb;                                               \
      if ((pJsGpuSb = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, NULL)) != 0) \
      {                                                                        \
          if (OK != JavaScriptPtr()->FromJsval(*pValue, &jsArray))             \
          {                                                                    \
             JS_ReportError(pContext, "Failed to set JsGpuSubdevice." #propName); \
             return JS_FALSE;                                                  \
          }                                                                    \
                                                                               \
          if(OK != pJsGpuSb->SetNamedProp(#propName, &jsArray))                \
          {                                                                    \
             JS_ReportError(pContext, "Error Setting " #propName);             \
             *pValue = JSVAL_NULL;                                             \
             return JS_FALSE;                                                  \
          }                                                                    \
          return JS_TRUE;                                                      \
      }                                                                        \
      return JS_FALSE;                                                         \
   }                                                                           \
   static JsGpusubdeviceNamedProperty s_NamedProp##propName                    \
   (                                                                           \
       #propName,                                                              \
       JsGpuSubdevice_Get_##propName,                                          \
       JsGpuSubdevice_Set_##propName                                           \
   )

P_(JsGpuSubdevice_Get_IlwalidProp)
{
    MASSERT(pContext != 0);
    string propName = "UNKNOWN";
    JavaScriptPtr()->FromJsval(PropertyId, &propName);
    JS_ReportError(pContext, "Invalid JS Named Property : %s", propName.c_str());
    *pValue = JSVAL_NULL;
    return JS_TRUE;
}
P_(JsGpuSubdevice_Set_IlwalidProp)
{
    MASSERT(pContext != 0);
    string propName = "UNKNOWN";
    JavaScriptPtr()->FromJsval(PropertyId, &propName);
    JS_ReportError(pContext, "Invalid JS Named Property : %s", propName.c_str());
    return JS_FALSE;
}

NAMED_PROP(TpcEnableMask, UINT32, "Tpc Enable Mask)");
NAMED_PROP(SmEnableMask, UINT32, "Sm Enable Mask)");
NAMED_PROP(FbEnableMask, UINT32, "Fb Enable Mask)");
NAMED_PROP(MevpEnableMask, UINT32, "Mevp Enable Mask)");
NAMED_PROP(GpcEnableMask, UINT32, "GPC Enable Mask");
NAMED_PROP(NoFsRestore, bool, "disable restoring initial floorsweeping settings");
NAMED_PROP(FsReset, bool, "reset floorsweeping settings before applying new settings");
NAMED_PROP(AdjustFsArgs, bool, "auto complete dependent floorsweeping settings");
NAMED_PROP(AddLwrFsMask, bool, "Auto add masking to the command line floorsweeping with the current mask");
NAMED_PROP(AdjustFsL2Noc, bool, "auto complete FBP L2 NOC pair floorsweeping");
NAMED_PROP(FixFsInitMasks, bool, "Make FS masks consistent after ATE floorsweeping");
NAMED_PROP(FbMs01Mode, bool, "Framebuffer MS01 Mode");
NAMED_ARRAY_PROP(TpcEnableMaskOnGpc, "TPC Enable Mask on the given GPC");
NAMED_ARRAY_PROP(ZlwllEnableMaskOnGpc, "Zlwll Enable Mask on the given GPC");
NAMED_PROP(FermiFsArgs, string, "Fermi Floorsweeping settings");
NAMED_PROP(FbGddr5x16Mode, bool, "Framebuffer GDDR5 x16 mode");
NAMED_PROP(FbGddr5x8Mode, bool, "Framebuffer GDDR5 x8 mode");
NAMED_PROP(DumpMicronGDDR6Info, bool, "Dump Micron GDDR6 ChipId information");
NAMED_PROP(FbBankSwizzle, UINT32, "Framebuffer Bank Swizzle Setting");
NAMED_PROP(FermiGpcTileMap, string, "Fermi GPC/TPC tile map setup");
NAMED_PROP(PreGpuInitScript, string, "Script to execute pre GPU init");
NAMED_PROP(IsFbBroken, bool, "Framebuffer is broken");
NAMED_PROP(PpcTceBypassMode, UINT32, "TCE bypass mode for PPC platforms");
NAMED_PROP(FbMc2MasterMask, UINT32, "FB MC2 master Mask");
NAMED_PROP(UseFakeBar1, bool, "Use fake bar1 on chip w/o bar1");
NAMED_PROP(SmcPartitions, string, "SMC partitioning configuration");
NAMED_PROP(SmcPartIdx, UINT32, "Explicitly requested SMC partition");
NAMED_PROP(SmcEngineIdx, UINT32, "Explicitly requested SMC engine");
NAMED_PROP(ActiveSwizzId, UINT32, "SwizzId of active SMC partition");
NAMED_PROP(ActiveSmcEngine, UINT32, "Local index of active SMC engine within the SMC partition");

//-----------------------------------------------------------------------------
// GpuSubdevice JS feature list properties
#define DEFINE_GPUSUB_FEATURE( feat, featid, desc ) \
   PROP_CONST( JsGpuSubdevice, feat, Device::feat );
#define DEFINE_GPUDEV_FEATURE(feat, featid, desc)
#define DEFINE_MCP_FEATURE(feat, featid, desc)
#define DEFINE_SOC_FEATURE(feat, featid, desc)
#include "core/include/featlist.h"
#undef DEFINE_GPUSUB_FEATURE
#undef DEFINE_GPUDEV_FEATURE
#undef DEFINE_MCP_FEATURE
#undef DEFINE_SOC_FEATURE

//-----------------------------------------------------------------------------
// GpuSubdevice Methods
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, HasBug, bugNum, UINT32,
                        "Report whether the subdevice has the specified bug");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, HasFeature, feature, UINT32,
                        "Report whether the subdevice has the specified feature");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, IsFeatureEnabled, feature, UINT32,
                        "Report whether the subdevice enables the specified feature");
JS_SMETHOD_BIND_NO_ARGS(JsGpuSubdevice, IsSOC,
                        "Report whether the subdevice is part of SOC");
JS_SMETHOD_BIND_NO_ARGS(JsGpuSubdevice, GetChipSelwrityFuse,
                        "Report whether the gpu is debug fused or prod fused");
JS_SMETHOD_BIND_NO_ARGS(JsGpuSubdevice, IsChipSltFused,
                        "Report whether the gpu has gone through final SLT fusing or not");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, HasDomain, domain, UINT32,
                        "Report whether the subdevice has the given clock domain");
JS_SMETHOD_BIND_NO_ARGS(JsGpuSubdevice, HasGpcRg,
                        "Report whether GPC RG is supported");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, CanClkDomainBeProgrammed, domain, UINT32,
                        "Report whether the subdevice can program the given clock domain");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetTpcMaskOnGpc, Gpc, UINT32,
                        "Get the Tpc mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, IsGpcGraphicsCapable, hwGpc, UINT32,
                        "Get the Graphics capability for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetGraphicsTpcMaskOnGpc, hwGpc, UINT32,
                        "Get the Graphics Tpc mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, SupportsReconfigTpcMaskOnGpc, Gpc, UINT32,
                        "Get the Reconfig Tpc mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetReconfigTpcMaskOnGpc, Gpc, UINT32,
                        "GPU Supports a Reconfig Tpc mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetPesMaskOnGpc, Gpc, UINT32,
                        "Get the Pes mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetL2MaskOnFbp, Fbp, UINT32,
                        "Get the L2 mask for the specified Fbp");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, SupportsRopMaskOnGpc, Gpc, UINT32,
                        "GPU supports a Rop mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetRopMaskOnGpc, Gpc, UINT32,
                        "Get the Rop mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, SupportsCpcMaskOnGpc, Gpc, UINT32,
                        "GPU supports a CPC mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetCpcMaskOnGpc, Gpc, UINT32,
                        "Get the CPC mask for the specified Gpc");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, SupportsL2SliceMaskOnFbp, Fbp, UINT32,
                        "GPU supports an L2 slice mask for the specified Fbp");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetL2SliceMaskOnFbp, Fbp, UINT32,
                        "Get the L2 slice mask for the specified Fbp");
JS_SMETHOD_BIND_ONE_ARG(JsGpuSubdevice, GetL2CacheSizePerUGpu, uGpu, UINT32,
                        "Get the L2 cache size for a speficied uGpu");
JS_SMETHOD_BIND_NO_ARGS(JsGpuSubdevice, IsMfgModsSmcEnabled,
                        "Report whether the subdevice has MfgMods SMC enabled");
JS_SMETHOD_LWSTOM(JsGpuSubdevice, GetSmMaskOnTpc, 2,
                  "Get the SM mask for the specified Tpc")
{
    STEST_HEADER(2, 2, "Usage: GpuSubdevice.GetSmMaskOnTpc(gpc, tpc)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, gpc);
    STEST_ARG(1, UINT32, tpc);

    if (pJavaScript->ToJsval(pJsGpuSubdevice->GetSmMaskOnTpc(gpc, tpc),
                                 pReturlwalue) != OK)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
C_(JsGpuSubdevice_GetRamSvop);
static STest JsGpuSubdevice_GetRamSvop
(
    JsGpuSubdevice_Object,
    "GetRamSvop",
    C_JsGpuSubdevice_GetRamSvop,
    1,
    "Return the RAM SVOP settings",
    MODS_INTERNAL_ONLY
);
C_(JsGpuSubdevice_GetRamSvop)
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetRamSvop([sp, rg, pdp, dp])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);

    RC rc;
    SvopValues svop = { 0 };
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->GetRamSvop(&svop));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "sp", (UINT32)svop.sp));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "rg", (UINT32)svop.rg));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "pdp", (UINT32)svop.pdp));
    RETURN_RC(pJavaScript->SetProperty(pReturlwals, "dp", (UINT32)svop.dp));
}

//-----------------------------------------------------------------------------
C_(JsGpuSubdevice_SetRamSvop);
static STest JsGpuSubdevice_SetRamSvop
(
    JsGpuSubdevice_Object,
    "SetRamSvop",
    C_JsGpuSubdevice_SetRamSvop,
    1,
    "Set the RAM SVOP settings",
    MODS_INTERNAL_ONLY
);
C_(JsGpuSubdevice_SetRamSvop)
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.SetRamSvop([sp, rg, pdp, dp])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pReturlwals);

    RC rc;
    SvopValues svop = { 0 };
    vector<string> props;
    set<string> uniqueProps;
    UINT32 tmp;

    C_CHECK_RC(pJavaScript->GetProperties(pReturlwals, &props));
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->GetRamSvop(&svop));

    uniqueProps.insert(props.begin(), props.end());
    if (uniqueProps.count("sp"))
    {
        C_CHECK_RC(pJavaScript->GetProperty(pReturlwals, "sp", &tmp));
        svop.sp = tmp;
    }
    if (uniqueProps.count("rg"))
    {
        C_CHECK_RC(pJavaScript->GetProperty(pReturlwals, "rg", &tmp));
        svop.rg = tmp;
    }
    if (uniqueProps.count("pdp"))
    {
        C_CHECK_RC(pJavaScript->GetProperty(pReturlwals, "pdp", &tmp));
        svop.pdp = tmp;
    }
    if (uniqueProps.count("dp"))
    {
        C_CHECK_RC(pJavaScript->GetProperty(pReturlwals, "dp", &tmp));
        svop.dp = tmp;
    }
    RETURN_RC(pJsGpuSubdevice->GetGpuSubdevice()->SetRamSvop(&svop));
}

JS_STEST_LWSTOM(JsGpuSubdevice, GetEccEnabled, 1, "Get ECC Enabled Status")
{
    STEST_HEADER(1, 1,
            "Usage: GpuSubdevice.GetEccEnabled([Supported, EnableMask])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pEccStatus);

    RC rc;
    bool bSupported = false;
    UINT32 enableMask = 0;
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->GetEccEnabled(
                &bSupported, &enableMask));
    C_CHECK_RC(pJavaScript->SetElement(pEccStatus, 0, bSupported));
    RETURN_RC(pJavaScript->SetElement(pEccStatus, 1, enableMask));
}

JS_STEST_LWSTOM(JsGpuSubdevice, GetEdcEnabled, 1, "Get EDC Enabled Status")
{
    STEST_HEADER(1, 1,
            "Usage: GpuSubdevice.GetEdcEnabled([Supported, EnableMask])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pEdcStatus);

    RC rc;
    bool bSupported = false;
    UINT32 enableMask = 0;
    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->GetEdcEnabled(
                &bSupported, &enableMask));
    C_CHECK_RC(pJavaScript->SetElement(pEdcStatus, 0, bSupported));
    RETURN_RC(pJavaScript->SetElement(pEdcStatus, 1, enableMask));
}

C_(JsGpuSubdevice_GetEdcErrTotalIndex);
static STest JsGpuSubdevice_GetEdcErrTotalIndex
(
    JsGpuSubdevice_Object,
    "GetEdcErrTotalIndex",
    C_JsGpuSubdevice_GetEdcErrTotalIndex,
    0,
    "Get index into EDC error list containing "
);
C_(JsGpuSubdevice_GetEdcErrTotalIndex)
{
    const UINT32 ALL_PARTITIONS = static_cast<UINT32>(~0);

    STEST_HEADER(1, 2, "Usage: GpuSubdevice.GetEdcErrTotalIndex(RetObj, [Partition])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetObj);
    STEST_OPTIONAL_ARG(1, UINT32, partition, ALL_PARTITIONS);

    UINT32 index;
    if(ALL_PARTITIONS == partition)
        index = EdcErrCounter::TOTAL_EDC_ERRORS_RECORD;
    else
        index = pJsGpuSubdevice->GetGpuSubdevice()->GetEdcErrCounter()->
            GetEdcErrIndex(partition) + EdcErrCounter::TOTAL_EDC_ERRORS_RECORD;

    JavaScriptPtr pJs;
    RETURN_RC(pJs->PackFields(pRetObj, "I", "Index", index));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                CalibratePll,
                1,
                "Calibrate the specified Pll")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.CalibratePll(ClkSrc); // Use Gpu.ClkSrcXXX values");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, clkSrc);

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pGpuSubdevice->CalibratePLL((Gpu::ClkSrc)clkSrc));
}

JS_STEST_LWSTOM(JsGpuSubdevice, CheckInfoRom, 0, "Check if InfoROM is valid.")
{
    STEST_HEADER(0, 0, "Usage: GpuSubdevice.CheckInfoRom()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    RC rc;
    rc = pJsGpuSubdevice->GetGpuSubdevice()->CheckInfoRom();
    if (RC::LWRM_NOT_SUPPORTED == rc)
    {
        Printf(Tee::PriLow, "InfoROM is not supported on this board.\n");
    }
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetMonitorRegisters,
                1,
                "Set extra registers to print with -monitor X")
{
    STEST_HEADER(1, 1,
        "Usage: GpuSubdevice.SetMonitorRegisters(registerNamesOrOffsets)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, string, registers);

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC( pGpuSubdevice->SetMonitorRegisters(registers));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetMonitorRegistersFile,
                1,
                "Set extra registers to print with -monitor X read from a file")
{
    STEST_HEADER(1, 1,
        "Usage: GpuSubdevice.SetMonitorRegisters(filename.txt)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, string, filename);

    const string fullFilename = Utility::DefaultFindFile(filename, true);
    if (!Xp::DoesFileExist(fullFilename))
    {
        Printf(Tee::PriNormal,
            "Error: Monitor registers file \"%s\" not found.\n",
            fullFilename.c_str());
        return JS_FALSE;
    }

    FileHolder file;
    long fileSize = 0;
    if ((file.Open(fullFilename, "rb") != OK) ||
        (Utility::FileSize(file.GetFile(), &fileSize) != OK))
    {
        Printf(Tee::PriNormal,
            "Error opening monitor registers file \"%s\".\n",
            fullFilename.c_str());
        return JS_FALSE;
    }

    string fileContent;
    fileContent.resize(fileSize);

    if ((fileContent.size() != (size_t)fileSize) ||
        (fread(&fileContent[0], 1, fileSize, file.GetFile()) != (size_t)fileSize))
    {
        Printf(Tee::PriNormal,
            "Error reading monitor registers file \"%s\".\n",
            fullFilename.c_str());
        return JS_FALSE;
    }

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC( pGpuSubdevice->SetMonitorRegisters(fileContent));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetFbMinimum,
                1,
                "Set Fb size in MB")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.SetFbMinimum(sizeInMB)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, fbMinMB);

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC( pGpuSubdevice->SetFbMinimum(fbMinMB));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                IlwalidateL2Cache,
                1,
                "Ilwalidate L2 cache.")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.IlwalidateL2Cache(flags)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, flags);

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC( pGpuSubdevice->IlwalidateL2Cache(flags));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetBoardInfo,
                1,
                "Get BoardName and BoardDefIndex")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetBoardInfo(Object)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pObjectToReturn);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    UINT32 DUTSupportedVersion = BoardDB::Get().GetDUTSupportedVersion(pSubdev);
    string BoardName;
    UINT32 BoardDefIndex;
    C_CHECK_RC(pSubdev->GetBoardInfoFromBoardDB(&BoardName, &BoardDefIndex, false));
    C_CHECK_RC(pJavaScript->SetProperty(pObjectToReturn,
                                        "DUTSupportedVersion",
                                        DUTSupportedVersion));
    C_CHECK_RC(pJavaScript->SetProperty(pObjectToReturn,
                                        "BoardName",
                                        BoardName));
    RETURN_RC(pJavaScript->SetProperty(pObjectToReturn,
                                       "BoardDefIndex",
                                       BoardDefIndex));
}

JS_SMETHOD_LWSTOM(JsGpuSubdevice, GetAllEccErrors, 1, "Get all ecc errors")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetAllEccErrors([SBE, DBE])");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pEccErrors);

    RC rc;
    UINT32 sbe;
    UINT32 dbe;

    C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->GetAllEccErrors(&sbe, &dbe));
    C_CHECK_RC(pJavaScript->SetElement(pEccErrors, 0, sbe));
    RETURN_RC(pJavaScript->SetElement(pEccErrors, 1, dbe));
}

JS_STEST_LWSTOM(JsGpuSubdevice, SetGpuUtilSensorInformation, 0,
                "Set GPU Util Sensor information")
{
    STEST_HEADER(0, 0, "Usage: GpuSubdevice.SetGpuUtilSensorInformation()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    RETURN_RC(pJsGpuSubdevice->GetGpuSubdevice()->SetGpuUtilSensorInformation());
}

JS_SMETHOD_LWSTOM(JsGpuSubdevice, GetGraphicsUtilization, 1, "Get Utilization of Graphics engine")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetGraphicsUtilization()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pGraphicsUtilization);

    RC rc;

    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    pGpuSubdevice->SetGpuUtilStatusInformation();
    GpuSubdevice::GpuSensorInfo sensorInfo = pGpuSubdevice->GetGpuUtilSensorInformation();
    // Before polling the function continously, the USER must call SetGpuUtilSensorInformation once
    // Because RM returns the graphics utilization as the difference between the times these funcs
    // are called. So SetGpuUtilSensorInformation must be called initially
    if (sensorInfo.queried == false)
    {
        Printf(Tee::PriError, "GPU Utilization Sensor information has not been initialized \n");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }
    UINT32 idx;
    float fvalue = 0.0;
    string unit = "%";
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(idx, &sensorInfo.info.super.objMask.super)

        const auto& topo = sensorInfo.info.topologys[idx];

        if (topo.bNotAvailable)
        {
            continue;
        }

        if (topo.label == LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_GR)
        {
            fvalue = pGpuSubdevice->GetGpuUtilizationFromSensorTopo(sensorInfo, idx);
            switch (topo.unit)
            {
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_PERCENTAGE:     unit = "%";    break;
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_BYTES_PER_NSEC: unit = "MBps"; break;
                case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_GHZ:            unit = "MHz";  break;

                default:
                    unit = Utility::StrPrintf("0x%X", topo.unit);
                    break;
            }
         }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    C_CHECK_RC(pJavaScript->SetElement(pGraphicsUtilization, 0, fvalue));
    RETURN_RC(pJavaScript->SetElement(pGraphicsUtilization, 1, unit));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    GetUnexpectedEccErrors,
    1,
    "Get counts of unexpected ECC errors since MODS started"
)
{
    // TODO(stewarts): Update with new Turing ECC units.
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] =
        "var errs = new Object;\n"
        "Subdevice.GetUnexpectedEccErrs(errs)\n"
        "Print(errs.fbCorr, errs.l2Corr, errs.l1Corr, errs.smCorr)\n"
        "Print(errs.fbUncorr, errs.l2Uncorr, errs.l1Uncorr, errs.smUncorr)\n";

    JavaScriptPtr pJavaScript;
    JsGpuSubdevice *pJsGpuSubdevice =
        JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, "GpuSubdevice");
    JSObject *pEccCounts;
    RC rc;

    if ((pJsGpuSubdevice == NULL) ||
        (NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pEccCounts)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    EccErrCounter *pEccErrCounter = pGpuSubdevice->GetEccErrCounter();
    vector<UINT64> eccCounts;
    C_CHECK_RC(pEccErrCounter->GetUnexpectedErrors(&eccCounts));
    C_CHECK_RC(pJavaScript->SetProperty(pEccCounts, "fbCorr",
                                        eccCounts[EccErrCounter::UnitErrCountIndex(Ecc::Unit::DRAM, Ecc::ErrorType::CORR)]));
    C_CHECK_RC(pJavaScript->SetProperty(pEccCounts, "fbUncorr",
                                        eccCounts[EccErrCounter::UnitErrCountIndex(Ecc::Unit::DRAM, Ecc::ErrorType::UNCORR)]));
    C_CHECK_RC(pJavaScript->SetProperty(pEccCounts, "l2Corr",
                                        eccCounts[EccErrCounter::UnitErrCountIndex(Ecc::Unit::L2, Ecc::ErrorType::CORR)]));
    C_CHECK_RC(pJavaScript->SetProperty(pEccCounts, "l2Uncorr",
                                        eccCounts[EccErrCounter::UnitErrCountIndex(Ecc::Unit::L2, Ecc::ErrorType::UNCORR)]));
    C_CHECK_RC(pJavaScript->SetProperty(pEccCounts, "l1Corr",
                                        eccCounts[EccErrCounter::UnitErrCountIndex(Ecc::Unit::L1, Ecc::ErrorType::CORR)]));
    C_CHECK_RC(pJavaScript->SetProperty(pEccCounts, "l1Uncorr",
                                        eccCounts[EccErrCounter::UnitErrCountIndex(Ecc::Unit::L1, Ecc::ErrorType::UNCORR)]));
    C_CHECK_RC(pJavaScript->SetProperty(pEccCounts, "smCorr",
                                        eccCounts[EccErrCounter::UnitErrCountIndex(Ecc::Unit::LRF, Ecc::ErrorType::CORR)]));
    C_CHECK_RC(pJavaScript->SetProperty(pEccCounts, "smUncorr",
                                        eccCounts[EccErrCounter::UnitErrCountIndex(Ecc::Unit::LRF, Ecc::ErrorType::UNCORR)]));
    RETURN_RC(rc);
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                ValidateVbios,
                3,
                "Validate that the VBIOS is valid")
{
    STEST_HEADER(1, 2,
            "Usage: GpuSubdevice.ValidateVbios(bIgnoreFailure[, Object])\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, bool, bOfficial);
    STEST_OPTIONAL_ARG(1, JSObject*, pVbiosStatus, nullptr);

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    GpuSubdevice::VbiosStatus status = GpuSubdevice::VBIOS_OK;
    bool bFailureIgnored = false;
    StickyRC rc;

    rc = pSubdev->ValidateVbios(bOfficial, &status, &bFailureIgnored);
    if (pVbiosStatus != NULL)
    {
        rc = pJavaScript->SetElement(pVbiosStatus, 0, status);
        rc = pJavaScript->SetElement(pVbiosStatus, 1, bFailureIgnored);
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetEmulationInfo,
                1,
                "Get emulation related info from RM")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: GpuSubdevice.GetEmulationInfo([netlist_num,hw_cl_num,total_dram,chunk_bits])\n";
    JavaScriptPtr pJs;
    JSObject * pReturlwals = NULL;
    LW208F_CTRL_GPU_GET_EMULATION_INFO_PARAMS params;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &pReturlwals)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        StickyRC rc;
        C_CHECK_RC(pSubdev->GetEmulationInfo(&params));
        // Build the return structure.
        C_CHECK_RC(pJs->PackFields(pReturlwals, "IIII",
                                   "lwrrentNetlistNumber", params.lwrrentNetlistNumber,
                                   "hwChangelistNumber", params.hwChangelistNumber,
                                   "totalDram", params.totalDram,
                                   "chunkBits", params.chunkBits));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsGpuSubdevice, DumpInfoRom, 0,
                "Dump the InfoRom contents in readable format")
{
    STEST_HEADER(0, 0, "Usage: GpuSubdevice.DumpInfoRom()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    JsonItem jsi(pContext, pObject, "InfoRomDump");
    RETURN_RC(pJsGpuSubdevice->GetGpuSubdevice()->DumpInfoRom(&jsi));
}

JS_SMETHOD_LWSTOM(JsGpuSubdevice, CheckInfoRomForEccErr, 2,
                  "Read InfoRom for any correctable or uncorrectable ECC error")
{
    UINT32 unit;
    UINT32 errType;
    JavaScriptPtr pJs;
    // Check the arguments.
    if ((NumArguments != 2) ||
        (RC::OK != pJs->FromJsval(pArguments[0], &unit)) ||
        (RC::OK != pJs->FromJsval(pArguments[1], &errType)))
    {
        JS_ReportError(pContext, "Usage: GpuSubdevice.CheckInfoRomForEccErr(Eclwnit, EccErrType)");
        return JS_FALSE;
    }

    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        RETURN_RC(pJsGpuSubdevice->GetGpuSubdevice()->CheckInfoRomForEccErr(static_cast<Ecc::Unit>(unit),
                                                                            static_cast<Ecc::ErrorType>(errType)));
    }

    return JS_FALSE;

}

JS_SMETHOD_LWSTOM(JsGpuSubdevice, CheckInfoRomForPBLCapErr, 1,
                  "Read InfoRom and check whether blacklist table is full or not")
{
    JavaScriptPtr pJs;
    UINT32 PBLCapacity;
    // Check the arguments.
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &PBLCapacity)))
    {
        JS_ReportError(pContext, "Usage: GpuSubdevice.CheckInfoRomForPBLCapErr(PBLCapacity)");
        return JS_FALSE;
    }

    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        RETURN_RC(pJsGpuSubdevice->GetGpuSubdevice()->CheckInfoRomForPBLCapErr(PBLCapacity));
    }

    return JS_FALSE;

}

JS_STEST_LWSTOM(JsGpuSubdevice,
                CheckInfoRomForDprTotalErr,
                1,
                "Read InfoRom and check number of DPR blacklisted pages")
{
    STEST_HEADER(1, 1, "GpuSubdevice.CheckInfoRomForDprTotalErr(maxPages)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, maxPages);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pSubdev->CheckInfoRomForDprTotalErr(maxPages, false));
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                CheckInfoRomForDprDbeTotalErr,
                1,
                "Read InfoRom and check number of DPR DBE blacklisted pages")
{
    STEST_HEADER(1, 1, "GpuSubdevice.CheckInfoRomForDprDbeTotalErr(maxPages)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, maxPages);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pSubdev->CheckInfoRomForDprTotalErr(maxPages, true));
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                CheckInfoRomForDprDbeRateErr,
                2,
                "Read InfoRom and check whether blacklisted pages are within retirement rate")
{
    STEST_HEADER(2, 2, "GpuSubdevice.CheckInfoRomForDprDbeRateErr(ratePages, rateDays)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, ratePages);
    STEST_ARG(1, UINT32, rateDays);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pSubdev->CheckInfoRomForDprDbeRateErr(ratePages, rateDays));
}

JS_SMETHOD_LWSTOM(JsGpuSubdevice, CheckInfoRomForRemappedRowsErr, 2,
                  "Read InfoRom and check if the number of remapped rows violates policy")
{
    JavaScriptPtr pJs;
    RowRemapper::MaxRemapsPolicy policy = {};

    // Check the arguments.
    if ((NumArguments != 2) ||
        (RC::OK != pJs->FromJsval(pArguments[0], &policy.totalRows)) ||
        (RC::OK != pJs->FromJsval(pArguments[1], &policy.numRowsPerBank)))
    {
        JS_ReportError(pContext,
                       "Usage: GpuSubdevice.CheckInfoRomForRemappedRowsErr(maxTotalRemappedRows,\n"
                       "                                                   maxPerBankRemappedRows)");
        return JS_FALSE;
    }

    JsGpuSubdevice* pJsSubdev = nullptr;
    if ((pJsSubdev = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                    "GpuSubdevice")) != 0)
    {
        StickyRC rc;
        rc = pJsSubdev->GetGpuSubdevice()->GetFB()->CheckMaxRemappedRows(policy);
        rc = pJsSubdev->GetGpuSubdevice()->GetFB()->CheckRemapError();
        RETURN_RC(rc);
    }

    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetAspmEntryCounts,
                1,
                "Read ASPM entry counters")
{
    STEST_HEADER(1, 1,
            "var AspmCounters = new Array;\n"
            "var Subdev = new GpuSubdev(0,0);\n"
            "Subdev.GetAspmEntryCounts(AspmCounters)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetAspmCounts);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    Pcie::AspmEntryCounters aspmCounts = {0};
    C_CHECK_RC(pSubdev->GetInterface<Pcie>()->GetAspmEntryCounts(&aspmCounts));
    C_CHECK_RC(pJavaScript->SetElement(pRetAspmCounts,
                                       JsGpuSubdevice::GPU_XMIT_L0S_ID,
                                       aspmCounts.XmitL0SEntry));
    C_CHECK_RC(pJavaScript->SetElement(pRetAspmCounts,
                                       JsGpuSubdevice::UPSTREAM_XMIT_L0S_ID,
                                       aspmCounts.UpstreamL0SEntry));
    C_CHECK_RC(pJavaScript->SetElement(pRetAspmCounts,
                                       JsGpuSubdevice::L1_ID,
                                       aspmCounts.L1Entry));
    C_CHECK_RC(pJavaScript->SetElement(pRetAspmCounts,
                                       JsGpuSubdevice::L1P_ID,
                                       aspmCounts.L1PEntry));
    C_CHECK_RC(pJavaScript->SetElement(pRetAspmCounts,
                                       JsGpuSubdevice::DEEP_L1_ID,
                                       aspmCounts.DeepL1Entry));
    C_CHECK_RC(pJavaScript->SetElement(pRetAspmCounts,
                                       JsGpuSubdevice::L1_1_SS_ID,
                                       aspmCounts.L1_1_Entry));
    RETURN_RC(pJavaScript->SetElement(pRetAspmCounts,
                                      JsGpuSubdevice::L1_2_SS_ID,
                                      aspmCounts.L1_2_Entry));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                ResetAspmEntryCounters,
                0,
                "Reset ASPM entry counters")
{
    STEST_HEADER(0, 0,
            "var Subdev = new GpuSubdev(0,0);\n"
            "Subdev.ResetAspmEntryCounters()\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pSubdev->GetInterface<Pcie>()->ResetAspmEntryCounters());
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
        AddRcToInforom,
        0,
        "Set rc to Black Box object in Inforom")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.AddRcToInforom(ErrorStr)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, string, errorStr);

    RC rc;
    UINT64 errorCode;
    LwRmPtr pLwRm;
    LwRm::Handle cls90E7Handle;
    LW90E7_CTRL_BBX_SET_FIELD_DIAG_RESULT_PARAMS bbxParams = {0};

    errorCode = Utility::Strtoull(errorStr.c_str(), NULL, 10);
    bbxParams.fieldDiagResult = errorCode;
    string versionStr = g_Version;
    versionStr.erase(std::remove(versionStr.begin(), versionStr.end(), '.'),
                     versionStr.end());
    bbxParams.fieldDiagVersion = atoi(versionStr.c_str());
    C_CHECK_RC(pLwRm->Alloc(
                pLwRm->GetSubdeviceHandle(pJsGpuSubdevice->GetGpuSubdevice()),
                &cls90E7Handle,
                GF100_SUBDEVICE_INFOROM,
                NULL));

    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_SET_FIELD_DIAG_RESULT,
                &bbxParams,
                sizeof(bbxParams));

    pLwRm->Free(cls90E7Handle);

    //Returns OK, as this failure is independent from Mods test sequence
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriLow, "InfoRom black box object not found\n");
        RETURN_RC(OK);
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
        PrintMiscFuseRegs,
        0,
        "Get a dump of misc fuse regs")
{
    STEST_HEADER(0, 0, "Usage: GpuSubdevice.PrintMiscFuseRegs()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    GpuSubdevice * pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pGpuSubdevice->PrintMiscFuseRegs());
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetEccConfiguration,
                1,
                "Get the ECC configuration from InfoRom.")
{
    STEST_HEADER(1, 1,
            "var config = {};\n"
            "Subdev.GetEccConfiguration(config);\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pJsEccConfig);

    RC rc;
    LW2080_CTRL_GPU_QUERY_ECC_CONFIGURATION_PARAMS params = {0};
    C_CHECK_RC(LwRmPtr()->ControlBySubdevice(
                   pJsGpuSubdevice->GetGpuSubdevice(),
                   LW2080_CTRL_CMD_GPU_QUERY_ECC_CONFIGURATION,
                   &params, sizeof(params)));
    C_CHECK_RC(pJavaScript->SetProperty(pJsEccConfig, "lwrrentConfig", params.lwrrentConfiguration));
    C_CHECK_RC(pJavaScript->SetProperty(pJsEccConfig, "defaultConfig", params.defaultConfiguration));

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetEccConfiguration,
                0,
                "Set the ECC configuration in inforom.")
{
    STEST_HEADER(1, 1,
            "Subdev.SetEccConfiguration(val)\n"
            "Current values:\n"
            "    0 - disable ECC\n"
            "    1 - enable ECC\n"
            "New configuration takes effect on next reboot\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, newConfiguration);

    LW2080_CTRL_GPU_SET_ECC_CONFIGURATION_PARAMS params = {0};
    params.newConfiguration = newConfiguration;
    RETURN_RC(LwRmPtr()->ControlBySubdevice(
                pJsGpuSubdevice->GetGpuSubdevice(),
                LW2080_CTRL_CMD_GPU_SET_ECC_CONFIGURATION,
                &params, sizeof(params)));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                ToggleMigConfiguration,
                0,
                "Set the MIG configuration in inforom.")
{
    STEST_HEADER(1, 1,
            "Subdev.ToggleMigConfiguration(val)\n"
            "Current values:\n"
            "    0 - disable MIG\n"
            "    1 - enable MIG\n"
            "New configuration takes effect on next reboot\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, bool, enable);

    LW2080_CTRL_GPU_SET_PARTITIONING_MODE_PARAMS partModeParams = {0};
    partModeParams.partitioningMode =
        enable ? LW2080_CTRL_GPU_SET_PARTITIONING_MODE_REPARTITIONING_FAST_RECONFIG
               : LW2080_CTRL_GPU_SET_PARTITIONING_MODE_REPARTITIONING_LEGACY;
    RETURN_RC(LwRmPtr()->ControlBySubdevice(pJsGpuSubdevice->GetGpuSubdevice(),
                                            LW2080_CTRL_CMD_GPU_SET_PARTITIONING_MODE,
                                            &partModeParams,
                                            sizeof(partModeParams)));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetPoisonStatus,
                1,
                "Get whether ECC poison is enabled on the current chip")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetPoisonStatus(outputObject)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    bool poisonEnabled = false;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pSubdev->GetPoisonEnabled(&poisonEnabled));
    C_CHECK_RC(pJavaScript->SetProperty(pRetVal, "isEnabled", poisonEnabled));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GetPcieLinkSpeed,
                  1,
                  "Get the PCIE link speed at the GPU")
{
    STEST_HEADER(1, 1,
            "var Subdev = new GpuSubdev(0,0);\n"
            "var LinkSpeed = Subdev.GetPcieLinkSpeed(ExpectedSpeed)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, nExpSpd);

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    Pci::PcieLinkSpeed expSpd = static_cast<Pci::PcieLinkSpeed>(nExpSpd);

    if (pJavaScript->ToJsval(pSubdev->GetInterface<Pcie>()->GetLinkSpeed(expSpd),
                             pReturlwalue) != OK )
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
P_(JsGpuSubdevice_Get_PciELinkSpeed);
P_(JsGpuSubdevice_Get_PciELinkSpeed)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    static Deprecation depr("GpuSubdevice.PciELinkSpeed", "3/10/2017",
                            "PciELinkSpeed is deprecated. Please use PcieLinkSpeed.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JsGpuSubdevice * pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, NULL)) != 0)
    {
        UINT32 result = pJsGpuSubdevice->GetPcieLinkSpeed();
        if (OK != JavaScriptPtr()->ToJsval(result, pValue))
        {
           JS_ReportError(pContext, "Failed to get JsGpuSubdevice.PciELinkSpeed" );
           *pValue = JSVAL_NULL;
           return JS_FALSE;
        }
        return JS_TRUE;
    }
    return JS_FALSE;
}
static SProperty JsGpuSubdevice_PciELinkSpeed
(
    JsGpuSubdevice_Object,
    "PciELinkSpeed",
    0,
    0,
    JsGpuSubdevice_Get_PciELinkSpeed,
    0,
    JSPROP_READONLY | 0,
    "Pcie link speed"
);

//-----------------------------------------------------------------------------
P_(JsGpuSubdevice_Get_PciELinkWidth);
P_(JsGpuSubdevice_Get_PciELinkWidth)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    static Deprecation depr("GpuSubdevice.PciELinkWidth", "3/10/2017",
                            "PciELinkWidth is deprecated. Please use PcieLinkWidth.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JsGpuSubdevice * pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, NULL)) != 0)
    {
        UINT32 result = pJsGpuSubdevice->GetPcieLinkWidth();
        if (OK != JavaScriptPtr()->ToJsval(result, pValue))
        {
           JS_ReportError(pContext, "Failed to get JsGpuSubdevice.PciELinkWidth" );
           *pValue = JSVAL_NULL;
           return JS_FALSE;
        }
        return JS_TRUE;
    }
    return JS_FALSE;
}
static SProperty JsGpuSubdevice_PciELinkWidth
(
    JsGpuSubdevice_Object,
    "PciELinkWidth",
    0,
    0,
    JsGpuSubdevice_Get_PciELinkWidth,
    0,
    JSPROP_READONLY | 0,
    "Pcie link width"
);

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                PrintNewBlacklist,
                1,
                "Print the pages blacklisted since mods started")
{
    STEST_HEADER(1, 1,
            "Subdevice.PrintNewBlacklist(pri)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, pri);

    RC rc;
    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    Blacklist blacklist;
    C_CHECK_RC(blacklist.LoadIfSupported(pGpuSubdevice));
    blacklist.Subtract(pGpuSubdevice->GetBlacklistOnEntry());
    C_CHECK_RC(blacklist.Print(static_cast<Tee::Priority>(pri)));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GetNumNewBlacklisted,
                  0,
                  "Get number of pages blacklisted since mods started")
{
    STEST_HEADER(0, 0,
        "var numNewBlacklisted = Subdevice.GetNumNewBlacklisted();\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    *pReturlwalue = JSVAL_NULL;
    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    Blacklist blacklist;
    if (blacklist.LoadIfSupported(pGpuSubdevice) != OK)
    {
        return JS_FALSE;
    }
    blacklist.Subtract(pGpuSubdevice->GetBlacklistOnEntry());
    if (pJavaScript->ToJsval(blacklist.GetSize(), pReturlwalue) != OK)
    {
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GetNumBlacklisted,
                  0,
                  "Get number of pages blacklisted since mods started")
{
    STEST_HEADER(0, 0,
        "var numBlacklisted = Subdevice.GetNumBlacklisted();\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    Blacklist blacklist;
    if (blacklist.LoadIfSupported(pGpuSubdevice) != RC::OK ||
        pJavaScript->ToJsval(blacklist.GetSize(), pReturlwalue) != RC::OK)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetErrCount,
                1,
                "Get the Ecc/Edc error count summary")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Subdevice.GetErrCount()\n";

    JavaScriptPtr pJavaScript;
    JsGpuSubdevice *pJsGpuSubdevice =
        JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, "GpuSubdevice");

    if ((pJsGpuSubdevice == NULL) ||
        (NumArguments != 0))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    string errStr;
    *pReturlwalue = JSVAL_NULL;
    GpuSubdevice *pSubdev = pJsGpuSubdevice->GetGpuSubdevice();

    // Dump ECC stats if ECC is supported
    bool eccSupported = false;
    UINT32 eccEnableMask = 0;
    if (RC::OK == pSubdev->GetEccEnabled(&eccSupported, &eccEnableMask) && eccSupported)
    {
        errStr += pSubdev->GetEccErrCounter()->DumpStats();
    }

    // Dump EDC stats if EDC is supported
    bool edcSupported = false;
    UINT32 edcEnableMask = 0;
    if (RC::OK == pSubdev->GetEdcEnabled(&edcSupported, &edcEnableMask) && edcSupported)
    {
        errStr += pSubdev->GetEdcErrCounter()->DumpStats();
    }

    if (pJavaScript->ToJsval(errStr, pReturlwalue) != OK )
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GetBusinessCycle,
                  0,
                  "Get the business cycle number of this subdevice")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Subdevice.GetBusinessCycle()\n";

    JsGpuSubdevice *pJsGpuSubdevice =
        JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, "GpuSubdevice");

    if (pJsGpuSubdevice == NULL || NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    *pReturlwalue = INT_TO_JSVAL(pGpuSubdevice->GetBusinessCycle());
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                ValidateSoftwareTree,
                0,
                "Verify that this MODS branch supports this subdevice")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Subdevice.ValidateSoftwareTree()\n";

    JsGpuSubdevice *pJsGpuSubdevice =
        JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, "GpuSubdevice");

    if (pJsGpuSubdevice == NULL || NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pGpuSubdevice->ValidateSoftwareTree());
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                SetThreadCpuAffinity,
                0,
                "Set the current thread CPU affinity for the GPU")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        RETURN_RC(pSubdev->SetThreadCpuAffinity());
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    GetGOMMode,
    1,
    "Get the current GOM Mode"
)
{
    STEST_HEADER(1, 1,
            "var mode = new Object;\n"
            "Subdevice.GetGOMMode(Mode)\n"
            "Print(Mode.lwrrentMode)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pMode);

    RC rc;
    UINT32 mode;
    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetGOMMode(&mode));
    C_CHECK_RC(pJavaScript->SetProperty(pMode, "lwrrentMode", mode));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    GetAspmL1ssEnabled,
    1,
    "Get a Pci.ASPM_SUB_* mask and cya mask of which L1 substates we are allowed to enter"
)
{
    STEST_HEADER(1, 1, "var mask = Subdevice.GetAspmL1ssEnabled(object)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject *, pAspmL1SS);

    RC rc;
    UINT32 mask;
    auto pPcie = pJsGpuSubdevice->GetGpuSubdevice()->GetInterface<Pcie>();
    C_CHECK_RC(pPcie->GetAspmL1ssEnabled(&mask));
    C_CHECK_RC(pJavaScript->SetProperty(pAspmL1SS, "mask", mask));
    C_CHECK_RC(pPcie->GetAspmCyaL1SubState(&mask));
    C_CHECK_RC(pJavaScript->SetProperty(pAspmL1SS, "cya", mask));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    GetGpuLTREnabled,
    1,
    "Retrieve whether LTR is enabled on the GPU"
)
{
    STEST_HEADER(1, 1, "var mask = Subdevice.GetGpuLTREnabled(object)\n");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject *, pLTR);

    RC rc;
    bool isEnabled;
    auto pPcie = pJsGpuSubdevice->GetGpuSubdevice()->GetInterface<Pcie>();
    C_CHECK_RC(pPcie->GetLTREnabled(&isEnabled));
    C_CHECK_RC(pJavaScript->SetProperty(pLTR, "isEnabled", isEnabled));
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
static RC ParseHoldIsmHelper(
    uintN        NumArgs
    ,uintN        startArg
    ,jsval      * pArguments
    ,IsmSpeedo::HoldInfo *pHoldInfo
    ,JSObject **pRetData
)
{
    JavaScriptPtr pJavaScript;
    RC rc;
    INT32 mode;
    CHECK_RC(pJavaScript->FromJsval(pArguments[startArg], &mode));
    CHECK_RC(pJavaScript->FromJsval(pArguments[startArg+1], &pHoldInfo->t1Value));
    CHECK_RC(pJavaScript->FromJsval(pArguments[startArg+2], &pHoldInfo->t2Value));
    pHoldInfo->mode = static_cast<IsmSpeedo::HoldMode>(mode);

    if ((mode == IsmSpeedo::HOLD_AUTO) || (mode == IsmSpeedo::HOLD_MANUAL))
    {
        if (NumArgs != (startArg + 6))
            return RC::BAD_PARAMETER;

        CHECK_RC(pJavaScript->FromJsval(pArguments[startArg+3], &pHoldInfo->bDoubleClk));
        CHECK_RC(pJavaScript->FromJsval(pArguments[startArg+4], &pHoldInfo->modeSel));
        CHECK_RC(pJavaScript->FromJsval(pArguments[startArg+5], pRetData));
    }
    else if ((mode == IsmSpeedo::HOLD_OSC) && (NumArgs == (startArg + 5)))
    {
        CHECK_RC(pJavaScript->FromJsval(pArguments[startArg+3], &pHoldInfo->oscClkCount));
        CHECK_RC(pJavaScript->FromJsval(pArguments[startArg+4], pRetData));
    }
    else
    {
        return RC::BAD_PARAMETER;
    }

    return rc;
}

//------------------------------------------------------------------------------
static RC HoldIsmDataHelper(
    JSContext   * pContext
    ,JSObject   * pRetData
    ,const vector<IsmSpeedo::HoldInfo> &holdIsmData
)
{
    RC rc;
    JavaScriptPtr pJs;

    for (UINT32 idx = 0; idx < holdIsmData.size(); idx++)
    {
        JSObject *pInfo = JS_NewObject(pContext,&DummyClass,0,0);
        jsval tmpjs;
        CHECK_RC(pJs->SetProperty(pInfo,"chainId",    holdIsmData[idx].chainId));
        CHECK_RC(pJs->SetProperty(pInfo,"chiplet",    holdIsmData[idx].chiplet));
        CHECK_RC(pJs->SetProperty(pInfo,"startBit",   holdIsmData[idx].startBit));
        CHECK_RC(pJs->SetProperty(pInfo,"mode",       static_cast<INT32>(holdIsmData[idx].mode)));
        CHECK_RC(pJs->SetProperty(pInfo,"bDoubleClk", holdIsmData[idx].bDoubleClk));
        CHECK_RC(pJs->SetProperty(pInfo,"modeSel",    holdIsmData[idx].modeSel));
        CHECK_RC(pJs->SetProperty(pInfo,"t1Value",    holdIsmData[idx].t1Value));
        CHECK_RC(pJs->SetProperty(pInfo,"t2Value",    holdIsmData[idx].t2Value));
        CHECK_RC(pJs->SetProperty(pInfo,"oscClkCount",holdIsmData[idx].oscClkCount));
        CHECK_RC(pJs->SetProperty(pInfo,"t1RawCount", holdIsmData[idx].t1RawCount));
        CHECK_RC(pJs->SetProperty(pInfo,"t2RawCount", holdIsmData[idx].t2RawCount));
        CHECK_RC(pJs->SetProperty(pInfo,"t1OscPs",    holdIsmData[idx].t1OscPs));
        CHECK_RC(pJs->SetProperty(pInfo,"t2OscPs",    holdIsmData[idx].t2OscPs));
        CHECK_RC(pJs->SetProperty(pInfo,"bMismatchFound", holdIsmData[idx].bMismatchFound));
        CHECK_RC(pJs->ToJsval(pInfo, &tmpjs));
        CHECK_RC(pJs->SetElement(pRetData, idx, tmpjs));
    }
    return rc;
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GP10x supports it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadHoldISMs,
                6,
                "Read all hold ISMs")
{
    STEST_HEADER(5 /*minArgs*/, 6 /*maxArgs*/,
        "Usage:\n"
        "    GpuSubdevice.ReadHoldISMs([AUTO,MANUAL], t1, t2, doubleClk, modesel, retarray)\n"
        "    GpuSubdevice.ReadHoldISMs([OSC], t1, t2, oscCount, retarray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    RC rc;
    IsmSpeedo::HoldInfo holdSettings;
    JSObject *pRetData;
    rc = ParseHoldIsmHelper(NumArguments, 0, pArguments, &holdSettings, &pRetData);
    if (OK != rc)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));

    vector<IsmSpeedo::HoldInfo> holdIsmData;
    C_CHECK_RC(pISM->ReadHoldISMs(holdSettings, &holdIsmData));
    C_CHECK_RC(HoldIsmDataHelper(pContext, pRetData, holdIsmData));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
// This feature is not supported on all GPUs. GP10x supports it.
JS_STEST_LWSTOM(JsGpuSubdevice,
                ReadHoldISMsOnChain,
                6,
                "Setup all the Hold ISMs for a specified chain.")
{

    STEST_HEADER(7 /*minArgs*/, 8 /*maxArgs*/,
        "Usage:\n"
        "    GpuSubdevice.ReadHoldISMsOnChain(chainId, chiplet, [AUTO,MANUAL], t1, t2, doubleClk, modesel, retarray)\n"
        "    GpuSubdevice.ReadHoldISMsOnChain(chainId, chiplet, [OSC], t1, t2, oscCount, retarray)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, INT32, chainId);
    STEST_ARG(1, INT32, chiplet);

    RC rc;
    IsmSpeedo::HoldInfo holdSettings;
    JSObject *pRetData;
    rc = ParseHoldIsmHelper(NumArguments, 2, pArguments, &holdSettings, &pRetData);
    if (OK != rc)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    GpuSubdevice * psd = pJsGpuSubdevice->GetGpuSubdevice();
    GpuIsm * pISM;
    C_CHECK_RC(psd->GetISM(&pISM));

    vector<IsmSpeedo::HoldInfo> holdIsmData;
    C_CHECK_RC(pISM->ReadHoldISMsOnChain(chainId,
                                         chiplet,
                                         holdSettings,
                                         &holdIsmData));
    C_CHECK_RC(HoldIsmDataHelper(pContext, pRetData, holdIsmData));
    RETURN_RC(rc);
}

P_( JsGpuSubdevice_Get_HBMDev);
static SProperty JsGpuSubdevice_HBMDev
(
    JsGpuSubdevice_Object,
    "HBMDev",
    0,
    0,
    JsGpuSubdevice_Get_HBMDev,
    0,
    JSPROP_READONLY,
    "acquire the HBM device - null if nothing"
);
P_( JsGpuSubdevice_Get_HBMDev )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        if ((pSubdev->GetHBMImpl() == nullptr) ||
            (pSubdev->GetHBMImpl()->GetHBMDev().get() == nullptr))
        {
            *pValue = JSVAL_NULL;
            return JS_TRUE;
        }

        JsHBMDev * pJsHBMDev = new JsHBMDev();
        MASSERT(pJsHBMDev);
        HBMDevicePtr pHBMDev = pSubdev->GetHBMImpl()->GetHBMDev();
        pJsHBMDev->SetHBMDev(pHBMDev);
        if (pJsHBMDev->CreateJSObject(pContext, pObject) != OK)
        {
            delete pJsHBMDev;
            JS_ReportError(pContext, "Unable to create JsHBMDev JSObject");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }

        if (pJs->ToJsval(pJsHBMDev->GetJSObject(), pValue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_( JsGpuSubdevice_Get_NumaNode);
static SProperty JsGpuSubdevice_NumaNode
(
    JsGpuSubdevice_Object,
    "NumaNode",
    0,
    0,
    JsGpuSubdevice_Get_NumaNode,
    0,
    JSPROP_READONLY,
    "NUMA node in which the device is situated, or else -1"
);
P_( JsGpuSubdevice_Get_NumaNode )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject,
                                          "GpuSubdevice")) != 0)
    {
        GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
        if (pJs->ToJsval(pSubdev->GetNumaNode(), pValue) == RC::OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                HotReset,
                0,
                "Performs a secondary bus reset")
{
    STEST_HEADER(0, 0, "Usage: Subdev.HotReset()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pSubdev->HotReset());
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    GetHBMTypeInfo,
    1,
    "Read Vendor and Revision of HBM")
{
    STEST_HEADER(1, 1, "Usage: Subdevice.GetHBMTypeInfo(HBMInfo)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject *, pHBMData);

    RC rc;
    string vendor, revision;
    if (pJsGpuSubdevice->GetGpuSubdevice()->GetHBMTypeInfo(&vendor, &revision))
    {
        C_CHECK_RC(pJavaScript->SetElement(pHBMData, 0, vendor));
        C_CHECK_RC(pJavaScript->SetElement(pHBMData, 1, revision));
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    GetHBMSiteInfo,
    2,
    "Read HBM Site information")
{
    STEST_HEADER(2, 2, "Usage: Subdevice.GetHBMSiteInfo(HBMSiteInfo, siteID)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject *, pHBMData);
    STEST_ARG(1, UINT32, siteID);

    RC rc;
    GpuSubdevice::HbmSiteInfo hbmSiteInfo;
    if (pJsGpuSubdevice->GetGpuSubdevice()->GetHBMSiteInfo(siteID, &hbmSiteInfo))
    {
        C_CHECK_RC(pJavaScript->SetProperty(pHBMData, "manufacturingYear",
                                            hbmSiteInfo.manufacturingYear));
        C_CHECK_RC(pJavaScript->SetProperty(pHBMData, "manufacturingWeek",
                                            hbmSiteInfo.manufacturingWeek));
        C_CHECK_RC(pJavaScript->SetProperty(pHBMData, "manufacturingLoc",
                                            hbmSiteInfo.manufacturingLoc));
        C_CHECK_RC(pJavaScript->SetProperty(pHBMData, "serialNumber",
                                            /* JS does not support uint64. Pass value as string */
                                            Utility::StrPrintf("0x%09llx", hbmSiteInfo.serialNumber)));
        C_CHECK_RC(pJavaScript->SetProperty(pHBMData, "modelPartNumber",
                                            hbmSiteInfo.modelPartNumber));
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    ReconfigGpuHbmLanes,
    1,
    "Reconfigure GPU HBM Lanes")
{
    STEST_HEADER(1, 1, "Usage: subDev.ReconfigGpuLanes(LanesToReconfigure).");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JsArray, jsRepairLanes);

    RC rc;
    for (UINT32 i = 0; i < jsRepairLanes.size(); i++)
    {
        JSObject* jsAddrObj;
        string repairTypeStr;
        C_CHECK_RC(pJavaScript->FromJsval(jsRepairLanes[i], &jsAddrObj));

        string laneName;
        UINT32 hwFbpa, subpartition, laneBit;

        C_CHECK_RC(pJavaScript->UnpackFields(
            jsAddrObj,      "sIIIs",
            "LaneName",     &laneName,
            "HwFbio",       &hwFbpa,
            "Subpartition", &subpartition,
            "LaneBit",      &laneBit,
            "RepairType",   &repairTypeStr));

        LaneError laneError(Memory::HwFbpa(hwFbpa), laneBit,
                            LaneError::GetLaneTypeFromString(repairTypeStr));
        C_CHECK_RC(pJsGpuSubdevice->GetGpuSubdevice()->ReconfigGpuHbmLanes(laneError));
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    GetSmcPartitionInfo,
    2,
    "Read SMC partition information")
{
    STEST_HEADER(2, 2, "Usage: Subdevice.GetSmcPartitionInfo(partitionID, SmcPartitionInfo)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, partitionID);
    STEST_ARG(1, JSObject *, pSMCInfo);

    RC rc;
    GpuSubdevice::SmcPartTopo smcPartTopo;
    if (pJsGpuSubdevice->GetGpuSubdevice()->
        GetSmcPartitionInfo(partitionID, &smcPartTopo) == RC::OK)
    {
        UINT32 gpcCount = 0;
        UINT32 tpcCount = 0;

        for (const auto engine : smcPartTopo.smcEngines)
        {
            gpcCount += static_cast<UINT32>((engine.gpcs).size());
            for (const auto gpc : engine.gpcs)
            {
                tpcCount += gpc.numTpcs;
            }
        }

        C_CHECK_RC(pJavaScript->SetProperty(pSMCInfo, "gpcCount", gpcCount));
        C_CHECK_RC(pJavaScript->SetProperty(pSMCInfo, "tpcCount", tpcCount));
    }
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsGpuSubdevice,
    AddTempCtrl,
    8,
    "Instantiate and add an new temp controller")
{
    STEST_HEADER(8, 8,
        "Usage: GpuSubdevice.Add(Id, Name, Min, Max, NumInst, Unit, InterfaceType, InterfaceObj)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT32, id);
    STEST_ARG(1, string, name);
    STEST_ARG(2, UINT32, min);
    STEST_ARG(3, UINT32, max);
    STEST_ARG(4, UINT32, numInst);
    STEST_ARG(5, string, units);
    STEST_ARG(6, string, interfaceType);
    STEST_ARG(7, JSObject *, pInterfaceParams);

    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    RETURN_RC(pSubdev->AddTempCtrl(id, name, min, max, numInst, units,
                                   interfaceType, pInterfaceParams));
}

JS_CLASS(RgbMlwType);
static SObject RgbMlwType_Object
(
    "RgbMlwType",
    RgbMlwTypeClass,
    0,
    0,
    "RGB MLW to check FW version for in CheckConfig test"
);
PROP_CONST(RgbMlwType, RgbMlwGpu, GpuSubdevice::RgbMlwGpu);
PROP_CONST(RgbMlwType, RgbMlwSli, GpuSubdevice::RgbMlwSli);

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetRgbMlwFwVersion,
                2,
                "Get major and minor version of the RGB MLW FW")
{
    STEST_HEADER(2, 2, "Usage: GpuSubdevice.GetRgbMlwFwVersion(RgbMlwType, Version)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, UINT08, type);
    STEST_ARG(1, JSObject *, pVersion);

    RC rc;
    UINT32 majorVer, minorVer;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pSubdev->GetRgbMlwFwVersion((GpuSubdevice::RgbMlwType)(type),
                                           &majorVer,
                                           &minorVer));
    C_CHECK_RC(pJavaScript->SetProperty(pVersion, "major", majorVer));
    RETURN_RC(pJavaScript->SetProperty(pVersion, "minor", minorVer));
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuSubdevice,
                  GetPmuDmemInfo,
                  1,
                  "Get information about memory usage in PMU firmware")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetPmuDmemInfo(OutInfo={heapSize, heapFree})");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject *, pInfo);

    LW2080_CTRL_FLCN_GET_DMEM_USAGE_PARAMS params = { };
    params.flcnID = LW2080_ENGINE_TYPE_PMU;

    RC rc;
    const auto pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(LwRmPtr()->ControlBySubdevice(pSubdev,
                                             LW2080_CTRL_CMD_FLCN_GET_DMEM_USAGE,
                                             &params,
                                             sizeof(params)));

    C_CHECK_RC(pJavaScript->SetProperty(pInfo, "heapSize", params.heapSize));
    RETURN_RC(pJavaScript->SetProperty(pInfo, "heapFree", params.heapFree));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuSubdevice,
                GetSubRevision,
                1,
                "Get Sub revision from OPT fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetSubRevision(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 subRev = 0;
    GpuSubdevice *pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pGpuSubdevice->GetChipSubRevision(&subRev));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, subRev));
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                GetSkuId,
                1,
                "Get SKU ID from OPT fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetSkuId(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 skuId = 0;
    const RegHal& regs = pJsGpuSubdevice->GetGpuSubdevice()->Regs();
    C_CHECK_RC(regs.Read32Priv(MODS_FUSE_OPT_SKU_ID_DATA, &skuId));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, skuId));
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                GetFuseFileVersion,
                1,
                "Get fuse file version from OPT fuse")
{
    STEST_HEADER(1, 1, "Usage: GpuSubdevice.GetFuseFileVersion(RetVal)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    UINT32 version = 0;
    const RegHal& regs = pJsGpuSubdevice->GetGpuSubdevice()->Regs();
    C_CHECK_RC(regs.Read32Priv(MODS_FUSE_OPT_FUSE_FILE_VERSION_DATA, &version));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, version));
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                OverrideQuickSlowdown,
                2,
                "Force override of slowdown signal sent to quick_slowdown module")
{
    STEST_HEADER(2, 2,
                 "Usage: GpuSubdevice.OverrideQuickSlowdown(enable, mask)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, bool,   enable);
    STEST_ARG(1, UINT32, ovrMask);

    GpuSubdevice* pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    RegHal&       regs          = pGpuSubdevice->Regs();
    const UINT32  maxGpcCount   = pGpuSubdevice->GetMaxGpcCount();
    const UINT32  gpcMask       = pGpuSubdevice->GetFsImpl()->GpcMask();
    RC rc;

    // If slowdown isn't supported, fail
    if (!regs.IsSupported(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL))
    {
        Printf(Tee::PriError,
               "quick_slowdown module is not present on this GPU!\n");
        RETURN_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
    }

    for (UINT32 hwGpc = 0; hwGpc < maxGpcCount; ++hwGpc)
    {
        // Only override valid GPCs
        if (((1 << hwGpc) & gpcMask & ovrMask) == 0)
        {
            continue;
        }

        // If a valid GPC doesn't have slowdown enabled, fail
        if (!regs.Test32(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_ENABLE_YES,
                         hwGpc))
        {
            Printf(Tee::PriError,
                   "quick_slowdown module is not enabled on phys GPC %d!\n",
                   hwGpc);
            RETURN_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }

        regs.Write32(enable ?
                     MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_VAL_YES :
                     MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_VAL_NO,
                     hwGpc);
        regs.Write32(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_YES,
                     hwGpc);
    }

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                OverrideQuickSlowdownB,
                2,
                "Force override of quick slowdown control B")
{
    STEST_HEADER(2, 2,
                 "Usage: GpuSubdevice.OverrideQuickSlowdownB(enable, ovrMask)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, bool,   enable);
    STEST_ARG(1, UINT32, ovrMask);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pSubdev->OverrideQuickSlowdownB(enable, ovrMask));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                OverrideIssueRate,
                1,
                "Force override of instruction issue rate ")
{
    STEST_HEADER(1, 1,
                 "Usage: GpuSubdevice.OverrideIssueRate(overrideStr)");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");
    STEST_ARG(0, string, overrideStr);

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    C_CHECK_RC(pSubdev->OverrideIssueRate(overrideStr, nullptr));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsGpuSubdevice,
                DumpMSVDDRamAssistInfo,
                0,
                "Dump the MSVDD RAM Assist Information")
{
    STEST_HEADER(0, 0,
                 "Usage: GpuSubdevice.DumpMSVDDRamAssistInfo()");
    STEST_PRIVATE(JsGpuSubdevice, pJsGpuSubdevice, "GpuSubdevice");

    RC rc;
    GpuSubdevice* pSubdev = pJsGpuSubdevice->GetGpuSubdevice();
    GpuSubdevice::MSVDDRAMAssistStatus msvddRAMAssistStatus = {};
    C_CHECK_RC(pSubdev->GetMsvddRAMAssistStatus(&msvddRAMAssistStatus));

    string outRAMAssis = "MSVDD RAM Assist Config: \n";
    outRAMAssis += Utility::StrPrintf("    Type = Dynamic With HW LUT \n");
    outRAMAssis += Utility::StrPrintf("    Engaged = %d \n", msvddRAMAssistStatus.ramAssistEngaged);
    outRAMAssis += Utility::StrPrintf("    VCritical Low  = %d uV\n",
        msvddRAMAssistStatus.vCritLowLUTVal*(pSubdev->GetAvfs()->GetLutStepSizeuV()) + pSubdev->GetAvfs()->GetLutMilwoltageuV());
    outRAMAssis += Utility::StrPrintf("    VCritical High = %d uV\n",
        msvddRAMAssistStatus.vCritHighLUTVal*(pSubdev->GetAvfs()->GetLutStepSizeuV()) + pSubdev->GetAvfs()->GetLutMilwoltageuV());
    outRAMAssis += Utility::StrPrintf("    Floating Point Temperature from SYS_TSENSE sensor = %.2f \n",
        msvddRAMAssistStatus.sysTsenseTempFP);
    outRAMAssis += Utility::StrPrintf("    SYS_TSENSE sensor state = %d \n",
        msvddRAMAssistStatus.tsenseStateValid);
    outRAMAssis += Utility::StrPrintf("    VCritical Value hardware lookup table index = %d \n",
        msvddRAMAssistStatus.hwlutIndex);
    Printf(Tee::PriNormal, "%s", outRAMAssis.c_str());

    RETURN_RC(rc);
}

RC JsGpuSubdevice::EnableGpcRg(bool bEnable)
{
    return GetGpuSubdevice()->EnableGpcRg(bEnable);
}
JS_STEST_BIND_ONE_ARG(JsGpuSubdevice, EnableGpcRg,
                      toggle, bool,
                      "Enable/Disable GPC RG if supported");
