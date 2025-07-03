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

#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/deprecat.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "device/interface/i2c.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/js_gpusb.h"
#include <math.h>
#include <vector>

//------------------------------------------------------------------------------
// This class helps us keep track of the states of various digipots
//------------------------------------------------------------------------------
class DigipotState
{
   public:

      DigipotState(UINT32 Port, UINT32 Board, UINT32 PotNum, UINT32 Value)
      {
         m_Port   = Port;
         m_Board  = Board;
         m_PotNum = PotNum;
         m_Value  = Value;
      };

      bool IsMatch( UINT32 Port, UINT32 Board, UINT32 PotNum )
      {
         return (m_Port   == Port)   &&
                (m_Board  == Board)  &&
                (m_PotNum == PotNum);
      }

      void SetValue(UINT32 Value)
      {
         m_Value = Value;
      }

      UINT32 GetValue()
      {
         return m_Value;
      }

   private:

      DigipotState(); // Inhibit default constructor

      UINT32 m_Port;
      UINT32 m_Board;
      UINT32 m_PotNum;
      UINT32 m_Value;
};

typedef vector<DigipotState>::iterator DigipotIterator;

//------------------------------------------------------------------------------
// VoltShmoo shmoo board control software
//------------------------------------------------------------------------------
namespace VoltShmoo
{
   const INT32 ADC_COUNT    = 8;
   const INT32 POT_COUNT    = 4;

   const UINT32 COMMAND_CODE = 5; // Command code for DAS operations

   const UINT32 DAS_PORT  = 2;
   const UINT32 POT_PORT  = 0;

   //Range and Polarity Selection for MAX127 ADC
   enum RNG
   {
        RNG05  = 0,  // range - corresponds to 0->5V
        RNG10  = 1   // range - corresponds to 0->10V
   };
   enum POL
   {
        UNIPOL = 0,  // unipolar
        BIPOL  = 1   // bipolar
   };
   const UINT32 PD0    = 0;  // power down clock?
   const UINT32 PD1    = 1;  // standby mode (saves power no turn on delay)

   // The DS1844 digital pot has legal values from 0 to 63.
   const INT32 DEFAULT_DAC_VALUE = 32;
   const INT32 MAX_DAC_VALUE     = 63;

   RC GetVoltage( GpuSubdevice* pSubdev,
                  UINT32 ChannelNumber,
                  FLOAT64 *Value,
                  FLOAT64 *Spread = 0);

   RC SetPot( GpuSubdevice* pSubdev,
              INT32 PotNumber,
              INT32 Data);

   RC SetVoltage( GpuSubdevice* pSubdev,
                  UINT32  Sense,
                  UINT32  Channel,
                  FLOAT64 TargetValue);

   RC FindNextPotValue(FLOAT64 *Values,
                       FLOAT64 TargetValue,
                       INT32   *NextIndex,
                       bool    *Continue);

   bool FindNextPotValue(FLOAT64 *Values,
                         FLOAT64 TargetValue,
                         INT32 *NextIndex);

   RC FindCorrelations(GpuSubdevice* pSubdev);

   RC SampleVoltages( GpuSubdevice* pSubdev, FLOAT64 *Value );
   RC FindBoards(GpuSubdevice* pSubdev, bool SetFirstBoardFound);

   struct InputSignal
   {
      FLOAT64 Milwoltage;
      FLOAT64 MaxVoltage;
      FLOAT64 Coefficient[POT_COUNT];
   };

   InputSignal d_PotParameters[ADC_COUNT];

   UINT32 d_SampleCount = 50;

   UINT32 d_PortNumber  = 0;
   UINT32 d_BoardNumber = 0;

   vector<DigipotState> d_Digipots;
}
//------------------------------------------------------------------------------
// Script objects, properties, and methods.
//------------------------------------------------------------------------------

JS_CLASS(VoltShmoo);
static SObject VoltShmoo_Object
(
   "VoltShmoo",
   VoltShmooClass,
   0,
   0,
   "Class to control digipot shmoo boards"
);

