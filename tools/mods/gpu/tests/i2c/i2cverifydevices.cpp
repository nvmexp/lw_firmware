/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
/**
* @file   I2cVerifyDevices.cpp
* @brief  Implementation to test EEPROM devices over I2C
*
*/

#include "core/include/mgrmgr.h"
#include "core/include/tasker.h"
#include "device/interface/i2c.h"
#include "gpu/include/testdevice.h"
#include "gpu/tests/gputest.h"
#include<set>

class I2cVerifyDevices : public GpuTest
{
    public:
        I2cVerifyDevices();
        ~I2cVerifyDevices();

        RC InitFromJs() override;
        bool IsSupported() override;
        RC Setup() override;
        RC Run() override;

        enum Mode
        {
             MODE_READ  = 1
            ,MODE_WRITE = 2
        };

        SETGET_PROP(DoEepromTest, bool);
        SETGET_PROP(ModeMask,     UINT32);
        SETGET_PROP(WritePattern, UINT32);
        SETGET_PROP(AddrOffset,   UINT32);
        UINT32  GetFlavor() const { return static_cast<UINT32>(m_Flavor); }
        RC SetFlavor(UINT32 val) { m_Flavor = static_cast<I2c::I2cFlavor>(val); return RC::OK; }

    private:
        bool           m_DoEepromTest  = false;
        UINT32         m_ModeMask      = 0x3;
        UINT32         m_WritePattern  = 0x12;
        UINT32         m_AddrOffset    = 0x0;
        I2c::I2cFlavor m_Flavor        = I2c::FLAVOR_HW;

        struct I2CDeviceData
        {
            I2CDeviceData() = default;
            I2CDeviceData(UINT32 p, UINT32 a, UINT32 s) : Port(p), Addr(a), SpeedKhz(s) {}
            UINT32 Port = 0;
            UINT32 Addr = 0;
            UINT32 SpeedKhz = 100;
            bool operator<(const I2CDeviceData &rhs) const
            {
                if (Port < rhs.Port)
                    return true;
                if (Addr < rhs.Addr)
                    return true;
                return SpeedKhz < rhs.SpeedKhz;
            }
        };
        set<I2CDeviceData> m_DevicesToVerify;

        // These values are hard-coded
        // TODO: Look these up in a table somewhere
        // There is lwrrently no way to programmatically determine
        // the board/sku of a particular LwSwitch device, plus there is
        // only the one board type, so it's a moot point
        // to store the information in table right now.
              UINT32 NUM_PORTS   = 4;
        const UINT32 EEPROM_ADDR = 0xA0;

        RC DoEepromTest();
        RC TestLwSwitch();
        RC VerifyDevicesPresent();
};

I2cVerifyDevices::I2cVerifyDevices()
{
    SetName("I2cVerifyDevices");
}

I2cVerifyDevices::~I2cVerifyDevices()
{
}

RC I2cVerifyDevices::InitFromJs()
{
    RC rc;
    JavaScriptPtr pJs;
    jsval jsvDeviceData;
    rc = pJs->GetPropertyJsval(GetJSObject(), "DeviceList", &jsvDeviceData);
    if (RC::OK == rc)
    {
        if (JSVAL_IS_OBJECT(jsvDeviceData))
        {
            JsArray jsaDeviceData;
            CHECK_RC(pJs->FromJsval(jsvDeviceData, &jsaDeviceData));

            m_DevicesToVerify.clear();
            for (size_t i = 0; i < jsaDeviceData.size(); ++i)
            {
                JSObject *pDeviceObj;
                I2CDeviceData devData;
                CHECK_RC(pJs->FromJsval(jsaDeviceData[i], &pDeviceObj));

                CHECK_RC(pJs->GetProperty(pDeviceObj, "Port",  &devData.Port));
                CHECK_RC(pJs->GetProperty(pDeviceObj, "Addr",  &devData.Addr));
                CHECK_RC(pJs->GetProperty(pDeviceObj, "SpeedKhz", &devData.SpeedKhz));
                if ((devData.SpeedKhz != 100) && (devData.SpeedKhz != 400))
                {
                    Printf(Tee::PriError,
                           "%s : Unknown I2C speed %ukhz", GetName().c_str(), devData.SpeedKhz);
                    return RC::ILWALID_ARGUMENT;
                }
                m_DevicesToVerify.insert(devData);
            }
        }
        else
        {
            Printf(Tee::PriError,
                   "%s : Invalid JS specification of DeviceList", GetName().c_str());
            return RC::ILWALID_ARGUMENT;
        }
    }
    else if (m_DoEepromTest)
    {
        // Eeprom will add its own devices to verify later
        rc.Clear();
    }
    else
    {
        Printf(Tee::PriError,
               "%s : No devices specified to test", GetName().c_str());
        return RC::NO_TESTS_RUN;
    }
    return rc;
}

bool I2cVerifyDevices::IsSupported()
{
    return (GetBoundTestDevice()->GetType() == TestDevice::TYPE_LWIDIA_LWSWITCH
            && GetBoundTestDevice()->SupportsInterface<I2c>());
}

