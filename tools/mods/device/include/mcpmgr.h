/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file mcpmgr.h
//! \brief Header file for MCP device managers.
//!
//! Contains class, constant, and macro definitions for MCP devices and
//! device managers.
//!

#ifndef INCLUDED_MCPMGR_H
#define INCLUDED_MCPMGR_H

#ifndef INCLUDED_RC_H
    #include "core/include/rc.h"
#endif
#ifndef INCLUDED_JSCRIPT_H
    #include "core/include/jscript.h"
#endif
#ifndef INCLUDED_MCPDEV_H
    #include "mcpdev.h"
#endif
#ifndef INCLUDED_MCPMCR_H
    #include "mcpmcr.h"
#endif
#ifndef INCLUDED_SCRIPT_H
    #include "core/include/script.h"
#endif
#ifndef INCLUDED_TEE_H
    #include "core/include/tee.h"
#endif
#ifndef INCLUDED_STL_VECTOR_H
    #include <vector>
#endif
#ifndef INCLUDED_DEPRECAT_H
    #include "core/include/deprecat.h"
#endif

//! \brief A container class for keeping track of mcp devices of a certain sort.
//!
//! This will be subclassed for each type of
//! device, as each device needs to be found/created differently
//! (e.g., by searching for a different class code).
//!
//! The real meat of this class is in the FindDevices function, which not
//! only searches for devices on the bus, but also does whatever
//! device-specific setup is required (this is implemented in the virtual
//! Setup() function. In addition to performing these
//! setup functions, this class keeps track of the "current" device, which is
//! the one refered to in calls from JavaScript. (Note that all devices can
//! actually be used at the same time--the "current" device is merely the
//! one that is default target of API calls.)
class McpDeviceManager
{
public:

    McpDeviceManager();
    virtual ~McpDeviceManager();

    //! \brief Find devices on the bus and create corresponding software objects.
    //!
    //! Does NOT do any initialization of the devices, and does not
    //! touch the device hardware. It will call Setup() to do any
    //! device-specific setup.
    //! \sa PrivateFindDevices(), Setup()
    RC Find(bool bFindMoreDevices = false);

    //! \brief Clears out the state created in FindDevices.
    //!
    //! Makes sure that we have properly disconnected from the hw, then
    //! deletes all the pointers to devices and resets all internal
    //! variables.
    //! \sa FindDevices()
    RC Forget();

    //! \brief user force the page's range.
    //--------------------------------------------------------------------------
    RC ForcePageRange(UINT32 ForceAddrBits);

    //! \brief Gets the number of devices
    //!
    //! \param nDevices On return, will be set to number of devices
    RC GetNumDevices(UINT32 * pNumDev);

    //! \brief Gets the index of the current device.
    //!
    //! Javascript should refer to devices by index number.
    //! \param index On return, will be set to the index of the current device.
    //! \param pDev If non-NULL, on return will point to the current device
    //! \sa SetDeviceIndex(), GetLwrrentDevice(), GetByIndex()
    RC GetLwrrent(UINT32* pIndex);
    //! \brief Gets a pointer to the current device.
    //!
    //! \param pDev On return, will point to the current device.
    //! \sa SetDeviceIndex(), GetLwrrentDevice(), GetByIndex()
    RC GetLwrrent(McpDev** ppDev);

    //! \brief Set the index of the current device.
    //!
    //! \param index The index to set as the current device.
    //! \sa GetLwrrentDeviceIndex(), GetLwrrentDevice(), GetByIndex()
    RC SetLwrrent(UINT32 Index);
    //! \brief Gets a pointer to the device at the specified index.
    //!
    //! \param index The index of interest.
    //! \param pDev On return, will be set to point to the device of interest.
    //! \sa GetLwrrentDeviceIndex(), SetDeviceIndex(), GetLwrrentDevice()
    RC GetByIndex(UINT32 Index, McpDev** pDev);

    //! \brief Colwenience function to initialize the device whose index is passed
    //!
    RC InitializeByDevIndex(UINT32 index);

    //! \brief Colwenience function to initialize current devices
    //!
    //! \sa InitializeLwrrentDevice(), McpDev::Initialize()
    RC Initialize(McpDev::InitStates State = McpDev::INIT_DONE);

    //! \brief Colwenience function to uninitialize all devices known by
    //! this manager.
    //!
    //! \sa InitializeAllDevices(), McpDev::Initialize()
    RC InitializeAll(McpDev::InitStates State = McpDev::INIT_DONE);

    //! \brief Colwenience function to uninitialize the device whose index is passed
    //!
    RC UninitializeByDevIndex(UINT32 index);

    //! \brief Colwenience function to uninitialize current devices known by
    //! this manager.
    //!
    //! \sa Uninitialize(), McpDev::Uninitialize()
    RC Uninitialize(McpDev::InitStates State = McpDev::INIT_CREATED);

    //! \brief Colwenience function to uninitialize all devices known by
    //! this manager.
    //!
    //! \sa UninitializeAllDevices(), McpDev::Uninitialize()
    RC UninitializeAll(McpDev::InitStates State = McpDev::INIT_CREATED);

    //! \brief Colwenience function to print information about lwrent devices
    //! known by this manager.
    //!
    //! \param Pri Priority to print at. Default is normal priority.
    //! \param IsBasic Whether to print basic information (domain/bus/dev/fun,
    //!        interrupt info, etc) about each device.
    //! \param IsCtrl Whether to print controller-specific (register values,
    //!        state, etc) about each device.
    //! \param IsExternal Whether to print information about external
    //!        compontents (PHYs, codecs, etc) connected to each device.
    //! \sa McpDev::PrintInfo
    RC PrintInfo(Tee::Priority Pri = Tee::PriNormal,
                 bool IsPrintBsc = true,
                 bool IsPrintCtrl = false,
                 bool IsPrintExt = false);

    //! \brief Colwenience function to print information about all devices
    //! known by this manager.
    //!
    //! \param Pri Priority to print at. Default is normal priority.
    //! \param IsBasic Whether to print basic information (domain/bus/dev/fun,
    //!        interrupt info, etc) about each device.
    //! \param IsCtrl Whether to print controller-specific (register values,
    //!        state, etc) about each device.
    //! \param IsExternal Whether to print information about external
    //!        compontents (PHYs, codecs, etc) connected to each device.
    //! \sa McpDev::PrintInfo
    RC PrintInfoAll(Tee::Priority Pri = Tee::PriNormal,
                    bool IsPrintBsc = true,
                    bool IsPrintCtrl = false,
                    bool IsPrintExt = false);

