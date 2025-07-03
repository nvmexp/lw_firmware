/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

// This function contains code that interfaces with the Winbond 83L785R
// Monitoring IC

#include "core/include/deprecat.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/i2c.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/js_gpusb.h"

#define CK_RC_READBYTE( Reg, Val )                                      \
    Printf(Tee::PriLow, "I2C Read  %02x\n", Reg);                       \
    CHECK_RC(d_pGpuSubdevice->GetInterface<I2c>()->I2cDev(WINBOND_PORT, \
                    WINBOND_ADDRESS).Read(Reg, Val));

#define CK_RC_WRITEBYTE( Reg, Val )                                     \
    Printf(Tee::PriLow, "I2C Write %02x = %02x\n", Reg, Val);           \
    CHECK_RC(d_pGpuSubdevice->GetInterface<I2c>()->I2cDev(WINBOND_PORT, \
                    WINBOND_ADDRESS).Write(Reg, Val));

//------------------------------------------------------------------------------
// HwMon Namespace
//------------------------------------------------------------------------------
namespace HwMon
{
   bool  IsInitialized = false;

   const UINT32 WINBOND_ADDRESS = 0x5A;
   const UINT32 WINBOND_PORT    = 2;

   //---------------------------------------------------------------------------
   // HwMon Register Locations
   const UINT32 ADDR_VCORE_READING       = 0x20; // VCORE reading
   const UINT32 ADDR_V2_5_READING        = 0x21; // +2.5V reading
   const UINT32 ADDR_V1_5_READING        = 0x22; // +1.5V reading
   const UINT32 ADDR_VCC_READING         = 0x23; // VCC reading (internal divisor 3)
   const UINT32 ADDR_TEMP2_READING       = 0x26; // Temperature 2 reading
   const UINT32 ADDR_TEMP1_READING       = 0x27; // Temperature 1 reading
   const UINT32 ADDR_FAN1_READING        = 0x28; // FAN1 reading
   const UINT32 ADDR_FAN2_READING        = 0x29; // FAN2 reading
   const UINT32 ADDR_VCORE_HI_LIMIT      = 0x2B; // VCORE High Limit. (0xff)
   const UINT32 ADDR_VCORE_LOW_LIMIT     = 0x2C; // VCORE Low Limit. (0x00)
   const UINT32 ADDR_V2_5_HI_LIMIT       = 0x2D; // +2.5V High Limit. (0xff)
   const UINT32 ADDR_V2_5_LOW_LIMIT      = 0x2E; // +2.5V Low Limit. (0x00)
   const UINT32 ADDR_V1_5_HI_LIMIT       = 0x2F; // +1.5V High Limit. (0xff)
   const UINT32 ADDR_V1_5_LOW_LIMIT      = 0x30; // +1.5V Low Limit. (0x00)
   const UINT32 ADDR_VCC_HI_LIMIT        = 0x31; // VCC High Limit. (0xff)
   const UINT32 ADDR_VCC_LOW_LIMIT       = 0x32; // VCC Low Limit. (0x00)
   const UINT32 ADDR_OVER_TEMP2_LIMIT    = 0x37; // Over Temperature 2 Limit (High). (0x50)
   const UINT32 ADDR_OVER_TEMP2_HYS_LIMIT= 0x38; // Temperature 2 Hysteresis Limit (Low). (0x4b)
   const UINT32 ADDR_OVER_TEMP1_LIMIT    = 0x39; // Over Temperature 1 Limit (High). (0x50)
   const UINT32 ADDR_OVER_TEMP1_HYS_LIMIT= 0x3A; // Temperature 1 Hysteresis Limit (Low). (0x4b)
   const UINT32 ADDR_FAN1_COUNT_LIMIT    = 0x3B; // FAN1 Fan Count Limit. (0xff)
   const UINT32 ADDR_FAN2_COUNT_LIMIT    = 0x3C; // FAN2 Fan Count Limit. (0xff)
   const UINT32 ADDR_CONFIG_REGISTER     = 0x40; // Configuration Register
   const UINT32 ADDR_SMI_STATUS          = 0x41; // Smi# Status Register 1
   const UINT32 ADDR_SMI_MASK            = 0x43; // Smi# Mask Register 1
   const UINT32 ADDR_REALTIME_HWSTAT     = 0x45; // Real Time Hardware Status Register I
   const UINT32 ADDR_FAN_DIVISOR         = 0x47; // Fan Divisor Register
   const UINT32 ADDR_GPIO_CONTROL1       = 0x48; // Fanin1/Gpo1, Fanin2/Gpo2, Smi#/Gpo7, And Ovt#/Gpio8 Control Register
   const UINT32 ADDR_GPIO_CONTROL2       = 0x49; // Temp_Fault#/Gpio11, +2 5v/Gpo14, +1 5v/Gpo15, And Vcore/Gpo16 Control Register
   const UINT32 ADDR_GPIO_OUTPUT_EN1     = 0x4A; // Gpio1/2, Gpio7/8, And Gpio11/14/15/16 Output Control Register
   const UINT32 ADDR_GPIO_OUTPUT_EN2     = 0x4B; // Gpio5, Gpio6, And Gpio13 Control Register
   const UINT32 ADDR_VENDOR_ID_LO        = 0x4C; // HwMon Vendor Id (Low Byte)
   const UINT32 ADDR_VENDOR_ID_HI        = 0x4D; // HwMon Vendor Id (High Byte)
   const UINT32 ADDR_CHIP_ID             = 0x4E; // Chip Id
   const UINT32 ADDR_OVT_PROP            = 0x50; // Ovt# Property Select
   const UINT32 ADDR_SMI_PROP            = 0x51; // Smi# Property Select
   const UINT32 ADDR_SENSOR_TYPE         = 0x52; // Thermal Sensor 1/2 Type Register
   const UINT32 ADDR_SENSOR1_FAULT_LIMIT = 0x53; // Temperature Sensor 1 Fault Limit
   const UINT32 ADDR_SENSOR2_FAULT_LIMIT = 0x54; // Temperature Sensor 2 Fault Limit
   const UINT32 ADDR_PWM1_PRESCALE       = 0x80; // Pwm 1 Pre-Scale Register
   const UINT32 ADDR_PWM1_DUTY_CYCLE     = 0x81; // Pwm 1 Duty Cycle Select Register
   const UINT32 ADDR_PWM2_PRESCALE       = 0x82; // Pwm 2 Pre-Scale Register
   const UINT32 ADDR_PWM2_DUTY_CYCLE     = 0x83; // Pwm2 Duty Cycle Select Register
   const UINT32 ADDR_TEMP_SENSOR1        = 0x85; // Temperature Sensor 1 (For Environment) Offset Register
   const UINT32 ADDR_TEMP_SENSOR2        = 0x86; // Temperature Sensor 2 (For Cpu) Offset Register

