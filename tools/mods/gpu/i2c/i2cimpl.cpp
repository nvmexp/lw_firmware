/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "i2cimpl.h"
#include "ctrl/ctrl402c.h"
#include "gpu/include/testdevice.h"
#include "vbiosdcb4x.h"

/* static */ const UINT32 I2c::NUM_PORTS = LW402C_CTRL_NUM_I2C_PORTS;
/* static */ const UINT32 I2c::DYNAMIC_PORT = LW402C_CTRL_DYNAMIC_PORT;
/* static */ const UINT08 I2c::CHIL_WRITE_CMD = 0xd0;
/* static */ const UINT08 I2c::CHIL_READ_CMD = 0xd1;
/* static */ const UINT32 I2c::NUM_DEV = LW402C_CTRL_I2C_MAX_DEVICES;

//--------------------------------------------------------------------
//! \brief Returns the name of an I2C device based on its type
//!        in accordance with the VBIOS DCB spec.
/* static */ const char* I2c::DevTypeToStr(UINT32 devType)
{
    switch (devType)
    {
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_W83L785R:
            return "THERMAL_W83L785R";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_ADM1032:
            return "THERMAL_ADM1032";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_MAX6649:
            return "THERMAL_MAX6649";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_LM99:
            return "THERMAL_LM99";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_MAX1617:
            return "THERMAL_MAX1617";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_LM64:
            return "THERMAL_LM64";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_ADT7473:
            return "THERMAL_ADT7473";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_LM89:
            return "THERMAL_LM89";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_TMP411:
            return "THERMAL_TMP411";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_ADT7461:
            return "THERMAL_ADT7461";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_TMP451:
            return "THERMAL_TMP451";
        case LW_DCB4X_I2C_DEVICE_TYPE_THERMAL_TMP461:
            return "THERMAL_TMP461";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_ADS1112:
            return "POWER_SENSOR_ADS1112";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_NCP81276:
            return "POWER_CTRL_NCP81276";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_VOLTERRA1103:
            return "POWER_CTRL_VOLTERRA1103";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_PX3540:
            return "POWER_CTRL_PX3540";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_VT1165:
            return "POWER_CTRL_VT1165";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_CHIL_8203_8212_8213_8214:
            return "POWER_CTRL_CHIL_8203";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_NCP4208:
            return "POWER_CTRL_NCP4208";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_MP2888:
            return "POWER_CTRL_MP2888";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_NCP81610:
            return "POWER_CTRL_NCP81610";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_UP9512:
            return "POWER_CTRL_UP9512";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_RT8848A:
            return "POWER_CTRL_RT8848A";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_CHIL_8112_8225_8228:
            return "POWER_CTRL_CHIL_8112";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_CHIL_8266_8316:
            return "POWER_CTRL_CHIL_8266";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_DS4424N:
            return "POWER_CTRL_DS4424N";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_CONTRL_NCT3933U:
            return "POWER_CTRL_NCT3933U";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_INA219:
            return "POWER_SENSOR_INA219";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_INA209:
            return "POWER_SENSOR_INA209";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_INA3221:
            return "POWER_SENSOR_INA3221";
        case LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_INA226:
            return "POWER_SENSOR_INA226";
        case LW_DCB4X_I2C_DEVICE_TYPE_EXT_CLK_GEN_CY2XP304:
            return "EXT_CLK_GEN_CY2XP304";
        case LW_DCB4X_I2C_DEVICE_TYPE_HDTV_EIAJ4120_PCA9555:
            return "HDTV_EIAJ4120_PCA9555";
        case LW_DCB4X_I2C_DEVICE_TYPE_TCA6408ARGTR:
            return "TCA6408ARGTR";
        case LW_DCB4X_I2C_DEVICE_TYPE_TCA6424ARGJR:
            return "TCA6424ARGJR";
        case LW_DCB4X_I2C_DEVICE_TYPE_FAN_ADT7473:
            return "FAN_ADT7473";
        case LW_DCB4X_I2C_DEVICE_TYPE_HDMI_SI1930UC:
            return "HDMI_SI1930UC";
        case LW_DCB4X_I2C_DEVICE_TYPE_PCA9536:
            return "PCA9536";
        case LW_DCB4X_I2C_DEVICE_TYPE_HDCP_EEPROM_GENERIC:
            return "HDCP_EEPROM_GENERIC";
        case LW_DCB4X_I2C_DEVICE_TYPE_STM32F051K8U6TR:
            return "STM32F051K8U6TR";
        case LW_DCB4X_I2C_DEVICE_TYPE_STM32L031G6U6TR:
            return "STM32L031G6U6TR";
        case LW_DCB4X_I2C_DEVICE_TYPE_GMAC_MLW:
            return "GMAC_MLW";
        case LW_DCB4X_I2C_DEVICE_TYPE_INFOROM_EEPROM:
            return "INFOROM_EEPROM";
        case LW_DCB4X_I2C_DEVICE_TYPE_LWLINK_BRIDGE_ROM:
            return "LWLINK_BRIDGE_ROM";
        case LW_DCB4X_I2C_DEVICE_TYPE_GPU_I2CS_GT21X:
            return "GPU_I2CS_GT21X";
        case LW_DCB4X_I2C_DEVICE_TYPE_GPU_I2CS_GF11X:
            return "GPU_I2CS_GF11X";
        case LW_DCB4X_I2C_DEVICE_TYPE_SKIP_ENTRY:
            return "SKIP_ENTRY";
        default:
            MASSERT(!"Unknown I2C device type");
            return "UNKNOWN";
    }
}