//------------------------------------------------------------------------------
// GetVoltage
//------------------------------------------------------------------------------
C_(VoltShmoo_GetVoltage)
{
    static Deprecation depr("VoltShmoo.GetVoltage", "3/30/2019",
                            "Use VoltShmoo.GetDevVoltage instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if((NumArguments != 1) && (NumArguments != 2))
   {
      JS_ReportWarning(pContext, "Usage: VoltShmoo.GetVoltage(Channel, [Value, Spread])");
      JS_ReportWarning(pContext, "   or: VoltShmoo.GetVoltage(Channel)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   UINT32 Channel;

   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;

   if ( OK != pJavaScript->FromJsval(pArguments[0], &Channel ) )
   {
      JS_ReportError(pContext, "Error in VoltShmoo.GetVoltage()");
      return JS_FALSE;
   }

   RC rc;
   FLOAT64 Voltage;
   FLOAT64 Spread;

   GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();

   C_CHECK_RC(VoltShmoo::GetVoltage(pSubdev, Channel, &Voltage, &Spread));

   if( 2 == NumArguments )
   {
      if (OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))
      {
         JS_ReportError(pContext, "Error getting array in VoltShmoo.GetVoltage()");
         return JS_FALSE;
      }
      C_CHECK_RC( pJavaScript->SetElement(pReturlwals, 0, Voltage));
      C_CHECK_RC( pJavaScript->SetElement(pReturlwals, 1, Spread));
   }
   else
   {
      Printf(Tee::PriNormal,
             "ADC %d voltage: %fV  Spread: +/- %fV\n",
             Channel,
             Voltage,
             Spread);
   }

   RETURN_RC( OK );
}

STest VoltShmoo_GetVoltage
(
   VoltShmoo_Object,
   "GetVoltage",
   C_VoltShmoo_GetVoltage,
   2,
   "Read one of the voltages on the board"
);

JS_STEST_LWSTOM
(
    VoltShmoo,
    GetDevVoltage,
    3,
    "Read one of the voltages on the board"
)
{
    STEST_HEADER(2, 3, "Usage: VoltShmoo.GetDevVoltage(GpuSubdevice, Channel, [Value, Spread])");
    STEST_PRIVATE_ARG(0, JsGpuSubdevice, pJsSubdev, "GpuSubdevice");
    STEST_ARG(1, UINT32, channel);
    STEST_OPTIONAL_ARG(2, JSObject*, pRetVals, nullptr);

    RC rc;
    FLOAT64 voltage = 0.0, spread = 0.0;
    C_CHECK_RC(VoltShmoo::GetVoltage(pJsSubdev->GetGpuSubdevice(), channel, &voltage, &spread));

    if (pRetVals)
    {
       C_CHECK_RC( pJavaScript->SetElement(pRetVals, 0, voltage));
       C_CHECK_RC( pJavaScript->SetElement(pRetVals, 1, spread));
    }
    else
    {
       Printf(Tee::PriNormal,
              "ADC %d voltage: %fV  Spread: +/- %fV\n",
              channel,
              voltage,
              spread);
    }

    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
// SetVoltage
//------------------------------------------------------------------------------
C_(VoltShmoo_SetVoltage)
{
    static Deprecation depr("VoltShmoo.SetVoltage", "3/30/2019",
                            "Use VoltShmoo.SetDevVoltage instead");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 3)
   {
      JS_ReportError(pContext, "Usage: VoltShmoo.SetVoltage(Sense, Channel, Value)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   UINT32  Channel;
   UINT32  Sense;
   FLOAT64 Value;

   *pReturlwalue = JSVAL_NULL;

   if ( (OK != pJavaScript->FromJsval(pArguments[0], &Sense   )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Channel )) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value   )) )
   {
      JS_ReportError(pContext, "Error in VoltShmoo.SetVoltage()");
      return JS_FALSE;
   }

   GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();

   RETURN_RC(VoltShmoo::SetVoltage(pSubdev, Sense, Channel, Value));
}

STest VoltShmoo_SetVoltage
(
   VoltShmoo_Object,
   "SetVoltage",
   C_VoltShmoo_SetVoltage,
   2,
   "Set one of the voltages on the board"
);

JS_STEST_LWSTOM
(
    VoltShmoo,
    SetDevVoltage,
    4,
    "Set one of the voltages on the board"
)
{
    STEST_HEADER(4, 4, "Usage: VoltShmoo.SetDevVoltage(GpuSubdevice, Sense, Channel, Value)");
    STEST_PRIVATE_ARG(0, JsGpuSubdevice, pJsSub, "GpuSubdevice");
    STEST_ARG(1, UINT32, sense);
    STEST_ARG(2, UINT32, channel);
    STEST_ARG(3, FLOAT64, value);

    RETURN_RC(VoltShmoo::SetVoltage(pJsSub->GetGpuSubdevice(), sense, channel, value));
}

//------------------------------------------------------------------------------
// SetPot
//------------------------------------------------------------------------------
C_(VoltShmoo_SetPot)
{
    static Deprecation depr("VoltShmoo.SetPot", "3/30/2019",
                            "Use VoltShmoo.SetPot instead");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 2)
   {
      JS_ReportWarning(pContext, "Usage: VoltShmoo.SetPot(Channel, Value)");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   INT32 Channel;
   INT32 Value;

   *pReturlwalue = JSVAL_NULL;

   if (  (OK != pJavaScript->FromJsval(pArguments[0], &Channel )) ||
         (OK != pJavaScript->FromJsval(pArguments[1], &Value   )) )
   {
      JS_ReportError(pContext, "Error in VoltShmoo.SetPot()");
      return JS_FALSE;
   }

   GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();

   RETURN_RC(VoltShmoo::SetPot(pSubdev, Channel, Value));
}

STest VoltShmoo_SetPot
(
   VoltShmoo_Object,
   "SetPot",
   C_VoltShmoo_SetPot,
   2,
   "Program one of the potentiometers on the board"
);

JS_STEST_LWSTOM
(
    VoltShmoo,
    SetDevPot,
    3,
    "Program one of the potentiometers on the board"
)
{
    STEST_HEADER(3, 3, "Usage: VoltShmoo.SetDevPot(GpuSubdevice, Channel, Value)");
    STEST_PRIVATE_ARG(0, JsGpuSubdevice, pJsSub, "GpuSubdevice");
    STEST_ARG(1, INT32, channel);
    STEST_ARG(2, INT32, value);

    RETURN_RC(VoltShmoo::SetPot(pJsSub->GetGpuSubdevice(), channel, value));
}

//------------------------------------------------------------------------------
// FindCorrelations
//------------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    VoltShmoo,
    FindCorrelations,
    1,
    "Compute correlations between pots and ADCs"
)
{
   STEST_HEADER(0, 1, "Usage: VoltShmoo.FindCorrelations(GpuSubdevice)");
   STEST_PRIVATE_OPTIONAL_ARG(0, JsGpuSubdevice, pJsObj, "GpuSubdevice");

   GpuSubdevice* pSubdev = nullptr;
   if (!pJsObj)
   {
       static Deprecation depr("VoltShmoo.FindCorrelations", "3/30/2019",
                               "Must specify a GpuSubdevice parameter to FindCorrelations");
       if (!depr.IsAllowed(pContext))
           return JS_FALSE;

       pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
   }
   else
   {
       pSubdev = pJsObj->GetGpuSubdevice();
   }

   RETURN_RC(VoltShmoo::FindCorrelations(pSubdev));
}