    RC Validate(UINT32 Index);

    //! \brief Initialization required for the device on the CheetAh platform
    //
    //! This is restricted ONLY initialization steps that need to be performed
    //! before the device can be found.
    //! This is similar to the BIOS sequence.
    //! All other initialization steps should go in the device Initialization.
    virtual RC TegraPreInit() { return OK; }

protected:

    //! \brief Do device-specific setup.
    //!
    //! This should be restricted to software setup that must be
    //! done by the manager, not the device, for visibility reasons.
    //! Hardware setup and setup that is not contingent on the manager's
    //! knowledge of the entire system should be done in the device's
    //! Initialize function.
    //! \sa McpDev::Initialize()
    virtual RC SetupDevice(UINT32 Index){return OK;}

    //! \brief Device-specific code for finding devices on a bus.
    //!
    //! Every subclass will have its own implementation. This code should find
    //! the devices, allocate and create corresponding software objects,
    //! store the devices in m_vpDevices
    //! \sa FindDevices(), m_vpDevices
    virtual RC PrivateFindDevices() = 0;

    //will be activated in next check in.
    //! \brief Device-specific code for validate devices found Called in PrivateFindDevices.
    //!
    //! Every subclass will have its own implementation. This code should validate
    //! the devices found. If the device found is not valid for mods, return not-valid device
    //! otherwise return OK.
    virtual RC PrivateValidateDevice(UINT32 DomainNum, UINT32 BusNum,
                                     UINT32 DevNum, UINT32 FunNum,
                                     UINT32 * ChipVersion = NULL) = 0;

    //! \brief Stores pointers to known devices.
    //!
    //! It stores all the Device pointer. m_vpDevices.size() would give the
    //! number of devices found.
    vector<McpDev*> m_vpDevices;

    //! \brief The index of the device lwrrently being pointed to.
    UINT32 m_LwrrentIndex;
private:
    UINT32 m_FindCount;
};

//-----------------------------------------------------------------------------
// ISR Macros
//-----------------------------------------------------------------------------

//! \brief Defines a per-device ISR function that calls the class's ISR method.
//!
//! Used in the inheriting manager *namespace* (not class) to define ISR
//! functions of the format IsrNum (e.g., Isr0) for their associated
//! devices. This macro will be used once for each device, up to the
//! maximum number of devices, and defines a simple pass-through
//! method that gets the appropriate device from the manager and calls
//! the specified method (the ISR method) of that device. The ISR method
//! must be careful not to touch any static state in the device class.
#define DEVICE_ISR(DeviceNum, DeviceType, IsrFunc)      \
    long Isr##DeviceNum(void*){                         \
        McpDev* pDev;                                   \
        Mgr()->GetByIndex(DeviceNum, &pDev);            \
        DeviceType *pTypedDevice = (DeviceType *) pDev; \
        return pTypedDevice->IsrFunc();                 \
    };

//! \brief Defines a per-device named ISR function that calls the class's ISR method.
//!
//! Just like DEVICE_ISR, but allows you to name the ISR. Defines functions
//! in the form IsrNameNum (for example, IsrReceived0). Useful when you
//! need to define multiple ISRs for different MSI channels.
#define DEVICE_NAMED_ISR(DeviceNum, DeviceType, IsrName, IsrFunc) \
    long Isr##IsrName##DeviceNum(void*){                          \
        McpDev* pDev;                                             \
        Mgr()->GetByIndex(DeviceNum, &pDev);                      \
        DeviceType *pTypedDevice = (DeviceType *) pDev;           \
        return pTypedDevice->IsrFunc();                           \
    };

//-----------------------------------------------------------------------------
// Javascript Macros
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
//  Tests/Method to Manager
//-----------------------------------------------------------------------------

// Define a simple test with no arguments. Calls one of the functions
// in the manager, but still uses the same JS class (e.g., Device.FindDevices)
#define MCPMGR_TEST_NOARG(JsClass, Test, CommentString)            \
    MCP_JSCLASS_TEST(JsClass, Test, 0, CommentString)              \
    C_(JsClass##_##Test)                                           \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test"()"); \
           return JS_FALSE;                                        \
       }                                                           \
       RETURN_RC(JsClass##Mgr::Mgr()->Test());                     \
    }
// Define a simple method with no arguments. Calls one of the functions
// in the manager, but still uses the same JS class (e.g., Device.FindDevices)
#define MCPMGR_METHOD_NOARG(JsClass, Method, ReturnType, CommentString) \
    MCP_JSCLASS_METHOD(JsClass, Method, 0, CommentString)          \
    C_(JsClass##_##Method)                                         \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       ReturnType data = 0;                                        \
       if (OK != JsClass##Mgr::Mgr()->Method(&data))               \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       if (OK != JavaScriptPtr()->ToJsval(data, pReturlwalue))     \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       return JS_TRUE;                                             \
    }
// Define a simple test with one argument. You can specify the type of
// the argument. Calls one of the functions in the manager, but still
// uses the same JS class (e.g., Device.FindDevices)
#define MCPMGR_TEST_ONEARG(JsClass, Test, ArgName, ArgType, CommentString) \
    MCP_JSCLASS_TEST(JsClass, Test, 1, CommentString)              \
    C_(JsClass##_##Test)                                           \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       if (NumArguments != 1)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test"("#ArgName")");\
           return JS_FALSE;                                        \
       }                                                           \
       ArgType arg = 0;                                            \
       if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &arg))  \
       {                                                           \
           JS_ReportError(pContext, #ArgName" bad value");         \
           return JS_FALSE;                                        \
       }                                                           \
       RETURN_RC(JsClass##Mgr::Mgr()->Test(arg));                  \
    }

//-----------------------------------------------------------------------------
//  Tests/Method on current device
//-----------------------------------------------------------------------------
#define MCPDEV_REPORT_LWRRENT(JsClass)\
       UINT32 devIndex;                                         \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent(&devIndex));  \
       Printf(Tee::PriLow, "Operating on Current Device %d\n", devIndex);

