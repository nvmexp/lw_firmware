/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
/**
* @file   i2cdcbsanitytest.cpp
* @brief  Implementation to test dcb i2c device table.
*
*/
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/i2c.h"
#include "gpu/js_gpusb.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "vbiosdcb4x.h"

// DCBTest Class
class I2cDcbSanityTest : public GpuTest
{
    public:
        // Constructor
        I2cDcbSanityTest();
        //Test DCB I2C Device Table
        RC I2cDcbDeviceTableTest(unsigned long Dev, GpuSubdevice *subdev,
                                    I2c::I2cDcbDevInfo I2cDcbDevInfo);
        // Virtual methods from ModsTest
        RC Run();
        bool IsSupported();
        RC InitFromJs() override;
        SETGET_PROP(UseI2cReads, bool);
        SETGET_PROP(PrintI2cDevices, bool);
        SETGET_PROP(SkipPortMask, UINT32);
        SETGET_PROP(UseScratchBitsWAR, bool);

    private:
        bool m_UseI2cReads = false;
        bool m_PrintI2cDevices = false;
        UINT32 m_SkipPortMask = 0;
        bool m_UseScratchBitsWAR = false;
        UINT32 m_ScratchRegVal = 0;
        map<UINT08, bool> m_I2cPortProtected = {};
        struct SkipDevice
        {
            UINT32 port = 0;
            UINT32 address = 0;
        };
        struct I2CSkipInfo
        {
            UINT32 skipI2COnBOM;
            vector<SkipDevice> skipDevices;
        };
        vector<I2CSkipInfo> m_I2CSkipDevices;
        // Finds and records all I2C ports that have priv security enabled
        void RecordProtectedPorts(const I2c::I2cDcbDevInfo& dcbInfo);

        RC VerifyI2cDevFromScratchBit
        (
            const I2c::I2cDcbDevInfoEntry& dcbEntry
        );
};

bool I2cDcbSanityTest::IsSupported()
{
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();
    if (pGpuSubdev->IsSOC())
    {
        return false;
    }

    if (!pGpuSubdev->SupportsInterface<I2c>())
    {
        return false;
    }

    if (pGpuSubdev->IsEmOrSim())
    {
        JavaScriptPtr pJs;
        bool sanityMode = false;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_SanityMode", &sanityMode);
        if (!sanityMode)
        {
            Printf(Tee::PriLow, "Skipping I2C DCB Sanity Test because"
                    "it is not supported on emulation/simulation\n");
            return false;
        }
    }

    if (!pGpuSubdev->HasFeature(Device::GPUSUB_HAS_RMCTRL_I2C_DEVINFO))
    {
        return false;
    }
    if (pGpuSubdev->HasBug(1385287))
    {
        Printf(Tee::PriLow,
                "Skipping I2C DCB Sanity Test due to Bug 1385287\n");
        return false;
    }

    GpuSubdevice * pSubdev = GetBoundGpuSubdevice();
    I2c::I2cDcbDevInfo I2cDcbDevInfo;
    if (pSubdev->GetInterface<I2c>()->GetI2cDcbDevInfo(&I2cDcbDevInfo) != RC::OK)
    {
        Printf(Tee::PriLow,
            "Skipping I2cDcbSanityTest because we could not retrieve the list of I2C devices\n");
        return false;
    }
    if (I2cDcbDevInfo.empty())
    {
        Printf(Tee::PriLow,
            "Skipping I2cDcbSanityTest because there are no I2C devices to test\n");
        return false;
    }

    return true;
}

I2cDcbSanityTest::I2cDcbSanityTest()
{
    SetName("I2cDcbSanityTest");
}

JS_CLASS_INHERIT(I2cDcbSanityTest, GpuTest,
        "I2CDCBSANITY test");

CLASS_PROP_READWRITE(I2cDcbSanityTest, UseI2cReads, bool,
                     "Use i2c reads to detect devices on the i2c bus (default: use pings)");
CLASS_PROP_READWRITE(I2cDcbSanityTest, PrintI2cDevices, bool,
                     "Print all the tested I2C devices");
CLASS_PROP_READWRITE(I2cDcbSanityTest, SkipPortMask, UINT32,
                     "Specifies a mask of I2C ports for which this test should be skipped");
CLASS_PROP_READWRITE(I2cDcbSanityTest, UseScratchBitsWAR, bool,
                     "Read a scratch register to verify the I2C devices are present");