//------------------------------------------------------------------------------
// FindBoards
//------------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    VoltShmoo,
    FindBoards,
    1,
    "Compute correlations between pots and ADCs"
)
{
   STEST_HEADER(0, 1, "Usage: VoltShmoo.FindBoards(GpuSubdevice)");
   STEST_PRIVATE_OPTIONAL_ARG(0, JsGpuSubdevice, pJsObj, "GpuSubdevice");

   GpuSubdevice* pSubdev = nullptr;
   if (!pJsObj)
   {
       static Deprecation depr("VoltShmoo.FindBoards", "3/30/2019",
                               "Must specify a GpuSubdevice parameter to FindBoards");
       if (!depr.IsAllowed(pContext))
           return JS_FALSE;

       pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
   }
   else
   {
       pSubdev = pJsObj->GetGpuSubdevice();
   }

   RETURN_RC(VoltShmoo::FindBoards(pSubdev, false));
}

//------------------------------------------------------------------------------
// Initialize
//------------------------------------------------------------------------------
C_(VoltShmoo_Initialize)
{
    static Deprecation depr("VoltShmoo.Initialize", "3/30/2019",
                            "Use VoltShmoo.InitializeDev instead");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   using namespace VoltShmoo;

   if(NumArguments != 2)
   {
      JS_ReportWarning(pContext, "Usage: VoltShmoo.Initialize(Port, Board)");
      JS_ReportWarning(pContext, "       Use VoltShmoo.Initialize(0,0) to auto-detect");
      return JS_FALSE;
   }

   UINT32 Port;
   UINT32 Board;
   *pReturlwalue = JSVAL_NULL;
   JavaScriptPtr pJavaScript;

   if (  (OK != pJavaScript->FromJsval(pArguments[0], &Port  )) ||
         (OK != pJavaScript->FromJsval(pArguments[1], &Board )) )
   {
      JS_ReportError(pContext, "Error in VoltShmoo.Initialize()");
      return JS_FALSE;
   }

   if((0 == Port) && (0 != Board))
   {
      Printf(Tee::PriNormal, "Invalid port number\n");
      return JS_FALSE;
   }

   GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();

   if((0 == Port) && (0 == Board))
   {
      RETURN_RC(FindBoards(pSubdev, true));
   }
   else
   {
      d_PortNumber  = Port;
      d_BoardNumber = Board;

      FLOAT64 Dummy[8];

      if(OK == SampleVoltages( pSubdev, Dummy ))
      {
         RETURN_RC(OK);
      }

      d_PortNumber  = 0;
      d_BoardNumber = 0;
      RETURN_RC( RC::WAS_NOT_INITIALIZED );
   }
}