//-----------------------------------------------------------------------------
void I2c::Dev::PrintSpec(Tee::Priority pri) const
{
    Printf(pri, "---I2C Device Spec---\n");
    Printf(pri, "  On Device: %s\n  Port: 0x%x, Addr: 0x%0x\n",
           m_pI2c->GetName().c_str(), m_Port, m_Addr);
    Printf(pri, "  OffsetLength = %d, MessageLength: = %d\n",
           m_OffsetLength, m_MessageLength);
    Printf(pri, "  MSbyte first = %s\n", m_MsbFirst? "true":"false");
    Printf(pri, "  Speed = %d KHz, AddrLength = %d bits\n",
           m_SpeedInKHz, m_AddrMode);

    Printf(pri, "---------------------\n");
}
//-----------------------------------------------------------------------------
RC I2c::Dev::SetSpec(I2cDevice deviceType, Tee::Priority pri)
{
    switch (deviceType)
    {
        case I2C_ADT7473:
            m_OffsetLength  = 1;
            m_MessageLength = 1;
            break;
        case I2C_INA219:
        case I2C_INA3221:
            m_OffsetLength  = 1;
            m_MessageLength = 2;
            break;
        case I2C_ADS1112:
            m_OffsetLength  = 0;
            m_MessageLength = 3;
            break;
        default:
            Printf(Tee::PriNormal, "unknown device type\n");
            return RC::BAD_PARAMETER;
    }
    PrintSpec(pri);
    return OK;
}

//--------------------------------------------------------------------
//! Copy the bytes of a UINT32 into an array of LwU8, in the endian
//! order determined by m_MsbFirst.
//!
void I2c::Dev::IntToArray(UINT32 Int, LwU8* pArray, UINT32 size) const
{
    MASSERT(pArray);
    MASSERT(size <= sizeof(Int));

    for (UINT32 ii = 0; ii < size; ++ii)
    {
        UINT32 shift = 0;
        if (m_MsbFirst)
            shift = 8 * (size - ii - 1);
        else
            shift = 8 * ii;

        pArray[ii] = (Int >> shift) & 0xFF;
    }
}

//--------------------------------------------------------------------
//! Copy an array of LwU8 into the bytes of a UINT32, in the endian
//! order determined by m_MsbFirst.
//!
void I2c::Dev::ArrayToInt(const LwU8* pArray, UINT32* pInt, UINT32 size) const
{
    MASSERT(pArray);
    MASSERT(pInt);
    MASSERT(size <= sizeof(*pInt));

    *pInt = 0;
    for (UINT32 ii = 0; ii < size; ++ii)
    {
        UINT32 shift = 0;
        if (m_MsbFirst)
            shift = 8 * (size - ii - 1);
        else
            shift = 8 * ii;

        *pInt |= static_cast<UINT32>(pArray[ii]) << shift;
    }
}

//-----------------------------------------------------------------------------
RC I2c::Dev::SetPortBaseOne(UINT32 port)
{
    return SetPort(port == 0 ? DYNAMIC_PORT : port - 1);
}
//-----------------------------------------------------------------------------
UINT32 I2c::Dev::GetPortBaseOne() const
{
    return (m_Port == DYNAMIC_PORT) ? 0 : (m_Port + 1);
}


//--------------------------------------------------------------------
//! \brief Find all I2C devices on this Device
//!
//! Find all I2C devices on this Device
//!
//! \param pDevices On exit, points to an array of all the I2c::Dev
//!     objects found.  The objects in this array may have the wrong
//!     OffsetLength, MessageLength, etc; only the constructor args
//!     (Port, Addr) are guaranteed to be correct.
//!
RC I2cImpl::DoFind(vector<Dev>* pDevices)
{
    MASSERT(pDevices);
    RC rc;

    pDevices->clear();

    PortInfo portInfo;
    CHECK_RC(GetPortInfo(&portInfo));

    Dev i2cDev = I2cDev();

    // Important: Set offset-length to 0 so that, when we write a "0"
    // to the device below, it gets interpreted as a register/offset
    // instead of data.  In theory, we would get the same effect by
    // reading or writing register 0 with OffsetLength=1 and
    // MessageLength=0, but the RM doesn't let us set MessageLength=0.
    //
    CHECK_RC(i2cDev.SetOffsetLength(0));

    // Loop through the devices on the valid I2C ports,
    // ping each device, and see which ones ACK.
    //
    for (UINT32 port = 0; port < NUM_PORTS; port++)
    {
        if (portInfo[port].Implemented)
        {
            CHECK_RC(i2cDev.SetPort(port));
            for (UINT32 address = 0; address < 256; address += 2)
            {
                CHECK_RC(i2cDev.SetAddr(address));
                if (OK == i2cDev.Ping())
                {
                    pDevices->push_back(I2cDev(port, address));
                }
            }
        }
    }

    return rc;
}

string I2cImpl::DoGetName() const
{
    return GetDevice()->GetName();
}