   //---------------------------------------------------------------------------
   // Misc constants related to programming the HwMon chip
   const UINT32 WINBOND_VENDOR_ID        = 0x5CA3;

   //---------------------------------------------------------------------------
   GpuSubdevice* d_pGpuSubdevice = nullptr;
   // Misc methods
   RC Initialize(GpuSubdevice* pSubdevice);
   RC Uninitialize();
   RC Dump();

   // GPIO interfaces functions
   RC IsOutput(  UINT32 WhichGpio, bool *isOutput    );
   RC SetOutput( UINT32 WhichGpio, bool OutputLevel  );
   RC GetOutput( UINT32 WhichGpio, bool *OutputLevel );
   RC GetInput(  UINT32 WhichGpio, bool *inLevel     );

   RC GetLwVdd(FLOAT64 *Volts);
   RC SetLwVdd(FLOAT64 Volts);
   RC SetFanSpeed(UINT32 FanNumber, UINT32 Speed);
   RC GetFanSpeed(UINT32 FanNumber, UINT32 *Speed);

   RC MeasureLwVdd(FLOAT64 *Volts);
   RC MeasureFbVddq(FLOAT64 *Volts);
   RC MeasureFbVdd(FLOAT64 *Volts);
   RC Measure3_3(FLOAT64 *Volts);
   RC MeasureTemp(UINT32 TempNumber, UINT32 *Temp);
}

struct Reg
{
   UINT32 Addr;
   const char *Description;
};
//------------------------------------------------------------------------------
// Javascript Interface
//------------------------------------------------------------------------------
JS_CLASS(HwMon);

static SObject HwMon_Object
(
   "HwMon",
   HwMonClass,
   0,
   0,
   "Winbond 83L785R interface class"
);

//------------------------------------------------------------------------------
C_(HwMon_Initialize);
STest HwMon_Initialize
(
   HwMon_Object,
   "Initialize",
   C_HwMon_Initialize,
   1,
   "Initialize"
);

C_(HwMon_Initialize)
{
    STEST_HEADER(0, 1, "Usage: HwMon.Initialize()");
    STEST_PRIVATE_OPTIONAL_ARG(0, JsGpuSubdevice, pJsSub, "GpuSubdevice");

    GpuSubdevice* pGpuSubdevice = nullptr;
    if (!pJsSub)
    {
        static Deprecation depr("HwMon.Initialize", "3/30/2019",
                                "Must specify a GpuSubdevice object parameter to initialize with\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;

        pGpuSubdevice = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    }
    else
    {
        pGpuSubdevice = pJsSub->GetGpuSubdevice();
    }

    RETURN_RC( HwMon::Initialize(pGpuSubdevice) );
}

//------------------------------------------------------------------------------
C_(HwMon_Uninitialize);
STest HwMon_Uninitialize
(
   HwMon_Object,
   "Uninitialize",
   C_HwMon_Uninitialize,
   1,
   "Uninitialize"
);

C_(HwMon_Uninitialize)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);
   RETURN_RC( HwMon::Uninitialize() );
}

//------------------------------------------------------------------------------
C_(HwMon_Dump);
STest HwMon_Dump
(
   HwMon_Object,
   "Dump",
   C_HwMon_Dump,
   1,
   "Dump"
);

C_(HwMon_Dump)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);
   RETURN_RC( HwMon::Dump() );
}

//------------------------------------------------------------------------------
C_(HwMon_IsOutput);
static STest HwMon_IsOutput
(
   HwMon_Object,
   "IsOutput",
   C_HwMon_IsOutput,
   1,
   ""
);