STest VoltShmoo_Initialize
(
   VoltShmoo_Object,
   "Initialize",
   C_VoltShmoo_Initialize,
   2,
   "Initialize a shmoo board"
);

JS_STEST_LWSTOM
(
    VoltShmoo,
    InitializeDev,
    3,
    "Initialize a shmoo board"
)
{
    STEST_HEADER(1, 3, "Usage: VoltShmoo.Initialize(GpuSubdevice, Port, Board)\n"
                       "   Use VoltShmoo.Initialize(GpuSubdevice,) to auto-detect");
    STEST_PRIVATE_ARG(0, JsGpuSubdevice, pJsSub, "GpuSubdevice");
    STEST_OPTIONAL_ARG(1, UINT32, port, 0);
    STEST_OPTIONAL_ARG(2, UINT32, board, 0);

    if (port == 0 && board != 0)
    {
        JS_ReportError(pContext, "Invalid port number\n");
        return JS_FALSE;
    }

    if (port == 0 && board == 0)
    {
       RETURN_RC(VoltShmoo::FindBoards(pJsSub->GetGpuSubdevice(), true));
    }
    else
    {
       VoltShmoo::d_PortNumber  = port;
       VoltShmoo::d_BoardNumber = board;

       FLOAT64 dummy[8];

       if (RC::OK != VoltShmoo::SampleVoltages(pJsSub->GetGpuSubdevice(), dummy))
       {
           VoltShmoo::d_PortNumber  = 0;
           VoltShmoo::d_BoardNumber = 0;
           RETURN_RC(RC::WAS_NOT_INITIALIZED);
       }
    }

    RETURN_RC(RC::OK);
}

//------------------------------------------------------------------------------
// SetVoltage
//------------------------------------------------------------------------------
static SProperty VoltShmoo_SampleCount
(
   VoltShmoo_Object,
   "SampleCount",
   0,
   VoltShmoo::d_SampleCount,
   0,
   0,
   0,
   "Number of samples to take when reading the voltage."
);