#define MCPSUBDEV_REPORT_LWRRENT(JsClass, SubClass)\
       UINT32 devIndex, subIndex;                               \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent(&devIndex));  \
       C_CHECK_RC(pDev->GetLwrrentSubDev(&subIndex));       \
       Printf(Tee::PriLow, "Operating on Current Device %d"#SubClass" %d\n", devIndex, subIndex);

// Define a simple test with no arguments in a class. Gets the current
// device from the relevant manager
#define MCPDEV_TEST_NOARG(JsClass, Test, CommentString)            \
    MCP_JSCLASS_TEST(JsClass, Test, 0, "Current Device: " CommentString)\
    C_(JsClass##_##Test)                                           \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test"()");\
           return JS_FALSE;                                        \
       }                                                           \
       JsClass##Device* pDev = NULL;                               \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       MCPDEV_REPORT_LWRRENT(JsClass)                              \
       RETURN_RC(pDev->Test());                                    \
    }

// Define a simple test with no arguments in a class. Gets the device(index)
// from the relevant manager
#define MCPDEV_TEST_BY_DEV_INDEX(JsClass, Test, CommentString)     \
    MCP_JSCLASS_TEST(JsClass, Test##_ByDevIndex, 1, CommentString) \
    C_(JsClass##_##Test##_ByDevIndex)                              \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 1)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test"_ByDevIndex(devIndex)");\
           return JS_FALSE;                                        \
       }                                                           \
                                                                   \
       UINT32 index = 0;                                           \
       if (OK != JavaScriptPtr()->FromJsval(pArguments[0],&index)) \
        {                                                          \
            JS_ReportError(pContext, "device index: bad value");   \
            return JS_FALSE;                                       \
        }                                                          \
                                                                   \
       JsClass##Device* pDev = NULL;                               \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetByIndex(index, (McpDev**)&pDev)); \
       RETURN_RC(pDev->Test());                                    \
    }

#define MCPDEV_ALL_TEST_NOARG(JsClass, Test, CommentString)            \
    MCP_JSCLASS_TEST(JsClass, Test##All, 0, "All Devices: " CommentString)\
    C_(JsClass##_##Test##All)                                           \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test"()");\
           return JS_FALSE;                                        \
       }                                                           \
       JsClass##Device* pDev = NULL;                               \
       UINT32 totalDev;                                            \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetNumDevices(&totalDev));  \
       for(UINT32 i = 0; i < totalDev; ++i)                        \
       {                                                           \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetByIndex(i,(McpDev**)&pDev));\
            Printf(Tee::PriNormal,"Controller %02d\n",i);          \
            C_CHECK_RC(pDev->Test());                              \
       }                                                           \
       RETURN_RC(OK);                                              \
    }

// Define a simple method with no arguments. Gets the device(index)
// from the relevant manager.
#define MCPDEV_METHOD_BY_DEV_INDEX(JsClass, Method, ReturnType, CommentString) \
    MCP_JSCLASS_METHOD(JsClass, Method##_ByDevIndex, 1, CommentString)\
    C_(JsClass##_##Method##_ByDevIndex)                            \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 1)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Method"_ByDevIndex(devIndex)"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
                                                                   \
       UINT32 index = 0;                                           \
       if (OK != JavaScriptPtr()->FromJsval(pArguments[0],&index)) \
        {                                                          \
            JS_ReportError(pContext, "device index: bad value");   \
            return JS_FALSE;                                       \
        }                                                          \
                                                                   \
       ReturnType data = 0;                                        \
       JsClass##Device* pDev  = NULL;                              \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetByIndex(index, (McpDev**)&pDev)); \
       if (OK != pDev->Method(&data))                              \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"_ByDevIndex(devIndex)"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       if (OK != JavaScriptPtr()->ToJsval(data, pReturlwalue))     \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"_ByDevIndex(devIndex)"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       return JS_TRUE;                                             \
    }

// Define a simple method with no arguments. Gets the current device
// from the relevant manager.
#define MCPDEV_METHOD_NOARG(JsClass, Method, ReturnType, CommentString) \
    MCP_JSCLASS_METHOD(JsClass, Method, 0, "Current Device: " CommentString)\
    C_(JsClass##_##Method)                                         \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       ReturnType data = 0;                                        \
       JsClass##Device* pDev  = NULL;                              \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       MCPDEV_REPORT_LWRRENT(JsClass)                              \
       if (OK != pDev->Method(&data))                              \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       if (OK != JavaScriptPtr()->ToJsval(data, pReturlwalue))     \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       return JS_TRUE;                                             \
    }

#define MCPDEV_VIOD_METHOD_NOARG(JsClass, Method, CommentString)   \
    MCP_JSCLASS_METHOD(JsClass, Method, 0, "Current Device: " CommentString)\
    C_(JsClass##_##Method)                                         \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       JsClass##Device* pDev  = NULL;                              \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       MCPDEV_REPORT_LWRRENT(JsClass)                              \
       pDev->Method();                                             \
       return JS_TRUE;                                             \
    }

// Define a simple test with one argument. You can specify the type of
// the argument. Gets the current device from the relevant manager.
#define MCPDEV_TEST_ONEARG(JsClass, Test, ArgName, ArgType, CommentString) \
    MCP_JSCLASS_TEST(JsClass, Test, 1, "Current Device: " CommentString)    \
    C_(JsClass##_##Test)                                           \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 1)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test"("#ArgName")"); \
           return JS_FALSE;                                        \
       }                                                           \
       ArgType arg = 0;                                            \
       if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &arg))  \
       {                                                           \
           JS_ReportError(pContext, #ArgName" bad value");         \
           return JS_FALSE;                                        \
       }                                                           \
       JsClass##Device* pDev = NULL;                               \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       MCPDEV_REPORT_LWRRENT(JsClass)                              \
       RETURN_RC(pDev->Test(arg));                                 \
    }

// Define a simple method with one argument. You can specify the type of
// the argument. Gets the current device from the relevant manager.
#define MCPDEV_METHOD_ONEARG(JsClass, Method, ArgName, ArgType, ReturnType, CommentString) \
    MCP_JSCLASS_METHOD(JsClass, Method, 1, "Current Device: " CommentString)          \
    C_(JsClass##_##Method)                                         \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 1)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Method"("#ArgName")"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       ArgType arg = 0;                                            \
       if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &arg))  \
       {                                                           \
           JS_ReportError(pContext, #ArgName" bad value");         \
           return JS_FALSE;                                        \
       }                                                           \
       ReturnType data = 0;                                        \
       JsClass##Device* pDev = NULL;                               \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       MCPDEV_REPORT_LWRRENT(JsClass)                              \
       if (OK != pDev->Method(arg, &data))                         \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       if (OK != JavaScriptPtr()->ToJsval(data, pReturlwalue))     \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       return JS_TRUE;                                             \
    }

