/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/jscript.h"
#include "pcsensor.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/usbtherm.h"

UsbTemperatureSensor::UsbTemperatureSensor() : m_UsbDev(nullptr)
{
    Initialize();
}

UsbTemperatureSensor::~UsbTemperatureSensor()
{
    Close();
}

RC UsbTemperatureSensor::Initialize()
{
    RC rc;
    CHECK_RC(PcSensor::InitializeLibUsb());
    rc = OpenUsbDev();
    if (rc != OK)
    {
        m_UsbDev = nullptr;
    }
    return rc;
}

//! \brief Opens device structure for USB temperature sensor
//! Opens USB temperature sensor and reads from it in order to determine
//! if it was initialized or not. Will retry MAX_OPEN_RETRY_COUNT times.
//! Returns EXTERNAL_DEVICE_NOT_FOUND on failure
RC UsbTemperatureSensor::OpenUsbDev()
{
    float temperature;
    int retryCount;
    /* Sometimes (roughly every 16 opens), the sensor will fail to read the
     * temperature. It returns -273.15 when this happens. reopen sensor if
     * this oclwrs. */
    for (retryCount = 0;
         retryCount < UsbTemperatureSensor::MAX_OPEN_RETRY_COUNT;
         ++retryCount)
    {
        /* If this isn't the first time through the loop (because previous
         * iterations failed) close and reopen dev */
        if (m_UsbDev != nullptr)
        {
            PcSensor::pcsensor_close(m_UsbDev);
        }

        m_UsbDev = PcSensor::pcsensor_open();
        // If we failed to open USB device, try again
        if (m_UsbDev == nullptr)
        {
            continue;
        }
        if (GetTemperature(&temperature) == OK)
        {
            break;
        }
        // Anything other than OK means retry
    }

    if (retryCount >= UsbTemperatureSensor::MAX_OPEN_RETRY_COUNT)
    {
        return RC::EXTERNAL_DEVICE_NOT_FOUND;
    }
    else
    {
        return OK;
    }
}

//! \brief Closes USB temperature sensor connection
RC UsbTemperatureSensor::CloseUsbDev()
{
    PcSensor::pcsensor_close(m_UsbDev);
    m_UsbDev = nullptr;
    return OK;
}

RC UsbTemperatureSensor::Close()
{
    if (m_UsbDev != nullptr)
    {
        return CloseUsbDev();
    }
    return OK;
}

//! \brief Reads temperature from external USB temperature sensor
//! Calls underlying API to get the temperature then uses the class
//! constants in order to compute the actual temperature. Returns
//! THERMAL_SENSOR_ERROR if the underlying API returns absolute zero.
RC UsbTemperatureSensor::GetTemperature(float *temperature)
{
    // A failure to read a temperature results in returning -273.15
    *temperature = PcSensor::pcsensor_get_temperature(m_UsbDev);
    float difference = *temperature - PcSensor::ABS_ZERO;
    if (-0.00001 < difference && difference < 0.00001)
    {
        return RC::THERMAL_SENSOR_ERROR;
    }
    // According to online resources, some calibration offsets help accuracy
    *temperature = (*temperature * UsbTemperatureSensor::TEMPERATURE_SCALE +
                    UsbTemperatureSensor::TEMPERATURE_OFFSET);
    return OK;
}

bool UsbTemperatureSensor::IsOpen()
{
    return (m_UsbDev != nullptr);
}

//! \brief Max number of times we close-reopen a temp sensor before we give up
const int UsbTemperatureSensor::MAX_OPEN_RETRY_COUNT = 6;

/* Calibration coefficients callwlated based off of an internal study (see bug 1521232).
   A linear compensation resulting from this study was y = 1.0448x - 0.3256
   The coefficients are different here because the study was run with these constants
   set to 1.0287 and -0.85 respectively. If these two calibration functions are
   composed (y = 1.0448(1.0287x - 0.85) - 0.3256), you get the constants below. */
const float UsbTemperatureSensor::TEMPERATURE_SCALE = 1.0748;
const float UsbTemperatureSensor::TEMPERATURE_OFFSET = -1.2137;

static void C_UsbTemperatureSensor_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    UsbTemperatureSensor * pUsbTemperatureSensor = \
        (UsbTemperatureSensor *)JS_GetPrivate(cx, obj);

    if (pUsbTemperatureSensor)
    {
        delete pUsbTemperatureSensor;
    }
};

//-----------------------------------------------------------------------------
static JSClass UsbTemperatureSensorClass =
{
    "UsbTemperatureSensor",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_UsbTemperatureSensor_finalize
};

//-----------------------------------------------------------------------------
static JSBool C_JsonItem_constructor
(
    JSContext *cx,
    JSObject *thisObj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    // Store C++ object in the JS object.
    JS_SetPrivate(cx, thisObj, new UsbTemperatureSensor());
    return JS_TRUE;
}

// Making this global
SObject UsbTemperatureSensor_Object
(
    "UsbTemperatureSensor",
    UsbTemperatureSensorClass,
    0,
    0,
    "External Temper1 USB temperature sensor.",
    C_JsonItem_constructor
);

JS_STEST_LWSTOM(UsbTemperatureSensor, Initialize, 0,
                "Initialize USB temperature sensor.")
{
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: UsbTemperatureSensor.Initialize()");
        return JS_FALSE;
    }

    UsbTemperatureSensor *pSensor;
    if ((pSensor = JS_GET_PRIVATE(UsbTemperatureSensor,
                                 pContext, pObject,
                                 "UsbTemperatureSensor")) != 0)
    {
        RETURN_RC(pSensor->Initialize());
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(UsbTemperatureSensor, Close, 0,
                "Close USB temperature sensor.")
{
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: UsbTemperatureSensor.Close()");
        return JS_FALSE;
    }

    UsbTemperatureSensor *pSensor;
    if ((pSensor = JS_GET_PRIVATE(UsbTemperatureSensor,
                                 pContext, pObject,
                                 "UsbTemperatureSensor")) != 0)
    {
        RETURN_RC(pSensor->Close());
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(UsbTemperatureSensor, GetTemperature, 1,
                "Retrieve temperature from USB temperature sensor.")
{
    JavaScriptPtr pJs;
    JSObject *pReturlwals;

    if
    (
        (NumArguments != 1)
        || (pJs->FromJsval(pArguments[0], &pReturlwals))
    )
    {
        JS_ReportError(pContext, "Usage: "
                       "UsbTemperatureSensor.GetTemperature([temperature])");
        return JS_FALSE;
    }

    UsbTemperatureSensor *pSensor;
    if ((pSensor = JS_GET_PRIVATE(UsbTemperatureSensor,
                                 pContext, pObject,
                                 "UsbTemperatureSensor")) != 0)
    {
        RC rc;
        float temperature;
        jsval js_temperature;
        C_CHECK_RC(pSensor->GetTemperature(&temperature));
        pJs->ToJsval(temperature, &js_temperature);
        RETURN_RC(pJs->SetElement(pReturlwals, 0, js_temperature));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(UsbTemperatureSensor, IsOpen, 0,
                "Tells whether a USB device has been detected and opened.")
{
    JavaScriptPtr pJs;
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: IsOpen()");
        return JS_FALSE;
    }

    UsbTemperatureSensor *pSensor;
    if ((pSensor = JS_GET_PRIVATE(UsbTemperatureSensor,
                                 pContext, pObject,
                                 "UsbTemperatureSensor")) != 0)
    {
        pJs->ToJsval(pSensor->IsOpen(), pReturlwalue);
        return JS_TRUE;
    }
    return JS_FALSE;
}