C_(HwMon_IsOutput)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if((NumArguments < 1) || (NumArguments > 2 ))
   {
      JS_ReportError(pContext,
         "Usage: IsOutput(WhichGpio) or IsOutput(WhichGpio, Array)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;

   UINT32 WhichGpio;
   bool isOut;

   if(OK != pJavaScript->FromJsval(pArguments[0], &WhichGpio))
   {
      JS_ReportError(pContext,
         "Usage: IsOutput(WhichGpio) or IsOutput(WhichGpio, Array)");
      return JS_FALSE;
   }

   RC rc = OK;
   C_CHECK_RC( HwMon::IsOutput( WhichGpio, &isOut ));

   if (NumArguments == 2)
   {
      if(OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))
      {
         JS_ReportError(pContext, "Error in IsOutput()");
         return JS_FALSE;
      }
      else
      {
         RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, (UINT32)isOut));
      }
   }

   // NumArguments == 1 if we got here
   Printf(Tee::PriNormal, "Output State = 0x%02x\n", (unsigned char)isOut);

   RETURN_RC(OK);
}

//------------------------------------------------------------------------------
C_(HwMon_GetInput);
static STest HwMon_GetInput
(
   HwMon_Object,
   "GetInput",
   C_HwMon_GetInput,
   1,
   ""
);

C_(HwMon_GetInput)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if((NumArguments < 1) || (NumArguments > 2 ))
   {
      JS_ReportError(pContext,
         "Usage: GetInput(WhichGpio) or GetInput(WhichGpio, Array)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;

   UINT32 WhichGpio;
   bool Level;

   if(OK != pJavaScript->FromJsval(pArguments[0], &WhichGpio))
   {
      JS_ReportError(pContext,
         "Usage: GetInput(WhichGpio) or GetInput(WhichGpio, Array)");
      return JS_FALSE;
   }

   RC rc = OK;
   C_CHECK_RC( HwMon::GetInput( WhichGpio, &Level ));

   if (NumArguments == 2)
   {
      if(OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))
      {
         JS_ReportError(pContext, "Error in GetInput()");
         return JS_FALSE;
      }
      else
      {
         RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, (UINT32)Level));
      }
   }

   // NumArguments == 1 if we got here
   Printf(Tee::PriNormal, "Output State = 0x%02x\n", (unsigned char)Level);

   RETURN_RC(OK);
}

//------------------------------------------------------------------------------
C_(HwMon_GetOutput);
static STest HwMon_GetOutput
(
   HwMon_Object,
   "GetOutput",
   C_HwMon_GetOutput,
   1,
   ""
);

C_(HwMon_GetOutput)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if((NumArguments < 1) || (NumArguments > 2 ))
   {
      JS_ReportError(pContext,
         "Usage: GetOutput(WhichGpio) or GetOutput(WhichGpio, Array)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;

   UINT32 WhichGpio;
   bool Level;

   if(OK != pJavaScript->FromJsval(pArguments[0], &WhichGpio))
   {
      JS_ReportError(pContext,
         "Usage: GetOutput(WhichGpio) or GetOutput(WhichGpio, Array)");
      return JS_FALSE;
   }

   RC rc = OK;
   C_CHECK_RC( HwMon::GetOutput( WhichGpio, &Level ));

   if (NumArguments == 2)
   {
      if(OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))
      {
         JS_ReportError(pContext, "Error in GetOutput()");
         return JS_FALSE;
      }
      else
      {
         RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, (UINT32)Level));
      }
   }

   // NumArguments == 1 if we got here
   Printf(Tee::PriNormal, "Output State = 0x%02x\n", (unsigned char)Level);

   RETURN_RC(OK);
}

//------------------------------------------------------------------------------
C_(HwMon_SetOutput);
static STest HwMon_SetOutput
(
   HwMon_Object,
   "SetOutput",
   C_HwMon_SetOutput,
   1,
   ""
);

C_(HwMon_SetOutput)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 2 )
   {
      JS_ReportError(pContext,
         "Usage: SetOutput(WhichGpio, Level)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;

   UINT32 WhichGpio;
   bool Level;

   if( (OK != pJavaScript->FromJsval(pArguments[0], &WhichGpio)) ||
       (OK != pJavaScript->FromJsval(pArguments[1], &Level)) )
   {
      JS_ReportError(pContext,
         "Usage: SetOutput(WhichGpio, Level)");
      return JS_FALSE;
   }

   RETURN_RC( HwMon::SetOutput( WhichGpio, Level ));
}

//------------------------------------------------------------------------------
C_(HwMon_GetFanSpeed);
static STest HwMon_GetFanSpeed
(
   HwMon_Object,
   "GetFanSpeed",
   C_HwMon_GetFanSpeed,
   1,
   ""
);

C_(HwMon_GetFanSpeed)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if((NumArguments < 1) || (NumArguments > 2 ))
   {
      JS_ReportError(pContext,
         "Usage: GetFanSpeed(WhichFan) or GetFanSpeed(WhichFan, Array)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;

   UINT32 WhichFan;
   UINT32 Speed;

   if(OK != pJavaScript->FromJsval(pArguments[0], &WhichFan))
   {
      JS_ReportError(pContext,
         "Usage: GetFanSpeed(WhichFan) or GetFanSpeed(WhichFan, Array)");
      return JS_FALSE;
   }

   RC rc = OK;
   C_CHECK_RC( HwMon::GetFanSpeed( WhichFan, &Speed ));

   if (NumArguments == 2)
   {
      if(OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))
      {
         JS_ReportError(pContext, "Error in GetFanSpeed()");
         return JS_FALSE;
      }
      else
      {
         RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, Speed));
      }
   }

   // NumArguments == 1 if we got here
   Printf(Tee::PriNormal, "Fan %d speed = %d percent\n", WhichFan, Speed);

   RETURN_RC(OK);
}