//-----------------------------------------------------------------------------
// FindCorrelations(): See which supplies correlate with which pots
//-----------------------------------------------------------------------------
RC VoltShmoo::FindCorrelations(GpuSubdevice* pSubdev)
{
   INT32 i,j;
   RC rc = OK;

   FLOAT64 CorrelationArray[ ADC_COUNT * POT_COUNT ];

   for( i=0 ; i< POT_COUNT ; i++ )
   {
      FLOAT64 OneLess[8];
      FLOAT64 TwoLess[8];
      FLOAT64 OneMore[8];
      FLOAT64 TwoMore[8];

      CHECK_RC(SetPot(pSubdev, i, DEFAULT_DAC_VALUE - 1));
      CHECK_RC(SampleVoltages(pSubdev, OneLess));

      CHECK_RC(SetPot(pSubdev, i, DEFAULT_DAC_VALUE - 2));
      CHECK_RC(SampleVoltages(pSubdev, TwoLess));

      CHECK_RC(SetPot(pSubdev, i, DEFAULT_DAC_VALUE + 1));
      CHECK_RC(SampleVoltages(pSubdev, OneMore));

      CHECK_RC(SetPot(pSubdev, i, DEFAULT_DAC_VALUE + 2));
      CHECK_RC(SampleVoltages(pSubdev, TwoMore));

      CHECK_RC(SetPot(pSubdev, i, DEFAULT_DAC_VALUE));

      for( j=0 ; j<ADC_COUNT ; j++)
      {
         FLOAT64 Correlation = 1000 * ( OneMore[j] +
                                        TwoMore[j] -
                                        OneLess[j] -
                                        TwoLess[j] ) / 6;

         CorrelationArray[ (POT_COUNT * j) + i ] = Correlation;
      }
   }

   Printf(Tee::PriNormal,"Voltage correlations in mV/tick\n");

   Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_HIGHLIGHT, "      ");

   for( i=0 ; i< POT_COUNT ; i++)
   {
      Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_HIGHLIGHT,
             "    POT %1d", i);
   }
   Printf(Tee::PriNormal,"\n");

   for( i=0 ; i < ADC_COUNT ; i++ )
   {
      Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_HIGHLIGHT,
             "ADC %1d ", i);

      for( j=0 ; j < POT_COUNT ; j++)
      {
         Printf(Tee::PriNormal," ");
         FLOAT64 Value = CorrelationArray[ (POT_COUNT * i) + j ];
         Tee::ScreenPrintState sps = Tee::SPS_NORMAL;
         if( fabs(Value) >= 1.00 )
         {
            if( fabs(Value) >= 2.00 )
               sps = Tee::SPS_PASS;
            else
               sps = Tee::SPS_FAIL;
         }
         Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), sps, "%8.2f", Value );
      }
      Printf(Tee::PriNormal,"\n");
   }

   return OK;
}

//------------------------------------------------------------------------------
// SetPot(): Program a particular potentiometer to a particular value
//------------------------------------------------------------------------------
RC VoltShmoo::SetPot(GpuSubdevice* pSubdev, INT32 PotNumber, INT32 Value)
{
   RC rc;

   if( 0 == d_PortNumber )
      return RC::WAS_NOT_INITIALIZED;

   DigipotIterator di;
   bool FoundPot = false;

   // We can only use two address bits with the digital potentiometers.
   if( PotNumber > 3 )
   {
      Printf(Tee::PriNormal, "SetPot(): Number %d invalid.\n", PotNumber);
      return RC::BAD_PARAMETER;
   }

   // Search through our records of which pots have been programmed to which
   // values.  If we've touched this pot before, let's update its value
   for( di = d_Digipots.begin(); di != d_Digipots.end(); di++)
   {
      if( di->IsMatch( d_PortNumber, d_BoardNumber, PotNumber ))
      {
         di->SetValue(Value);
         FoundPot = true;
         break;
      }
   }

   // If we haven't touched this pot before, create a new item in the list.
   if( !FoundPot )
   {
      d_Digipots.insert( d_Digipots.end(),
                         DigipotState( d_PortNumber,
                                       d_BoardNumber,
                                       PotNumber,
                                       Value ));
   }

   if(Value > MAX_DAC_VALUE)
   {
      Printf(Tee::PriWarn, "SetPot(): Value %x exceeds max value of %x\n",
             Value, MAX_DAC_VALUE);
   }

   I2c::Dev i2cDev = pSubdev->GetInterface<I2c>()->I2cDev();
   CHECK_RC(i2cDev.SetPortBaseOne(d_PortNumber));
   CHECK_RC(i2cDev.SetAddr((COMMAND_CODE << 4) | (d_BoardNumber + POT_PORT)));
   CHECK_RC(i2cDev.SetOffsetLength(0));
   CHECK_RC(i2cDev.Write(0, Value + (PotNumber << 6)));

   return rc;
}