//-----------------------------------------------------------------------------
//  Tests/Method on current subdevice
//-----------------------------------------------------------------------------
// Dfine a simple test with no arguments in a class. Gets the current
// subdevice from the current device of relevant manager
#define MCPSUBDEV_TEST_NOARG(JsClass, SubClass, Test, CommentString)\
    MCP_JSCLASS_TEST(JsClass, Test, 0, "Current "#SubClass": " CommentString)\
    C_(JsClass##_##Test)                                           \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test"()");\
           return JS_FALSE;                                        \
       }                                                           \
       JsClass##Device* pDev = NULL;                               \
       JsClass##SubClass * pSubDev = NULL;                         \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       C_CHECK_RC(pDev->GetLwrrentSubDev((SmbPort**)&pSubDev)); \
       MCPSUBDEV_REPORT_LWRRENT(JsClass, SubClass)                 \
       RETURN_RC(pSubDev->Test());                                 \
    }

#define MCPSUBDEV_TEST_ONEARG(JsClass, SubClass, Test, ArgName, ArgType, CommentString) \
    MCP_JSCLASS_TEST(JsClass, Test, 1, "Current "#SubClass": " CommentString)\
    C_(JsClass##_##Test)                                           \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 1)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test"("#ArgName")"); \
           return JS_FALSE;                                        \
       }                                                           \
       ArgType arg = 0;                                            \
       if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &arg))  \
       {                                                           \
           JS_ReportError(pContext, #ArgName" bad value");         \
           return JS_FALSE;                                        \
       }                                                           \
       JsClass##Device* pDev = NULL;                               \
       JsClass##SubClass * pSubDev = NULL;                         \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       C_CHECK_RC(pDev->GetLwrrentSubDev((SmbPort**)&pSubDev)); \
       MCPSUBDEV_REPORT_LWRRENT(JsClass, SubClass)                 \
       RETURN_RC(pSubDev->Test(arg));                              \
    }