//------------------------------------------------------------------------------
C_(HwMon_SetFanSpeed);
static STest HwMon_SetFanSpeed
(
   HwMon_Object,
   "SetFanSpeed",
   C_HwMon_SetFanSpeed,
   1,
   ""
);

C_(HwMon_SetFanSpeed)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 2 )
   {
      JS_ReportError(pContext,
         "Usage: SetFanSpeed(WhichFan, Speed)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;

   UINT32 WhichFan;
   UINT32 Speed;

   if( (OK != pJavaScript->FromJsval(pArguments[0], &WhichFan)) ||
       (OK != pJavaScript->FromJsval(pArguments[1], &Speed)) )
   {
      JS_ReportError(pContext,
         "Usage: SetFanSpeed(WhichFan, Speed)");
      return JS_FALSE;
   }

   RETURN_RC( HwMon::SetFanSpeed( WhichFan, Speed ));
}

//------------------------------------------------------------------------------
C_(HwMon_GetLwVdd);
static STest HwMon_GetLwVdd
(
   HwMon_Object,
   "GetLwVdd",
   C_HwMon_GetLwVdd,
   1,
   ""
);

C_(HwMon_GetLwVdd)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments > 1 )
   {
      JS_ReportError(pContext, "Usage: GetLwVdd() or GetLwVdd(Array)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;

   FLOAT64 Volts;

   RC rc = OK;
   C_CHECK_RC( HwMon::GetLwVdd( &Volts ));

   if (NumArguments == 1)
   {
      if(OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
      {
         JS_ReportError(pContext, "Error in GetLwVdd()");
         return JS_FALSE;
      }
      else
      {
         RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, Volts));
      }
   }

   // NumArguments == 0 if we got here
   Printf(Tee::PriNormal, "LwVdd = %1.3fV\n", Volts);

   RETURN_RC(OK);
}

//------------------------------------------------------------------------------
C_(HwMon_SetLwVdd);
static STest HwMon_SetLwVdd
(
   HwMon_Object,
   "SetLwVdd",
   C_HwMon_SetLwVdd,
   1,
   ""
);

C_(HwMon_SetLwVdd)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 1 )
   {
      JS_ReportError(pContext, "Usage: SetLwVdd(Volts)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;

   FLOAT64 Volts;

   if( OK != pJavaScript->FromJsval(pArguments[0], &Volts))
   {
      JS_ReportError(pContext, "Usage: SetLwVdd(Volts)");
      return JS_FALSE;
   }

   RETURN_RC( HwMon::SetLwVdd( Volts ));
}

//------------------------------------------------------------------------------
C_(HwMon_GetMeasurements);
static STest HwMon_GetMeasurements
(
   HwMon_Object,
   "GetMeasurements",
   C_HwMon_GetMeasurements,
   1,
   ""
);

C_(HwMon_GetMeasurements)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments > 1 )
   {
      JS_ReportError(pContext,
         "Usage: GetMeasurements() or "
                "GetMeasurements([ LwVdd, FbVddq, FbVdd, 3.3V, Temp1, Temp2 ])");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;

   FLOAT64 LwVdd        = 0.0;
   FLOAT64 FbVddq       = 0.0;
   FLOAT64 FbVdd        = 0.0;
   FLOAT64 V3_3         = 0.0;
   UINT32  Temperature1 = 0;
   UINT32  Temperature2 = 0;

   RC rc = OK;

   C_CHECK_RC( HwMon::MeasureLwVdd( &LwVdd ));
   C_CHECK_RC( HwMon::MeasureFbVddq( &FbVddq));
   C_CHECK_RC( HwMon::MeasureFbVdd( &FbVdd));
   C_CHECK_RC( HwMon::Measure3_3( &V3_3));
   C_CHECK_RC( HwMon::MeasureTemp( 1, &Temperature1 ));
   C_CHECK_RC( HwMon::MeasureTemp( 2, &Temperature2 ));

   if (NumArguments == 1)
   {
      if(OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
      {
         JS_ReportError(pContext, "Error in GetMeasurements()");
         return JS_FALSE;
      }
      else
      {
         C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, LwVdd ));
         C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 1, FbVddq ));
         C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 2, FbVdd ));
         C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 3, V3_3 ));
         C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 4, Temperature1 ));
         C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 5, Temperature2 ));
         RETURN_RC(OK);
      }
   }

   // NumArguments == 0 if we got here
   Printf(Tee::PriNormal, "LwVdd        = %1.3f V\n", LwVdd );
   Printf(Tee::PriNormal, "FbVddq       = %1.3f V\n", FbVddq );
   Printf(Tee::PriNormal, "FbVdd        = %1.3f V\n", FbVdd );
   Printf(Tee::PriNormal, "V3_3         = %1.3f V\n", V3_3 );
   Printf(Tee::PriNormal, "Temperature1 =    %d C\n", Temperature1 );
   Printf(Tee::PriNormal, "Temperature2 =    %d C\n", Temperature2 );

   RETURN_RC(OK);
}