//------------------------------------------------------------------------------
// SampleVoltages(): Sample all eight ADCs on this board
//------------------------------------------------------------------------------
RC VoltShmoo::SampleVoltages(GpuSubdevice* pSubdev, FLOAT64 *Value)
{
   INT32 i;
   RC rc = OK;

   for ( i=0 ; i<ADC_COUNT ; i++ )
   {
      CHECK_RC( GetVoltage( pSubdev, i, Value+i) );
   }

   return OK;
}

//------------------------------------------------------------------------------
// FindBoards(): Search I2C for shmoo boards
//------------------------------------------------------------------------------
RC VoltShmoo::FindBoards(GpuSubdevice* pSubdev, bool SetFirstBoardFound)
{
   UINT32 OrigSample = d_SampleCount;
   UINT32 OrigPort   = d_PortNumber;
   UINT32 OrigBoard  = d_BoardNumber;

   UINT32 Port, Board;

   for( Port=1 ; Port<=3 ; Port++ )
   {
      for( Board=0 ; Board<=16 ; Board +=4 )
      {
         d_SampleCount = 1;
         d_PortNumber  = Port;
         d_BoardNumber = Board;

         FLOAT64 Dummy[8];

         if(OK == SampleVoltages( pSubdev, Dummy ))
         {
            // If this flag is set, we want to set this board.  Just return
            // without restoring the original state
            if(SetFirstBoardFound)
            {
               d_SampleCount = OrigSample;
               return OK;
            }

            Printf(Tee::PriNormal, "Found a board at port=%d, board=%d\n",
                   Port,
                   Board);
         }
      }
   }

   d_SampleCount = OrigSample;
   d_PortNumber  = OrigPort;
   d_BoardNumber = OrigBoard;

   // If this flag is set, we expected to find a board, but didn't.
   if(SetFirstBoardFound)
   {
      return RC::WAS_NOT_INITIALIZED;
   }

   return OK;
}

//------------------------------------------------------------------------------
// GetVoltage(): Read one of the analog-to-digital colwerters
//------------------------------------------------------------------------------
RC VoltShmoo::GetVoltage(GpuSubdevice* pSubdev, UINT32 Channel, FLOAT64 *Value, FLOAT64 *Spread)
{
   if( 0 == d_PortNumber )
      return RC::WAS_NOT_INITIALIZED;

   FLOAT64 Sum  =    0.0;
   FLOAT64 Low  = 1000.0;
   FLOAT64 High =    0.0;

   UINT32 i;
   RC rc = OK;

   MASSERT( Value );

   // We can only use three address bits with the MAX127 ADC
   if( Channel > 7 )
   {
      Printf(Tee::PriNormal, "GetVoltage(): Channel %d invalid.\n", Channel);
      return RC::BAD_PARAMETER;
   }

   CHECK_RC(JavaScriptPtr()->GetProperty( VoltShmoo_Object,
                                          VoltShmoo_SampleCount,
                                          &d_SampleCount));

   for (i=0; i<d_SampleCount; i++)
   {
      const FLOAT64 VoltageScalingFactor = 5.0 / 4095.0;

      UINT32 AddressByte = (COMMAND_CODE << 4) | (d_BoardNumber + DAS_PORT);

      UINT32 ControlByte = 0x80 | (Channel << 4)
                                | (RNG05   << 3)
                                | (UNIPOL  << 2)
                                | (PD1     << 1)
                                | PD0;

      UINT32 DataIn;

      I2c::Dev i2cDev = pSubdev->GetInterface<I2c>()->I2cDev();
      CHECK_RC(i2cDev.SetPortBaseOne(d_PortNumber));
      CHECK_RC(i2cDev.SetAddr(AddressByte));
      CHECK_RC(i2cDev.SetOffsetLength(0));
      CHECK_RC(i2cDev.SetMessageLength(1));
      CHECK_RC(i2cDev.Write(0, ControlByte));
      CHECK_RC(i2cDev.SetMessageLength(2));
      CHECK_RC(i2cDev.Read(0, &DataIn));

      // Recombine the 12-bit value
      UINT32 UnscaledVoltage = (DataIn >> 4);

      FLOAT64 ScaledVoltage = FLOAT64(UnscaledVoltage) * VoltageScalingFactor;

      Sum += ScaledVoltage;

      if(ScaledVoltage > High) High = ScaledVoltage;
      if(ScaledVoltage < Low)  Low  = ScaledVoltage;
   }

   *Value  = Sum / d_SampleCount;

   if(Spread)
      *Spread = High - Low;

   return rc;
}