// Define a simple method with no arguments. Gets the current subdevice
// from the relevant manager.
#define MCPSUBDEV_METHOD_NOARG(JsClass, SubClass, Method, ReturnType, CommentString) \
    MCP_JSCLASS_METHOD(JsClass, Method, 0, "Current "#SubClass": " CommentString)\
    C_(JsClass##_##Method)                                         \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       ReturnType data = 0;                                        \
       JsClass##Device* pDev  = NULL;                              \
       JsClass##SubClass* pSubDev = NULL;                          \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       C_CHECK_RC(pDev->GetLwrrentSubDev((SmbPort**)&pSubDev)); \
                                                                   \
       MCPSUBDEV_REPORT_LWRRENT(JsClass, SubClass)                 \
       if (OK != pSubDev->Method(&data))                           \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       if (OK != JavaScriptPtr()->ToJsval(data, pReturlwalue))     \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       return JS_TRUE;                                             \
    }

// Define a simple test with one argument. Function name will become
// Test<<SubClass>>. You can specify the type of the argument. Gets the
// current device from the relevant manager.
#define MCPDEV_TEST_SUBDEV_ONEARG(JsClass, SubClass, Test, ArgName, ArgType, CommentString) \
    MCP_JSCLASS_TEST(JsClass, Test##SubClass, 1, "Current Device: " CommentString)    \
    C_(JsClass##_##Test##SubClass)                                 \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 1)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Test#SubClass"("#ArgName")"); \
           return JS_FALSE;                                        \
       }                                                           \
       ArgType arg = 0;                                            \
       if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &arg))  \
       {                                                           \
           JS_ReportError(pContext, #ArgName" bad value");         \
           return JS_FALSE;                                        \
       }                                                           \
       JsClass##Device* pDev = NULL;                               \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       MCPDEV_REPORT_LWRRENT(JsClass)                              \
       RETURN_RC(pDev->Test##SubDev(arg));                         \
    }

// Define a simple method with no arguments. Function name will become
// Method<<SubClass>>. Gets the current device from the relevant manager.
#define MCPDEV_METHOD_SUBDEV_NOARG(JsClass, SubClass, Method, ReturnType, CommentString) \
    MCP_JSCLASS_METHOD(JsClass, Method##SubClass, 0, "Current Device: " CommentString)\
    C_(JsClass##_##Method##SubClass)                               \
    {                                                              \
       MASSERT(pContext     != 0);                                 \
       MASSERT(pArguments   != 0);                                 \
       MASSERT(pReturlwalue != 0);                                 \
       RC rc;                                                      \
       if (NumArguments != 0)                                      \
       {                                                           \
           JS_ReportError(pContext, "Usage: "#JsClass"."#Method#SubClass"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       ReturnType data = 0;                                        \
       JsClass##Device* pDev  = NULL;                              \
       C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent((McpDev**)&pDev)); \
       MCPDEV_REPORT_LWRRENT(JsClass)                              \
       if (OK != pDev->Method##SubDev(&data))                      \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method#SubClass"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       if (OK != JavaScriptPtr()->ToJsval(data, pReturlwalue))     \
       {                                                           \
           JS_ReportError(pContext, "Error in "#JsClass"."#Method#SubClass"()"); \
           *pReturlwalue = JSVAL_NULL;                             \
           return JS_FALSE;                                        \
       }                                                           \
       return JS_TRUE;                                             \
    }

//! \brief Defines a standard interface for subdevice.
//!
//! One simple macro line expands to expose a standardized sub device API

#define MCP_SUBDEV_EXPOSE_TO_JS(JsClass, SubClass)                              \
    MCPDEV_TEST_SUBDEV_ONEARG(JsClass, SubClass, SetLwrrent, SubClass##Number,  \
        UINT32, "Set current "#SubClass" to X")                                 \
    MCPDEV_METHOD_SUBDEV_NOARG(JsClass, SubClass, GetLwrrent,                   \
        UINT32, "Get current "#SubClass)                                        \
    MCPDEV_METHOD_SUBDEV_NOARG(JsClass, SubClass##s, GetNum,                    \
        UINT32, "Get number of "#SubClass"s")

//! \brief Defines a javascript device and a standard interface for that device.
//!
//! One simple macro line expands to expose a device to javascript and
//! give it a standardized API!
#define MCPDEV_EXPOSE_TO_JS(JsClass, ClassComment)                            \
    MCP_JS_OBJECT(JsClass, ClassComment)                                      \
    MCP_JSCLASS_TEST(JsClass, Find, 1,                                        \
        "Find ALL devices of this kind on bus")                               \
    C_(JsClass##_##Find)                                                      \
    {                                                                         \
       MASSERT(pContext     != 0);                                            \
       MASSERT(pArguments   != 0);                                            \
       MASSERT(pReturlwalue != 0);                                            \
       if (NumArguments > 1)                                                  \
       {                                                                      \
           JS_ReportError(pContext,                                           \
                      "Usage: "#JsClass".Find(bFindMoreDevices = false)");    \
           return JS_FALSE;                                                   \
       }                                                                      \
       bool bFindMoreDevices = false;                                         \
       if ((NumArguments == 1) &&                                             \
           (OK != JavaScriptPtr()->FromJsval(pArguments[0],                   \
                                             &bFindMoreDevices)))             \
       {                                                                      \
           JS_ReportError(pContext, "bFindMoreDevices: bad value");           \
           return JS_FALSE;                                                   \
       }                                                                      \
       RETURN_RC(JsClass##Mgr::Mgr()->Find(bFindMoreDevices));                \
    }                                                                         \
                                                                              \
    MCPMGR_TEST_NOARG(JsClass, Forget,                                        \
        "Forget ALL devices of this kind on bus")                             \
    MCPMGR_TEST_ONEARG(JsClass,ForcePageRange, ForceAddrBits, UINT32,         \
        "Force mods to allocate required page")                               \
    MCPMGR_TEST_NOARG(JsClass, TegraPreInit,                                  \
        "Current Device: PreInitialize the current device")                   \
    MCPMGR_METHOD_NOARG(JsClass,GetNumDevices, UINT32,                        \
        "Get the total Number of this kind of devices lwrrently in system")   \
    MCPMGR_TEST_ONEARG(JsClass, SetLwrrent, "index", UINT32,                  \
        "Selects the current device to which commands are sent")              \
    MCPMGR_METHOD_NOARG(JsClass, GetLwrrent, UINT32,                          \
        "Gets index of the lwrrently selected device")                        \
    MCPDEV_METHOD_NOARG(JsClass,GetAddrBits, UINT32,                          \
        "Current Device: Dma Address bits")                                   \
    MCPDEV_TEST_ONEARG(JsClass,OverrideCap,ChipVersion,UINT32,                \
        "Current Device: Override chip version")                              \
    MCPDEV_TEST_NOARG(JsClass, Search,                                        \
        "Current Device: Search external connectivity's of device/subdevice") \
    MCPDEV_ALL_TEST_NOARG(JsClass, Search,                                    \
        "Search external connectivity's of all devices/subdevices")           \
    MCPDEV_TEST_NOARG(JsClass, LinkExtDev,                                    \
        "Current Device: Establish link to external devices")                 \
    MCPDEV_TEST_BY_DEV_INDEX(JsClass, LinkExtDev,                             \
        "For device index passed: Establish link to external devices")        \
    MCPDEV_TEST_NOARG(JsClass, PrintReg,                                      \
        "Current Device: Print controller's registers")                       \
    MCPDEV_TEST_NOARG(JsClass, PrintCapTable,                                 \
        "Print capabilities of the device for all chipsets")                  \
    MCPDEV_TEST_NOARG(JsClass, PrintCap,                                      \
        "Current Device: Print capabilities of the current device")           \
    MCP_JSCLASS_METHOD(JsClass,GetCap, 1,                                     \
        "Current Device: Read the cap, return the value")                     \
        C_(JsClass##_##GetCap)                                                \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            RC rc;                                                            \
            if (NumArguments != 1)                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".GetCap()");       \
                return JS_FALSE;                                              \
            }                                                                 \
            UINT32 capValue = 0 ;                                             \
            UINT32 capIndex = 0;                                              \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &capIndex))   \
            {                                                                 \
                JS_ReportError(pContext, "capIndex bad value");               \
                return JS_FALSE;                                              \
            }                                                                 \
            McpDev* pDev = NULL;                                              \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent(&pDev));               \
            if (OK != pDev->GetCap(capIndex,&capValue))                       \
            {                                                                 \
                 JS_ReportError(pContext, "Error in "#JsClass".GetCap()");    \
                 *pReturlwalue = JSVAL_NULL;                                  \
                 return JS_FALSE;                                             \
            }                                                                 \
            if (OK != JavaScriptPtr()->ToJsval(capValue, pReturlwalue))       \
            {                                                                 \
                JS_ReportError(pContext, "Error in "#JsClass".GetCap()");     \
                *pReturlwalue = JSVAL_NULL;                                   \
                return JS_FALSE;                                              \
            }                                                                 \
            return JS_TRUE;                                                   \
        }                                                                     \
    MCP_JSCLASS_METHOD(JsClass,GetDomainBusDevFunc_ByDevIndex, 1,             \
        "For device index passed: Get the domain, bus, device, function number")\
        C_(JsClass##_##GetDomainBusDevFunc_ByDevIndex)                        \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            RC rc;                                                            \
            if (NumArguments != 1)                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".GetDomainBusDevFunc_ByDevIndex(index)");\
                return JS_FALSE;                                              \
            }                                                                 \
                                                                              \
            UINT32 index = 0;                                                 \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &index))      \
            {                                                                 \
                JS_ReportError(pContext, "device index: bad value");          \
                return JS_FALSE;                                              \
            }                                                                 \
                                                                              \
            UINT32 domain = 0;                                                \
            UINT32 bus = 0;                                                   \
            UINT32 dev = 0;                                                   \
            UINT32 func = 0;                                                  \
            McpDev* pDev = NULL;                                              \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetByIndex(index, &pDev));        \
            if (OK != pDev->GetDomainBusDevFunc(&domain, &bus, &dev, &func))  \
            {                                                                 \
                 JS_ReportError(pContext, "Error in "#JsClass".GetDomainBusDevFunc()");\
                 *pReturlwalue = JSVAL_NULL;                                  \
                 return JS_FALSE;                                             \
            }                                                                 \
            JSObject* retVal = 0;                                             \
            JavaScriptPtr pJavaScript;                                        \
            C_CHECK_RC(pJavaScript->SetProperty(retVal, "domain", domain));   \
            C_CHECK_RC(pJavaScript->SetProperty(retVal, "bus",    bus));      \
            C_CHECK_RC(pJavaScript->SetProperty(retVal, "device", dev));      \
            C_CHECK_RC(pJavaScript->SetProperty(retVal, "func",   func));     \
            if (OK != pJavaScript->ToJsval(retVal, pReturlwalue))             \
            {                                                                 \
                JS_ReportError(pContext, "Error in "#JsClass".GetDomainBusDevFunc()");\
                *pReturlwalue = JSVAL_NULL;                                   \
                return JS_FALSE;                                              \
            }                                                                 \
            return JS_TRUE;                                                   \
        }                                                                     \
    MCP_JSCLASS_METHOD(JsClass,GetBusDevFunc_ByDevIndex, 1,                   \
        "For device index passed: Get the bus, device, function number")      \
        C_(JsClass##_##GetBusDevFunc_ByDevIndex)                              \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            static Deprecation depr(#JsClass".GetBusDevFunc", "6/1/2015",     \
                                    #JsClass".GetBusDevFunc has been replaced by "\
                                    #JsClass".GetDomainBusDevFunc.\n"         \
                                    #JsClass".GetDomainBusDevFunc returns an object with"\
                                    " data members: domain, bus, device, func.\n");\
            if (!depr.IsAllowed(pContext))                                    \
                return JS_FALSE;                                              \
            RC rc;                                                            \
            if (NumArguments != 1)                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".GetBusDevFunc_ByDevIndex(index)");\
                return JS_FALSE;                                              \
            }                                                                 \
                                                                              \
            UINT32 index = 0;                                                 \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &index))      \
            {                                                                 \
                JS_ReportError(pContext, "device index: bad value");          \
                return JS_FALSE;                                              \
            }                                                                 \
                                                                              \
            UINT32 domain = 0;                                                \
            UINT32 bus = 0;                                                   \
            UINT32 dev = 0;                                                   \
            UINT32 func = 0;                                                  \
            McpDev* pDev = NULL;                                              \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetByIndex(index, &pDev));        \
            if (OK != pDev->GetDomainBusDevFunc(&domain, &bus, &dev, &func))  \
            {                                                                 \
                 JS_ReportError(pContext, "Error in "#JsClass".GetBusDevFunc()");\
                 *pReturlwalue = JSVAL_NULL;                                  \
                 return JS_FALSE;                                             \
            }                                                                 \
            JsArray retVal(3, jsval(0));                                      \
            JavaScriptPtr pJavaScript;                                        \
            pJavaScript->ToJsval(bus, &retVal[0]);                            \
            pJavaScript->ToJsval(dev, &retVal[1]);                            \
            pJavaScript->ToJsval(func, &retVal[2]);                           \
            if (OK != pJavaScript->ToJsval(&retVal, pReturlwalue))            \
            {                                                                 \
                JS_ReportError(pContext, "Error in "#JsClass".GetBusDevFunc()");\
                *pReturlwalue = JSVAL_NULL;                                   \
                return JS_FALSE;                                              \
            }                                                                 \
            return JS_TRUE;                                                   \
        }                                                                     \
    MCP_JSCLASS_METHOD(JsClass,GetDomainBusDevFunc, 0,                        \
        "Current Device: Get the domain, bus, device, function number")       \
        C_(JsClass##_##GetDomainBusDevFunc)                                   \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            RC rc;                                                            \
            if (NumArguments != 0)                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".GetDomainBusDevFunc()");\
                return JS_FALSE;                                              \
            }                                                                 \
            UINT32 domain = 0;                                                \
            UINT32 bus = 0;                                                   \
            UINT32 dev = 0;                                                   \
            UINT32 func = 0;                                                  \
            McpDev* pDev = NULL;                                              \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent(&pDev));               \
            if (OK != pDev->GetDomainBusDevFunc(&domain, &bus, &dev, &func))  \
            {                                                                 \
                 JS_ReportError(pContext, "Error in "#JsClass".GetDomainBusDevFunc()");\
                 *pReturlwalue = JSVAL_NULL;                                  \
                 return JS_FALSE;                                             \
            }                                                                 \
            JSObject* retVal = 0;                                             \
            JavaScriptPtr pJavaScript;                                        \
            C_CHECK_RC(pJavaScript->SetProperty(retVal, "domain", domain));   \
            C_CHECK_RC(pJavaScript->SetProperty(retVal, "bus",    bus));      \
            C_CHECK_RC(pJavaScript->SetProperty(retVal, "device", dev));      \
            C_CHECK_RC(pJavaScript->SetProperty(retVal, "func",   func));     \
            if (OK != pJavaScript->ToJsval(retVal, pReturlwalue))             \
            {                                                                 \
                JS_ReportError(pContext, "Error in "#JsClass".GetDomainBusDevFunc()");\
                *pReturlwalue = JSVAL_NULL;                                   \
                return JS_FALSE;                                              \
            }                                                                 \
            return JS_TRUE;                                                   \
        }                                                                     \
    MCP_JSCLASS_METHOD(JsClass,GetBusDevFunc, 0,                              \
        "Current Device: Get the bus, device, function number")               \
        C_(JsClass##_##GetBusDevFunc)                                         \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            static Deprecation depr(#JsClass".GetBusDevFunc", "6/1/2015",     \
                                    #JsClass".GetBusDevFunc has been replaced by "\
                                    #JsClass".GetDomainBusDevFunc.\n"         \
                                    #JsClass".GetDomainBusDevFunc returns an object with"\
                                    " data members: domain, bus, device, func.\n");\
            if (!depr.IsAllowed(pContext))                                    \
                return JS_FALSE;                                              \
            RC rc;                                                            \
            if (NumArguments != 0)                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".GetBusDevFunc()");\
                return JS_FALSE;                                              \
            }                                                                 \
            UINT32 domain = 0;                                                \
            UINT32 bus = 0;                                                   \
            UINT32 dev = 0;                                                   \
            UINT32 func = 0;                                                  \
            McpDev* pDev = NULL;                                              \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent(&pDev));               \
            if (OK != pDev->GetDomainBusDevFunc(&domain, &bus, &dev, &func))  \
            {                                                                 \
                 JS_ReportError(pContext, "Error in "#JsClass".GetBusDevFunc()");\
                 *pReturlwalue = JSVAL_NULL;                                  \
                 return JS_FALSE;                                             \
            }                                                                 \
            JsArray retVal(3, jsval(0));                                      \
            JavaScriptPtr pJavaScript;                                        \
            pJavaScript->ToJsval(bus, &retVal[0]);                            \
            pJavaScript->ToJsval(dev, &retVal[1]);                            \
            pJavaScript->ToJsval(func, &retVal[2]);                           \
            if (OK != pJavaScript->ToJsval(&retVal, pReturlwalue))            \
            {                                                                 \
                JS_ReportError(pContext, "Error in "#JsClass".GetBusDevFunc()");\
                *pReturlwalue = JSVAL_NULL;                                   \
                return JS_FALSE;                                              \
            }                                                                 \
            return JS_TRUE;                                                   \
        }                                                                     \
    MCP_JSCLASS_METHOD(JsClass, RegRd, 2,                                     \
        "Current Device: Reads a register, return the value")                 \
        C_(JsClass##_##RegRd)                                                 \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            RC rc;                                                            \
            if (NumArguments != 1 && NumArguments != 2)                       \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".RegRd(offset)");  \
                return JS_FALSE;                                              \
            }                                                                 \
            UINT32 data = 0;                                                  \
            UINT32 offset = 0;                                                \
            bool isDebug = false;                                             \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &offset))     \
            {                                                                 \
                JS_ReportError(pContext, "Offset bad value");                 \
                return JS_FALSE;                                              \
            }                                                                 \
            if (NumArguments == 2)                                            \
            {                                                                 \
                if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &isDebug))\
                {                                                             \
                    JS_ReportError(pContext, "IsDebug bad value");            \
                    return JS_FALSE;                                          \
                }                                                             \
            }                                                                 \
            McpDev* pDev = NULL;                                              \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent(&pDev));               \
            if (OK != pDev->RegRd(offset, &data, isDebug))                    \
            {                                                                 \
                JS_ReportError(pContext, "Error in "#JsClass".RegRg()");      \
                *pReturlwalue = JSVAL_NULL;                                   \
                return JS_FALSE;                                              \
            }                                                                 \
            if (OK != JavaScriptPtr()->ToJsval(data, pReturlwalue))           \
            {                                                                 \
                JS_ReportError(pContext, "Error in "#JsClass".RegRd()");      \
                *pReturlwalue = JSVAL_NULL;                                   \
                return JS_FALSE;                                              \
            }                                                                 \
            return JS_TRUE;                                                   \
        }                                                                     \
    MCP_JSCLASS_TEST(JsClass, RegWr, 3,                                       \
        "Current Device: Writes a register")                                  \
        C_(JsClass##_##RegWr)                                                 \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            RC rc;                                                            \
            if (NumArguments != 2 && NumArguments != 3)                       \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".RegWr(offset, data)"); \
                return JS_FALSE;                                              \
            }                                                                 \
            UINT32 data = 0;                                                  \
            UINT32 offset = 0;                                                \
            bool isDebug = false;                                             \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &offset))     \
            {                                                                 \
                JS_ReportError(pContext, "Offset bad value");                 \
                return JS_FALSE;                                              \
            }                                                                 \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &data))       \
            {                                                                 \
                JS_ReportError(pContext, "Data bad value");                   \
                return JS_FALSE;                                              \
            }                                                                 \
            if (NumArguments == 3)                                            \
            {                                                                 \
                if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &isDebug)) \
                {                                                             \
                    JS_ReportError(pContext, "IsDebug bad value");            \
                    return JS_FALSE;                                          \
                }                                                             \
            }                                                                 \
            McpDev* pDev = NULL;                                              \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent(&pDev));               \
            RETURN_RC(pDev->RegWr(offset, data, isDebug));                    \
        }                                                                     \
    MCPDEV_METHOD_ONEARG(JsClass, PciRd32, "offset", UINT32, UINT32,          \
        "Reads a 32-bit PCI config register, return the value")               \
    MCP_JSCLASS_TEST(JsClass, PciWr32, 2,                                     \
        "Current Device: Writes a 32-bit PCI config register")                \
        C_(JsClass##_##PciWr32)                                               \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            RC rc;                                                            \
            if (NumArguments != 2)                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".PciWr32(offset, data)"); \
                return JS_FALSE;                                              \
            }                                                                 \
            UINT32 data = 0;                                                  \
            UINT32 offset = 0;                                                \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &offset))     \
            {                                                                 \
                JS_ReportError(pContext, "Offset bad value");                 \
                return JS_FALSE;                                              \
            }                                                                 \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &data))       \
            {                                                                 \
                JS_ReportError(pContext, "Data bad value");                   \
                return JS_FALSE;                                              \
            }                                                                 \
            McpDev* pDev = NULL;                                              \
            C_CHECK_RC(JsClass##Mgr::Mgr()->GetLwrrent(&pDev));               \
            RETURN_RC(pDev->PciWr32(offset, data));                           \
        }                                                                     \
    MCPDEV_TEST_NOARG(JsClass, DumpPci,                                       \
        "Dump the content of the PCI config space")                           \
    MCP_JSCLASS_TEST(JsClass, PrintInfo, 3,                                   \
        "Current Device: Prints information: Basic, Controller, External-Device")\
        C_(JsClass##_##PrintInfo)                                             \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            if (NumArguments != 3)                       \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".PrintInfo(IsBasic, IsController, IsExternal)"); \
                return JS_FALSE;                                              \
            }                                                                 \
            bool printBasic = true;                                           \
            bool printController = true;                                      \
            bool printExternal = true;                                        \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &printBasic) \
                || OK != JavaScriptPtr()->FromJsval(pArguments[1], &printController) \
                || OK != JavaScriptPtr()->FromJsval(pArguments[2], &printExternal)) \
            {                                                             \
                JS_ReportError(pContext, "Bad parameter");                \
                return JS_FALSE;                                          \
            }                                                             \
            RETURN_RC(JsClass##Mgr::Mgr()->PrintInfo(Tee::PriNormal,         \
                      printBasic, printController, printExternal));           \
        }                                                                     \
        MCP_JSCLASS_TEST(JsClass, PrintInfoAll, 3,                            \
        "All devices: Prints information: Basic, Controller, External-Device")\
        C_(JsClass##_##PrintInfoAll)                                          \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            if (NumArguments != 3)                       \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".PrintInfoAll(IsBasic, IsController, IsExternal)"); \
                return JS_FALSE;                                              \
            }                                                                 \
            bool printBasic = true;                                           \
            bool printCtrl = true;                                            \
            bool printExternal = true;                                        \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &printBasic)) \
            {                                                             \
                JS_ReportError(pContext, "PrintBasic bad value");         \
                return JS_FALSE;                                          \
            }                                                             \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &printCtrl)) \
            {                                                             \
                JS_ReportError(pContext, "PrintCtrl bad value");          \
                return JS_FALSE;                                          \
            }                                                             \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &printExternal)) \
            {                                                             \
                JS_ReportError(pContext, "PrintExternal bad value");      \
                return JS_FALSE;                                          \
            }                                                             \
            RETURN_RC(JsClass##Mgr::Mgr()->PrintInfoAll(Tee::PriNormal,       \
            printBasic, printCtrl, printExternal));                           \
        }                                                                     \
        MCP_JSCLASS_TEST(JsClass, Init_ByDevIndex, 1,                         \
        "For device index passed: Initialize")                                \
        C_(JsClass##_##Init_ByDevIndex)                                       \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            if (NumArguments != 1)                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".Init_ByDevIndex()");  \
                return JS_FALSE;                                              \
            }                                                                 \
                                                                              \
            UINT32 index = 0;                                                 \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &index))      \
            {                                                                 \
                JS_ReportError(pContext, "device index: bad value");          \
                return JS_FALSE;                                              \
            }                                                                 \
                                                                              \
            RETURN_RC(JsClass##Mgr::Mgr()->InitializeByDevIndex(index));         \
        }                                                                     \
        MCP_JSCLASS_TEST(JsClass, Init, 0,                                    \
        "Current Device: Initialize")                                         \
        C_(JsClass##_##Init)                                                  \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            if (NumArguments > 1)                                             \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".Init() or .Init(State)");\
                return JS_FALSE;                                              \
            }                                                                 \
            McpDev::InitStates state = McpDev::INIT_DONE;                     \
            if (NumArguments == 1 && OK != JavaScriptPtr()->FromJsval(pArguments[0], (UINT32*)&state)) \
            {                                                                 \
                JS_ReportError(pContext, "State bad value");                  \
                return JS_FALSE;                                              \
            }                                                                 \
            RETURN_RC(JsClass##Mgr::Mgr()->Initialize(state));                \
        }                                                                     \
        MCP_JSCLASS_TEST(JsClass, InitAll, 0,                                 \
        "All Devices: Initialize to 'done state")                             \
        C_(JsClass##_##InitAll)                                               \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            if (NumArguments > 1)                                             \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".InitAll() or InitAll(State)"); \
                return JS_FALSE;                                              \
            }                                                                 \
            McpDev::InitStates state = McpDev::INIT_DONE;                     \
            if (NumArguments == 1 && OK != JavaScriptPtr()->FromJsval(pArguments[0], (UINT32*)&state)) \
            {                                                                 \
                JS_ReportError(pContext, "State bad value");                  \
                return JS_FALSE;                                              \
            }                                                                 \
            RETURN_RC(JsClass##Mgr::Mgr()->InitializeAll(state));             \
        }                                                                     \
        MCP_JSCLASS_TEST(JsClass, Uninit_ByDevIndex, 1,                       \
        "For the device index passed: Uninitialize")                          \
        C_(JsClass##_##Uninit_ByDevIndex)                                     \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            if (NumArguments != 1)                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".Uninit_ByDevIndex(index)");       \
                return JS_FALSE;                                              \
            }                                                                 \
                                                                              \
            UINT32 index = 0;                                                 \
            if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &index))      \
            {                                                                 \
                JS_ReportError(pContext, "device index: bad value");          \
                return JS_FALSE;                                              \
            }                                                                 \
            RETURN_RC(JsClass##Mgr::Mgr()->UninitializeByDevIndex(index));    \
        }                                                                     \
        MCP_JSCLASS_TEST(JsClass, Uninit, 0,                                  \
        "Current Device: Uninitialize")                                       \
        C_(JsClass##_##Uninit)                                                \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            if (NumArguments > 1 )                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".Uninit() or .Unit(State)"); \
                return JS_FALSE;                                              \
            }                                                                 \
            McpDev::InitStates state = McpDev::INIT_CREATED;                  \
            if (NumArguments == 1 && OK != JavaScriptPtr()->FromJsval(pArguments[0], (UINT32*)&state)) \
            {                                                                 \
                JS_ReportError(pContext, "State bad value");                  \
                return JS_FALSE;                                              \
            }                                                                 \
            RETURN_RC(JsClass##Mgr::Mgr()->Uninitialize(state));              \
        }                                                                     \
        MCP_JSCLASS_TEST(JsClass, UninitAll, 0,                               \
        "All Devices: Uninitialize all devices to 'created state'")           \
        C_(JsClass##_##UninitAll)                                             \
        {                                                                     \
            MASSERT(pContext     != 0);                                       \
            MASSERT(pArguments   != 0);                                       \
            MASSERT(pReturlwalue != 0);                                       \
            if (NumArguments > 1 )                                            \
            {                                                                 \
                JS_ReportError(pContext, "Usage: "#JsClass".UninitAll() or .UninitAll(State)"); \
                return JS_FALSE;                                              \
            }                                                                 \
            McpDev::InitStates state = McpDev::INIT_CREATED;                  \
            if (NumArguments == 1 && OK != JavaScriptPtr()->FromJsval(pArguments[0], (UINT32*)&state)) \
            {                                                                 \
                JS_ReportError(pContext, "State bad value");                  \
                return JS_FALSE;                                              \
            }                                                                 \
            RETURN_RC(JsClass##Mgr::Mgr()->UninitializeAll(state));           \
        }

#endif // INCLUDED_MCPMGR_H

