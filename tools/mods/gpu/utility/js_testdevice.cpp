/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/js_testdevice.h"

#include "core/include/ism.h"
#include "core/include/js_uint64.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "device/interface/js/js_c2c.h"
#include "device/interface/js/js_gpio.h"
#include "device/interface/js/js_i2c.h"
#include "device/interface/js/js_lwlink.h"
#include "device/interface/js/js_pcie.h"
#include "device/interface/js/js_portpolicyctrl.h"
#include "device/interface/js/js_xusbhostctrl.h"
#include "gpu/display/js_disp.h"
#include "gpu/fuse/js_fuse.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/perf/js_perf.h"
#include "gpu/perf/js_power.h"
#include "gpu/reghal/js_reghal.h"

//-----------------------------------------------------------------------------
// TestDevice JS Interface
//
namespace
{
#define SET_INTERFACE(name)                                                    \
    if (pTestDevice->SupportsInterface<name>())                                \
    {                                                                          \
        Js##name* pJs##name = new Js##name(pTestDevice->GetInterface<name>()); \
        MASSERT(pJs##name);                                                    \
        CHECK_RC(pJs##name->CreateJSObject(cx, obj));                          \
        pJsTestDevice->SetJs##name(pJs##name);                                 \
    }
#define SET_OBJ_INTERFACE(pDev, name)                    \
    if (pDev->Get##name())                               \
    {                                                    \
        Js##name* pJs##name = new Js##name();            \
        MASSERT(pJs##name);                              \
        pJs##name->Set##name##Obj(pDev->Get##name()); \
        CHECK_RC(pJs##name->CreateJSObject(cx, obj));    \
        pJsTestDevice->SetJs##name(pJs##name);           \
    }

    RC SetJSInterfaces(JsTestDevice* pJsTestDevice, JSContext* cx, JSObject* obj)
    {
        RC rc;

        TestDevicePtr pTestDevice = pJsTestDevice->GetTestDevice();
        MASSERT(pTestDevice.get() != nullptr);

        SET_INTERFACE(Pcie);
        SET_INTERFACE(I2c);
        SET_INTERFACE(Gpio);
        SET_INTERFACE(C2C);
#if defined(INCLUDE_LWLINK)
        SET_INTERFACE(LwLink);
#endif
#if defined(INCLUDE_XUSB)
        SET_INTERFACE(PortPolicyCtrl);
        SET_INTERFACE(XusbHostCtrl);
#endif
#if defined(INCLUDE_DGPU)
        SET_OBJ_INTERFACE(pTestDevice, Fuse);
#endif

        JsRegHal* pJsRegHal = new JsRegHal(&(pTestDevice->Regs()));
        MASSERT(pJsRegHal);
        CHECK_RC(pJsRegHal->CreateJSObject(cx, obj));
        pJsTestDevice->SetJsRegHal(pJsRegHal);

        auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
        if (pSubdev)
        {
            SET_OBJ_INTERFACE(pSubdev, Perf);
            SET_OBJ_INTERFACE(pSubdev, Power);
            SET_OBJ_INTERFACE(pSubdev, Display);
        }

        return OK;
    }
}

//-----------------------------------------------------------------------------
//! \brief Constructor for a JS TestDevice object.
//!
//! This ctor takes in one argument that determines which test device
//! the user wants access to.  It then creates a JsTestDevice wrapper
//! and associates it with the given TestDevice.
//!
//! \note This function is only called from JS when a TestDevice object
//! is created in JS (i.e. sb = new TestDevice(i)).
static JSBool C_JsTestDevice_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    const char usage[] = "Usage: TestDevice(TestDeviceInst, [TestDeviceType])";

    if (argc > 2)
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJs;
    UINT32        devInst = 0;
    TestDevicePtr pTestDevice;

    if ((OK != pJs->FromJsval(argv[0], &devInst)))
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    UINT32 devTypeVal = static_cast<UINT32>(TestDevice::TYPE_UNKNOWN);
    if ((argc > 1) && (OK != pJs->FromJsval(argv[1], &devTypeVal)))
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }
    TestDevice::Type devType = static_cast<TestDevice::Type>(devTypeVal);

    if (!DevMgrMgr::d_TestDeviceMgr)
    {
        Printf(Tee::PriError, "JsTestDevice constructor called before Gpu.Initialize\n");
        return JS_FALSE;
    }

    //! Try and retrieve the specified TestDevice
    TestDeviceMgr *pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    if (devType == TestDevice::TYPE_UNKNOWN)
    {
        if (pTestDeviceMgr->GetDevice(devInst, &pTestDevice) != OK)
        {
            JS_ReportWarning(cx, "Invalid test device index");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
    }
    else
    {
        if (pTestDeviceMgr->GetDevice(devType, devInst, &pTestDevice) != OK)
        {
            JS_ReportWarning(cx, "Invalid test device index");
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);

    //! Create a JsTestDevice wrapper and associate it with the
    //! given TestDevice.
    unique_ptr<JsTestDevice> pJsTestDevice = make_unique<JsTestDevice>();
    MASSERT(pJsTestDevice.get());
    pJsTestDevice->SetTestDevice(pTestDevice);

    if (OK != SetJSInterfaces(pJsTestDevice.get(), cx, obj))
    {
        JS_ReportWarning(cx, "Could not create interfaces");
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    //! Finally tie the JsTestDevice wrapper to the new object
    return JS_SetPrivate(cx, obj, pJsTestDevice.release());
}

//-----------------------------------------------------------------------------
static void C_JsTestDevice_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    //! If a JsTestDevice was associated with this object, make
    //! sure to delete it
    JsTestDevice* pJsTestDevice = (JsTestDevice*)JS_GetPrivate(cx, obj);
    if (pJsTestDevice)
    {
        delete pJsTestDevice;
    }
    JS_SetPrivate(cx, obj, nullptr);
}

//-----------------------------------------------------------------------------
static JSClass JsTestDevice_class =
{
    "TestDevice",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsTestDevice_finalize
};

//-----------------------------------------------------------------------------
static SObject JsTestDevice_Object
(
    "TestDevice",
    JsTestDevice_class,
    0,
    0,
    "TestDevice JS Object",
    C_JsTestDevice_constructor
);

JS_CLASS(DeviceError);

//-----------------------------------------------------------------------------
RC JsTestDevice::CreateJSObject(JSContext* cx, JSObject* obj)
{
    RC rc;

    //! Only create one JSObject per TestDevice
    if (m_pJsTestDeviceObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsTestDevice.\n");
        return OK;
    }

    m_pJsTestDeviceObj = JS_DefineObject(cx,
                                         obj, // GpuTest object
                                         "BoundTestDevice", // Property name
                                         &JsTestDevice_class,
                                         JsTestDevice_Object.GetJSObject(),
                                         JSPROP_READONLY);

    if (!m_pJsTestDeviceObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsTestDevice instance into the private area
    //! of the new JSOBject.  This will tie the two together.
    if (JS_SetPrivate(cx, m_pJsTestDeviceObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsTestDevice.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsTestDeviceObj, "Help", &C_Global_Help, 1, 0);

    CHECK_RC(SetJSInterfaces(this, cx, m_pJsTestDeviceObj));

    return rc;
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetAteRev(UINT32* pVal)
{
    if (m_pTestDevice)
        return m_pTestDevice->GetAteRev(pVal);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
UINT32 JsTestDevice::GetBusType()
{
    if (m_pTestDevice)
        return m_pTestDevice->BusType();
    return 0;
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetChipPrivateRevisionString()
{
    UINT32 privRev = 0;
    if (m_pTestDevice && m_pTestDevice->GetChipPrivateRevision(&privRev) == OK)
    {
        return Utility::StrPrintf("%06x", privRev);
    }
    return "";
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetChipRevisionString()
{
    UINT32 rev = 0;
    if (m_pTestDevice && m_pTestDevice->GetChipRevision(&rev) == OK)
    {
        return Utility::StrPrintf("%02x", rev);
    }
    return "";
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetChipSkuModifier()
{
    string chipSkuMod;
    if (m_pTestDevice && (OK == m_pTestDevice->GetChipSkuModifier(&chipSkuMod)))
        return chipSkuMod;
    return "";
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetChipSkuNumber()
{
    string chipSkuNumber;
    if (m_pTestDevice && (OK == m_pTestDevice->GetChipSkuNumber(&chipSkuNumber)))
        return chipSkuNumber;
    return "";
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetChipTemp(FLOAT32 *pChipTempC)
{
    RC rc;
    vector<FLOAT32> chipTempsC;
    CHECK_RC(GetChipTemps(&chipTempsC));
    *pChipTempC = chipTempsC[0];
    return rc;
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetChipTemps(vector<FLOAT32> *pChipTempsC)
{
    if (m_pTestDevice)
        return m_pTestDevice->GetChipTemps(pChipTempsC);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetClockMHz(UINT32* pClockMhz)
{
    if (m_pTestDevice)
        return m_pTestDevice->GetClockMHz(pClockMhz);
    return RC::SOFTWARE_ERROR;
}


//-----------------------------------------------------------------------------
UINT32 JsTestDevice::GetDeviceId()
{
    if (m_pTestDevice)
        return static_cast<UINT32>(m_pTestDevice->GetDeviceId());
    return static_cast<UINT32>(Device::ILWALID_DEVICE);
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetDeviceIdString()
{
    if (m_pTestDevice)
        return m_pTestDevice->DeviceIdString();
    return "Unknown";
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetDeviceErrorList(vector<TestDevice::DeviceError>* pErrorList)
{
    if (m_pTestDevice)
        return m_pTestDevice->GetDeviceErrorList(pErrorList);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
UINT32 JsTestDevice::GetDeviceTypeInstance()
{
    if (m_pTestDevice)
        return m_pTestDevice->GetDeviceTypeInstance();
    return ~0U;
}

//-----------------------------------------------------------------------------
UINT32 JsTestDevice::GetDevInst()
{
    if (m_pTestDevice)
        return m_pTestDevice->DevInst();
    return ~0U;
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetEcidString(string* pStr)
{
    if (m_pTestDevice)
        return m_pTestDevice->GetEcidString(pStr);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetFoundryString()
{
    TestDevice::ChipFoundry foundry;
    if (m_pTestDevice && (OK == m_pTestDevice->GetFoundry(&foundry)))
    {
        switch (foundry)
        {
            case TestDevice::CF_TSMC: return "TSMC";
            case TestDevice::CF_UMC: return "UMC";
            case TestDevice::CF_IBM: return "IBM";
            case TestDevice::CF_SMIC: return "SMIC";
            case TestDevice::CF_CHARTERED: return "Chartered";
            case TestDevice::CF_TOSHIBA: return "Toshiba";
            case TestDevice::CF_SONY: return "Sony";
            case TestDevice::CF_SAMSUNG: return "Samsung";
            default: return "Unknown";
        }
    }
    return "";
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetName()
{
    if (m_pTestDevice)
        return m_pTestDevice->GetName();
    return "unknown";
}

//------------------------------------------------------------------------------
string JsTestDevice::GetPdiString()
{
    if (m_pTestDevice == nullptr)
        return "";

    UINT64 pdi;
    if (RC::OK != m_pTestDevice->GetPdi(&pdi))
    {
        return "";
    }
    return Utility::StrPrintf("0x%016llx", pdi);
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetRawEcidString()
{
    vector<UINT32> ecidData;
    if (m_pTestDevice && (OK == m_pTestDevice->GetRawEcid(&ecidData)))
    {
        string chipIdStr = "";

        for (UINT32 i = 0; i < ecidData.size(); i++)
        {
            char ecidWord[10];
            sprintf(ecidWord, "%08x", ecidData[i]);
            chipIdStr += ecidWord;
        }
        // Dont print leading zeros to save column space in mods.log
        auto pos = chipIdStr.find_first_not_of('0');
        if (pos != std::string::npos && pos != 0)
        {
            chipIdStr.erase(0, pos); // using pos as count here.
        }

        return chipIdStr.insert(0, "0x0");
    }
    return "";
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetRomVersion(string* pVersion)
{
    if (m_pTestDevice)
        return m_pTestDevice->GetRomVersion(pVersion);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetTestPhase(UINT32* pPhase)
{
    if (m_pTestDevice)
        return m_pTestDevice->GetTestPhase(pPhase);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
UINT32 JsTestDevice::GetType()
{
    if (m_pTestDevice)
        return static_cast<UINT32>(m_pTestDevice->GetType());
    return static_cast<UINT32>(TestDevice::TYPE_UNKNOWN);
}

//-----------------------------------------------------------------------------
string JsTestDevice::GetTypeName()
{
    if (m_pTestDevice)
        return m_pTestDevice->GetTypeName();
    return "Unknown";
}

//-----------------------------------------------------------------------------
RC JsTestDevice::GetVoltageMv(UINT32* pMv)
{
    if (m_pTestDevice)
        return m_pTestDevice->GetVoltageMv(pMv);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
bool JsTestDevice::HasBug(UINT32 bugNum)
{
    if (m_pTestDevice)
        return m_pTestDevice->HasBug(bugNum);
    return false;
}

//-----------------------------------------------------------------------------
bool JsTestDevice::HasFeature(UINT32 feature)
{
    if (m_pTestDevice)
        return m_pTestDevice->HasFeature(static_cast<Device::Feature>(feature));
    return false;
}

//-----------------------------------------------------------------------------
bool JsTestDevice::IsEmulation()
{
    if (m_pTestDevice)
        return m_pTestDevice->IsEmulation();
    return false;
}

//-----------------------------------------------------------------------------
bool JsTestDevice::IsSOC()
{
    if (m_pTestDevice)
        return m_pTestDevice->IsSOC();
    return false;
}

//-----------------------------------------------------------------------------
bool JsTestDevice::IsSysmem()
{
    if (m_pTestDevice)
        return m_pTestDevice->IsSysmem();
    return false;
}

//-----------------------------------------------------------------------------
UINT32 JsTestDevice::RegRd32(UINT32 address)
{
    if (m_pTestDevice)
        return m_pTestDevice->RegRd32(address);
    return 0;
}

//-----------------------------------------------------------------------------
void JsTestDevice::RegWr32(UINT32 address, UINT32 value)
{
    if (m_pTestDevice)
        m_pTestDevice->RegWr32(address, value);
}

//-----------------------------------------------------------------------------
void JsTestDevice::Release()
{
    m_pTestDevice.reset();
}

//-----------------------------------------------------------------------------
RC JsTestDevice::SetLwLinkSystemParameter(UINT64 linkMask, UINT08 setting, UINT08 val)
{
    const LwLink::SystemParameter ss = static_cast<LwLink::SystemParameter>(setting);
    LwLink::SystemParamValue spv;
    switch (ss)
    {
        case LwLink::SystemParameter::RESTORE_PHY_PARAMS:
        case LwLink::SystemParameter::SL_ENABLE:
        case LwLink::SystemParameter::L2_ENABLE:
        case LwLink::SystemParameter::LINK_DISABLE:
        case LwLink::SystemParameter::LINK_INIT_DISABLE:
        case LwLink::SystemParameter::DISABLE_UPHY_UCODE_LOAD:
        case LwLink::SystemParameter::ALI_ENABLE:
        case LwLink::SystemParameter::L1_ENABLE:
            spv.bEnable = static_cast<bool>(val);
            break;
        case LwLink::SystemParameter::LINE_RATE:
            spv.lineRate = static_cast<LwLink::SystemLineRate>(val);
            break;
        case LwLink::SystemParameter::LINE_CODE_MODE:
            spv.lineCode = static_cast<LwLink::SystemLineCode>(val);
            break;
        case LwLink::SystemParameter::TXTRAIN_OPT_ALGO:
            spv.txTrainAlg = static_cast<LwLink::SystemTxTrainAlg>(val);
            break;
        case LwLink::SystemParameter::BLOCK_CODE_MODE:
            spv.blockCodeMode = static_cast<LwLink::SystemBlockCodeMode>(val);
            break;
        case LwLink::SystemParameter::REF_CLOCK_MODE:
            spv.refClockMode = static_cast<LwLink::SystemRefClockMode>(val);
            break;
        case LwLink::SystemParameter::TXTRAIN_MIN_TRAIN_TIME_MANTISSA:
        case LwLink::SystemParameter::TXTRAIN_MIN_TRAIN_TIME_EXPONENT:
        case LwLink::SystemParameter::L1_MIN_RECAL_TIME_MANTISSA:
        case LwLink::SystemParameter::L1_MIN_RECAL_TIME_EXPONENT:
        case LwLink::SystemParameter::L1_MAX_RECAL_PERIOD_MANTISSA:
        case LwLink::SystemParameter::L1_MAX_RECAL_PERIOD_EXPONENT:
            spv.rangeValue = val;
            break;
        case LwLink::SystemParameter::CHANNEL_TYPE:
            spv.channelType = static_cast<LwLink::SystemChannelType>(val);
            break;
        default:
            Printf(Tee::PriError, "Unknown LwLink system setting %u\n", setting);
            return RC::BAD_PARAMETER;
    }

    // Need to call this here rather than in JsLwLink because LwLink isn't initialized
    // when this needs to be called, so LwLink will return IsSupported=false, and therefore
    // the device won't have a JsLwLink interface available. We should also not call
    // SupportsInterface here and merely get the interface directly.
    LwLink* pLwLink = m_pTestDevice->GetInterface<LwLink>();
    if (!pLwLink)
        return RC::UNSUPPORTED_FUNCTION;

    return pLwLink->SetSystemParameter(linkMask, ss, spv);
}

//-----------------------------------------------------------------------------
RC JsTestDevice::PexReset()
{
    if (m_pTestDevice)
        return m_pTestDevice->PexReset();
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
RC JsTestDevice::EndOfLoopCheck(bool captureReference)
{
    if (m_pTestDevice)
        return m_pTestDevice->EndOfLoopCheck(captureReference);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
CLASS_PROP_READONLY(JsTestDevice, AteRev,             UINT32, "Device ATE revision from fuses");                               //$
CLASS_PROP_READONLY(JsTestDevice, BusType,            UINT32, "Bus Type");                                                     //$
CLASS_PROP_READONLY(JsTestDevice, ChipRevisionString,        string, "Device chip revision as a string");                      //$
CLASS_PROP_READONLY(JsTestDevice, ChipPrivateRevisionString, string, "Device chip private revision as a string");              //$
CLASS_PROP_READONLY(JsTestDevice, ChipSkuNumber,             string, "SKU number of the device as a string");                  //$
CLASS_PROP_READONLY(JsTestDevice, ChipSkuModifier,           string, "SKU modifier of the device as a string");                //$
CLASS_PROP_READONLY(JsTestDevice, ChipTemp,           FLOAT32,"Device chip temperature in celcius");                           //$
CLASS_PROP_READONLY(JsTestDevice, ClockMHz,           UINT32, "Device clock in MHz");                                          //$
CLASS_PROP_READONLY(JsTestDevice, DeviceId,           UINT32, "Device ID");                                                    //$
CLASS_PROP_READONLY(JsTestDevice, DeviceIdString,     string, "Device ID string");                                             //$
CLASS_PROP_READONLY(JsTestDevice, DeviceTypeInstance, UINT32, "Instance of this device among other devices of the same type"); //$
CLASS_PROP_READONLY(JsTestDevice, DevInst,            UINT32, "Device instance of this test device");                          //$
CLASS_PROP_READONLY(JsTestDevice, EcidString,         string, "Device chip ID as a string");                                   //$
CLASS_PROP_READONLY(JsTestDevice, FoundryString,      string, "Foundry for the device");                                       //$
CLASS_PROP_READONLY(JsTestDevice, Name,               string, "Name of this test device");                                     //$
CLASS_PROP_READONLY(JsTestDevice, RawEcidString,      string, "Device raw chip ID as a string");                               //$
CLASS_PROP_READONLY(JsTestDevice, RomVersion,         string, "Device BIOS Version string");                                   //$
CLASS_PROP_READONLY(JsTestDevice, TestPhase,          UINT32, "Test phase of the device - Switching, VF Point and PState");    //$
CLASS_PROP_READONLY(JsTestDevice, Type,               UINT32, "Returns the test device type");                                 //$
CLASS_PROP_READONLY(JsTestDevice, TypeName,           string, "Returns the test device type name");                            //$
CLASS_PROP_READONLY(JsTestDevice, VoltageMv,          UINT32, "Device voltage in mv");                                         //$
CLASS_PROP_READONLY(JsTestDevice, PdiString,          string, "lwpu PDI as a string");

JS_SMETHOD_BIND_ONE_ARG(JsTestDevice, HasBug, bugNum, UINT32,
                        "Report whether the test device has the specified bug");
JS_SMETHOD_BIND_ONE_ARG(JsTestDevice, HasFeature, feature, UINT32,
                        "Report whether the test device has the specified feature");
JS_SMETHOD_BIND_NO_ARGS(JsTestDevice, IsEmulation,
                        "Report whether the testdevice is an emulation device");
JS_SMETHOD_BIND_NO_ARGS(JsTestDevice, IsSOC,
                        "Report whether the testdevice is part of SOC");
JS_SMETHOD_BIND_NO_ARGS(JsTestDevice, IsSysmem,
                        "Report whether the testdevice is connected to sysmem");
JS_SMETHOD_BIND_ONE_ARG(JsTestDevice, RegRd32, address, UINT32,
                        "Read 32bit value from register");
JS_SMETHOD_VOID_BIND_NO_ARGS(JsTestDevice, Release,
                             "Release the TestDevicePtr ownership");
JS_STEST_BIND_NO_ARGS(JsTestDevice, PexReset,
                      "Issue a PEX reset");
JS_STEST_BIND_ONE_ARG(JsTestDevice, EndOfLoopCheck, captureReference, bool,
                      "Check for per loop resource leaks");

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsTestDevice,
                GetAteDvddIddq,
                1,
                "Get Dvdd Iddq number from ATE burnt fuse")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.GetAteDvddIddq(RetVal)");
    STEST_PRIVATE(JsTestDevice, pJsTestDevice, "TestDevice");
    STEST_ARG(0, JSObject*, pRetData);

    RC rc;
    UINT32 iddq = 0;
    TestDevicePtr pTestDevice = pJsTestDevice->GetTestDevice();
    C_CHECK_RC(pTestDevice->GetAteDvddIddq(&iddq));
    C_CHECK_RC(pJavaScript->SetProperty(pRetData, "Value", iddq));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsTestDevice,
                GetAteIddq,
                1,
                "Get Iddq number from ATE burnt fuse")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.GetAteIddq(pRetJsObj)");
    STEST_PRIVATE(JsTestDevice, pJsTestDevice, "TestDevice");
    STEST_ARG(0, JSObject*, pRetData);

    RC rc;
    UINT32 iddq    = 0;
    UINT32 version = 0;
    TestDevicePtr pTestDevice = pJsTestDevice->GetTestDevice();
    C_CHECK_RC(pTestDevice->GetAteIddq(&iddq, &version));
    C_CHECK_RC(pJavaScript->SetProperty(pRetData, "Value", iddq));
    C_CHECK_RC(pJavaScript->SetProperty(pRetData, "Version", version));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsTestDevice,
                GetAteSpeedo,
                1,
                "Get speedo number from ATE burnt fuse")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.GetAteSpeedo(ateSpeedoObj)");
    STEST_PRIVATE(JsTestDevice, pJsTestDevice, "TestDevice");
    STEST_ARG(0, JSObject*, pRetData);

    TestDevicePtr pTestDevice = pJsTestDevice->GetTestDevice();
    vector<UINT32> ateSpeedos;
    UINT32 ateSpeedoVersion = 0;
    RC rc;
    C_CHECK_RC(pTestDevice->GetAteSpeedos(&ateSpeedos, &ateSpeedoVersion));
    if (ateSpeedos.empty())
    {
        C_CHECK_RC(RC::UNSUPPORTED_FUNCTION);
    }
    C_CHECK_RC(pJavaScript->SetProperty(pRetData, "Values", ateSpeedos));
    C_CHECK_RC(pJavaScript->SetProperty(pRetData, "Version", ateSpeedoVersion));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsTestDevice,
                GetDeviceErrorList,
                1,
                "Returns the list of general device errors")
{
    STEST_HEADER(1, 1,
                 "Usage:\n"
                 "var errors = new Array;\n"
                 "TestDevice.GetDeviceErrorList(errors);\n");
    STEST_ARG(0, JSObject*, pErrorList);
    STEST_PRIVATE(JsTestDevice, pJsTestDevice, "TestDevice");

    RC rc;

    vector<TestDevice::DeviceError> errorlist;
    C_CHECK_RC(pJsTestDevice->GetDeviceErrorList(&errorlist));

    for (UINT32 i = 0; i < errorlist.size(); i++)
    {
        const auto& error = errorlist[i];
        JSObject* jsError = JS_NewObject(pContext, &DeviceErrorClass, NULL, NULL);
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "typeId",      error.typeId));
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "instance",    error.instance));
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "subinstance", error.subinstance));
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "fatal",       error.bFatal));
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "name",        error.name));
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "jsonTag",     error.jsonTag));

        C_CHECK_RC(pJavaScript->SetElement(pErrorList, i, jsError));
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsTestDevice, RegWr32, 2, "Write 32bit value to register")
{
    STEST_HEADER(2, 2, "Usage: TestDevice.RegWr32(address, value);");
    STEST_PRIVATE(JsTestDevice, pJsTestDevice, "TestDevice");
    STEST_ARG(0, UINT32, address);
    STEST_ARG(1, UINT32, value);
    pJsTestDevice->RegWr32(address, value);
    return JS_TRUE;
}

// Implementation is in js_gpusb.cpp
extern RC ISMInfoFromJSObject(JSObject    * pJsObj, IsmInfo  * pInfo);
extern RC IsmInfoToJsObj
(
    const vector<UINT32>    &speedos         //!< Measured ring cycles
    , const vector<IsmInfo> &info            //!< ISM info
    , PART_TYPES            SpeedoType       //!< Type of speedo
    , JSContext             *pContext        //!< JS context
    , JSObject              *pReturlwals     //!< JS object pointer
);

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsTestDevice,
                ReadBinSpeedometers,
                7,
                "Read the ring-oscillator counts for each BIN ISM.")
{
    STEST_HEADER(1, 7,
        "Usage: TestDevice.ReadBinSpeedometers(return_array "
        "[,OscIdx=-1,Duration=-1,OutDiv=-1,Mode=-1,Chiplet=-1,InstrId=-1])");
    STEST_PRIVATE(JsTestDevice, pJsTestDevice, "TestDevice");

    RC rc;
    IsmInfo ismArgs;
    JSObject* pReturlwals = nullptr;
    UINT32 duration = ~0;

    switch (NumArguments)
    {
        case 7:
            C_CHECK_RC(pJavaScript->FromJsval(pArguments[6], &ismArgs.chainId));
        case 6: // fall through
            C_CHECK_RC(pJavaScript->FromJsval(pArguments[5], &ismArgs.chiplet));
        case 5: // fall through (process mode/Adj value)
            C_CHECK_RC(pJavaScript->FromJsval(pArguments[4], &ismArgs.mode));
        case 4: // fall through
            C_CHECK_RC(pJavaScript->FromJsval(pArguments[3], &ismArgs.outDiv));
        case 3: // fall through
            C_CHECK_RC(pJavaScript->FromJsval(pArguments[2], &duration));
        case 2: // fall through
            C_CHECK_RC(pJavaScript->FromJsval(pArguments[1], &ismArgs.oscIdx));
        case 1: // fall through
            C_CHECK_RC(pJavaScript->FromJsval(pArguments[0], &pReturlwals));
            break;
    }

    Ism* pIsm = pJsTestDevice->GetTestDevice()->GetIsm();
    if (!pIsm)
    {
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    vector<IsmInfo> ismSettings;
    ismSettings.push_back(ismArgs);

    vector<UINT32> speedos;
    speedos.resize(pIsm->GetNumISMs(IsmSpeedo::BIN), 0);

    vector<IsmInfo> info;
    C_CHECK_RC(pIsm->ReadISMSpeedometers(&speedos, &info,
                                         IsmSpeedo::BIN, &ismSettings, duration));
    C_CHECK_RC(IsmInfoToJsObj(speedos, info, IsmSpeedo::BIN, pContext, pReturlwals));

    RETURN_RC(rc);
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
    rc = pJavaScript->FromJsval(pArguments[0], &pReturlwals);
    if (RC::OK != rc)
    {
        pJavaScript->Throw(pContext, rc, 
                           Utility::StrPrintf("Invalid value for pReturlwals\n%s", usage));
        return JS_FALSE;
    }

    JSObject * pJsIsmInfo;
    rc = pJavaScript->FromJsval(pArguments[1], &pJsIsmInfo);
    if (RC::OK != rc)
    {
        pJavaScript->Throw(pContext, rc,
                           Utility::StrPrintf("Invalid value for pJsIsmInfo\n%s", usage));
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
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                           Utility::StrPrintf("%s\nDuration can't be zero!\n", usage));
        RETURN_RC(RC::BAD_PARAMETER);
    }


    JsTestDevice *pJsTestDevice;
    if ((pJsTestDevice = JS_GET_PRIVATE(JsTestDevice, pContext, pObject, "TestDevice")) != 0)
    {
        Ism* pIsm = pJsTestDevice->GetTestDevice()->GetIsm();
        if (!pIsm)
        {
            RETURN_RC(RC::UNSUPPORTED_FUNCTION);
        }

        vector<UINT32> Speedos;
        vector<IsmInfo> Info;
        C_CHECK_RC(pIsm->ReadISMSpeedometers(&Speedos,
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
JS_STEST_LWSTOM(JsTestDevice,
                ReadBinISMs,
                2,
                "Read the ring-oscillator counts for each BIN ISM.")
{
    STEST_HEADER(2, 2,
        "Usage: TestDevice.ReadBinISMs(returnArray, JsIsmInfoObject");

    return ReadISMHelper(pContext, pObject, NumArguments, pArguments, pReturlwalue, usage, BIN);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsTestDevice,
                SetOverTempRange,
                3,
                "Set overtemp range values for a specific sensor")
{
    STEST_HEADER(3, 3, "Usage: TestDevice.SetOverTempRange(sensor, min, max)");
    STEST_PRIVATE(JsTestDevice, pJsTestDevice, "TestDevice");
    STEST_ARG(0, UINT32, sensor);
    STEST_ARG(1, UINT32, min);
    STEST_ARG(2, UINT32, max);

    TestDevicePtr pTestDevice = pJsTestDevice->GetTestDevice();
    OverTempCounter::TempType sensorType = static_cast<OverTempCounter::TempType>(sensor);

    RC rc;
    C_CHECK_RC(pTestDevice->SetOverTempRange(sensorType, min, max));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsTestDevice,
                SetLwLinkSystemParameter,
                3,
                "Set the specified lwlink system parameter")
{
    STEST_HEADER(3, 3, "Usage: GpuSubdevice.SetLwLinkSystemParameter(mask, setting, value)");
    STEST_ARG(0, UINT08, setting);
    STEST_ARG(1, JSObject*, pJsoMask);
    STEST_ARG(2, UINT08, val);
    STEST_PRIVATE(JsTestDevice, pJsTestDevice, "TestDevice");

    JsUINT64* pJsMask = static_cast<JsUINT64*>(GetPrivateAndComplainIfNull(pContext,
                                                                           pJsoMask,
                                                                           "JsUINT64",
                                                                           "UINT64"));
    if (!pJsMask)
    {
        Printf(Tee::PriError, "Link mask is not a JsUINT64!\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    RETURN_RC(pJsTestDevice->SetLwLinkSystemParameter(pJsMask->GetValue(), setting, val));
}

//-----------------------------------------------------------------------------
// TestDevice Constants
//
JS_CLASS(TestDeviceConst);
static SObject TestDeviceConst_Object
(
    "TestDeviceConst",
    TestDeviceConstClass,
    0,
    0,
    "TestDeviceConst JS Object"
);

//-----------------------------------------------------------------------------
PROP_CONST(TestDeviceConst, TYPE_EBRIDGE, TestDevice::TYPE_EBRIDGE);
PROP_CONST(TestDeviceConst, TYPE_IBM_NPU, TestDevice::TYPE_IBM_NPU);
PROP_CONST(TestDeviceConst, TYPE_LWIDIA_GPU, TestDevice::TYPE_LWIDIA_GPU);
PROP_CONST(TestDeviceConst, TYPE_LWIDIA_LWSWITCH, TestDevice::TYPE_LWIDIA_LWSWITCH);
PROP_CONST(TestDeviceConst, TYPE_TEGRA_MFG, TestDevice::TYPE_TEGRA_MFG);
PROP_CONST(TestDeviceConst, TYPE_UNKNOWN, TestDevice::TYPE_UNKNOWN);