//------------------------------------------------------------------------------
// FindNextPotValue(): Used by SetVoltage() to pick the next pot value to test
//------------------------------------------------------------------------------
RC VoltShmoo::FindNextPotValue(FLOAT64 *Values,
                               FLOAT64 TargetValue,
                               INT32   *NextIndex,
                               bool    *Continue)
{
   FLOAT64 LowestDelta  = 100000.0;
   FLOAT64 LowestValue  = 100000.0;
   FLOAT64 HighestValue =      0.0;

   INT32   BestIndex    = 0;
   INT32   HighestIndex = 0;
   INT32   LowestIndex  = 0;

   bool FoundHigher = false;
   bool FoundLower  = false;

   // Print out the array of known voltages...for debugging
   Printf(Tee::PriLow,"Values array:\n");

   INT32 i;
   for(i=0;i<=MAX_DAC_VALUE;i+=8)
   {
      Printf(Tee::PriLow,"   ");
      INT32 j;
      for(j=i;j<i+8;j++)
         Printf(Tee::PriLow,"%3.5f  ",Values[j]);

      Printf(Tee::PriLow,"\n");
   }

   for(i=0; i<=MAX_DAC_VALUE ; i++)
   {
      // Values[] is a 64-element array.  Only elements that have actually
      // been tested should have a nonzero value in them. Compare for < 0.001
      // instead of =0.0 to avoid problems with floating-point accuracy.
      if( 0.001 < Values[i] )
      {
         // Keep track of the closest value to the target frequency.
         if( fabs(Values[i] - TargetValue) < LowestDelta)
         {
            LowestDelta = fabs( Values[i] - TargetValue );
            BestIndex   = i;
         }

         // We need to keep track of whether there are values both higher and
         // lower in the list of values.
         if( Values[i] > TargetValue )
         {
            FoundHigher = true;
         }

         if( Values[i] < TargetValue )
         {
            FoundLower = true;
         }

         // Keep track of the lowest and highest values.  This will later
         // be used to callwlate the gradient (mV per potentiometer tick)
         if( Values[i] < LowestValue )
         {
            LowestValue = Values[i];
            LowestIndex = i;
         }

         if( Values[i] >= HighestValue )
         {
            HighestValue = Values[i];
            HighestIndex = i;
         }
      }
   }

   // If we found values that were both lower and higher than the target value,
   // we're done!  Return the best one.
   if( FoundHigher && FoundLower )
   {
      Printf(Tee::PriLow,"We're done index = %d\n", BestIndex);
      *NextIndex = BestIndex;
      *Continue  = false;
      return OK;
   }

   // Something is wrong.  When this function is called, there should be at
   // least one nonzero element in the Values[] array.  This check can probably
   // be removed in the future; it should never happen.
   if( !FoundHigher && !FoundLower )
   {
      Printf(Tee::PriNormal,"Internal error determining next pot value\n");
      *Continue = false;
      return RC::SOFTWARE_ERROR;
   }

   FLOAT64 Gradient = 0.0;

   // If HighestIndex == LowestIndex, it means there is only one element in
   // the array.  In this case, we can't callwlate the gradient.  So, let's
   // just make sure that the next value we pick is a legal value for the
   // digital potentiometer.
   if( HighestIndex == LowestIndex )
   {
      if(MAX_DAC_VALUE == HighestIndex)
      {
         *NextIndex = MAX_DAC_VALUE -1;
         *Continue  = true;
         return OK;
      }

      if(0 == HighestIndex)
      {
         *NextIndex = 1;
         *Continue  = true;
         return OK;
      }
   }
   else
   {
      // Callwlate the gradient (volts per potentiometer tick)
      Gradient = (HighestValue - LowestValue) / FLOAT64(HighestIndex - LowestIndex);
   }

   bool GradientPositive = (Gradient > 0.00) ? true : false;

   if(  (  GradientPositive && FoundLower  ) ||
        ( !GradientPositive && FoundHigher ) )
   {
      // We need to pick the next highest untested pot value.  To do this,
      // start at the top of the array and go down, looking for the first
      // nonzero (untested) potentiometer value.
      for( i=MAX_DAC_VALUE ; i>=0 ; i--)
      {
         if( 0.0001 < Values[i] )
         {
            *NextIndex = i+1;
            break;
         }
      }
   }
   else
   {
      // We need to pick the next lowest untested pot value.  To do this,
      // start at the bottom of the array and go up, looking for the first
      // nonzero (untested) potentiometer value.
      for(i=0; i<= MAX_DAC_VALUE ; i++)
      {
         if( 0.0001 < Values[i] )
         {
            *NextIndex = i-1;
            break;
         }
      }
   }

   // Make sure that the next value we picked isn't out of range.  If it is,
   // then there is no valid pot value to match the user's request.  Bail.
   if((*NextIndex > MAX_DAC_VALUE) || (*NextIndex < 0))
   {
      Printf(Tee::PriNormal,"Voltage is out of range\n");
      *Continue = false;
      return RC::ILWALID_INPUT;
   }

   // If we got here, the computed next index is OK.  Return normally.
   *Continue = true;
   return OK;
}