//------------------------------------------------------------------------------
// Namespace functions
//------------------------------------------------------------------------------
RC HwMon::Initialize(GpuSubdevice* pGpuSubdevice)
{
    d_pGpuSubdevice = pGpuSubdevice;

    UINT32 Low  = 0xFFFFFFFF;
    UINT32 High = 0xFFFFFFFF;

    RC rc = OK;

    CK_RC_READBYTE( ADDR_VENDOR_ID_LO, &Low);
    CK_RC_READBYTE( ADDR_VENDOR_ID_HI, &High);

    UINT32 VendorId = (Low & 0xFF) | ((High & 0xFF) << 8);
    Printf(Tee::PriLow, "Detected part with vendor ID = 0x%04x\n", VendorId);

    if( WINBOND_VENDOR_ID != VendorId )
    {
       return RC::SERIAL_COMMUNICATION_ERROR;
    }

    // Set Thermal sensors to Diode type
    CK_RC_WRITEBYTE( ADDR_SENSOR_TYPE, 0x0F);

    // Set Temp limits to 125C
    CK_RC_WRITEBYTE( ADDR_SENSOR1_FAULT_LIMIT, 0x7d);
    CK_RC_WRITEBYTE( ADDR_SENSOR2_FAULT_LIMIT, 0x7d);

    // Disable SMI
    CK_RC_WRITEBYTE( ADDR_SMI_MASK, 0xFF);

    // Enables Temp monitors
    CK_RC_WRITEBYTE( ADDR_CONFIG_REGISTER, 0x0F);

    // Set TEMP offsets
    CK_RC_WRITEBYTE( ADDR_TEMP_SENSOR1, 0xE0);
    CK_RC_WRITEBYTE( ADDR_TEMP_SENSOR2, 0xE0);

    // Resume monitoring
    CK_RC_WRITEBYTE( ADDR_CONFIG_REGISTER, 0x0d);

    // Set FAN clock/devisors
    CK_RC_WRITEBYTE( ADDR_PWM1_PRESCALE, 0x81);
    CK_RC_WRITEBYTE( ADDR_PWM2_PRESCALE, 0x81);

    IsInitialized=true;

    return OK;
}

//------------------------------------------------------------------------------
RC HwMon::Uninitialize()
{
    d_pGpuSubdevice = nullptr;
    IsInitialized = false;
    return OK;
}