RC I2cVerifyDevices::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    if (m_WritePattern > _UINT08_MAX)
    {
        Printf(Tee::PriError, "WritePattern can be at most 1 byte large.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
    if (GetBoundTestDevice()->GetDeviceId() == Device::LR10)
    {
        NUM_PORTS = 3;
    }
#endif

    if (m_DoEepromTest)
    {
        for (UINT32 port = 0; port < NUM_PORTS; port++)
        {
            m_DevicesToVerify.emplace(port, EEPROM_ADDR, 100);
        }
    }
    if (m_DevicesToVerify.empty())
    {
        Printf(Tee::PriError, "No devices found to test!\n");
        return RC::NO_TESTS_RUN;
       
    }
    return OK;
}

RC I2cVerifyDevices::Run()
{
    RC rc;

    return TestLwSwitch();
}

RC I2cVerifyDevices::TestLwSwitch()
{
    RC rc;

    CHECK_RC(VerifyDevicesPresent());
    if (m_DoEepromTest)
    {
        CHECK_RC(DoEepromTest());
    }

    return RC::OK;
}

RC I2cVerifyDevices::DoEepromTest()
{
    RC rc;
    auto pI2c = GetBoundTestDevice()->GetInterface<I2c>();

    for (UINT32 port = 0; port < NUM_PORTS; port++)
    {
        vector<I2c::I2cSpeed> speeds = {I2c::SPEED_100KHz,
                                        I2c::SPEED_400KHz};
        for (auto speed : speeds)
        {
            I2c::Dev i2cDev = pI2c->I2cDev(port, EEPROM_ADDR);
            i2cDev.SetSpeedInKHz(speed);
            i2cDev.SetFlavor(m_Flavor);
            i2cDev.SetOffsetLength(2);
            i2cDev.SetMsbFirst(true);
            i2cDev.SetMessageLength(2);

            UINT32 origValue = 0;
            if (m_ModeMask & MODE_READ)
            {
                Printf(Tee::PriLow, "Reading from port=%d, addr=0x%x, offset=0x%x, speed=%dKHz\n",
                       port, EEPROM_ADDR, m_AddrOffset, speed);
                CHECK_RC(i2cDev.Read(m_AddrOffset, &origValue));
            }

            if (m_ModeMask & MODE_WRITE)
            {
                Printf(Tee::PriLow, "Writing 0x%x to port=%d, addr=0x%x, offset=0x%x, speed=%dKHz\n",
                       m_WritePattern, port, EEPROM_ADDR, m_AddrOffset, speed);
                CHECK_RC(i2cDev.Write(m_AddrOffset, m_WritePattern));
            }

            if ((m_ModeMask & MODE_READ) && (m_ModeMask & MODE_WRITE))
            {
                UINT32 postValue = ~m_WritePattern;
                CHECK_RC(i2cDev.Read(m_AddrOffset, &postValue));

                if (postValue != m_WritePattern)
                {
                    Printf(Tee::PriError, "Wrote 0x%x but read back 0x%x!\n",
                                          m_WritePattern, postValue);
                    return RC::DATA_MISMATCH;
                }

                CHECK_RC(i2cDev.Write(m_AddrOffset, origValue));
            }
        }
    }
    return rc;
}

RC I2cVerifyDevices::VerifyDevicesPresent()
{
    StickyRC rc;

    for (const auto & dev : m_DevicesToVerify)
    {
        I2c::Dev i2cDev = GetBoundTestDevice()->GetInterface<I2c>()->I2cDev(dev.Port, dev.Addr);
        i2cDev.SetSpeedInKHz((dev.SpeedKhz == 100) ? I2c::SPEED_100KHz : I2c::SPEED_400KHz);
        if (OK != i2cDev.Ping())
        {
            Printf(Tee::PriError, "Cannot ping device 0x%x on port 0x%x @ %ukhz!\n",
                   dev.Addr, dev.Port, dev.SpeedKhz);
            rc = RC::DEVICE_NOT_FOUND;
        }
    }

    return rc;
}

JS_CLASS_INHERIT(I2cVerifyDevices, GpuTest, "I2cVerifyDevices test");

CLASS_PROP_READWRITE(I2cVerifyDevices, DoEepromTest, bool,
                     "Set to true to run the EEPRRM read/write test");
CLASS_PROP_READWRITE(I2cVerifyDevices, ModeMask, UINT32,
                     "Modes to run. 0x1=Read, 0x2=Write, 0x3=Read&Write");
CLASS_PROP_READWRITE(I2cVerifyDevices, WritePattern, UINT32,
                     "The value to write into the EEPROMs");
CLASS_PROP_READWRITE(I2cVerifyDevices, AddrOffset, UINT32,
                     "The address offset to read from / write to in the EEPROMs");
CLASS_PROP_READWRITE(I2cVerifyDevices, Flavor, UINT32,
                     "The I2C Flavor to use. 0 = HW, 1 = SW.");