//------------------------------------------------------------------------------
// SetVoltage(): Set a particular voltage given a pot # and feedback ADC #
//------------------------------------------------------------------------------
RC VoltShmoo::SetVoltage( GpuSubdevice* pSubdev, UINT32 Sense, UINT32 Channel, FLOAT64 TargetValue)
{
   if( 0 == d_PortNumber )
      return RC::WAS_NOT_INITIALIZED;

   RC rc = OK;
   FLOAT64 Spread = 0;
   INT32   LwrrentPotSetting = DEFAULT_DAC_VALUE;

   DigipotIterator di;
   bool FoundPot = false;

   // Search through our records of which pots have been programmed to which
   // values.  If we've touched this pot before, recall its value from the list.
   for( di = d_Digipots.begin(); di != d_Digipots.end(); di++)
   {
      if( di->IsMatch( d_PortNumber, d_BoardNumber, Channel ))
      {
         LwrrentPotSetting = di->GetValue();
         FoundPot = true;
         break;
      }
   }

   // If we haven't touched it before, let's set it to a known value.
   if( !FoundPot )
      CHECK_RC( SetPot( pSubdev, Channel, DEFAULT_DAC_VALUE ));

   FLOAT64 VoltageValues[MAX_DAC_VALUE + 1] = {0.0};

   bool Continue = true;
   while( Continue )
   {
      CHECK_RC( SetPot( pSubdev, Channel, LwrrentPotSetting  ));

      CHECK_RC( GetVoltage( pSubdev, Sense,
                            VoltageValues + LwrrentPotSetting,
                            &Spread ) );

      CHECK_RC( FindNextPotValue(VoltageValues,
                                 TargetValue,
                                 &LwrrentPotSetting,
                                 &Continue) );

   }

   // Set the final value
   CHECK_RC( SetPot( pSubdev, Channel, LwrrentPotSetting  ));

   Printf(Tee::PriLow,
          "Final pot value %d (%fV)\n",
          LwrrentPotSetting,
          VoltageValues[LwrrentPotSetting]);

   return rc;
}