//------------------------------------------------------------------------------
RC HwMon::Dump()
{
   if ( !IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   static Reg s_Regs[] =
   {
      { ADDR_VCORE_READING        ,"VCORE reading" },
      { ADDR_V2_5_READING         ,"+2.5V reading" },
      { ADDR_V1_5_READING         ,"+1.5V reading" },
      { ADDR_VCC_READING          ,"VCC reading" },
      { ADDR_TEMP2_READING        ,"Temperature 2 reading" },
      { ADDR_TEMP1_READING        ,"Temperature 1 reading" },
      { ADDR_FAN1_READING         ,"FAN1 reading" },
      { ADDR_FAN2_READING         ,"FAN2 reading" },
      { ADDR_VCORE_HI_LIMIT       ,"VCORE High Limit" },
      { ADDR_VCORE_LOW_LIMIT      ,"VCORE Low Limit" },
      { ADDR_V2_5_HI_LIMIT        ,"+2.5V High Limit" },
      { ADDR_V2_5_LOW_LIMIT       ,"+2.5V Low Limit" },
      { ADDR_V1_5_HI_LIMIT        ,"+1.5V High Limit" },
      { ADDR_V1_5_LOW_LIMIT       ,"+1.5V Low Limit" },
      { ADDR_VCC_HI_LIMIT         ,"VCC High Limit" },
      { ADDR_VCC_LOW_LIMIT        ,"VCC Low Limit" },
      { ADDR_OVER_TEMP2_LIMIT     ,"Over Temperature 2 Limit (High)" },
      { ADDR_OVER_TEMP2_HYS_LIMIT ,"Temperature 2 Hysteresis Limit (Low)" },
      { ADDR_OVER_TEMP1_LIMIT     ,"Over Temperature 1 Limit (High)" },
      { ADDR_OVER_TEMP1_HYS_LIMIT ,"Temperature 1 Hysteresis Limit (Low)" },
      { ADDR_FAN1_COUNT_LIMIT     ,"FAN1 Fan Count Limit" },
      { ADDR_FAN2_COUNT_LIMIT     ,"FAN2 Fan Count Limit" },
      { ADDR_CONFIG_REGISTER      ,"Configuration" },
      { ADDR_SMI_STATUS           ,"Smi# Status 1" },
      { ADDR_SMI_MASK             ,"Smi# Mask 1" },
      { ADDR_REALTIME_HWSTAT      ,"Real Time Hardware Status" },
      { ADDR_FAN_DIVISOR          ,"Fan Divisor" },
      { ADDR_GPIO_CONTROL1        ,"Fanin/Gpo/Smi#/Ovt# Control" },
      { ADDR_GPIO_CONTROL2        ,"Temp_Fault#/Gpio/Vcore/Gpo16 Control" },
      { ADDR_GPIO_OUTPUT_EN1      ,"Misc Gpio Output Control" },
      { ADDR_GPIO_OUTPUT_EN2      ,"Gpio5, Gpio6, And Gpio13 Control" },
      { ADDR_VENDOR_ID_LO         ,"HwMon Vendor Id (Low Byte)" },
      { ADDR_VENDOR_ID_HI         ,"HwMon Vendor Id (High Byte)" },
      { ADDR_CHIP_ID              ,"Chip Id" },
      { ADDR_OVT_PROP             ,"Ovt# Property Select" },
      { ADDR_SMI_PROP             ,"Smi# Property Select" },
      { ADDR_SENSOR_TYPE          ,"Thermal Sensor 1/2 Type" },
      { ADDR_SENSOR1_FAULT_LIMIT  ,"Temperature Sensor 1 Fault Limit" },
      { ADDR_SENSOR2_FAULT_LIMIT  ,"Temperature Sensor 2 Fault Limit" },
      { ADDR_PWM1_PRESCALE        ,"Pwm 1 Pre-Scale" },
      { ADDR_PWM1_DUTY_CYCLE      ,"Pwm 1 Duty Cycle Select" },
      { ADDR_PWM2_PRESCALE        ,"Pwm 2 Pre-Scale" },
      { ADDR_PWM2_DUTY_CYCLE      ,"Pwm2 Duty Cycle Select" },
      { ADDR_TEMP_SENSOR1         ,"Temperature Sensor 1 (Environment) Offset" },
      { ADDR_TEMP_SENSOR2         ,"Temperature Sensor 2 (Cpu) Offset" }
   };

   RC rc = OK;
   size_t i;
   UINT32 Data;

   Printf(Tee::PriNormal, "ADDR    DATA                      DESC\n");
   Printf(Tee::PriNormal, "---- ---------- ----------------------------------------\n");

   for (i = 0; i < sizeof(s_Regs)/sizeof(Reg); ++i)
   {

      CK_RC_READBYTE( s_Regs[i].Addr, &Data);

      Printf(Tee::PriNormal,
             "0x%02x 0x%02x %4d  %-40s\n",
             s_Regs[i].Addr,
             Data,
             Data,
             s_Regs[i].Description);
   }

   return OK;
}

//------------------------------------------------------------------------------
RC HwMon::IsOutput(UINT32 WhichGpio, bool *isOutput)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   *isOutput = false;

   UINT32 Data = 0;

   UINT32 Register = 0;
   UINT32 Bit      = 0;

   switch (WhichGpio)
   {
      case 1:  Register = ADDR_GPIO_OUTPUT_EN1; Bit = 0; break;
      case 2:  Register = ADDR_GPIO_OUTPUT_EN1; Bit = 1; break;
      case 7:  Register = ADDR_GPIO_OUTPUT_EN1; Bit = 2; break;
      case 8:  Register = ADDR_GPIO_OUTPUT_EN1; Bit = 3; break;
      case 11: Register = ADDR_GPIO_OUTPUT_EN1; Bit = 4; break;
      case 14: Register = ADDR_GPIO_OUTPUT_EN1; Bit = 5; break;
      case 15: Register = ADDR_GPIO_OUTPUT_EN1; Bit = 6; break;
      case 16: Register = ADDR_GPIO_OUTPUT_EN1; Bit = 7; break;

      case 6:  Register = ADDR_GPIO_OUTPUT_EN2; Bit = 1; break;
      case 5:  Register = ADDR_GPIO_OUTPUT_EN2; Bit = 3; break;
      case 13: Register = ADDR_GPIO_OUTPUT_EN2; Bit = 5; break;

      default:
         Printf(Tee::PriError, "Gpio pin %d not supported.\n", WhichGpio);
         return RC::SOFTWARE_ERROR;
   }

   CK_RC_READBYTE( Register, &Data );

   *isOutput = ( Data & (1<<Bit) ) ? true : false;

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::SetOutput( UINT32 WhichGpio, bool OutputLevel )
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   UINT32 Data = 0;

   UINT32 Register = 0;
   UINT32 Bit      = 0;

   switch (WhichGpio)
   {
      case 1:  Register = ADDR_GPIO_OUTPUT_EN1; Bit = 0; break;
      case 2:  Register = ADDR_GPIO_OUTPUT_EN1; Bit = 1; break;
      case 7:  Register = ADDR_GPIO_OUTPUT_EN1; Bit = 2; break;
      case 8:  Register = ADDR_GPIO_OUTPUT_EN1; Bit = 3; break;
      case 11: Register = ADDR_GPIO_OUTPUT_EN1; Bit = 4; break;
      case 14: Register = ADDR_GPIO_OUTPUT_EN1; Bit = 5; break;
      case 15: Register = ADDR_GPIO_OUTPUT_EN1; Bit = 6; break;
      case 16: Register = ADDR_GPIO_OUTPUT_EN1; Bit = 7; break;

      case 6:  Register = ADDR_GPIO_OUTPUT_EN2; Bit = 1; break;
      case 5:  Register = ADDR_GPIO_OUTPUT_EN2; Bit = 3; break;
      case 13: Register = ADDR_GPIO_OUTPUT_EN2; Bit = 5; break;

      default:
         Printf(Tee::PriError, "Gpio pin %d not supported.\n", WhichGpio);
         return RC::SOFTWARE_ERROR;
   }

   CK_RC_READBYTE( Register, &Data );

   Data = ( Data & ~(1<<Bit) ) | (OutputLevel ? (1<<Bit) : 0 );

   CK_RC_WRITEBYTE( Register, Data );

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::GetOutput( UINT32 WhichGpio, bool *OutputLevel )
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   *OutputLevel = false;

   UINT32 Data = 0;

   UINT32 Register = 0;
   UINT32 Bit      = 0;

   switch (WhichGpio)
   {
      case 1:  Register = ADDR_GPIO_CONTROL1; Bit = 1; break;
      case 2:  Register = ADDR_GPIO_CONTROL1; Bit = 3; break;
      case 7:  Register = ADDR_GPIO_CONTROL1; Bit = 5; break;
      case 8:  Register = ADDR_GPIO_CONTROL1; Bit = 7; break;

      case 11: Register = ADDR_GPIO_CONTROL2; Bit = 1; break;
      case 14: Register = ADDR_GPIO_CONTROL2; Bit = 3; break;
      case 15: Register = ADDR_GPIO_CONTROL2; Bit = 5; break;
      case 16: Register = ADDR_GPIO_CONTROL2; Bit = 7; break;

      case 5:  Register = ADDR_GPIO_OUTPUT_EN2; Bit = 0; break;
      case 6:  Register = ADDR_GPIO_OUTPUT_EN2; Bit = 2; break;
      case 13: Register = ADDR_GPIO_OUTPUT_EN2; Bit = 4; break;

      default:
         Printf(Tee::PriError, "Gpio pin %d not supported.\n", WhichGpio);
         return RC::SOFTWARE_ERROR;
   }

   CK_RC_READBYTE( Register, &Data );

   *OutputLevel = ( Data & (1<<Bit) ) ? true : false;

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::GetInput( UINT32 WhichGpio, bool *inLevel )
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   *inLevel = false;
   RC rc = OK;

   UINT32 Data = 0;

   UINT32 Register = 0;
   UINT32 Bit      = 0;

   switch (WhichGpio)
   {
      case 1:  Register = ADDR_GPIO_CONTROL1; Bit = 1; break;
      case 2:  Register = ADDR_GPIO_CONTROL1; Bit = 3; break;
      case 7:  Register = ADDR_GPIO_CONTROL1; Bit = 5; break;
      case 8:  Register = ADDR_GPIO_CONTROL1; Bit = 7; break;

      case 11: Register = ADDR_GPIO_CONTROL2; Bit = 1; break;
      case 14: Register = ADDR_GPIO_CONTROL2; Bit = 3; break;
      case 15: Register = ADDR_GPIO_CONTROL2; Bit = 5; break;
      case 16: Register = ADDR_GPIO_CONTROL2; Bit = 7; break;

      case 5:  Register = ADDR_GPIO_OUTPUT_EN2; Bit = 0; break;
      case 6:  Register = ADDR_GPIO_OUTPUT_EN2; Bit = 2; break;
      case 13: Register = ADDR_GPIO_OUTPUT_EN2; Bit = 4; break;

      default:
         Printf(Tee::PriError, "Gpio pin %d not supported.\n", WhichGpio);
         return RC::SOFTWARE_ERROR;
   }

   CK_RC_READBYTE( Register, &Data );

   *inLevel = ( Data & (1<<Bit) ) ? true : false;

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::GetLwVdd(FLOAT64 *Volts)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   UINT32 Value = 0;
   UINT32 LwVddIdx = 0;
   RC rc = OK;

   // Set GPIO 1+2 to GPIO function
   CK_RC_READBYTE(  ADDR_GPIO_CONTROL1, &Value);
   CK_RC_WRITEBYTE( ADDR_GPIO_CONTROL1, Value | 0x05);

   bool Gpio1  = false;
   bool Gpio5  = false;
   bool Gpio6  = false;
   bool Gpio13 = false;

   CHECK_RC(GetInput(  1, &Gpio1  ));
   CHECK_RC(GetInput(  5, &Gpio5  ));
   CHECK_RC(GetInput(  6, &Gpio6  ));
   CHECK_RC(GetInput( 13, &Gpio13 ));

   if(Gpio1  ) LwVddIdx += 1;
   if(Gpio5  ) LwVddIdx += 2;
   if(Gpio6  ) LwVddIdx += 4;
   if(Gpio13 ) LwVddIdx += 8;

   *Volts = (1550.0 - 50.0 * float(LwVddIdx))/1000;

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::SetLwVdd(FLOAT64 Volts)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   UINT32 Value = 0;

   if (0.001 > Volts) // if zero or very close to zero
   {
      SetOutput( 2, true);

      CK_RC_READBYTE(  ADDR_GPIO_CONTROL1, &Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_CONTROL1, Value | 0x08);

      CK_RC_READBYTE(  ADDR_GPIO_OUTPUT_EN1, &Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_OUTPUT_EN1, Value & 0x2A);
   }
   else if ((Volts >= 0.8) && (Volts <= 1.55))
   {
      UINT32 VddIndex = UINT32((1.55 - Volts) / 0.05 + 0.5);

      // Set GPIO 1+2 to GPIO function
      CK_RC_READBYTE(  ADDR_GPIO_CONTROL1, &Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_CONTROL1, Value | 0x05);

      // Read/Set current VID state for GPIO 1
      CK_RC_READBYTE(  ADDR_GPIO_CONTROL1, &Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_CONTROL1, Value);

      // Read/Set current VID state and put GPIO 5,6,13 to output
      CK_RC_READBYTE(  ADDR_GPIO_OUTPUT_EN2, &Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_OUTPUT_EN2, Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_OUTPUT_EN2, Value | 0x2A);

      // Set GPIO1 to output
      CK_RC_READBYTE(  ADDR_GPIO_OUTPUT_EN1, &Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_OUTPUT_EN1, Value | 0x01);

      // Default Setting prepared for disable (GPIO2 LOW)
      CK_RC_READBYTE(  ADDR_GPIO_CONTROL1, &Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_CONTROL1, Value & 0xF7);

      // Set GPIO2 to output -> disable Default
      CK_RC_READBYTE(  ADDR_GPIO_OUTPUT_EN1, &Value);
      CK_RC_WRITEBYTE( ADDR_GPIO_OUTPUT_EN1, Value | 0x02);

      // Set value for GPIO 5,6,13 (VID 2,3,4)
      CK_RC_READBYTE( ADDR_GPIO_OUTPUT_EN2, &Value);
      Value =  (Value & 0xEA) | 0x2a;

      if (VddIndex & 0x08)
          Value =  Value | 0x10;

      if (VddIndex & 0x04)
          Value =  Value | 0x04;

      if (VddIndex & 0x02)
          Value =  Value | 0x01;
      CK_RC_WRITEBYTE( ADDR_GPIO_OUTPUT_EN2, Value);

      // Set value for GPIO 1 (VID 1)
      CK_RC_READBYTE( ADDR_GPIO_CONTROL1, &Value);
      Value =  (Value & 0xFD) | 0x01;
      if (VddIndex & 0x01)
          Value =  Value | 0x02;
      CK_RC_WRITEBYTE( ADDR_GPIO_CONTROL1, Value);
   }
   else
   {
      Printf(Tee::PriNormal, "Value out of range 0.8..1.5\n");
   }

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::SetFanSpeed(UINT32 FanNumber, UINT32 Speed)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   if( Speed > 100 )
   {
      Printf(Tee::PriNormal, "Invalid fan speed--must be from 0 to 100\n");
      return RC::SOFTWARE_ERROR;
   }

   if( (FanNumber != 1) && (FanNumber != 2) )
   {
      Printf(Tee::PriNormal, "Invalid fan number--must be 1 or 2\n");
      return RC::SOFTWARE_ERROR;
   }

   UINT32 RegVal = UINT32( float(Speed) *2.55+0.5);

   if(1 == FanNumber)
   {
      CK_RC_WRITEBYTE( ADDR_PWM1_PRESCALE, 0x81);
      CK_RC_WRITEBYTE( ADDR_PWM1_DUTY_CYCLE, RegVal);
   }
   else
   {
      CK_RC_WRITEBYTE( ADDR_PWM2_PRESCALE, 0x81);
      CK_RC_WRITEBYTE( ADDR_PWM2_DUTY_CYCLE, RegVal);
   }

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::GetFanSpeed(UINT32 FanNumber, UINT32 *Speed)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   UINT32 Value = 0;

   if( (FanNumber != 1) && (FanNumber != 2) )
   {
      Printf(Tee::PriNormal, "Invalid fan number--must be 1 or 2\n");
      return RC::SOFTWARE_ERROR;
   }

   UINT32 Register = (1 == FanNumber) ? ADDR_PWM1_DUTY_CYCLE : ADDR_PWM2_DUTY_CYCLE;

   CK_RC_READBYTE( Register, &Value);

   *Speed = UINT32( (( float(Value) / 2.55) * 10 + 0.5) / 10);

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::MeasureLwVdd(FLOAT64 *Volts)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   UINT32 Value;

   CK_RC_READBYTE( ADDR_VCORE_READING, &Value);

   *Volts = 8.0 * float(Value) / 1000.0;

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::MeasureFbVddq(FLOAT64 *Volts)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   UINT32 Value;

   CK_RC_READBYTE( ADDR_V2_5_READING, &Value);

   *Volts = 2.0 * 8.0 * float(Value) / 1000.0;

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::MeasureFbVdd(FLOAT64 *Volts)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   UINT32 Value;

   CK_RC_READBYTE( ADDR_V1_5_READING, &Value);

   *Volts = 2.0 * 8.0 * float(Value) / 1000.0;

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::Measure3_3(FLOAT64 *Volts)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   UINT32 Value;

   CK_RC_READBYTE( ADDR_VCC_READING, &Value);

   *Volts = 3.0 * 8.0 * float(Value) / 1000.0;

   return rc;
}

//------------------------------------------------------------------------------
RC HwMon::MeasureTemp(UINT32 TempNumber, UINT32 *Temp)
{
   if (!IsInitialized )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   UINT32 Value;

   if( (TempNumber != 1) && (TempNumber != 2) )
   {
      Printf(Tee::PriNormal, "Invalid temperature number--must be 1 or 2\n");
      return RC::SOFTWARE_ERROR;
   }

   UINT32 Register = (1 == TempNumber ) ? ADDR_TEMP1_READING : ADDR_TEMP2_READING;

   CK_RC_READBYTE( Register, &Value);

   if (Value > 127)
      Value -= 256;

   *Temp = Value;

   return rc;
}