RC I2cDcbSanityTest::Run()
{
    RC rc;

    GpuSubdevice * pSubdev = GetBoundGpuSubdevice();
    I2c::I2cDcbDevInfo I2cDcbDevInfo;
    CHECK_RC(pSubdev->GetInterface<I2c>()->GetI2cDcbDevInfo(&I2cDcbDevInfo));

    RecordProtectedPorts(I2cDcbDevInfo);

    bool atLeastOneDevTested = false;
    DEFER
    {
        if (!atLeastOneDevTested)
        {
            Printf(Tee::PriWarn, "No I2C devices were tested\n");
        }
    };

    if (m_UseScratchBitsWAR)
    {
        if (pSubdev->GetBoardNumber() != "G189")
        {
            Printf(Tee::PriError, "UseScratchBitsWAR is only supported on PG189 boards\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        m_ScratchRegVal = pSubdev->Regs().Read32(MODS_PBUS_SW_SCRATCH, 3);
        VerbosePrintf("LW_PBUS_SW_SCRATCH_3=0x%08x\n", m_ScratchRegVal);
    }

    UINT32 boardBOM = ~0U;
    if (!m_I2CSkipDevices.empty())
    {
        CHECK_RC_MSG(pSubdev->GetBoardBOM(&boardBOM),
            "Cannot find the board BOM due to priv security\n");
        VerbosePrintf("Detected board BOM %u\n", boardBOM);
    }
    for (UINT32 dev = 0; dev < I2cDcbDevInfo.size(); ++dev)
    {
        if (I2cDcbDevInfo[dev].Type == LW_DCB4X_I2C_DEVICE_TYPE_SKIP_ENTRY)
        {
            continue;
        }

        auto i2cInfoSkipIt = std::find_if(
                                             m_I2CSkipDevices.begin(),
                                             m_I2CSkipDevices.end(),
                                             [&](const I2CSkipInfo& i2c)
                                             {
                                                return boardBOM == i2c.skipI2COnBOM;
                                             }
                                         );
        if (i2cInfoSkipIt != m_I2CSkipDevices.end())
        {
            auto skipDeviceIt =  std::find_if(
                                i2cInfoSkipIt->skipDevices.begin(),
                                i2cInfoSkipIt->skipDevices.end(),
                                    [&](const SkipDevice& sd)
                                    {
                                        return sd.port == I2cDcbDevInfo[dev].I2CLogicalPort && 
                                        sd.address == I2cDcbDevInfo[dev].I2CAddress; 
                                    });

            if (skipDeviceIt != i2cInfoSkipIt->skipDevices.end())
            {
                VerbosePrintf("Skipping I2cDcbSanityTest for %s at address 0x%02x on port %u\n",
                    I2c::DevTypeToStr(I2cDcbDevInfo[dev].Type),
                    I2cDcbDevInfo[dev].I2CAddress,
                    I2cDcbDevInfo[dev].I2CLogicalPort);
                continue;
            }
        }
        if ((1 << I2cDcbDevInfo[dev].I2CLogicalPort) & m_SkipPortMask)
        {
            VerbosePrintf("Skipping I2cDcbSanityTest for %s at address 0x%02x on port %u\n",
                I2c::DevTypeToStr(I2cDcbDevInfo[dev].Type),
                I2cDcbDevInfo[dev].I2CAddress,
                I2cDcbDevInfo[dev].I2CLogicalPort);
            continue;
        }
        atLeastOneDevTested = true;
        Printf(m_PrintI2cDevices ? Tee::PriNormal : GetVerbosePrintPri(),
            "%-23s (type=0x%02x), i2cAddress: 0x%02x, i2cLogicalPort: %u\n",
            I2c::DevTypeToStr(I2cDcbDevInfo[dev].Type),
            I2cDcbDevInfo[dev].Type,
            I2cDcbDevInfo[dev].I2CAddress,
            I2cDcbDevInfo[dev].I2CLogicalPort);
        if (m_UseScratchBitsWAR && I2cDcbDevInfo[dev].I2CLogicalPort == 2)
        {
            CHECK_RC(VerifyI2cDevFromScratchBit(I2cDcbDevInfo[dev]));
            continue;
        }
        CHECK_RC(I2cDcbDeviceTableTest(dev, pSubdev, I2cDcbDevInfo));
    }

    return rc;
}

RC I2cDcbSanityTest::I2cDcbDeviceTableTest
(
    unsigned long Dev,
    GpuSubdevice *subdev,
    I2c::I2cDcbDevInfo I2cDcbDevInfo
)
{
    MASSERT(subdev != 0);
    RC rc;

    UINT32 readData;

    I2c::Dev i2cDev = subdev->GetInterface<I2c>()->I2cDev(I2cDcbDevInfo[Dev].I2CLogicalPort,
                                                          I2cDcbDevInfo[Dev].I2CAddress);

    if (m_UseI2cReads || m_I2cPortProtected[I2cDcbDevInfo[Dev].I2CLogicalPort])
    {
        rc = i2cDev.Read(0, &readData);
    }
    else
    {
        rc = i2cDev.Ping();
    }

    if (rc != OK)
    {
        Printf(Tee::PriError,
            "%s I2C device (type=0x%02x) not found on I2C Port 0x%02x at I2C Address 0x%02x\n",
            I2c::DevTypeToStr(I2cDcbDevInfo[Dev].Type),
            I2cDcbDevInfo[Dev].Type,
            I2cDcbDevInfo[Dev].I2CLogicalPort,
            I2cDcbDevInfo[Dev].I2CAddress);
        return RC::I2C_DEVICE_NOT_FOUND; // WAR bug 2586472
    }

    return rc;
}

void I2cDcbSanityTest::RecordProtectedPorts(const I2c::I2cDcbDevInfo& dcbInfo)
{
    auto& regs = GetBoundGpuSubdevice()->Regs();

    set<UINT08> i2cPorts;
    for (const auto& i2cEntry : dcbInfo)
    {
        i2cPorts.insert(i2cEntry.I2CLogicalPort);
    }

    for (const auto& i2cPort : i2cPorts)
    {
        if (!regs.IsSupported(MODS_PMGR_I2Cx_PRIV_LEVEL_MASK, i2cPort) ||
            !regs.IsSupported(MODS_PMGR_I2Cx_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED) ||
            !regs.IsSupported(MODS_PMGR_I2Cx_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED_FUSE0))
        {
            continue;
        }

        VerbosePrintf("I2C port %u PLM: 0x%08x\n",
            i2cPort, regs.Read32(MODS_PMGR_I2Cx_PRIV_LEVEL_MASK, i2cPort));

        if (!regs.Test32(MODS_PMGR_I2Cx_PRIV_LEVEL_MASK_READ_PROTECTION,
                         i2cPort,
                         regs.LookupValue(MODS_PMGR_I2Cx_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED)) || //$
            !regs.Test32(MODS_PMGR_I2Cx_PRIV_LEVEL_MASK_WRITE_PROTECTION,
                         i2cPort,
                         regs.LookupValue(MODS_PMGR_I2Cx_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED_FUSE0))) //$
        {
            VerbosePrintf("Using I2C reads for port %u due to priv security\n", i2cPort);
            m_I2cPortProtected[i2cPort] = true;
        }
    }
}

// This function is a giant, hard-coded hack. See http://lwbugs/2615046/60
// Ideally, we need VBIOS to add a field to each I2C DCB entry to let us know
// which bit in the scratch register to check.
RC I2cDcbSanityTest::VerifyI2cDevFromScratchBit
(
    const I2c::I2cDcbDevInfoEntry& dcbEntry
)
{
    UINT32 maskToCheck = 0;

    if (dcbEntry.I2CAddress == 0x98 &&
        dcbEntry.Type == LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_TMP451)
    {
        maskToCheck = (1U << 8);
    }
    else if (dcbEntry.I2CAddress == 0x92 &&
             dcbEntry.Type == LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_TMP451)
    {
        maskToCheck = (1U << 9);
    }
    else if (dcbEntry.I2CAddress == 0x80 &&
             dcbEntry.Type == LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_INA3221)
    {
        maskToCheck = (1U << 10);
    }
    else if (dcbEntry.I2CAddress == 0x88 &&
             dcbEntry.Type == LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_INA226)
    {
        maskToCheck = (1U << 11);
    }
    else
    {
        Printf(Tee::PriError, "Cannot verify %s at address 0x%02x using ScratchBitsWAR\n",
            I2c::DevTypeToStr(dcbEntry.Type),
            dcbEntry.I2CAddress);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    MASSERT(maskToCheck);

    if (!(maskToCheck & m_ScratchRegVal))
    {
        Printf(Tee::PriError, "VBIOS devinit did not detect %s at address 0x%02x\n",
            I2c::DevTypeToStr(dcbEntry.Type),
            dcbEntry.I2CAddress);
        return RC::I2C_DEVICE_NOT_FOUND;
    }

    return RC::OK;
}

RC I2cDcbSanityTest::InitFromJs()
{
    RC rc;
    CHECK_RC(GpuTest::InitFromJs());

    JavaScriptPtr pJs;
    JsArray jsArrayI2CInfo;
    if (pJs->GetProperty(GetJSObject(), "I2CSkipInfo", &jsArrayI2CInfo) != RC::OK)
        return rc;

    for (JsArray::const_iterator it = jsArrayI2CInfo.begin(); jsArrayI2CInfo.end() != it; ++it)
    {
        JSObject *pjsObject;
        CHECK_RC(pJs->FromJsval(*it, &pjsObject));
        JsArray jsArrayskip;
        if (pJs->GetProperty(pjsObject, "SkipDevices", &jsArrayskip) != RC::OK)
            return rc;

        I2CSkipInfo i2cSkipInfo;
        CHECK_RC(pJs->GetProperty(pjsObject, "SkipI2COnBOM", &i2cSkipInfo.skipI2COnBOM));

        for (JsArray::const_iterator it = jsArrayskip.begin(); jsArrayskip.end() != it; ++it)
        {
            vector<UINT32> deviceList;
            CHECK_RC(pJs->FromJsval(*it, &deviceList));
            if (deviceList.size() != 2)
            {
                Printf(Tee::PriError, 
                "Skip Device has to be an multi-dimensional array, where the inner array is <port,address> \n");
                return RC::ILWALID_ARGUMENT;
            }
            SkipDevice skipDevice;
            skipDevice.port = deviceList[0];
            skipDevice.address = deviceList[1];
            i2cSkipInfo.skipDevices.push_back(skipDevice);
        }
        m_I2CSkipDevices.push_back(i2cSkipInfo);
    }
    return rc;
}
