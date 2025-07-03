/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2008-2011,2016,2019 by LWPU Corporation.  All rights reserved.  All information
* contained herein is proprietary and confidential to LWPU Corporation.  Any
* use, reproduction, or disclosure without the written permission of LWPU
* Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#include "dispstate.h"
#include "fermi/gf100/dev_disp.h"
#include "class/cl0073.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0073/ctrl0073system.h"
#include "ctrl/ctrl5070.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "core/include/memcheck.h"

DispState:: DispState()
{
    m_pSubdev = nullptr;
}

void DispState::BindGpuSubdevice(GpuSubdevice* pSubdev)
{
    m_pSubdev = pSubdev;
}

//-----------------------------------------------------
// checkDispMethodFifo()
// Dump out method fifo cmd/data and check for
// errors/violations
//
//-----------------------------------------------------
RC DispState:: checkDispMethodFifo( void )
{
    UINT32  status = LW_OK;
    UINT32  data32[LW_PDISP_DSI_FIFO_CMD__SIZE_1];
    UINT32  i;

    Printf(Tee::PriLow,"\n\tlw: Display Method Fifo status...\n");

    //dump out method fifo data/offsets
    Printf(Tee::PriLow,"lw: LW_PDISP_DSI_FIFO_DATA(0-39):\n");

    for (i = 0; i < LW_PDISP_DSI_FIFO_DATA__SIZE_1; i++)
    {
        Printf(Tee::PriLow,"\t 0x%08x %s", REG_RD32(LW_PDISP_DSI_FIFO_DATA(i)),
            ((i+1) % 4 == 0) ? "\n":"  ");
    }

    Printf(Tee::PriLow,"\n\tlw: LW_PDISP_DSI_FIFO_CMD_METHOD_OFS(0-39):\n");

    for (i = 0; i < LW_PDISP_DSI_FIFO_CMD__SIZE_1; i++)
    {
        data32[i] = REG_RD32(LW_PDISP_DSI_FIFO_CMD(i));

        Printf(Tee::PriLow,"\t 0x%04x %s", DRF_VAL(_PDISP, _DSI_FIFO_CMD, _METHOD_OFS,
            data32[i]), ((i+1) % 4 == 0) ? "\n":"  ");
    }

    //check for errors
    for (i = 0; i < LW_PDISP_DSI_FIFO_CMD__SIZE_1; i++)
    {
        if (DRF_VAL(_PDISP, _DSI_FIFO_CMD, _ILWALID_OPCODE, data32[i]))
        {
            Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_DSI_FIFO_CMD(%d)_ILWALID_OPCODE\n", i);
            //addUnitErr("\t + LW_PDISP_DSI_FIFO_CMD(%d)_ILWALID_OPCODE\n", i );
            status = LW_ERROR;
        }

        if (DRF_VAL(_PDISP, _DSI_FIFO_CMD, _LIMIT_VIOLATION , data32[i]))
        {
            Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_DSI_FIFO_CMD(%d)_LIMIT_VIOLATION\n", i);
            //addUnitErr("\t + LW_PDISP_DSI_FIFO_CMD(%d)_LIMIT_VIOLATION\n", i );

            // TBD here implement Mmu analyser Code ...
            // Limit Chekc Error Feature
            // Find out what he is doing here and do the check.
            // pMmu[indexGpu].mmuAnalyzeLimitError();
            // I haven't written code here i have to write analyze code here
            status = LW_ERROR;
        }

    }

    return status;
}

 //-----------------------------------------------------
 // checkDispDMAState()
 //
 //-----------------------------------------------------
RC DispState::checkDispDMAState( void )
 {
     UINT32  status = LW_OK;
     UINT32  get;
     UINT32  put;
     UINT32  i;

     Printf(Tee::PriLow,"\n\tlw: Checking DMA status...\n");

     for (i = 0; i < LW_UDISP_DSI_PUT__SIZE_1; i++)
     {
         put = REG_RD32(LW_UDISP_DSI_PUT(i));
         get = REG_RD32(LW_UDISP_DSI_GET(i));

         Printf(Tee::PriLow,"lw: LW_UDISP_DSI_GET(%d):   0x%08x\n", i, get);
         Printf(Tee::PriLow,"lw: LW_UDISP_DSI_PUT(%d):   0x%08x\n", i, put);
         Printf(Tee::PriLow,"lw: + LW_UDISP_DSI_PUT_POINTER_STATUS_%s\n",
             DRF_VAL(_UDISP, _DSI_PUT, _POINTER_STATUS, put) ? "LOCKED":"WRITABLE");

         if ( DRF_VAL( _UDISP, _DSI_PUT, _POINTER, put) !=
             DRF_VAL( _UDISP, _DSI_GET, _POINTER, get))
         {
             Printf(Tee::PriHigh,"lw: LW_UDISP_DSI_PUT(%d):   0x%08x != LW_UDISP_DSI_GET(%d):"
                 "   0x%08x\n", i, put, i, get);
             //addUnitErr("\t LW_UDISP_DSI_PUT(%d):   0x%08x != LW_UDISP_DSI_GET(%d):"
             //    "   0x%08x\n", i, put, i, get);
             status = LW_ERROR;
         }

     }

     return status;
 }

//-----------------------------------------------------
// checkDispChnCtlCore()
//
//-----------------------------------------------------
RC DispState::checkDispChnCtlCore( void )
{
    UINT32  status = LW_OK;
    UINT32  data32;
    UINT32  fifoReg;
    UINT32  fifoRd;
    UINT32  fifoWr;
    UINT32  i;
    bool    isIdle = false;

    Printf(Tee::PriLow,"\n\tlw: Checking PDISP_CHNCTL_CORE(i)...\n");

    for (i = 0; i < LW_PDISP_CHNCTL_CORE__SIZE_1; i++)
    {
        data32 = REG_RD32(LW_PDISP_CHNCTL_CORE(i));

        Printf(Tee::PriLow,"lw: LW_PDISP_CHNCTL_CORE(%d):               0x%08x\n", i, data32);

        //return if not connected or allocated
        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _ALLOCATION, data32))
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_ALLOCATION_ALLOCATE\n");
        }
        else
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_ALLOCATION_DEALLOCATE\n");
            return status;
        }

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _CONNECTION, data32))
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_CONNECTION_CONNECT\n");
        }
        else
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_CONNECTION_DISCONNECT\n");
            return status;
        }

        switch (DRF_VAL( _PDISP, _CHNCTL_CORE, _TYPE, data32))
        {
        case LW_PDISP_CHNCTL_CORE_TYPE_UNUSED:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_TYPE_UNUSED\n");
            break;

        case LW_PDISP_CHNCTL_CORE_TYPE_DMA:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_TYPE_DMA\n");
            break;

        default:
            Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_CORE_TYPE:  0x%02x\n",
                DRF_VAL( _PDISP, _CHNCTL_CORE, _TYPE, data32));
            //addUnitErr("\t Unknown LW_PDISP_CHNCTL_CORE_TYPE:  0x%02x\n",
             //   DRF_VAL( _PDISP, _CHNCTL_CORE, _TYPE, data32));
            status = LW_ERROR;
            break;
        }

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _PUTPTR_WRITE, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_PUTPTR_WRITE_ENABLE\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_PUTPTR_WRITE_DISABLE\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _FCODE, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_FCODE_ENABLE\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_FCODE_DISABLE\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _IGNORE_PI, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_IGNORE_PI_ENABLE\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_IGNORE_PI_DISABLE\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _SKIP_NOTIF, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_SKIP_NOTIF_ENABLE\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_SKIP_NOTIF_DISABLE\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _IGNORE_INTERLOCK, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_IGNORE_INTERLOCK_ENABLE\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_IGNORE_INTERLOCK_DISABLE\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _ERRCHECK_WHEN_DISCONNECTED, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_ERRCHECK_WHEN_DISCONNECTED_ENABLE\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_ERRCHECK_WHEN_DISCONNECTED_DISABLE\n");

        switch ( DRF_VAL( _PDISP, _CHNCTL_CORE, _TRASH_MODE, data32))
        {
        case LW_PDISP_CHNCTL_CORE_TRASH_MODE_DISABLE:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_TRASH_MODE_DISABLE\n");
            break;

        case LW_PDISP_CHNCTL_CORE_TRASH_MODE_TRASH_ONLY:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_TRASH_MODE_TRASH_ONLY\n");
            break;

        case LW_PDISP_CHNCTL_CORE_TRASH_MODE_TRASH_AND_ABORT:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_TRASH_MODE_TRASH_AND_ABORT\n");
            break;

        default:
            Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_CORE_TRASH_MODE:  0x%02x\n",
                DRF_VAL( _PDISP, _CHNCTL_CORE, _TRASH_MODE, data32));
            //addUnitErr("\t Unknown LW_PDISP_CHNCTL_CORE_TRASH_MODE:  0x%02x\n",
             //   DRF_VAL( _PDISP, _CHNCTL_CORE, _TRASH_MODE, data32));
            status = LW_ERROR;
            break;
        }

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _INTR_DURING_SHTDWN, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_INTR_DURING_SHTDWN_ENABLE\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_INTR_DURING_SHTDWN_DISABLE\n");

        switch (DRF_VAL( _PDISP, _CHNCTL_CORE, _STATE, data32))
        {
        case LW_PDISP_CHNCTL_CORE_STATE_DEALLOC:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_{INIT,DEALLOC}\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_DEALLOC_LIMBO:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_DEALLOC_LIMBO\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_LIMBO1:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_LIMBO1\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_LIMBO2:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_LIMBO2\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_FCODEINIT:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_FCODEINIT\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_FCODE:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_FCODE\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_VBIOSINIT:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_VBIOSINIT\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_VBIOSOPER:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_VBIOSOPER\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_UNCONNECTED:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_UNCONNECTED\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_INITIALIZE:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_INITIALIZE\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_IDLE:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_IDLE\n");
            isIdle = true;
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_WRTIDLE:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_WRTIDLE\n");
            isIdle = true;
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_EMPTY:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_EMPTY\n");
            isIdle = true;
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_FLUSHED:
            Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_CORE_STATE_FLUSHED\n");
            //addUnitErr("\t LW_PDISP_CHNCTL_CORE_STATE_FLUSHED\n");
            status = LW_ERROR;
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_BUSY:
            Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_CORE_STATE_BUSY\n");
            //addUnitErr("\t LW_PDISP_CHNCTL_CORE_STATE_BUSY\n");
            status = LW_ERROR;
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_SHUTDOWN1:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_SHUTDOWN1\n");
            break;

        case LW_PDISP_CHNCTL_CORE_STATE_SHUTDOWN2:
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATE_SHUTDOWN2\n");
            break;

        default:
            Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_CORE_STATE:  0x%02x\n",
                DRF_VAL( _PDISP, _CHNCTL_CORE, _STATE, data32));
            //addUnitErr("\t Unknown LW_PDISP_CHNCTL_CORE_STATE:  0x%02x\n",
             //   DRF_VAL( _PDISP, _CHNCTL_CORE, _STATE, data32));
            status = LW_ERROR;
            break;
        }

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _CORE_DRIVER_RESUME, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_CORE_DRIVER_RESUME_PENDING\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_CORE_DRIVER_RESUME_DONE\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _SAT_DRIVER_RESUME, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_SAT_DRIVER_RESUME_PENDING\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_SAT_DRIVER_RESUME_DONE\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _GOTO_LIMBO2, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_GOTO_LIMBO2_PENDING\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_GOTO_LIMBO2_DONE\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _SHOW_DRIVER_STATE, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_SHOW_DRIVER_STATE_AT_UPD\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_SHOW_DRIVER_STATE_NOW\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _STATUS_METHOD_FIFO, data32))
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_METHOD_FIFO_NOTEMPTY\n");
            //addUnitErr("\t LW_PDISP_CHNCTL_CORE_STATUS_METHOD_FIFO_NOTEMPTY\n");

            //check if method fifo RD/WR ptr match
            fifoReg = REG_RD32(LW_PDISP_DSI_FIFO_CORE);
            fifoRd = DRF_VAL( _PDISP, _DSI_FIFO_CORE, _RD_PTR, fifoReg);
            fifoWr = DRF_VAL( _PDISP, _DSI_FIFO_CORE, _WR_PTR, fifoReg);

            if ( fifoRd != fifoWr )
            {
                Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_FIFO_CORE_RD_PTR:   0x%02x  !=  "
                    "LW_PDISP_DSI_FIFO_CORE_WR_PTR:  0x%02x\n", fifoRd, fifoWr);
                //addUnitErr("\t LW_PDISP_DSI_FIFO_CORE_RD_PTR:   0x%02x  !=  "
                 //   "LW_PDISP_DSI_FIFO_CORE_WR_PTR:  0x%02x\n", fifoRd, fifoWr);
            }

            //dump out method fifo cmd/data and check for violations
            if (checkDispMethodFifo() == LW_ERROR)
            {
                Printf(Tee::PriHigh,"lw: Display method fifo is in invalid state\n");
                //addUnitErr("\t Display method fifo is in invalid state\n");
            }

            status = LW_ERROR;
        }
        else
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_METHOD_FIFO_EMPTY\n");
        }

        //
        //read only when in EMPTY/WRIDLE/IDLE state or when incoming methods
        //have been stopped or paused.
        //
        if (isIdle)
        {
            if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _STATUS_READ_PENDING, data32))
            {
                Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_READ_PENDING_YES\n");
            }
            else
            {
                Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_READ_PENDING_NO\n");
            }
        }
        else
        {
            Printf(Tee::PriLow,"lw: + STATE not idle, unable to check STATUS_READ_PENDING\n");
        }

        //#define NOTIF_WRITE_PENDING_NO 0x00000001
        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _STATUS_NOTIF_WRITE_PENDING, data32))
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_NOTIF_WRITE_PENDING_NO\n");
        }
        else
        {
            Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_NOTIF_WRITE_PENDING_YES\n");
            //addUnitErr("\t LW_PDISP_CHNCTL_CORE_STATUS_NOTIF_WRITE_PENDING_YES\n");
            status = LW_ERROR;
        }

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _STATUS_WILL_GO_IDLE, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_WILL_GO_IDLE_YES\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_WILL_GO_IDLE_NO\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _STATUS_WILL_GO_WRTIDLE, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_WILL_GO_WRTIDLE_YES\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_WILL_GO_WRTIDLE_NO\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _STATUS_QUIESCENT, data32))
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_QUIESCENT_YES\n");
        else
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_QUIESCENT_NO\n");

        if ( DRF_VAL( _PDISP, _CHNCTL_CORE, _STATUS_METHOD_EXEC, data32))
        {
            Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_METHOD_EXEC_RUNNING\n");
            //addUnitErr("\t LW_PDISP_CHNCTL_CORE_STATUS_METHOD_EXEC_RUNNING\n");
            status = LW_ERROR;
        }
        else
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_CORE_STATUS_METHOD_EXEC_IDLE\n");
        }

    }

    return status;
}

 //-----------------------------------------------------
 // checkDispChnCtlBase()
 //
 //-----------------------------------------------------
RC DispState::checkDispChnCtlBase( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  fifoReg;
     UINT32  fifoRd;
     UINT32  fifoWr;
     UINT32  i;
     bool    isIdle = false;

     Printf(Tee::PriLow,"\n\tlw: Checking PDISP_CHNCTL_BASE(i)...\n");

     for (i = 0; i < LW_PDISP_CHNCTL_BASE__SIZE_1; i++)
     {
         data32 = REG_RD32(LW_PDISP_CHNCTL_BASE(i));

         Printf(Tee::PriLow,"lw: LW_PDISP_CHNCTL_BASE(%d):               0x%08x\n", i, data32);

         //return if not connected or allocated
         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _ALLOCATION, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_ALLOCATION_ALLOCATE\n");
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_ALLOCATION_DEALLOCATE\n");
             return status;
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _CONNECTION, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_CONNECTION_CONNECT\n");
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_CONNECTION_DISCONNECT\n");
             return status;
         }

         switch (DRF_VAL( _PDISP, _CHNCTL_BASE, _TYPE, data32))
         {
         case LW_PDISP_CHNCTL_BASE_TYPE_UNUSED:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_TYPE_UNUSED\n");
             break;

         case LW_PDISP_CHNCTL_BASE_TYPE_DMA:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_TYPE_DMA\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_BASE_TYPE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_BASE, _TYPE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_BASE_TYPE:  0x%02x\n",
              //   DRF_VAL( _PDISP, _CHNCTL_BASE, _TYPE, data32));
             status = LW_ERROR;
             break;
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _PUTPTR_WRITE, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_PUTPTR_WRITE_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_PUTPTR_WRITE_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _IGNORE_PI, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_IGNORE_PI_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_IGNORE_PI_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _SKIP_NOTIF, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_SKIP_NOTIF_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_SKIP_NOTIF_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _SKIP_SEMA, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_SKIP_SEMA_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_SKIP_SEMA_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _IGNORE_INTERLOCK, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_IGNORE_INTERLOCK_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_IGNORE_INTERLOCK_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _IGNORE_FLIPLOCK, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_IGNORE_FLIPLOCK_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_IGNORE_FLIPLOCK_DISABLE\n");

         switch ( DRF_VAL( _PDISP, _CHNCTL_BASE, _TRASH_MODE, data32))
         {
         case LW_PDISP_CHNCTL_BASE_TRASH_MODE_DISABLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_TRASH_MODE_DISABLE\n");
             break;

         case LW_PDISP_CHNCTL_BASE_TRASH_MODE_TRASH_ONLY:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_TRASH_MODE_TRASH_ONLY\n");
             break;

         case LW_PDISP_CHNCTL_BASE_TRASH_MODE_TRASH_AND_ABORT:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_TRASH_MODE_TRASH_AND_ABORT\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_BASE_TRASH_MODE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_BASE, _TRASH_MODE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_BASE_TRASH_MODE:  0x%02x\n",
              //   DRF_VAL( _PDISP, _CHNCTL_BASE, _TRASH_MODE, data32));
             status = LW_ERROR;
             break;
         }

         switch (DRF_VAL( _PDISP, _CHNCTL_BASE, _STATE, data32))
         {
         case LW_PDISP_CHNCTL_BASE_STATE_DEALLOC:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATE_{INIT,DEALLOC}\n");
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_UNCONNECTED:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATE_UNCONNECTED\n");
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_INITIALIZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATE_INITIALIZE\n");
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_IDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATE_IDLE\n");
             isIdle = true;
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_WRTIDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATE_WRTIDLE\n");
             isIdle = true;
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_EMPTY:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATE_EMPTY\n");
             isIdle = true;
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_FLUSHED:
             Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_BASE_STATE_FLUSHED\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_BASE_STATE_FLUSHED\n");
             status = LW_ERROR;
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_BUSY:
             Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_BASE_STATE_BUSY\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_BASE_STATE_BUSY\n");
             status = LW_ERROR;
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_SHUTDOWN1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATE_SHUTDOWN1\n");
             break;

         case LW_PDISP_CHNCTL_BASE_STATE_SHUTDOWN2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATE_SHUTDOWN2\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_BASE_STATE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_BASE, _STATE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_BASE_STATE:  0x%02x\n",
             //    DRF_VAL( _PDISP, _CHNCTL_BASE, _STATE, data32));
             status = LW_ERROR;
             break;
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _STATUS_METHOD_FIFO, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_METHOD_FIFO_NOTEMPTY\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_BASE_STATUS_METHOD_FIFO_NOTEMPTY\n");

             //check if method fifo RD/WR ptr match
             fifoReg = REG_RD32(LW_PDISP_DSI_FIFO_BASE(i) );
             fifoRd = DRF_VAL( _PDISP, _DSI_FIFO_BASE, _RD_PTR, fifoReg);
             fifoWr = DRF_VAL( _PDISP, _DSI_FIFO_BASE, _WR_PTR, fifoReg);

             if ( fifoRd != fifoWr )
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_FIFO_BASE(%d)_RD_PTR:   0x%02x  !=  "
                     "LW_PDISP_DSI_FIFO_BASE(%d)_WR_PTR:   0x%02x\n",
                     i, fifoRd, i, fifoWr);
                 //addUnitErr("\t LW_PDISP_DSI_FIFO_BASE(%d)_RD_PTR:   0x%02x  !=  "
                 //    "LW_PDISP_DSI_FIFO_BASE(%d)_WR_PTR:   0x%02x\n",
                 //    i, fifoRd, i, fifoWr);
             }

             //dump out method fifo cmd/data and check for violations
             if (checkDispMethodFifo() == LW_ERROR)
             {
                 Printf(Tee::PriHigh,"lw: Display method fifo is in invalid state\n");
                 //addUnitErr("\t Display method fifo is in invalid state\n");
             }
             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_METHOD_FIFO_EMPTY\n");
         }

         //
         //read only when in EMPTY/WRIDLE/IDLE state or when incoming methods
         //have been stopped or paused.
         //
         if (isIdle)
         {
             if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _STATUS_READ_PENDING, data32))
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_READ_PENDING_YES\n");
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_READ_PENDING_NO\n");
             }
         }
         else
         {
             Printf(Tee::PriLow,"lw: + STATE not idle, unable to check STATUS_READ_PENDING\n");
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _STATUS_WILL_GO_IDLE, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_WILL_GO_IDLE_YES\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_WILL_GO_IDLE_NO\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _STATUS_WILL_GO_WRTIDLE, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_WILL_GO_WRTIDLE_YES\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_WILL_GO_WRTIDLE_NO\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _STATUS_QUIESCENT, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_QUIESCENT_YES\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_QUIESCENT_NO\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_BASE, _STATUS_METHOD_EXEC, data32))
         {
             Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_METHOD_EXEC_RUNNING\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_BASE_STATUS_METHOD_EXEC_RUNNING\n");
             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_BASE_STATUS_METHOD_EXEC_IDLE\n");
         }

     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispChnCtlOvly()
 //
 //-----------------------------------------------------
 RC DispState :: checkDispChnCtlOvly( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  fifoReg;
     UINT32  fifoRd;
     UINT32  fifoWr;
     UINT32  i;
     bool    isIdle = false;

     Printf(Tee::PriLow,"\n\tlw: Checking PDISP_CHNCTL_OVLY(i)...\n");

     for (i = 0; i < LW_PDISP_CHNCTL_OVLY__SIZE_1; i++)
     {
         data32 = REG_RD32(LW_PDISP_CHNCTL_OVLY(i));

         Printf(Tee::PriLow,"lw: LW_PDISP_CHNCTL_OVLY(%d):               0x%08x\n", i, data32);

         //return if not connected or allocated
         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _ALLOCATION, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_ALLOCATION_ALLOCATE\n");
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_ALLOCATION_DEALLOCATE\n");
             return status;
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _CONNECTION, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_CONNECTION_CONNECT\n");
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_CONNECTION_DISCONNECT\n");
             return status;
         }

         switch (DRF_VAL( _PDISP, _CHNCTL_OVLY, _TYPE, data32))
         {
         case LW_PDISP_CHNCTL_OVLY_TYPE_UNUSED:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_TYPE_UNUSED\n");
             break;

         case LW_PDISP_CHNCTL_OVLY_TYPE_DMA:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_TYPE_DMA\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_OVLY_TYPE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_OVLY, _TYPE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_OVLY_TYPE:  0x%02x\n",
             //DRF_VAL( _PDISP, _CHNCTL_OVLY, _TYPE, data32));
             status = LW_ERROR;
             break;
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _PUTPTR_WRITE, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_PUTPTR_WRITE_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_PUTPTR_WRITE_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _IGNORE_PI, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_IGNORE_PI_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_IGNORE_PI_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _SKIP_NOTIF, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_SKIP_NOTIF_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_SKIP_NOTIF_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _SKIP_SEMA, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_SKIP_SEMA_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_SKIP_SEMA_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _IGNORE_INTERLOCK, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_IGNORE_INTERLOCK_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_IGNORE_INTERLOCK_DISABLE\n");

         switch ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _TRASH_MODE, data32))
         {
         case LW_PDISP_CHNCTL_OVLY_TRASH_MODE_DISABLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_TRASH_MODE_DISABLE\n");
             break;

         case LW_PDISP_CHNCTL_OVLY_TRASH_MODE_TRASH_ONLY:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_TRASH_MODE_TRASH_ONLY\n");
             break;

         case LW_PDISP_CHNCTL_OVLY_TRASH_MODE_TRASH_AND_ABORT:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_TRASH_MODE_TRASH_AND_ABORT\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_OVLY_TRASH_MODE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_OVLY, _TRASH_MODE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_OVLY_TRASH_MODE:  0x%02x\n",
             //    DRF_VAL( _PDISP, _CHNCTL_OVLY, _TRASH_MODE, data32));
             status = LW_ERROR;
             break;
         }

         switch (DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATE, data32))
         {
         case LW_PDISP_CHNCTL_OVLY_STATE_DEALLOC:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_{INIT,DEALLOC}\n");
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_UNCONNECTED:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_UNCONNECTED\n");
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_INITIALIZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_INITIALIZE\n");
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_IDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_IDLE\n");
             isIdle = true;
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_WRTIDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_WRTIDLE\n");
             isIdle = true;
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_EMPTY:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_EMPTY\n");
             isIdle = true;
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_FLUSHED:
             Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_FLUSHED\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_OVLY_STATE_FLUSHED\n");
             status = LW_ERROR;
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_BUSY:
             Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_BUSY\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_OVLY_STATE_BUSY\n");
             status = LW_ERROR;
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_SHUTDOWN1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_SHUTDOWN1\n");
             break;

         case LW_PDISP_CHNCTL_OVLY_STATE_SHUTDOWN2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATE_SHUTDOWN2\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_OVLY_STATE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_OVLY_STATE:  0x%02x\n",
             //    DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATE, data32));
             status = LW_ERROR;
             break;
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATUS_METHOD_FIFO, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_METHOD_FIFO_NOTEMPTY\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_OVLY_STATUS_METHOD_FIFO_NOTEMPTY\n");

             //check if method fifo RD/WR ptr match
             fifoReg = REG_RD32(LW_PDISP_DSI_FIFO_OVLY(i) );
             fifoRd = DRF_VAL( _PDISP, _DSI_FIFO_OVLY, _RD_PTR, fifoReg);
             fifoWr = DRF_VAL( _PDISP, _DSI_FIFO_OVLY, _WR_PTR, fifoReg);

             if ( fifoRd != fifoWr )
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_FIFO_OVLY(%d)_RD_PTR:   0x%02x  !=  "
                     "LW_PDISP_DSI_FIFO_OVLY(%d)_WR_PTR:   0x%02x\n",
                     i, fifoRd, i, fifoWr);
                 //addUnitErr("\t LW_PDISP_DSI_FIFO_OVLY(%d)_RD_PTR:   0x%02x  !=  "
                 //    "LW_PDISP_DSI_FIFO_OVLY(%d)_WR_PTR:   0x%02x\n",
                 //    i, fifoRd, i, fifoWr);
             }

             //dump out method fifo cmd/data and check for violations
             if (checkDispMethodFifo() == LW_ERROR)
             {
                 Printf(Tee::PriHigh,"lw: Display method fifo is in invalid state\n");
                 //addUnitErr("\t Display method fifo is in invalid state\n");
             }

             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_METHOD_FIFO_EMPTY\n");
         }

         //
         //read only when in EMPTY/WRIDLE/IDLE state or when incoming methods
         //have been stopped or paused.
         //
         if (isIdle)
         {
             if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATUS_READ_PENDING, data32))
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_READ_PENDING_YES\n");
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_READ_PENDING_NO\n");
             }
         }
         else
         {
             Printf(Tee::PriLow,"lw: + STATE not idle, unable to check STATUS_READ_PENDING\n");
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATUS_WILL_GO_IDLE, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_WILL_GO_IDLE_YES\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_WILL_GO_IDLE_NO\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATUS_WILL_GO_WRTIDLE, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_WILL_GO_WRTIDLE_YES\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_WILL_GO_WRTIDLE_NO\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATUS_QUIESCENT, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_QUIESCENT_YES\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_QUIESCENT_NO\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_OVLY, _STATUS_METHOD_EXEC, data32))
         {
             Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_METHOD_EXEC_RUNNING\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_OVLY_STATUS_METHOD_EXEC_RUNNING\n");
             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVLY_STATUS_METHOD_EXEC_IDLE\n");
         }

     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispChnCtlOvim()
 //
 //-----------------------------------------------------
 RC DispState::checkDispChnCtlOvim( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  i;

     Printf(Tee::PriLow,"\n\tlw: Checking PDISP_CHNCTL_OVIM(i)...\n");

     for (i = 0; i < LW_PDISP_CHNCTL_OVIM__SIZE_1; i++)
     {
         data32 = REG_RD32(LW_PDISP_CHNCTL_OVIM(i));

         Printf(Tee::PriLow,"lw: LW_PDISP_CHNCTL_OVIM(%d):               0x%08x\n", i, data32);

         //return if not connected or allocated
         if ( DRF_VAL( _PDISP, _CHNCTL_OVIM, _ALLOCATION, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_ALLOCATION_ALLOCATE\n");
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_ALLOCATION_DEALLOCATE\n");
             return status;
         }

         switch (DRF_VAL( _PDISP, _CHNCTL_OVIM, _TYPE, data32))
         {
         case LW_PDISP_CHNCTL_OVIM_TYPE_UNUSED:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_TYPE_UNUSED\n");
             break;

         case LW_PDISP_CHNCTL_OVIM_TYPE_PIO:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_TYPE_PIO\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_OVIM_TYPE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_OVIM, _TYPE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_OVIM_TYPE:  0x%02x\n",
             //    DRF_VAL( _PDISP, _CHNCTL_OVIM, _TYPE, data32));
             status = LW_ERROR;
             break;
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_OVIM, _LOCK_PIO_FIFO, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_LOCK_PIO_FIFO_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_LOCK_PIO_FIFO_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_OVIM, _IGNORE_INTERLOCK, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_IGNORE_INTERLOCK_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_IGNORE_INTERLOCK_DISABLE\n");

         switch ( DRF_VAL( _PDISP, _CHNCTL_OVIM, _TRASH_MODE, data32))
         {
         case LW_PDISP_CHNCTL_OVIM_TRASH_MODE_DISABLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_TRASH_MODE_DISABLE\n");
             break;

         case LW_PDISP_CHNCTL_OVIM_TRASH_MODE_TRASH_ONLY:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_TRASH_MODE_TRASH_ONLY\n");
             break;

         case LW_PDISP_CHNCTL_OVIM_TRASH_MODE_TRASH_AND_ABORT:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_TRASH_MODE_TRASH_AND_ABORT\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_OVIM_TRASH_MODE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_OVIM, _TRASH_MODE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_OVIM_TRASH_MODE:  0x%02x\n",
             //    DRF_VAL( _PDISP, _CHNCTL_OVIM, _TRASH_MODE, data32));
             status = LW_ERROR;
             break;
         }

         switch (DRF_VAL( _PDISP, _CHNCTL_OVIM, _STATE, data32))
         {
         case LW_PDISP_CHNCTL_OVIM_STATE_DEALLOC:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_STATE_{INIT,DEALLOC}\n");
             break;

         case LW_PDISP_CHNCTL_OVIM_STATE_IDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_OVIM_STATE_IDLE\n");
             break;

         case LW_PDISP_CHNCTL_OVIM_STATE_BUSY:
             Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_OVIM_STATE_BUSY\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_OVIM_STATE_BUSY\n");
             status = LW_ERROR;
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_OVIM_STATE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_OVIM, _STATE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_OVIM_STATE:  0x%02x\n",
             //    DRF_VAL( _PDISP, _CHNCTL_OVIM, _STATE, data32));
             status = LW_ERROR;
             break;
         }
     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispChnCtlLwrs()
 //
 //-----------------------------------------------------
 RC DispState::checkDispChnCtlLwrs( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  i;

     Printf(Tee::PriLow,"\n\tlw: Checking PDISP_CHNCTL_LWRS(i)...\n");

     for (i = 0; i < LW_PDISP_CHNCTL_LWRS__SIZE_1; i++)
     {
         data32 = REG_RD32(LW_PDISP_CHNCTL_LWRS(i));

         Printf(Tee::PriLow,"lw: LW_PDISP_CHNCTL_LWRS(%d):               0x%08x\n", i, data32);

         //return if not connected or allocated
         if ( DRF_VAL( _PDISP, _CHNCTL_LWRS, _ALLOCATION, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_ALLOCATION_ALLOCATE\n");
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_ALLOCATION_DEALLOCATE\n");
             return status;
         }

         switch (DRF_VAL( _PDISP, _CHNCTL_LWRS, _TYPE, data32))
         {
         case LW_PDISP_CHNCTL_LWRS_TYPE_UNUSED:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_TYPE_UNUSED\n");
             break;

         case LW_PDISP_CHNCTL_LWRS_TYPE_PIO:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_TYPE_PIO\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_LWRS_TYPE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_LWRS, _TYPE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_LWRS_TYPE:  0x%02x\n",
             //    DRF_VAL( _PDISP, _CHNCTL_LWRS, _TYPE, data32));
             status = LW_ERROR;
             break;
         }

         if ( DRF_VAL( _PDISP, _CHNCTL_LWRS, _LOCK_PIO_FIFO, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_LOCK_PIO_FIFO_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_LOCK_PIO_FIFO_DISABLE\n");

         if ( DRF_VAL( _PDISP, _CHNCTL_LWRS, _IGNORE_INTERLOCK, data32))
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_IGNORE_INTERLOCK_ENABLE\n");
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_IGNORE_INTERLOCK_DISABLE\n");

         switch ( DRF_VAL( _PDISP, _CHNCTL_LWRS, _TRASH_MODE, data32))
         {
         case LW_PDISP_CHNCTL_LWRS_TRASH_MODE_DISABLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_TRASH_MODE_DISABLE\n");
             break;

         case LW_PDISP_CHNCTL_LWRS_TRASH_MODE_TRASH_ONLY:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_TRASH_MODE_TRASH_ONLY\n");
             break;

         case LW_PDISP_CHNCTL_LWRS_TRASH_MODE_TRASH_AND_ABORT:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_TRASH_MODE_TRASH_AND_ABORT\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_LWRS_TRASH_MODE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_LWRS, _TRASH_MODE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_LWRS_TRASH_MODE:  0x%02x\n",
             //    DRF_VAL( _PDISP, _CHNCTL_LWRS, _TRASH_MODE, data32));
             status = LW_ERROR;
             break;
         }

         switch (DRF_VAL( _PDISP, _CHNCTL_LWRS, _STATE, data32))
         {
         case LW_PDISP_CHNCTL_LWRS_STATE_DEALLOC:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_STATE_{INIT,DEALLOC}\n");
             break;

         case LW_PDISP_CHNCTL_LWRS_STATE_IDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_CHNCTL_LWRS_STATE_IDLE\n");
             break;

         case LW_PDISP_CHNCTL_LWRS_STATE_BUSY:
             Printf(Tee::PriHigh,"lw: + LW_PDISP_CHNCTL_LWRS_STATE_BUSY\n");
             //addUnitErr("\t LW_PDISP_CHNCTL_LWRS_STATE_BUSY\n");
             status = LW_ERROR;
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_CHNCTL_LWRS_STATE:  0x%02x\n",
                 DRF_VAL( _PDISP, _CHNCTL_LWRS, _STATE, data32));
             //addUnitErr("\t Unknown LW_PDISP_CHNCTL_LWRS_STATE:  0x%02x\n",
             //DRF_VAL( _PDISP, _CHNCTL_LWRS, _STATE, data32));
             status = LW_ERROR;
             break;
         }
     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispUpdatePreFSM()
 //
 //-----------------------------------------------------
 RC DispState::checkDispUpdatePreFSM( UINT32 *pre )
 {
     UINT32  status = LW_OK;
     UINT32  i;

     if ( pre[0] != LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_IDLE)
     {
         //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE2_PRE0 fsm is not idle\n");
         Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE2_PRE0 fsm is not idle\n");
         status = LW_ERROR;
     }

     if ( pre[1] != LW_PDISP_DSI_CORE_UPD_STATE2_PRE1_IDLE)
     {
         //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE2_PRE1 fsm is not idle\n");
         Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE2_PRE1 fsm is not idle\n");
         status = LW_ERROR;
     }

     for (i = 0; i < 2; i++)
     {
         switch (pre[i])
         {

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_IDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_IDLE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_GO_SNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_GO_SNOOZE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_SEND_SNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_SEND_SNOOZE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_SNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_SNOOZE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_GO_SAFE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_GO_SAFE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_SEND_SAFE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_SEND_SAFE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_SAFE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_SAFE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_UPD1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_UPD1\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_SEND_UPD1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_SEND_UPD1\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_UPD1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_UPD1\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_LV1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_LV1\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_POLL1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_POLL1\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_POLL1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_POLL1\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_GO_SLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_GO_SLEEP\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_SEND_SLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_SEND_SLEEP\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_SLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_SLEEP\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_UPD2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_UPD2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_SEND_UPD2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_SEND_UPD2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_UPD2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_UPD2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_LV2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_LV2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_POLL2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_POLL2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_POLL2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_POLL2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_GO_DETACH:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_GO_DETACH\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_WAIT_DETACH:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_WAIT_DETACH\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_PRE0_DONE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d_DONE\n",i);
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d: 0x%02x\n",
                 i, pre[i]);
             //addUnitErr("\t Unknown LW_PDISP_DSI_CORE_UPD_STATE2_PRE%d: 0x%02x\n",
             //    i, pre[i]);
             status = LW_ERROR;
             break;

         }
     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispUpdatePostFSM()
 //
 //-----------------------------------------------------
 RC DispState::checkDispUpdatePostFSM( UINT32 *post )
 {
     UINT32  status = LW_OK;
     UINT32  i;

     if ( post[0] != LW_PDISP_DSI_CORE_UPD_STATE2_POST0_IDLE)
     {
         //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE2_POST0 fsm is not idle\n");
         Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE2_POST0 fsm is not idle\n");
         status = LW_ERROR;
     }

     if ( post[1] != LW_PDISP_DSI_CORE_UPD_STATE2_POST1_IDLE)
     {
         //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE2_POST1 fsm is not idle\n");
         Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE2_POST1 fsm is not idle\n");
         status = LW_ERROR;
     }

     for (i = 0; i < 2; i++)
     {
         switch (post[i])
         {

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_IDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_IDLE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_ATTACH:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_ATTACH\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_ATTACH:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_ATTACH\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_POLL1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_POLL1\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_POLL1:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_POLL1\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_UNSLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_UNSLEEP\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_SEND_UNSLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_SEND_UNSLEEP\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_UNSLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_UNSLEEP\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_UNSAFE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_UNSAFE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_SEND_UNSAFE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_SEND_UNSAFE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_UNSAFE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_UNSAFE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_UPD2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_UPD2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_SEND_UPD2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_SEND_UPD2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_UPD2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_UPD2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_LV2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_LV2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_POLL2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_POLL2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_POLL2:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_POLL2\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_BACK2MAIN:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_BACK2MAIN\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_UNSNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_UNSNOOZE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_SEND_UNSNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_SEND_UNSNOOZE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_UNSNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_UNSNOOZE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_UPD3:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_UPD3\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_SEND_UPD3:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_SEND_UPD3\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_UPD3:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_UPD3\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_LV3:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_LV3\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_POLL3:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_POLL3\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_POLL3:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_POLL3\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_LV_ONLY:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_LV_ONLY\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_WAIT_4_LV_ONLY:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_WAIT_4_LV_ONLY\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE2_POST0_DONE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE2_POST%d_DONE\n",i);
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_DSI_CORE_UPD_STATE2_POST%d: 0x%02x\n",
                 i, post[i]);
             //addUnitErr("\t Unknown LW_PDISP_DSI_CORE_UPD_STATE2_POST%d: 0x%02x\n",
             //    i, post[i]);
             status = LW_ERROR;
             break;

         }
     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispUpdateUsubFSM()
 //
 //-----------------------------------------------------
 RC DispState::checkDispUpdateUsubFSM( UINT32 *usub )
 {
     UINT32  status = LW_OK;
     UINT32  i;

     if ( usub[0] != LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_IDLE)
     {
         //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_IDLE fsm is not idle\n");
         Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_IDLE fsm is not idle\n");
         status = LW_ERROR;
     }

     if ( usub[1] != LW_PDISP_DSI_CORE_UPD_STATE1_USUB1_IDLE)
     {
         //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE1_USUB1_IDLE fsm is not idle\n");
         Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE1_USUB1_IDLE fsm is not idle\n");
         status = LW_ERROR;
     }

     for (i = 0; i < 2; i++)
     {
         switch (usub[i])
         {
         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_IDLE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_IDLE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_WAITPOLL:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_WAITPOLL\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_START:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_START\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_WAIT4BASE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_WAIT4BASE\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_BSEND:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_BSEND\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_BWAIT:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_BWAIT\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_OSEND:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_OSEND\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_OWAIT:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_OWAIT\n",i);
             break;

         case LW_PDISP_DSI_CORE_UPD_STATE1_USUB0_DONE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d_DONE\n",i);
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d: 0x%02x\n",
                 i, usub[i]);
             //addUnitErr("\t Unknown LW_PDISP_DSI_CORE_UPD_STATE1_USUB%d: 0x%02x\n",
             //    i, usub[i]);
             status = LW_ERROR;
             break;

         }
     }

     return status;

 }

 //-----------------------------------------------------
 // checkDispUpdateCmgrFSM()
 //
 //-----------------------------------------------------
 RC DispState :: checkDispUpdateCmgrFSM( UINT32 cmgr )
 {
     UINT32  status = LW_OK;

     if ( cmgr != LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_IDLE)
     {
         //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE1_CMGR fsm is not idle\n");
         Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE1_CMGR fsm is not idle\n");
         status = LW_ERROR;
     }

     switch (cmgr)
     {
     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_IDLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_IDLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_BEGIN_PRECLK:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_BEGIN_PRECLK\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_FOR_ACK_PLLRESET_ENABLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_FOR_ACK_PLLRESET_ENABLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_GOTO_SAFE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_GOTO_SAFE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_SAFE :
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_SAFE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_SAFE_SETTLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_SAFE_SETTLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_BYPASS:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_BYPASS\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_BYPASS:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_BYPASS\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_BYPASS_SETTLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_BYPASS_SETTLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_DISABLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_DISABLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ON:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ON\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_BEGIN_POSTCLK:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_BEGIN_POSTCLK\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_COEFF:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_COEFF\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_ENABLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_ENABLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_VPLL_LOCK_SETTLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_VPLL_LOCK_SETTLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_UNBYPASS:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_UNBYPASS\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_UNBYPASS_SETTLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_UNBYPASS_SETTLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_SET_OWNER:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_SET_OWNER\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_SET_OWNER:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_SET_OWNER\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_PLL_RESET_DISABLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_PLL_RESET_DISABLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_FOR_ACK_PLLRESET_DISABLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_FOR_ACK_PLLRESET_DISABLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_MACROPLL_SETTLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_MACROPLL_SETTLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_GOTO_UNSAFE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_GOTO_UNSAFE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_UNSAFE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_UNSAFE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_UNSAFE_SETTLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_UNSAFE_SETTLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_POSTOFF:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_POSTOFF\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_GOTO_FAST:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_GOTO_FAST\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_FAST:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_FAST\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_FASTDONE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_FASTDONE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_GOTO_NORM:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_GOTO_NORM\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_NORM:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_WAIT_4_ACK_OF_NORM\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_NORMDONE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_CMGR_NORMDONE\n");
         break;

     default:
         Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_DSI_CORE_UPD_STATE1_CMGR: 0x%02x\n", cmgr);
         //addUnitErr("\t Unknown LW_PDISP_DSI_CORE_UPD_STATE1_CMGR: 0x%02x\n", cmgr);
         status = LW_ERROR;
         break;
     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispUpdatePreFSM()
 //
 //-----------------------------------------------------
RC DispState :: checkDispCoreUpdateFSM( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  main0;
     UINT32  pre[2];
     UINT32  post[2];
     UINT32  usub[2];
     UINT32  cmgr;

     Printf(Tee::PriLow,"\n\tlw: Checking Update FSMs status...\n");

     data32 = REG_RD32(LW_PDISP_DSI_CORE_UPD_STATE1);

     main0 = DRF_VAL(_PDISP, _DSI_CORE_UPD_STATE1, _MAIN, data32);
     cmgr = DRF_VAL(_PDISP, _DSI_CORE_UPD_STATE1, _CMGR, data32);

     usub[0] = DRF_VAL(_PDISP, _DSI_CORE_UPD_STATE1, _USUB0, data32);
     usub[1] = DRF_VAL(_PDISP, _DSI_CORE_UPD_STATE1, _USUB1, data32);

     data32 = REG_RD32(LW_PDISP_DSI_CORE_UPD_STATE2);

     pre[0] = DRF_VAL(_PDISP, _DSI_CORE_UPD_STATE2, _PRE0, data32);
     pre[1] = DRF_VAL(_PDISP, _DSI_CORE_UPD_STATE2, _PRE1, data32);

     post[0] = DRF_VAL(_PDISP, _DSI_CORE_UPD_STATE2, _POST0, data32);
     post[1] = DRF_VAL(_PDISP, _DSI_CORE_UPD_STATE2, _POST1, data32);

     switch (main0)
     {
     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_IDLE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_IDLE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_TO_BEGIN:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_TO_BEGIN\n");
         //
         // if waiting for any pending completion notifier writes to finish.
         // dump notifiers
         //
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_START:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_START\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_RM1:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_RM1\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_FAST:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_FAST\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_PRE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_PRE\n");

         if (checkDispUpdatePreFSM( pre ) == LW_ERROR)
             //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE1_PRE is not idle\n");
             Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE1_PRE is not idle\n");

         else
             //addUnitErr("\t Waiting on LW_PDISP_DSI_CORE_UPD_STATE1_PRE,"
         //    " which is idle. Should not be in this state\n");
          ;
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_PRECLK:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_PRECLK\n");

         if (checkDispUpdateCmgrFSM( cmgr ) == LW_ERROR)
             //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE1_CMGR is not idle\n");
             Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE1_CMGR is not idle\n");

         else
             //addUnitErr("\t Waiting on LW_PDISP_DSI_CORE_UPD_STATE1_CMGR,"
             //" which is idle. Should not be in this state\n");
             ;
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_RM2:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_RM2\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_USUB:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_USUB\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_USUB:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_USUB\n");

         if (checkDispUpdateUsubFSM( usub ) == LW_ERROR)
             //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE1_USUB is not idle\n");
             Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE1_USUB is not idle\n");
         else
             //addUnitErr("\t Waiting on LW_PDISP_DSI_CORE_UPD_STATE1_USUB,"
             //" which is idle. Should not be in this state\n");
             ;
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_POSTCLK:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_POSTCLK\n");

         if (checkDispUpdateCmgrFSM( cmgr ) == LW_ERROR)
             //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE1_CMGR is not idle\n");
              Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE1_CMGR is not idle\n");

         else
             //addUnitErr("\t Waiting on LW_PDISP_DSI_CORE_UPD_STATE1_CMGR,"
             //" which is idle. Should not be in this state\n");
              ;
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_POST1:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_POST1\n");

         if (checkDispUpdatePostFSM( post ) == LW_ERROR)
             //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE2_POST is not idle\n");
              Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE2_POST is not idle\n");

         else
             //addUnitErr("\t Waiting on LW_PDISP_DSI_CORE_UPD_STATE2_POST,"
             //" which is idle. Should not be in this state\n");
             ;
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_RM3:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_RM3\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_POST2:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_POST2\n");

         if (checkDispUpdatePostFSM(post) == LW_ERROR)
             //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE2_POST is not idle\n");
             Printf(Tee::PriHigh,"\t LW_PDISP_DSI_CORE_UPD_STATE2_POST is not idle\n");

         else
             //addUnitErr("\t Waiting on LW_PDISP_DSI_CORE_UPD_STATE2_POST,"
             //" which is idle. Should not be in this state\n");
             ;
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_NOTIF:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_NOTIF\n");
         break;

         //Wait for dispclk to resume to normal speed.
     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_NORMAL:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_WAIT_4_NORMAL\n");
         //print current dispclk
         // TBD Get the DispClock Need to write code
         //Printf(Tee::PriLow,"lw: Current dispclk: %d\n", clkGetDispclkFreqKHz());
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_DONE:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_DONE\n");
         break;

     case LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_QUITTING:
         Printf(Tee::PriLow,"lw: + LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_QUITTING\n");
         break;

     default:
         Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_DSI_CORE_UPD_STATE1_MAIN: 0x%02x\n",
             main0);
         //addUnitErr("\t Unknown LW_PDISP_DSI_CORE_UPD_STATE1_MAIN: 0x%02x\n",
         //    main0);
         status = LW_ERROR;
         break;
     }

     if ( main0 != LW_PDISP_DSI_CORE_UPD_STATE1_MAIN_IDLE)
     {
         //addUnitErr("\t LW_PDISP_DSI_CORE_UPD_STATE1_MAIN fsm is not idle\n");
         Printf(Tee::PriHigh,"LW_PDISP_DSI_CORE_UPD_STATE1_MAIN fsm is not idle\n");
         status = LW_ERROR;
     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispDmiStatus()
 //
 //-----------------------------------------------------
 RC DispState::checkDispDmiStatus( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  memStat;
     UINT32  memReq;
     UINT32  i=0,j=0;
     bool    bIsFetching = false;

     //check if DMI_MEMACC is fetching
     Printf(Tee::PriLow,"\n\tlw: Checking DMI status...\n");

    //  i am Getting here in modeSet Error  i have decided to Sleep here some time
    //  & after done with sleep wakeup and chekck for the status Lets do ..

     do
     {
         data32 = REG_RD32(LW_PDISP_DMI_MEMACC);

         for (i = 0; i< LW_PDISP_DMI_MEMACC_HEAD_STATUS__SIZE_1; i++)
         {
             memStat = DRF_IDX_VAL(_PDISP, _DMI_MEMACC_HEAD, _STATUS, i, data32);
             memReq = DRF_IDX_VAL(_PDISP, _DMI_MEMACC_HEAD, _REQUEST, i, data32);

             if (memStat == LW_PDISP_DMI_MEMACC_HEAD_STATUS_FETCHING)
             {
                 bIsFetching = true;
                 Printf(Tee::PriLow,"lw: + LW_PDISP_DMI_MEMACC_HEAD_STATUS(%d)_FETCHING\n", i);
             }

             //check if sw request doesnt match current status
             if ( DRF_IDX_VAL(_PDISP, _DMI_MEMACC_HEAD, _SETTING_NEW, i, data32))
             {
                 if (memStat != memReq)
                 {
                     Printf(Tee::PriLow,"lw: Error: DMI_MEMACC_HEAD_REQUEST(%d) doesn't match"
                         " current DMI_MEMACC_HEAD_STATUS(%d).\n", i, i);
                     Printf(Tee::PriHigh,"lw: LW_PDISP_DMI_MEMACC:       0x%08x\n", data32);
                     //addUnitErr("\t Error: DMI_MEMACC_HEAD_REQUEST(%d) doesn't match"
                     //    " current DMI_MEMACC_HEAD_STATUS(%d).\n", i, i);
                     status = LW_ERROR;
                 }
             }
         }

         //if no heads fetching, report error
         if (!bIsFetching)
         {
             //addUnitErr("\t LW_PDISP_DMI_MEMACC_HEAD_STATUS_STOPPED"
             //    " for all heads\n");
             status = LW_ERROR;
             // Sleep for 2 sec and Check DMi Status gain if it say still failing than
             // report error
             Tasker::Sleep(2000);

             Printf(Tee::PriLow,"LW_PDISP_DMI_MEMACC_HEAD_STATUS_STOPPED .. Reading Status Again \n");

             if((j++) == 2)
             {
                 Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_DMI_MEMACC_HEAD_STATUS_STOPPED"
                     " for all heads\n");
             }
         }
         else
         {
             status = LW_OK;
             break;
         }
     }while(j < 3);

     //check if non-isochronous writes are pending
     if (REG_RD_DRF(_PDISP, _FLUSH, _NON_ISO_FB)==
         LW_PDISP_FLUSH_NON_ISO_FB_PENDING)
     {
         Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_FLUSH_NON_ISO_FB_PENDING\n");
         //addUnitErr("\t LW_PDISP_FLUSH_NON_ISO_FB_PENDING\n");
         status = LW_ERROR;
     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispDpStatus()
 //
 //-----------------------------------------------------
 RC DispState ::checkDispDpStatus( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  reg;
     UINT32  lanesUp = 0;
     UINT32  i, j;
     bool    is2PortEn = false;
     bool    isActiveSymEn = false;

     Printf(Tee::PriLow,"\n\tlw: Checking DP status...\n");

     for (i = 0; i < LW_PDISP_SOR_DP_LINKCTL__SIZE_1 ; i++)
     {
         for (j = 0; j < LW_PDISP_SOR_DP_LINKCTL__SIZE_2; j++)
         {
             data32 = REG_RD32(LW_PDISP_SOR_DP_LINKCTL(i,j));

             if ( DRF_VAL( _PDISP, _SOR_DP_LINKCTL, _ENABLE, data32) ==
                 LW_PDISP_SOR_DP_LINKCTL_ENABLE_NO)
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_ENABLE_NO\n",
                     i, j);
                 is2PortEn = false;
                 continue;
             }

             //check that only 1 DP port on SOR is enabled
             if (j == 0)
             {
                 is2PortEn = true;
             }
             else if (is2PortEn == true)
             {
                 Printf(Tee::PriHigh,"lw: Error: Both DP ports on SOR are enabled: "
                     "LW_PDISP_SOR_DP_LINKCTL_ENABLE(%d, 0-1)_ENABLE_YES\n", i);
                 //addUnitErr("\t Both DP ports on SOR are enabled: "
                 //    "LW_PDISP_SOR_DP_LINKCTL_ENABLE(%d, 0-1)_ENABLE_YES\n", i);
                 status = LW_ERROR;
             }

             Printf(Tee::PriLow,"lw: LW_PDISP_SOR_DP_LINKCTL(%d,%d)_TUSIZE  0x%02x \n",
                 i, j, DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _TUSIZE, data32));

             //DP hardware treats all pixel clocks as asynch, so should be _DISABLE.
             if ( DRF_VAL( _PDISP, _SOR_DP_LINKCTL, _SYNCMODE, data32) ==
                 LW_PDISP_SOR_DP_LINKCTL_SYNCMODE_ENABLE)
             {
                 Printf(Tee::PriLow,"lw: Warning: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_SYNCMODE"
                     "_ENABLE\n", i, j);
                 //addUnitErr("\t Warning: LW_PDISP_SOR_DP_LINKCTL(%d,%d)_SYNCMODE"
                 //    "_ENABLE\n", i, j);
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_SYNCMODE_DISABLE\n",
                     i, j);
             }

             switch ( DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _SCRAMBLEREN, data32))
             {
             case LW_PDISP_SOR_DP_LINKCTL_SCRAMBLEREN_DISABLE:
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_SCRAMBLEREN"
                     "_DISABLE\n", i, j);
                 break;

             case LW_PDISP_SOR_DP_LINKCTL_SCRAMBLEREN_ENABLE_GALIOS:
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_SCRAMBLEREN"
                     "_ENABLE_GALIOS\n", i, j);
                 break;

             case LW_PDISP_SOR_DP_LINKCTL_SCRAMBLEREN_ENABLE_FIBONACCI:
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_SCRAMBLEREN"
                     "_ENABLE_FIBONACCI\n", i, j);
                 break;

             default:
                 Printf(Tee::PriHigh,"lw: Error: Unknown LW_PDISP_SOR_DP_LINKCTL(%d,%d)"
                     "_SCRAMBLEREN_ENABLE\n", i, j);
                 //addUnitErr("\t Unknown LW_PDISP_SOR_DP_LINKCTL(%d,%d)"
                 //    "_SCRAMBLEREN_ENABLE\n", i, j);
                 status = LW_ERROR;
                 break;
             }

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_ENHANCEDFRAME_%s\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _ENHANCEDFRAME, data32) ?
                 "ENABLE":"DISABLE");

             //check power controls for individual lanes in LW_PDISP_SOR_DP_PADCTL.
             reg = REG_RD32(LW_PDISP_SOR_DP_PADCTL(i,j));

             //count # of power enabled lanes
             lanesUp += DRF_VAL(_PDISP, _SOR_DP_PADCTL, _PD_TXD_0, reg);
             lanesUp += DRF_VAL(_PDISP, _SOR_DP_PADCTL, _PD_TXD_1, reg);
             lanesUp += DRF_VAL(_PDISP, _SOR_DP_PADCTL, _PD_TXD_2, reg);
             lanesUp += DRF_VAL(_PDISP, _SOR_DP_PADCTL, _PD_TXD_3, reg);

             switch ( DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _LANECOUNT, data32))
             {
             case LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_ZERO:
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_ZERO\n", i, j);

                 if (lanesUp != 0)
                 {
                     Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_ZERO but"
                         " LW_PDISP_SOR_DP_PADCTL shows %d powered lanes\n", i, j, lanesUp);
                     //addUnitErr("\t LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_ZERO but "
                     //    "LW_PDISP_SOR_DP_PADCTL shows %d powered lanes\n", i, j, lanesUp);
                     status = LW_ERROR;
                 }
                 break;

             case LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_ONE:
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_ONE\n", i, j);

                 if (lanesUp != 1)
                 {
                     Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_ONE but"
                         " LW_PDISP_SOR_DP_PADCTL shows %d powered lanes\n", i, j, lanesUp);
                     //addUnitErr("\t LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_ONE but "
                     //    "LW_PDISP_SOR_DP_PADCTL shows %d powered lanes\n", i, j, lanesUp);
                     status = LW_ERROR;
                 }
                 break;

             case LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_TWO:
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_TWO\n", i, j);

                 if (lanesUp != 2)
                 {
                     Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_TWO but"
                         " LW_PDISP_SOR_DP_PADCTL shows %d powered lanes\n", i, j, lanesUp);
                     //addUnitErr("\t LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_TWO but "
                     //    "LW_PDISP_SOR_DP_PADCTL shows %d powered lanes\n", i, j, lanesUp);
                     status = LW_ERROR;
                 }
                 break;

             case LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_FOUR:
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_FOUR\n", i, j);

                 if (lanesUp != 4)
                 {
                     Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_FOUR but"
                         " LW_PDISP_SOR_DP_PADCTL shows %d powered lanes\n", i, j, lanesUp);
                     //addUnitErr("\t LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT_FOUR but "
                     //    "LW_PDISP_SOR_DP_PADCTL shows %d powered lanes\n", i, j, lanesUp);
                     status = LW_ERROR;
                 }
                 break;

             default:
                 Printf(Tee::PriHigh,"lw: Error: Unknown LW_PDISP_SOR_DP_LINKCTL(%d,%d)"
                     "_LANECOUNT\n", i, j);
                 //addUnitErr("\t Unknown LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LANECOUNT"
                 //    "\n", i, j);
                 status = LW_ERROR;
                 break;
             }

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_CHANNELCODING_%s\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _CHANNELCODING, data32) ? "ENABLE":"DISABLE");

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_TRAININGPTTRN:      0x%02x\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _TRAININGPTTRN, data32));

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_LINKQUALPTTRN:      0x%02x\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _LINKQUALPTTRN, data32));

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_COMPLIANCEPTTRN_%s\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _COMPLIANCEPTTRN, data32) ? "COLORSQARE":"NOPATTERN");

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_LINKCTL(%d,%d)_FORCE_IDLEPTTRN_%s\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_LINKCTL, _FORCE_IDLEPTTRN, data32) ? "YES":"NO");

             //LW_PDISP_SOR_DP_PADCTL
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_PADCTL(%d,%d):           0x%08x\n", i, j, reg);

             //LW_PDISP_SOR_DP_CONFIG
             data32 = REG_RD32(LW_PDISP_SOR_DP_CONFIG(i,j));

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_CONFIG(%d,%d)_WATERMARK:           0x%02x\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_CONFIG, _WATERMARK, data32));

             if (DRF_VAL(_PDISP, _SOR_DP_CONFIG, _ACTIVESYM_CNTL, data32))
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_CONFIG(%d,%d)_ACTIVESYM_CNTL_ENABLE\n", i, j);
                 isActiveSymEn = true;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_CONFIG(%d,%d)_ACTIVESYM_CNTL_DISABLE\n", i, j);
             }

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_CONFIG(%d,%d)_ACTIVESYM_COUNT:     0x%02x\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_CONFIG, _ACTIVESYM_COUNT, data32));

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_CONFIG(%d,%d)_ACTIVESYM_FRAC:      0x%02x\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_CONFIG, _ACTIVESYM_FRAC, data32));

             if (DRF_VAL(_PDISP, _SOR_DP_CONFIG, _IDLE_BEFORE_ATTACH, data32) ==
                 LW_PDISP_SOR_DP_CONFIG0_IDLE_BEFORE_ATTACH_DISABLE)
             {
                 Printf(Tee::PriLow,"lw: Warning: + LW_PDISP_SOR_DP_CONFIG(%d,%d)_IDLE_BEFORE"
                     "_ATTACH_DISABLE - should be _ENABLE in production\n", i, j);
                 //addUnitErr("\t Warning: LW_PDISP_SOR_DP_CONFIG(%d,%d)_IDLE_BEFORE"
                 //    "_ATTACH_DISABLE - should be _ENABLE in production\n", i, j);
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_CONFIG(%d,%d)_IDLE_BEFORE"
                     "_ATTACH_ENABLE\n", i, j);
             }

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_CONFIG(%d,%d)_RD_RESET_VAL_%s\n", i, j,
                 DRF_VAL(_PDISP, _SOR_DP_CONFIG, _RD_RESET_VAL, data32) ? "NEGATIVE":"POSITIVE");

             //LW_PDISP_SOR_DP_DEBUG
             data32 = REG_RD32(LW_PDISP_SOR_DP_DEBUG(i,j));

             //
             //FIFO_UNDERFLOW is set when active symbol control is enabled
             //and lane fifos underflow.
             //
             if (isActiveSymEn)
             {
                 if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE0_FIFO_UNDERFLOW, data32))
                 {
                     Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                         "_FIFO_UNDERFLOW_YES\n", i, j);
                     //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0_FIFO"
                     //    "_UNDERFLOW_YES\n", i, j);
                     status = LW_ERROR;
                 }
                 else
                 {
                     Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0_FIFO"
                         "_UNDERFLOW_NO\n", i, j);
                 }

                 if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE1_FIFO_UNDERFLOW, data32))
                 {
                     Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                         "_FIFO_UNDERFLOW_YES\n", i, j);
                     //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1_FIFO"
                     //    "_UNDERFLOW_YES\n", i, j);
                     status = LW_ERROR;
                 }
                 else
                 {
                     Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1_FIFO"
                         "_UNDERFLOW_NO\n", i, j);
                 }

                 if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE2_FIFO_UNDERFLOW, data32))
                 {
                     Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                         "_FIFO_UNDERFLOW_YES\n", i, j);
                     //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2_FIFO"
                     //    "_UNDERFLOW_YES\n", i, j);
                     status = LW_ERROR;
                 }
                 else
                 {
                     Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2_FIFO"
                         "_UNDERFLOW_NO\n", i, j);
                 }

                 if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE3_FIFO_UNDERFLOW, data32))
                 {
                     Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                         "_FIFO_UNDERFLOW_YES\n", i, j);
                     //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3_FIFO"
                     //    "_UNDERFLOW_YES\n", i, j);
                     status = LW_ERROR;
                 }
                 else
                 {
                     Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3_FIFO"
                         "_UNDERFLOW_NO\n", i, j);
                 }
             }

             //_PIXPACK_OVERFLOW
             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE0_PIXPACK_OVERFLOW, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                     "_PIXPACK_OVERFLOW_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                 //    "_PIXPACK_OVERFLOW_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                     "_PIXPACK_OVERFLOW_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE1_PIXPACK_OVERFLOW, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                     "_PIXPACK_OVERFLOW_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                 //    "_PIXPACK_OVERFLOW_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                     "_PIXPACK_OVERFLOW_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE2_PIXPACK_OVERFLOW, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                     "_PIXPACK_OVERFLOW_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                 //    "_PIXPACK_OVERFLOW_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                     "_PIXPACK_OVERFLOW_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE3_PIXPACK_OVERFLOW, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                     "_PIXPACK_OVERFLOW_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                 //    "_PIXPACK_OVERFLOW_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                     "_PIXPACK_OVERFLOW_NO\n", i, j);
             }

             //_STEER_ERROR
             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE0_STEER_ERROR, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                     "_STEER_ERROR_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                 //    "_STEER_ERROR_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                     "_STEER_ERROR_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE1_STEER_ERROR, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                     "_STEER_ERROR_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                 //    "_STEER_ERROR_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                     "_STEER_ERROR_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE2_STEER_ERROR, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                     "_STEER_ERROR_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                 //    "_STEER_ERROR_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                     "_STEER_ERROR_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE3_STEER_ERROR, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                     "_STEER_ERROR_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                 //    "_STEER_ERROR_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                     "_STEER_ERROR_NO\n", i, j);
             }

             //SPKT_OVERRUN
             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _SPKT_OVERRUN, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)"
                     "_SPKT_OVERRUN_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)"
                 //    "_SPKT_OVERRUN_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)"
                     "_SPKT_OVERRUN_NO\n", i, j);
             }

             //FIFO_OVERFLOW
             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE0_FIFO_OVERFLOW, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                     "_FIFO_OVERFLOW_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                 //    "_FIFO_OVERFLOW_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE0"
                     "_FIFO_OVERFLOW_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE1_FIFO_OVERFLOW, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                     "_FIFO_OVERFLOW_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                 //    "_FIFO_OVERFLOW_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE1"
                     "_FIFO_OVERFLOW_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE2_FIFO_OVERFLOW, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                     "_FIFO_OVERFLOW_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                 //    "_FIFO_OVERFLOW_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE2"
                     "_FIFO_OVERFLOW_NO\n", i, j);
             }

             if (DRF_VAL(_PDISP, _SOR_DP_DEBUG, _LANE3_FIFO_OVERFLOW, data32))
             {
                 Printf(Tee::PriHigh,"lw: Error: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                     "_FIFO_OVERFLOW_YES\n", i, j);
                 //addUnitErr("\t LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                 //    "_FIFO_OVERFLOW_YES\n", i, j);
                 status = LW_ERROR;
             }
             else
             {
                 Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_DP_DEBUG(%d,%d)_LANE3"
                     "_FIFO_OVERFLOW_NO\n", i, j);
             }
         }
     }
     return status;
 }

 //-----------------------------------------------------
 // checkDispHdmiStatus()
 //
 //-----------------------------------------------------
RC DispState:: checkDispHdmiStatus( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  i;
     UINT32  lwalue = 0;
     bool    isHwCtsEn = false;
     bool    hdmiChEn[4];
     bool    isHdmiChEn = false;

     Printf(Tee::PriLow,"\n\tlw: Checking HDMI status...\n");

     for (i = 0; i < LW_PDISP_SOR_HDMI_CTRL__SIZE_1; i++)
     {

         data32 = REG_RD32(LW_PDISP_SOR_HDMI_CTRL(i));

         if ( DRF_VAL(_PDISP, _SOR_HDMI_CTRL, _ENABLE, data32) ==
             LW_PDISP_SOR_HDMI_CTRL_ENABLE_NO)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_CTRL(%d)_ENABLE_NO\n", i);
             hdmiChEn[i] = false;
             continue;
         }

         Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_CTRL(%d):       0x%08x\n", i, data32);
         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_CTRL(%d)_ENABLE_YES\n", i);

         //mark enabled HDMI sublink
         hdmiChEn[i] = true;
         isHdmiChEn = true;

         //if ctrl enabled, check other HDMI settings
         data32 = REG_RD32(LW_PDISP_SOR_HDMI_SPARE(i));

         Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_SPARE(%d):      0x%08x\n", i, data32);

         if ( DRF_VAL(_PDISP, _SOR_HDMI_SPARE_HW, _CTS, data32) ==
             LW_PDISP_SOR_HDMI_SPARE_HW_CTS_ENABLE)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE\n", i);
             isHwCtsEn = true;

             Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_SPARE(%i)_CTS_RESET_VAL:     0x%02x\n",
                 i, DRF_VAL(_PDISP, _SOR_HDMI_SPARE, _CTS_RESET_VAL, data32));

             if ( DRF_VAL(_PDISP, _SOR_HDMI_SPARE, _CTS_RESET_VAL, data32) != 1)
             {
                 Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE, but"
                     " LW_PDISP_SOR_HDMI_SPARE(%i)_CTS_RESET_VAL != 1\n", i, i);
                 //addUnitErr("\t LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE, but"
                 //    " LW_PDISP_SOR_HDMI_SPARE(%i)_CTS_RESET_VAL != 1\n", i, i);
                 status = LW_ERROR;
             }
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_DISABLE\n", i);
             isHwCtsEn = false;
         }

         if ( DRF_VAL(_PDISP, _SOR_HDMI_SPARE, _ACR_PRIORITY, data32) ==
             LW_PDISP_SOR_HDMI_SPARE_ACR_PRIORITY_HIGH)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPARE(%d)_ACR_PRIORITY_HIGH\n", i);
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPARE(%d)_ACR_PRIORITY_LOW\n", i);

             //if hw_cts is enabled, ack_prioirty has to be high
             if( isHwCtsEn == true )
             {
                 Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE, but"
                     " LW_PDISP_SOR_HDMI_SPARE(%d)_ACR_PRIORITY_LOW\n", i, i);
                 //addUnitErr("\t LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE, but"
                 //    " LW_PDISP_SOR_HDMI_SPARE(%d)_ACR_PRIORITY_LOW\n", i, i);
                 status = LW_ERROR;
             }
         }

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPARE(%d)_CTS_RESET_VAL:   0x%02x\n", i,
             DRF_VAL(_PDISP, _SOR_HDMI_SPARE, _CTS_RESET_VAL, data32));

         //LW_PDISP_SOR_HDMI_ACR_CTRL(i)
         data32 = REG_RD32(LW_PDISP_SOR_HDMI_ACR_CTRL(i));

         if ( DRF_VAL(_PDISP, _SOR_HDMI_ACR_CTRL, _PACKET_ENABLE, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_PACKET_ENABLE_YES\n", i);

             //if hw_cts is enabled, all acr_ctrl have to be set to disabled
             if( isHwCtsEn == true )
             {
                 Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_PACKET_ENABLE_YES"
                     " & LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE cannot be "
                     "both enabled\n",i, i);
                 //addUnitErr("lw: Error: LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_PACKET_ENABLE_YES"
                 //    " & LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE cannot be "
                 //    "both enabled\n",i, i);
                 status = LW_ERROR;
             }
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_PACKET_ENABLE_NO\n", i);
         }

         if ( DRF_VAL(_PDISP, _SOR_HDMI_ACR_CTRL, _MEASURE_ENABLE, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_MEASURE_ENABLE_YES\n", i);

             //if hw_cts is enabled, all acr_ctrl have to be set to disabled
             if( isHwCtsEn == true )
             {
                 Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_MEASURE_ENABLE_YES"
                     " & LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE cannot be "
                     "both enabled\n",i, i);
                 //addUnitErr("lw: Error: LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_MEASURE_ENABLE_YES"
                 //    " & LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE cannot be "
                 //    "both enabled\n",i, i);
                 status = LW_ERROR;
             }
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_MEASURE_ENABLE_NO\n", i);
         }

         if ( DRF_VAL(_PDISP, _SOR_HDMI_ACR_CTRL, _FREQS_ENABLE, data32))
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_FREQS_ENABLE_YES\n", i);

             //if hw_cts is enabled, all acr_ctrl have to be set to disabled
             if( isHwCtsEn == true )
             {
                 Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_FREQS_ENABLE_YES"
                     " & LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE cannot be "
                     "both enabled\n",i, i);
                 //addUnitErr("lw: Error: LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_FREQS_ENABLE_YES"
                 //    " & LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE cannot be "
                 //    "both enabled\n",i, i);
                 status = LW_ERROR;
             }

             switch (DRF_VAL(_PDISP, _SOR_HDMI_ACR_CTRL, _FREQS, data32))
             {
             case LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_INIT:
                 Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_{INIT,48KHZ}\n");
                 break;

             case LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_32KHZ:
                 Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_32KHZ\n");
                 break;

             case LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_44_1KHZ:
                 Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_44_1KHZ\n");
                 break;

             case LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_88_2KHZ:
                 Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_88_2KHZ\n");
                 break;

             case LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_96KHZ:
                 Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_96KHZ\n");
                 break;

             case LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_176_4KHZ :
                 Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_176_4KHZ \n");
                 break;

             case LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_192KHZ :
                 Printf(Tee::PriLow,"lw: LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS_192KHZ \n");
                 break;

             default:
                 Printf(Tee::PriHigh,"lw: Error: Unknown LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS:   "
                     "0x%02x\n",DRF_VAL(_PDISP, _SOR_HDMI_ACR_CTRL, _FREQS, data32));
                 //addUnitErr("\t Unknown LW_PDISP_SOR_HDMI_ACR_CTRL_FREQS:   "
                 //    "0x%02x\n",DRF_VAL(_PDISP, _SOR_HDMI_ACR_CTRL, _FREQS, data32));
                 status = LW_ERROR;
                 break;
             }
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_CTRL(%d)_FREQS_ENABLE_NO\n", i);
         }

         data32 = REG_RD32(LW_PDISP_SOR_HDMI_ACR_0441_SUBPACK_HIGH(i));

         if ( DRF_VAL(_PDISP, _SOR_HDMI_ACR_0441_SUBPACK_HIGH, _ENABLE, data32) ==
             LW_PDISP_SOR_HDMI_ACR_0441_SUBPACK_HIGH_ENABLE_YES)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_0441_SUBPACK_HIGH(%d)_ENABLE_YES\n", i);
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_0441_SUBPACK_HIGH(%d)_ENABLE_NO\n", i);

             if( isHwCtsEn == true )
             {
                 Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE, but"
                     " LW_PDISP_SOR_HDMI_ACR_0441_SUBPACK_HIGH(%d)_ENABLE_NO\n", i, i);
                 //addUnitErr("\t LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE, but"
                 //    " LW_PDISP_SOR_HDMI_ACR_0441_SUBPACK_HIGH(%d)_ENABLE_NO\n", i, i);
                 status = LW_ERROR;
             }
         }

         //check ACR_0441_SUBPACK_HIGH_N and AUDIO_N_VALUE
         if( isHwCtsEn == true )
         {
             lwalue = DRF_VAL(_PDISP, _SOR_HDMI_ACR_0441_SUBPACK_HIGH, _N, data32);

             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_ACR_0441_SUBPACK_HIGH(%d)_N:   0x%08x\n",
                 i, lwalue);

             if ( lwalue != REG_RD_DRF(_PDISP, _AUDIO_N, _VALUE)) {

                 Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE, but"
                     " ACR_0441_SUBPACK_HIGH_N != AUDIO_N_VALUE\n", i);
                 //addUnitErr("\t LW_PDISP_SOR_HDMI_SPARE(%d)_HW_CTS_ENABLE, but"
                 //    " ACR_0441_SUBPACK_HIGH_N != AUDIO_N_VALUE\n", i);
                 status = LW_ERROR;
             }
         }
     }

     //dump audio regs
     if (isHdmiChEn == true)
     {
         data32 = REG_RD32(LW_PDISP_AUDIO_CNTRL0);

         Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_CNTRL0_ERROR_TOLERANCE:    0x%04x\n",
             DRF_VAL( _PDISP, _AUDIO_CNTRL0, _ERROR_TOLERANCE, data32));

         if (DRF_VAL( _PDISP, _AUDIO_CNTRL0, _SOFT_RESET, data32))
         {
             Printf(Tee::PriHigh,"lw: Error: LW_PDISP_AUDIO_CNTRL0_SOFT_RESET_ASSERT\n");
             //addUnitErr("\t LW_PDISP_AUDIO_CNTRL0_SOFT_RESET_ASSERT\n");
             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_CNTRL0_SOFT_RESET_DEASSERT\n");
         }

         switch (DRF_VAL(_PDISP, _AUDIO_CNTRL0, _SAMPLING_FREQ , data32))
         {
         case LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_32_0KHZ:
             Printf(Tee::PriLow,"lw: LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_32_0KHZ\n");
             break;

         case LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_44_1KHZ:
             Printf(Tee::PriLow,"lw: LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_44_1KHZ\n");
             break;

         case LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_88_2KHZ:
             Printf(Tee::PriLow,"lw: LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_88_2KHZ\n");
             break;

         case LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_176_4KHZ:
             Printf(Tee::PriLow,"lw: LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_176_4KHZ\n");
             break;

         case LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_48_0KHZ:
             Printf(Tee::PriLow,"lw: LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_48_0KHZ\n");
             break;

         case LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_96_0KHZ :
             Printf(Tee::PriLow,"lw: LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_96_0KHZ \n");
             break;

         case LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_192_0KHZ :
             Printf(Tee::PriLow,"lw: LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_192_0KHZ\n");
             break;

         case LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_UNKNOWN :
             Printf(Tee::PriLow,"lw: LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ_UNKNOWN\n");
             break;

         default:
             Printf(Tee::PriHigh,"lw: Error: Unknown LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ:"
                 "   0x%02x\n", DRF_VAL(_PDISP, _AUDIO_CNTRL0, _SAMPLING_FREQ, data32));
             //addUnitErr("\t Unknown LW_PDISP_AUDIO_CNTRL0_SAMPLING_FREQ:"
             //    "   0x%02x\n", DRF_VAL(_PDISP, _AUDIO_CNTRL0, _SAMPLING_FREQ, data32));
             status = LW_ERROR;
             break;
         }

         Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_CNTRL0_FRAMES_PER_BLOCK:   0x%04x\n",
             DRF_VAL( _PDISP, _AUDIO_CNTRL0, _FRAMES_PER_BLOCK, data32));

         //audio debug regs
         data32 = REG_RD32(LW_PDISP_AUDIO_DEBUG0);

         if (DRF_VAL( _PDISP, _AUDIO_DEBUG0, _THRESHOLD_ERROR, data32))
         {
             Printf(Tee::PriHigh,"lw: Error: LW_PDISP_AUDIO_DEBUG0_THRESHOLD_ERROR_YES\n");
             //addUnitErr("\t LW_PDISP_AUDIO_DEBUG0_THRESHOLD_ERROR_YES\n");

             //print hi/lo thresholds
             Printf(Tee::PriHigh,"lw: + LW_PDISP_AUDIO_THRESHOLD_LOW:   0x%08x\n",
                 REG_RD_DRF(_PDISP, _AUDIO_THRESHOLD, _LOW));

             Printf(Tee::PriHigh,"lw: + LW_PDISP_AUDIO_THRESHOLD_HIGH:    0x%08x\n",
                 REG_RD_DRF(_PDISP, _AUDIO_THRESHOLD, _HIGH));

             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_DEBUG0_THRESHOLD_ERROR_NO\n");
         }

         if (DRF_VAL( _PDISP, _AUDIO_DEBUG0, _FRAME_ERROR, data32))
         {
             Printf(Tee::PriHigh,"lw: Error: LW_PDISP_AUDIO_DEBUG0_FRAME_ERROR_YES\n");
             //addUnitErr("\t LW_PDISP_AUDIO_DEBUG0_FRAME_ERROR_YES\n");
             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_DEBUG0_FRAME_ERROR_NO\n");
         }

         if (DRF_VAL( _PDISP, _AUDIO_DEBUG0, _BITS_ERROR, data32))
         {
             Printf(Tee::PriHigh,"lw: Error: LW_PDISP_AUDIO_DEBUG0_BITS_ERROR_YES\n");
             //addUnitErr("\t LW_PDISP_AUDIO_DEBUG0_BITS_ERROR_YES\n");
             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_DEBUG0_BITS_ERROR_NO\n");
         }

         if (DRF_VAL( _PDISP, _AUDIO_DEBUG0, _OVERFLOW_ERROR, data32))
         {
             Printf(Tee::PriLow,"lw: Error: LW_PDISP_AUDIO_DEBUG0_OVERFLOW_ERROR_YES\n");
             //addUnitErr("\t LW_PDISP_AUDIO_DEBUG0_OVERFLOW_ERROR_YES\n");

             //print overflow counter
             Printf(Tee::PriHigh,"lw: + LW_PDISP_AUDIO_DEBUG1_OVERFLOW_TH:        0x%04x\n",
                 REG_RD_DRF(_PDISP, _AUDIO_DEBUG1, _OVERFLOW_TH));

             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_DEBUG0_OVERFLOW_ERROR_NO\n");
         }

         if (DRF_VAL( _PDISP, _AUDIO_DEBUG0, _PREAMBLE_ERROR, data32))
         {
             Printf(Tee::PriLow,"lw: Error: LW_PDISP_AUDIO_DEBUG0_PREAMBLE_ERROR_YES\n");
             //addUnitErr("\t LW_PDISP_AUDIO_DEBUG0_FRAME_PREAMBLE_YES\n");

             Printf(Tee::PriHigh,"lw: + LW_PDISP_AUDIO_DEBUG1_PACKET_NOISE_FILTER_ENABLE_%s\n",
                 DRF_VAL(_PDISP, _AUDIO_DEBUG1, _PACKET_NOISE_FILTER_ENABLE, data32) ? "YES":"NO");

             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_DEBUG0_PREAMBLE_ERROR_NO\n");
         }

         Printf(Tee::PriLow,"lw: + LW_PDISP_AUDIO_DEBUG2:               0x%08x\n",
             REG_RD32(LW_PDISP_AUDIO_DEBUG2));
     }

     for (i = 0; i < LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1__SIZE_1; i++)
     {
         //fallthrough if hdmi sublink is disabled
         if (hdmiChEn[i] == false)
         {
             continue;
         }
         data32 = REG_RD32(LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(i));

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_USE_%s\n", i,
             DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _USE, data32) ? "PRO":"CONSUMER");

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_TYPE_%s\n", i,
             DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _TYPE, data32) ? "OTHER":"PCM");

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_COPYRIGHT_%s\n", i,
             DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _COPYRIGHT, data32) ? "NO":"YES");

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_D:        0x%04x\n",
             i, DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _D, data32));

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_MODE:     0x%04x\n",
             i, DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _MODE, data32));

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_CODE:     0x%04x\n",
             i, DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _CODE, data32));

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_SOURCE:   0x%04x\n",
             i, DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _SOURCE, data32));

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_CHANNEL:  0x%04x\n",
             i, DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _CHANNEL, data32));

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_SFREQ:    0x%04x\n",
             i, DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _SFREQ, data32));

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_HDMI_SPDIF_CHN_STATUS1(%d)_ACLWRACY: 0x%04x\n",
             i, DRF_VAL(_PDISP, _SOR_HDMI_SPDIF_CHN_STATUS1, _ACLWRACY, data32));

     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispORStatus()
 //
 //-----------------------------------------------------
RC DispState::checkDispOrStatus( void )
 {
     UINT32  status = LW_OK;
     UINT32  data32;
     UINT32  reg;
     UINT32  i;

     Printf(Tee::PriLow,"\n\tlw: Checking SOR status...\n");

     for (i = 0; i < LW_PDISP_SOR_TEST__SIZE_1; i++)
     {
         reg = REG_RD32(LW_PDISP_SOR_TEST(i));

         //skip unattached sublinks
         if (DRF_VAL(_PDISP, _SOR_TEST, _ATTACHED, reg) ==
             LW_PDISP_SOR_TEST_ATTACHED_FALSE)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_TEST(%d)_ATTACHED_FALSE\n", i);
             continue;
         }

         Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_TEST(%d)_ATTACHED_TRUE\n", i);

         switch (DRF_VAL(_PDISP, _SOR_TEST, _ACT_HEAD_OPMODE, reg))
         {
         case LW_PDISP_SOR_TEST_ACT_HEAD_OPMODE_SLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_TEST(%d)_ACT_HEAD_OPMODE_SLEEP\n", i);
             break;

         case LW_PDISP_SOR_TEST_ACT_HEAD_OPMODE_SNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_TEST(%d)_ACT_HEAD_OPMODE_SNOOZE\n", i);
             break;

         case LW_PDISP_SOR_TEST_ACT_HEAD_OPMODE_AWAKE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_TEST(%d)_ACT_HEAD_OPMODE_AWAKE\n", i);
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_SOR_TEST(%d)_ACT_HEAD_OPMODE: 0x%02x\n",
                 i, DRF_VAL(_PDISP, _SOR_TEST, _ACT_HEAD_OPMODE, reg));
             //addUnitErr("\t Unknown LW_PDISP_SOR_TEST(%d)_ACT_HEAD_OPMODE: 0x%02x\n",
             //    i, DRF_VAL(_PDISP, _SOR_TEST, _ACT_HEAD_OPMODE, reg));
             status = LW_ERROR;
             break;
         }

         data32 = REG_RD32(LW_PDISP_SOR_BLANK(i));

         if ( DRF_VAL(_PDISP, _SOR_BLANK, _OVERRIDE, data32) )
         {
             Printf(Tee::PriHigh,"lw: Error: LW_PDISP_SOR_BLANK(%d)_OVERRIDE_TRUE\n", i);
             //addUnitErr("\t LW_PDISP_SOR_BLANK(%d)_OVERRIDE_TRUE\n", i);
             status = LW_ERROR;
         }

         if ( DRF_VAL(_PDISP, _SOR_BLANK, _STATUS, data32) )
         {
             Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_SOR_BLANK(%d)_STATUS_BLANKED\n", i);
             //addUnitErr("\t LW_PDISP_SOR_BLANK(%d)_STATUS_BLANKED\n", i);
             status = LW_ERROR;
         }

         //check sequencer
         data32 = REG_RD32(LW_PDISP_SOR_SEQ_CTL(i));

         if ( DRF_VAL(_PDISP, _SOR_SEQ_CTL, _STATUS, data32) ==
             LW_PDISP_SOR_SEQ_CTL_STATUS_STOPPED)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_SEQ_CTL(%d)_STATUS_STOPPED\n", i);
         }
         else
         {
             Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_SOR_SEQ_CTL(%d)_STATUS_RUNNING\n", i);
             //addUnitErr("\t LW_PDISP_SOR_SEQ_CTL(%d)_STATUS_RUNNING\n", i);
             status = LW_ERROR;
         }

         data32 = REG_RD32(LW_PDISP_SOR_PWR(i));

         Printf(Tee::PriLow,"lw: LW_PDISP_SOR_PWR(%d):      0x%08x\n", i, data32);

         if ( DRF_VAL(_PDISP, _SOR_PWR, _MODE, data32) ==
             LW_PDISP_SOR_PWR_MODE_SAFE)
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_PWR(%d)_MODE_SAFE\n", i);
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_SOR_PWR(%d)_MODE_NORMAL\n", i);
     }

     //dump more useful regs
     for (i = 0; i < LW_PDISP_SOR_TEST__SIZE_1; i++)
     {
         Printf(Tee::PriLow,"lw: LW_PDISP_SOR_PLL0(%d):     0x%08x\n",
             i, REG_RD32(LW_PDISP_SOR_PLL0(i)));
         Printf(Tee::PriLow,"lw: LW_PDISP_SOR_PLL1(%d):     0x%08x\n",
             i, REG_RD32(LW_PDISP_SOR_PLL1(i)));

         Printf(Tee::PriLow,"lw: LW_PDISP_SOR_CAP(%d):      0x%08x\n",
             i, REG_RD32(LW_PDISP_SOR_CAP(i)));
     }

     Printf(Tee::PriLow,"\n\tlw: Checking PIOR status...\n");

     for (i = 0; i < LW_PDISP_PIOR_TEST__SIZE_1; i++)
     {
         reg = REG_RD32(LW_PDISP_PIOR_TEST(i));

         //skip unattached sublinks
         if (DRF_VAL(_PDISP, _PIOR_TEST, _ATTACHED, reg) ==
             LW_PDISP_PIOR_TEST_ATTACHED_FALSE)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_PIOR_TEST(%d)_ATTACHED_false\n", i);
             continue;
         }

         Printf(Tee::PriLow,"lw: + LW_PDISP_PIOR_TEST(%d)_ATTACHED_TRUE\n", i);

         switch (DRF_VAL(_PDISP, _PIOR_TEST, _ACT_HEAD_OPMODE, reg))
         {
         case LW_PDISP_PIOR_TEST_ACT_HEAD_OPMODE_SLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_PIOR_TEST(%d)_ACT_HEAD_OPMODE_SLEEP\n", i);
             break;

         case LW_PDISP_PIOR_TEST_ACT_HEAD_OPMODE_SNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_PIOR_TEST(%d)_ACT_HEAD_OPMODE_SNOOZE\n", i);
             break;

         case LW_PDISP_PIOR_TEST_ACT_HEAD_OPMODE_AWAKE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_PIOR_TEST(%d)_ACT_HEAD_OPMODE_AWAKE\n", i);
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_PIOR_TEST(%d)_ACT_HEAD_OPMODE: 0x%02x\n",
                 i, DRF_VAL(_PDISP, _PIOR_TEST, _ACT_HEAD_OPMODE, reg));
             //addUnitErr("\t Unknown LW_PDISP_PIOR_TEST(%d)_ACT_HEAD_OPMODE: 0x%02x\n",
             //    i, DRF_VAL(_PDISP, _PIOR_TEST, _ACT_HEAD_OPMODE, reg));
             status = LW_ERROR;
             break;
         }

         data32 = REG_RD32(LW_PDISP_PIOR_BLANK(i));

         if ( DRF_VAL(_PDISP, _PIOR_BLANK, _OVERRIDE, data32) )
         {
             Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_PIOR_BLANK(%d)_OVERRIDE_TRUE\n", i);
             //addUnitErr("\t LW_PDISP_PIOR_BLANK(%d)_OVERRIDE_TRUE\n", i);
             status = LW_ERROR;
         }

         if ( DRF_VAL(_PDISP, _PIOR_BLANK, _STATUS, data32) )
         {
             Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_PIOR_BLANK(%d)_STATUS_BLANKED\n", i);
             //addUnitErr("\t LW_PDISP_PIOR_BLANK(%d)_STATUS_BLANKED\n", i);
             status = LW_ERROR;
         }

         //check sequencer
         data32 = REG_RD32(LW_PDISP_PIOR_SEQ_CTL(i));

         if ( DRF_VAL(_PDISP, _PIOR_SEQ_CTL, _STATUS, data32) ==
             LW_PDISP_PIOR_SEQ_CTL_STATUS_STOPPED)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_PIOR_SEQ_CTL(%d)_STATUS_STOPPED\n", i);
         }
         else
         {
             Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_PIOR_SEQ_CTL(%d)_STATUS_RUNNING\n", i);
             //addUnitErr("\t LW_PDISP_PIOR_SEQ_CTL(%d)_STATUS_RUNNING\n", i);
             status = LW_ERROR;
         }

         data32 = REG_RD32(LW_PDISP_PIOR_PWR(i));

         Printf(Tee::PriLow,"lw: LW_PDISP_PIOR_PWR(%d):      0x%08x\n", i, data32);

         if ( DRF_VAL(_PDISP, _PIOR_PWR, _MODE, data32) ==
             LW_PDISP_PIOR_PWR_MODE_SAFE)
             Printf(Tee::PriLow,"lw: + LW_PDISP_PIOR_PWR(%d)_MODE_SAFE\n", i);
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_PIOR_PWR(%d)_MODE_NORMAL\n", i);

     }

     for (i = 0; i < LW_PDISP_PIOR_TEST__SIZE_1; i++)
     {
         Printf(Tee::PriLow,"lw: LW_PDISP_PIOR_CAP(%d):      0x%08x\n",
             i, REG_RD32(LW_PDISP_PIOR_CAP(i)));
     }

     Printf(Tee::PriLow,"\n\tlw: Checking DAC status...\n");

     for (i = 0; i < LW_PDISP_DAC_TEST__SIZE_1; i++)
     {
         reg = REG_RD32(LW_PDISP_DAC_TEST(i));

         //skip unattached sublinks
         if (DRF_VAL(_PDISP, _DAC_TEST, _ATTACHED, reg) ==
             LW_PDISP_DAC_TEST_ATTACHED_FALSE)
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_DAC_TEST(%d)_ATTACHED_false\n", i);
             continue;
         }

         Printf(Tee::PriLow,"lw: + LW_PDISP_DAC_TEST(%d)_ATTACHED_TRUE\n", i);

         switch (DRF_VAL(_PDISP, _DAC_TEST, _ACT_HEAD_OPMODE, reg))
         {
         case LW_PDISP_DAC_TEST_ACT_HEAD_OPMODE_SLEEP:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DAC_TEST(%d)_ACT_HEAD_OPMODE_SLEEP\n", i);
             break;

         case LW_PDISP_DAC_TEST_ACT_HEAD_OPMODE_SNOOZE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DAC_TEST(%d)_ACT_HEAD_OPMODE_SNOOZE\n", i);
             break;

         case LW_PDISP_DAC_TEST_ACT_HEAD_OPMODE_AWAKE:
             Printf(Tee::PriLow,"lw: + LW_PDISP_DAC_TEST(%d)_ACT_HEAD_OPMODE_AWAKE\n", i);
             break;

         default:
             Printf(Tee::PriHigh,"lw: + Unknown LW_PDISP_DAC_TEST(%d)_ACT_HEAD_OPMODE: 0x%02x\n",
                 i, DRF_VAL(_PDISP, _DAC_TEST, _ACT_HEAD_OPMODE, reg));
             //addUnitErr("\t Unknown LW_PDISP_DAC_TEST(%d)_ACT_HEAD_OPMODE: 0x%02x\n",
             //    i, DRF_VAL(_PDISP, _DAC_TEST, _ACT_HEAD_OPMODE, reg));
             status = LW_ERROR;
             break;
         }

         data32 = REG_RD32(LW_PDISP_DAC_BLANK(i));

         if ( DRF_VAL(_PDISP, _DAC_BLANK, _OVERRIDE, data32) )
         {
             Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_DAC_BLANK(%d)_OVERRIDE_TRUE\n", i);
             //addUnitErr("\t LW_PDISP_DAC_BLANK(%d)_OVERRIDE_TRUE\n", i);
             status = LW_ERROR;
         }

         if ( DRF_VAL(_PDISP, _DAC_BLANK, _STATUS, data32) )
         {
             Printf(Tee::PriHigh,"lw: + Error: LW_PDISP_DAC_BLANK(%d)_STATUS_BLANKED\n", i);
             //addUnitErr("\t LW_PDISP_DAC_BLANK(%d)_STATUS_BLANKED\n", i);
             status = LW_ERROR;
         }

         data32 = REG_RD32(LW_PDISP_DAC_PWR(i));
         Printf(Tee::PriLow,"lw: LW_PDISP_DAC_PWR(%d):      0x%08x\n", i, data32);

         if ( DRF_VAL(_PDISP, _DAC_PWR, _MODE, data32) ==
             LW_PDISP_DAC_PWR_MODE_SAFE)
             Printf(Tee::PriLow,"lw: + LW_PDISP_DAC_PWR(%d)_MODE_SAFE\n", i);
         else
             Printf(Tee::PriLow,"lw: + LW_PDISP_DAC_PWR(%d)_MODE_NORMAL\n", i);

     }

     for (i = 0; i < LW_PDISP_DAC_TEST__SIZE_1; i++)
     {
         Printf(Tee::PriLow,"lw: LW_PDISP_DAC_CAP(%d):      0x%08x\n",
             i, REG_RD32(LW_PDISP_DAC_CAP(i)));
     }

     return status;
 }

 //-----------------------------------------------------
 // checkDispUnderflow()
 //
 //-----------------------------------------------------
 RC DispState:: checkDispUnderflow( void )
 {
     RC status;

     UINT32  data32;
     UINT32  i;

     Printf(Tee::PriLow,"\n\tlw: Checking underflows...\n");

     for (i = 0; i < LW_PDISP_RG_UNDERFLOW__SIZE_1; i++)
     {
         data32 = REG_RD32(LW_PDISP_RG_UNDERFLOW(i));

         Printf(Tee::PriLow,"lw: + LW_PDISP_RG_UNDERFLOW(%d)_ENABLE_%s\n", i,
             DRF_VAL(_PDISP, _RG_UNDERFLOW, _ENABLE, data32) ? "ENABLE":"DISABLE");

         if (DRF_VAL( _PDISP, _RG_UNDERFLOW, _UNDERFLOWED, data32))
         {
             Printf(Tee::PriHigh,"lw: + LW_PDISP_RG_UNDERFLOW(%d)_UNDERFLOWED_YES\n", i);
             status = RC::LWRM_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: + LW_PDISP_RG_UNDERFLOW(%d)_UNDERFLOWED_NO\n", i);
         }

         Printf(Tee::PriLow,"lw: + LW_PDISP_RG_UNDERFLOW(%d)_MODE_%s\n", i,
             DRF_VAL( _PDISP, _RG_UNDERFLOW, _MODE, data32) ? "RED": "REPEAT");

         //print status information from the RG
         Printf(Tee::PriLow,"lw: + LW_PDISP_RG_STATUS(%d):      0x%08x\n", i,
             REG_RD32(LW_PDISP_RG_STATUS(i)));
     }

     return status;
 }

//!-----------------------------------------------------
//! SetUnderflowState
//! brief: This sets the underflow state for the given head
//! params  head[in]: The head no to set underflow proerty on.
//! params  enable[in]: Enable underflow reporting
//! params  clearUnderflow[in]:
//! params  mode[in]:
//!-----------------------------------------------------
RC DispState::SetUnderflowState(UINT32 head,
                                UINT32 enable,
                                UINT32 clearUnderflow,
                                UINT32 mode)
{
    LwRmPtr pLwRm;
    LW5070_CTRL_CMD_UNDERFLOW_PARAMS underflowProp =
    {
        UINT32(head),
        UINT32(enable),
        UINT32(clearUnderflow),
        UINT32(mode)
    };
    LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP_PARAMS params =
    {
        {UINT32(0)},
        LW5070_CTRL_CMD_UNDERFLOW_PARAMS(underflowProp)
    };

    return pLwRm->Control(pLwRm->GetDeviceHandle(m_pSubdev->GetParentDevice()),
                          LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP,
                          (void*)&params,
                          sizeof(params));
}

//TBD Check of DispClocks is disabled now ..
// clkGetDispclkFreqKHz func need to be wriiten
// //-----------------------------------------------------
// // clkGetPclkFreqKHz()
// //
// //-----------------------------------------------------
// INT32 DispState:: clkGetPclkFreqKHz( UINT32 rgId, UINT32 vpllNum )
// {
//     INT32   vpllFreq;
//     INT32   pclk;
//     UINT32  data32;
//     UINT32  rgDiv;
//
//     vpllFreq = clkGetVpllFreqKHz_GT212( vpllNum );
//
//     data32 = REG_RD32(LW_PDISP_CLK_REM_RG( rgId ));
//
//     if (DRF_VAL(_PDISP, _CLK_REM_RG, _STATE, data32))
//     {
//         DPRINTF("lw: + LW_PDISP_CLK_REM_RG(%d)_STATE_ENABLE\n", rgId);
//     }
//     else
//     {
//         DPRINTF("lw: Error: + LW_PDISP_CLK_REM_RG(%d)_STATE_DISABLE\n", rgId);
//         return -1;
//     }
//
//     rgDiv = DRF_VAL(_PDISP, _CLK_REM_RG, _DIV, data32) + 1;
//
//     if (rgDiv != 0)
//     {
//         pclk = vpllFreq / rgDiv;
//         DPRINTF("lw: pclk%d == %d KHz RG_DIV: %d\n\n", rgId, pclk, rgDiv);
//     }
//     else
//     {
//         DPRINTF("lw: Error: LW_PDISP_CLK_REM_RG%d_DIV == 0\n", rgId);
//         return -1;
//     }
//
//     return pclk;
// }

 //-----------------------------------------------------
 // checkDispClocks()
 //
 //-----------------------------------------------------
 RC DispState:: checkDispClocks(void)
 {
     UINT32  status = LW_OK;
     UINT32  dClkRem;
     UINT32  dClkLcl;
     UINT32  lwrRemRatio;
     UINT32  lwrLclRatio;
     UINT32  data32;
     INT32   dispclk = 0;
     INT32   pclk = 0;
     UINT32  i;
     UINT32  vpllRef;

     Printf(Tee::PriLow,"\n\tlw: Checking Display clocks status...\n");

     dClkRem = REG_RD32(LW_PDISP_CLK_REM_CONFIG);
     dClkLcl = REG_RD32(LW_PDISP_CLK_LCL_CONFIG);

     //check if lcl/remote clocks are running in same mode and ratio
     if ( DRF_VAL(_PDISP, _CLK_REM_CONFIG, _DCLK_MODE, dClkRem) !=
         DRF_VAL(_PDISP, _CLK_REM_CONFIG, _DCLK_MODE, dClkLcl))
     {
         Printf(Tee::PriHigh,"lw: Error: LW_PDISP_CLK_REM_CONFIG_DCLK_MODE != "
             "LW_PDISP_CLK_LCL_CONFIG_DCLK_MODE\n");
         //addUnitErr("\t LW_PDISP_CLK_REM_CONFIG_DCLK_MODE != "
         //    "LW_PDISP_CLK_LCL_CONFIG_DCLK_MODE\n");
         status = LW_ERROR;
     }

     if ( DRF_VAL(_PDISP, _CLK_REM_CONFIG, _DCLK_MODE, dClkRem) ==
         LW_PDISP_CLK_REM_CONFIG_DCLK_MODE_NRML)
     {
         lwrRemRatio  = DRF_VAL(_PDISP, _CLK_REM_CONFIG, _DCLK_NRML, dClkRem);
         lwrLclRatio  = DRF_VAL(_PDISP, _CLK_LCL_CONFIG, _DCLK_NRML, dClkLcl);
     }
     else
     {
         lwrRemRatio  = DRF_VAL(_PDISP, _CLK_REM_CONFIG, _DCLK_FAST, dClkRem);
         lwrLclRatio  = DRF_VAL(_PDISP, _CLK_LCL_CONFIG, _DCLK_FAST, dClkLcl);

         //DCLK_FAST should only be operated at full speed
         if (lwrRemRatio != LW_PDISP_CLK_REM_CONFIG_DCLK_FAST_FULL_SPEED)
         {
             Printf(Tee::PriHigh,"lw: Error: LW_PDISP_CLK_REM_CONFIG_DCLK_FAST is not at "
                 "full speed \n");
             //addUnitErr("\t LW_PDISP_CLK_REM_CONFIG_DCLK_FAST is not at "
             //    "full speed\n");
             status = LW_ERROR;
         }

         if (lwrLclRatio != LW_PDISP_CLK_LCL_CONFIG_DCLK_FAST_FULL_SPEED)
         {
             Printf(Tee::PriHigh,"lw: Error: LW_PDISP_CLK_LCL_CONFIG_DCLK_FAST is not at "
                 "full speed \n");
             //addUnitErr("\t LW_PDISP_CLK_LCL_CONFIG_DCLK_FAST is not at "
             //    "full speed\n");
             status = LW_ERROR;
         }
     }

     if (lwrRemRatio != lwrLclRatio)
     {
         Printf(Tee::PriHigh,"lw: Local & remote dispclk do not match. Lcl: 0x%02x."
             "   Remote: 0x%02x\n", lwrLclRatio, lwrRemRatio);
         //addUnitErr("\t Local & remote dispclk do not match. Lcl: 0x%02x."
         //    "   Remote: 0x%02x\n", lwrLclRatio, lwrRemRatio);
         status = LW_ERROR;
     }

     //compare dispclk and pclk
     // TBD Get the DispClock Need to write code here
     // Need to Get the DispClock Frequency Here

     //dispclk = clkGetDispclkFreqKHz();
     if (dispclk < 0)
     {
         Printf(Tee::PriHigh,"lw: Error: Unable to get dispClk\n");
         //addUnitErr("\t Unable to get dispClk\n");
         return LW_ERROR;
     }

     Printf(Tee::PriLow,"lw: dispclk == %d KHz\n\n",  dispclk);

     for (i = 0; i < LW_PDISP_CLK_REM_RG__SIZE_1; i++)
     {

         data32 = REG_RD32(LW_PDISP_CHNCTL_CORE(i));

         //skip if not connected or allocated
         if ( !DRF_VAL( _PDISP, _CHNCTL_CORE, _ALLOCATION, data32) ||
             !DRF_VAL( _PDISP, _CHNCTL_CORE, _CONNECTION, data32))
         {
             Printf(Tee::PriLow,"lw: + Channel %d not allocated or connected: skipping "
                 "dispclk < pclk test for this channel\n", i);
             continue;
         }

         //check which vpll sources each channel
         if (i == 0)
         {
             vpllRef = DRF_VAL(_PDISP, _CLK_REM_CONFIG, _EXT_REFCLKA_SRC, dClkRem);
             Printf(Tee::PriLow,"lw: + Channel %d pclk is sourced from vpll%d\n", i, vpllRef);
         }
         else
         {
             vpllRef = DRF_VAL(_PDISP, _CLK_REM_CONFIG, _EXT_REFCLKB_SRC, dClkRem);
             Printf(Tee::PriLow,"lw: + Channel %d pclk is sourced from vpll%d\n", i, vpllRef);
         }

      //   pclk = clkGetPclkFreqKHz( i, vpllRef );

         if (pclk < 0)
         {
             Printf(Tee::PriHigh,"lw: Error: Unable to get pclk\n");
             //addUnitErr("\t Unable to get pclk\n");
             status = LW_ERROR;
             continue;
         }

         if( dispclk * 0.75 < pclk )
         {
             Printf(Tee::PriHigh,"lw: Error: dispclk (%d KHz) < pclk%d (%d KHz) by more"
                 " then 25%%\n", dispclk, i, pclk);
             //addUnitErr("\t dispclk (%d KHz) < pclk%d (%d KHz) by more"
             //    " then 25%%\n", dispclk, i, pclk);
             status = LW_ERROR;
         }
         else
         {
             Printf(Tee::PriLow,"lw: dispclk = %d KHz, pclk%d = %d KHz\n", dispclk, i, pclk);
         }
     }
     return status;
 }

 //-----------------------------------------------------
 // checkDispExceptions()
 //
 //-----------------------------------------------------

RC DispState::checkDispExceptions( UINT32 chid )
  {
      RC    status = LW_OK;
      UINT32 data32;

      data32 = REG_RD32(LW_PDISP_EXCEPT(chid));

      Printf(Tee::PriLow,"\n\tlw: Checking Exceptions...\n");
      Printf(Tee::PriLow,"lw: LW_PDISP_EXCEPT(%d):                    0x%08x\n", chid, data32);

      Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_METHOD_OFFSET:       0x%08x\n",
          DRF_VAL( _PDISP,_EXCEPT, _METHOD_OFFSET, data32) );

      switch (DRF_VAL( _PDISP,_EXCEPT, _REASON, data32))
      {
      case LW_PDISP_EXCEPT_REASON_PUSHBUFFER_ERR:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_REASON_PUSHBUFFER_ERR\n");
          //addUnitErr("\t LW_PDISP_EXCEPT_REASON_PUSHBUFFER_ERR\n");
          break;

      case LW_PDISP_EXCEPT_REASON_TRAP:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_REASON_TRAP\n");
          //addUnitErr("\t LW_PDISP_EXCEPT_REASON_TRAP\n");
          break;

      case LW_PDISP_EXCEPT_REASON_RESERVED_METHOD:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_REASON_RESERVED_METHOD\n");
          //addUnitErr("\t LW_PDISP_EXCEPT_REASON_RESERVED_METHOD\n");
          break;

      case LW_PDISP_EXCEPT_REASON_ILWALID_ARG:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_REASON_ILWALID_ARG\n");
          //addUnitErr("\t LW_PDISP_EXCEPT_REASON_ILWALID_ARG\n");
          break;

      case LW_PDISP_EXCEPT_REASON_ILWALID_STATE:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_REASON_ILWALID_STATE\n");
          //addUnitErr("\t LW_PDISP_EXCEPT_REASON_ILWALID_STATE\n");
          break;

      case LW_PDISP_EXCEPT_REASON_UNRESOLVABLE_HANDLE:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_REASON_UNRESOLVABLE_HANDLE\n");
          //addUnitErr("\t LW_PDISP_EXCEPT_REASON_UNRESOLVABLE_HANDLE\n");
          break;

      default:
          Printf(Tee::PriHigh,"lw: + Unkown LW_PDISP_EXCEPT_REASON: 0x%04x\n",
              DRF_VAL( _PDISP,_EXCEPT, _REASON, data32));
          //addUnitErr("\t Unkown LW_PDISP_EXCEPT_REASON: 0x%04x\n",
          //    DRF_VAL( _PDISP,_EXCEPT, _REASON, data32));
          status = LW_ERROR;
          break;
      }

      switch (DRF_VAL( _PDISP,_EXCEPT, _ERRCODE, data32))
      {
      case LW_PDISP_EXCEPT_ERRCODE_NONE:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_NONE\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507A_ILWALID_01:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_ {507A_ILWALID_01, 507C_ILWALID_INTERNAL,"
              "507D_ILWALID_CONFIGURATION, 507E_ILWALID_INTERNAL}\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507A_ILWALID_CHECK :
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_507{A, B, C, D}_"
              "{ILWALID_CHECK, ILWALID_ENUM_RANGE}\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507C_ILWALID_CORE_CONSISTENCY :
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_{507C_ILWALID_CORE_CONSISTENCY,"
              " 507D_ILWALID_LUT_VIOLATION, 507E_ILWALID_CORE_CONSISTENCY}\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507C_ILWALID_IMPROPER_SURFACE_SPECIFICATION:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_{507C_ILWALID_IMPROPER_SURFACE_SPECIFICATION,"
              " 507D_ILWALID_LOCKING_VIOLATION, 507E_ILWALID_IMPROPER_SURFACE_SPECIFICATION\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507C_ILWALID_IMPROPER_USAGE:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_{507C_ILWALID_IMPROPER_USAGE,"
              " 507D_ILWALID_INTERNAL, 507E_ILWALID_IMPROPER_USAGE\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507C_ILWALID_LUT_VIOLATION :
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_507{C, E}_ILWALID_LUT_VIOLATION \n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507C_ILWALID_NOTIFIER_VIOLATION :
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_507{C, D, E}_ILWALID_NOTIFIER_VIOLATION\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507C_ILWALID_SEMAPHORE_VIOLATION:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_{507C_ILWALID_SEMAPHORE_VIOLATION,"
              " 507D_ILWALID_BLOCKING_VIOLATION, 507E_ILWALID_SEMAPHORE_VIOLATION\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507C_ILWALID_SURFACE_VIOLATES_CONTEXT_DMA:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_{507C_ILWALID_SURFACE_VIOLATES_CONTEXT_DMA,"
              "507D_ILWALID_ILWALID_RASTER, 507E_ILWALID_SURFACE_VIOLATES_CONTEXT_DMA\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_LWRSOR_VIOLATION:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_LWRSOR_VIOLATION\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_IMPROPER_SURFACE_SPECIFICATION:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_IMPROPER_SURFACE_SPECIFICATION\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_ISO_VIOLATION:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_ISO_VIOLATION\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_SCALING_VIOLATION:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_SCALING_VIOLATION\n");
          break;

      case LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_VIEWPORT_VIOLATION:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_ERRCODE_507D_ILWALID_VIEWPORT_VIOLATION\n");
          break;

      default:
          Printf(Tee::PriHigh,"lw: + Unkown LW_PDISP_EXCEPT_ERRCODE:      0x%04x\n",
              DRF_VAL( _PDISP,_EXCEPT, _ERRCODE, data32));
          status = LW_ERROR;
          break;
      }

      switch (DRF_VAL( _PDISP,_EXCEPT, _RESTART_MODE, data32))
      {
      case LW_PDISP_EXCEPT_RESTART_MODE_RESUME:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_RESTART_MODE_RESUME\n");
          break;

      case LW_PDISP_EXCEPT_RESTART_MODE_SKIP:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_RESTART_MODE_SKIP\n");
          break;

      case LW_PDISP_EXCEPT_RESTART_MODE_REPLAY:
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_RESTART_MODE_REPLAY\n");
          break;
      default:
          Printf(Tee::PriHigh,"lw: + Unkown LW_PDISP_EXCEPT_RESTART_MODE: 0x%04x\n",
              DRF_VAL( _PDISP,_EXCEPT, _RESTART_MODE, data32));
          status = LW_ERROR;
          break;
      }

      if ( DRF_VAL( _PDISP, _EXCEPT, _RESTART, data32) ==
          LW_PDISP_EXCEPT_RESTART_PENDING)
      {
          Printf(Tee::PriLow,"lw: + LW_PDISP_EXCEPT_RESTART_PENDING\n");
      }

      //get argument of the method causing the exception
      data32 = REG_RD32(LW_PDISP_EXCEPTARG(chid));

      Printf(Tee::PriLow,"lw: LW_PDISP_EXCEPTARG(%d)_RDARG:           0x%08x\n",
          chid, DRF_VAL(_PDISP,_EXCEPTARG, _RDARG, data32));

      return status;
  }

//-----------------------------------------------------
// checkDispInterrupts()
//
//-----------------------------------------------------
RC DispState::checkDispInterrupts( void )
{
    RC      status = LW_OK;
    UINT32  regIntr;
    UINT32  regIntrEn0;
    UINT32  regIntrEn1;
    UINT32  i;

    regIntrEn0 = REG_RD32(LW_PDISP_INTR_EN_0);
    regIntrEn1 = REG_RD32(LW_PDISP_INTR_EN_1);
    regIntr = REG_RD32(LW_PDISP_INTR_0) & regIntrEn0;

    Printf(Tee::PriLow,"\n\tlw: Checking Interrupts...\n");

    //print disabled interrupts
    for (i = 0; i < LW_PDISP_INTR_EN_0_CHN_AWAKEN__SIZE_1; i++)
    {
        if (DRF_IDX_VAL(_PDISP, _INTR_EN_0_CHN, _AWAKEN, i, regIntrEn0) ==
            LW_PDISP_INTR_EN_0_CHN_AWAKEN_DISABLE)
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_INTR_EN_0_CHN_%d_AWAKEN_DISABLE\n", i);
        }
    }

    for (i = 0; i < LW_PDISP_INTR_EN_0_CHN_EXCEPTION__SIZE_1; i++)
    {
        if (DRF_IDX_VAL(_PDISP, _INTR_EN_0_CHN, _EXCEPTION, i, regIntrEn0) ==
            LW_PDISP_INTR_EN_0_CHN_EXCEPTION_DISABLE)
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_INTR_EN_0_CHN_%d_EXCEPTION_DISABLE\n", i);
        }
    }

    //INTR_1
    for (i = 0; i < LW_PDISP_INTR_EN_1_HEAD_VBLANK__SIZE_1 ; i++)
    {
        if (DRF_IDX_VAL(_PDISP, _INTR_EN_1_HEAD, _VBLANK, i, regIntrEn1) ==
            LW_PDISP_INTR_EN_1_HEAD_VBLANK_DISABLE)
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_INTR_EN_1_HEAD_%d_VBLANK_DISABLE\n", i);
        }
    }

    for (i = 0; i < LW_PDISP_INTR_EN_1_SUPERVISOR__SIZE_1 ; i++)
    {
        if (DRF_IDX_VAL(_PDISP, _INTR_EN_1, _SUPERVISOR, i, regIntrEn1) ==
            LW_PDISP_INTR_EN_1_SUPERVISOR_DISABLE  )
        {
            Printf(Tee::PriLow,"lw: + LW_PDISP_INTR_EN_1_SUPERVISOR%d_DISABLE\n", i);
        }
    }

    if (DRF_VAL(_PDISP, _INTR_EN_1, _VBIOS_RELEASE, regIntrEn0) ==
        LW_PDISP_INTR_EN_1_VBIOS_RELEASE_DISABLE)
    {
        Printf(Tee::PriLow,"lw: + LW_PDISP_INTR_EN_1_VBIOS_RELEASE_DISABLE\n");
    }

    if (DRF_VAL(_PDISP, _INTR_EN_1, _VBIOS_ATTENTION, regIntrEn0) ==
        LW_PDISP_INTR_EN_1_VBIOS_ATTENTION_DISABLE)
    {
        Printf(Tee::PriLow,"lw: + LW_PDISP_INTR_EN_1_VBIOS_ATTENTION_DISABLE\n");
    }

    //check interrupts
    Printf(Tee::PriLow,"lw: LW_PDISP_INTR_0:   0x%08x\n", regIntr);

    for (i = 0; i < LW_PDISP_INTR_0_CHN_AWAKEN__SIZE_1; i++)
    {
        if (DRF_IDX_VAL(_PDISP, _INTR_0_CHN, _AWAKEN, i, regIntr) ==
            LW_PDISP_INTR_0_CHN_AWAKEN_PENDING)
        {
            //not an error
            Printf(Tee::PriLow,"lw: + LW_PDISP_INTR_0_CHN_%d_AWAKEN_PENDING\n", i);
        }
    }

    for (i = 0; i < LW_PDISP_INTR_0_CHN_EXCEPTION__SIZE_1; i++)
    {
        if (DRF_IDX_VAL(_PDISP, _INTR_0_CHN, _EXCEPTION, i, regIntr) ==
            LW_PDISP_INTR_0_CHN_EXCEPTION_PENDING)
        {
            Printf(Tee::PriHigh,"lw: + LW_PDISP_INTR_0_CHN_%d_EXCEPTION_PENDING\n", i);
            ////addUnitErr("\t LW_PDISP_INTR_0_CHN_%d_EXCEPTION_PENDING\n", i);
            status = LW_ERROR;

            //if set excetion set, check EXCEPT regs for this channel
            // just disabling tis time.
             checkDispExceptions( i );
        }
    }

    //INTR_1
    regIntr = REG_RD32(LW_PDISP_INTR_1) & regIntrEn1;

    Printf(Tee::PriLow,"lw: LW_PDISP_INTR_1:   0x%08x\n", regIntr);

    //do not report LW_PDISP_INTR_1_HEAD_VBLANK_PENDING

    for (i = 0; i < LW_PDISP_INTR_1_SUPERVISOR__SIZE_1 ; i++)
    {
        if (DRF_IDX_VAL(_PDISP, _INTR_1, _SUPERVISOR, i, regIntr) ==
            LW_PDISP_INTR_1_SUPERVISOR_PENDING  )
        {
            //not an error
            Printf(Tee::PriLow,"lw: + LW_PDISP_INTR_1_SUPERVISOR%d_PENDING\n", i);
        }
    }

    if (DRF_VAL(_PDISP, _INTR_1, _VBIOS_RELEASE, regIntr) ==
        LW_PDISP_INTR_1_VBIOS_RELEASE_PENDING)
    {
        Printf(Tee::PriHigh,"lw: + LW_PDISP_INTR_1_VBIOS_RELEASE_PENDING\n");
        ////addUnitErr("\t LW_PDISP_INTR_1_VBIOS_RELEASE_PENDING\n");
        status = LW_ERROR;
    }

    if (DRF_VAL(_PDISP, _INTR_1, _VBIOS_ATTENTION, regIntr) ==
        LW_PDISP_INTR_1_VBIOS_ATTENTION_PENDING)
    {
        Printf(Tee::PriHigh,"lw: + LW_PDISP_INTR_1_VBIOS_ATTENTION_PENDING\n");
        ////addUnitErr("\t LW_PDISP_INTR_1_VBIOS_ATTENTION_PENDING\n");
        status = LW_ERROR;
    }

    return status;
}

//-----------------------------------------------------
// dispTestDisplayState()
//
//-----------------------------------------------------
RC DispState::dispTestDisplayState(void)
{
    RC    status;

    if ((status = checkDispInterrupts()) == LW_ERROR)
    {
        Printf(Tee::PriHigh,"lw: *** Display interrupts/exceptions are pending\n");
        ////addUnitErr("\t Display interrupts/exceptions are pending\n");
    }
    else
    {
        Printf(Tee::PriLow,"lw: *** No Display interrupts/exceptions are pending\n");
    }

    if (checkDispChnCtlCore() == LW_ERROR)
    {
        Printf(Tee::PriHigh,"lw: *** LW_PDISP_CHNCTL_CORE status is invalid\n");
        //addUnitErr("\t LW_PDISP_CHNCTL_CORE status is invalid\n");
        status = LW_ERROR;
    }
    else
    {
        Printf(Tee::PriLow,"lw: *** LW_PDISP_CHNCTL_CORE status OK\n");
    }

     if (checkDispChnCtlBase() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** LW_PDISP_CHNCTL_BASE status is invalid\n");
         //addUnitErr("\t LW_PDISP_CHNCTL_BASE status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** LW_PDISP_CHNCTL_BASE status OK\n");
     }

     if (checkDispChnCtlOvly() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** LW_PDISP_CHNCTL_OVLY status is invalid\n");
         //addUnitErr("\t LW_PDISP_CHNCTL_OVLY status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** LW_PDISP_CHNCTL_OVLY status OK\n");
     }

     if (checkDispChnCtlOvim() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** LW_PDISP_CHNCTL_OVIM status is invalid\n");
         //addUnitErr("\t LW_PDISP_CHNCTL_OVIM status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** LW_PDISP_CHNCTL_OVIM status OK\n");
     }

     if (checkDispChnCtlLwrs() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** LW_PDISP_CHNCTL_LWRS status is invalid\n");
         //addUnitErr("\t LW_PDISP_CHNCTL_LWRS status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** LW_PDISP_CHNCTL_LWRS status OK\n");
     }

     if (checkDispDMAState() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** DMA status is invalid\n");
         //addUnitErr("\t DMA status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** DMA status OK\n");
     }

     if (checkDispCoreUpdateFSM() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** Core Update FSM is in invalid state\n");
         //addUnitErr("\t Core Update FSM is in invalid state\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** Core Update FSM status OK\n");
     }

     if (checkDispDmiStatus() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** DMI status is invalid\n");
         //addUnitErr("\t DMI status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** DMI status OK\n");
     }

     if (checkDispOrStatus() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** OR status is invalid\n");
         //addUnitErr("\t OR status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** OR status OK\n");
     }

     if (checkDispHdmiStatus() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** HDMI status is invalid\n");
         //addUnitErr("\t HDMI status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** HDMI status OK\n");
     }

     if (checkDispDpStatus() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** DP status is invalid\n");
         //addUnitErr("\t DP status is invalid\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** DP status OK\n");
     }

     if (checkDispUnderflow() == LW_ERROR)
     {
         Printf(Tee::PriHigh,"lw: *** Display underflowed! \n");
         //addUnitErr("\t Display underflowed\n");
         status = LW_ERROR;
     }
     else
     {
         Printf(Tee::PriLow,"lw: *** Display underflow status OK\n");
     }

//Check for dispClock has to be done haven't done yet

//      if (checkDispClocks() == LW_ERROR)
//      {
//          Printf(Tee::PriLow,"lw: *** Display clocks in invalid state! \n");
//          //addUnitErr("\t Display clocks in invalid state\n");
//          status = LW_ERROR;
//      }
//      else
//      {
//          Printf(Tee::PriLow,"lw: *** Display clocks OK\n");
//      }

    //TODO: check IMP status:
    // -Dump out modesets :pIMPLastModeset,
    // -Dump mempool registers and check that limits for each head are valid  this has to be done
    // -Check SMU is above the minimum perf level required (MCP only) MCPs i Gpu s but its not specific so we ca leave
    // BTW good utility will found the Bugs
    //

    return status;
}

//
// Code is Written for DAnalyze Blank
// code is not in Active path need to Enable this as Part of DispStateCheck
// Common function for the danalyzeblank
//
// RC DispState::dispAnalyzeBlankCommon(INT32 headNum)
// {
//     UINT32 status = LW_OK;
//     UINT32 data32 = 0;
//     UINT32 i = 0;
//     UINT32 temp32 = 0;
//     UINT32 temp = 0;
//
//     if ((headNum > 1) || (headNum <= -1))
//     {
//         headNum = 0;
//         Printf(Tee::PriLow, "Wrong head!!! hence Setting Head = %d\n\n", headNum);
//     }
//
//     //
//     // Returns the DispOwner
//     //
//     data32 = REG_RD32(LW_PDISP_VGA_CR_REG58);
//     data32 = DRF_VAL(_PDISP, _VGA_CR_REG58, _SET_DISP_OWNER, data32);
//
//     Printf(Tee::PriLow,
//            "---------------------------------------------------------------------\n");
//     Printf(Tee::PriLow ,
//            "Display Owner:            %s\n",
//            (data32 == LW_PDISP_VGA_CR_REG58_SET_DISP_OWNER_DRIVER) ?
//            "Driver         (Looks OK)": "Vbios          (Did you expect vbios mode?)");
//
//     //
//     // Status of the Core, Base and Ovly channel
//     //
//     data32 = REG_IDX_RD_DRF(_PDISP, _CHNCTL_CORE, 0, _STATE);
//     Printf(Tee::PriLow ,"Core Channel State:       %s\n", dCoreState[data32]);
//
//     data32 = REG_IDX_RD_DRF(_PDISP, _CHNCTL_BASE, headNum, _STATE);
//     if (data32 != LW_PDISP_CHNCTL_BASE_STATE_DEALLOC)
//     {
//         Printf(Tee::PriLow ,"Base Channel State:       %s\n", dBaseState[data32]);
//     }
//
//     data32 = REG_IDX_RD_DRF(_PDISP, _CHNCTL_OVLY, headNum, _STATE);
//     if (data32 != LW_PDISP_CHNCTL_OVLY_STATE_DEALLOC)
//     {
//         Printf(Tee::PriLow , "Ovly Channel State:       %s\n", dOvlyState[data32]);
//     }
//
//     //
//     // Is an exception pending in core or base channels that's
//     // preventing the channels from proceeding
//     //
//     data32 = REG_RD32(LW_PDISP_INTR_0);
//     for (i = 0; i < LW_PDISP_INTR_0_CHN_EXCEPTION__SIZE_1; i++)
//     {
//         temp32 = DRF_IDX_VAL(_PDISP, _INTR_0_CHN, _EXCEPTION, i, data32);
//         if (temp32 == LW_PDISP_INTR_0_CHN_EXCEPTION_PENDING)
//         {
//             Printf(Tee::PriHigh,
//                    ".....................................................................\n");
//             if (i > 0)
//             {
//                 Printf(Tee::PriHigh, "Exception pending in %s[%d]\n",
//                         dispChanState_LW50[i].name, dispChanState_LW50[i].headNum);
//             }
//             else
//             {
//                 Printf(Tee::PriHigh ,"Exception pending in %s\n", dispChanState_LW50[i].name);
//             }
//             Printf(Tee::PriHigh,
//                    ".....................................................................\n");
//
//             status = LW_ERROR;
//         }
//     }
//
//     // DMI_MEMACC status. It is fetching or stopped?
//     data32 = REG_RD32(LW_PDISP_DMI_MEMACC);
//
//     for (i = 0; i < LW_PDISP_DMI_MEMACC_HEAD_STATUS__SIZE_1; i++)
//     {
//         if (i != headNum)
//             continue;
//
//         temp32 = DRF_IDX_VAL(_PDISP, _DMI_MEMACC_HEAD, _STATUS, i, data32);
//         Printf(Tee::PriLow,"Dmi Fetch Status:         %s\n",
//                 (temp32 == LW_PDISP_DMI_MEMACC_HEAD_STATUS_FETCHING) ?
//                "Fetching       (Looks OK)" : "Stopped        (Did you expect head to be not fetching data?)");
//
//         temp  = DRF_IDX_VAL(_PDISP, _DMI_MEMACC_HEAD, _SETTING_NEW, i, data32);
//         if (temp == LW_PDISP_DMI_MEMACC_HEAD_SETTING_NEW_PENDING)
//         {
//             Printf(Tee::PriLow,"A new setting for DMI_MEMACC is pending\n");
//
//             temp  = DRF_IDX_VAL(_PDISP, _DMI_MEMACC_HEAD, _REQUEST, i, data32);
//
//             if ( ((temp32 == LW_PDISP_DMI_MEMACC_HEAD_STATUS_FETCHING) &&
//                  ( temp   != LW_PDISP_DMI_MEMACC_HEAD_REQUEST_FETCH))  ||
//                  ((temp32 != LW_PDISP_DMI_MEMACC_HEAD_STATUS_FETCHING) &&
//                  ( temp   == LW_PDISP_DMI_MEMACC_HEAD_REQUEST_FETCH)) )
//             {
//                 Printf(Tee::PriHigh,"Software's request to DMI: %s\n",
//                         (temp == LW_PDISP_DMI_MEMACC_HEAD0_REQUEST_FETCH) ? "Fetch" : "Stop");
//                 Printf(Tee::PriHigh,"Software's request doesn't match the current status\n");
//                 status = LW_ERROR;
//             }
//         }
//     }
//
//     return status;
// }
//

//
// Analyze the blank display
//
// RC DispState::dispAnalyzeBlank(INT32 headNum)
// {
//     UINT32 status     = LW_OK;
//     UINT32 data32     = 0;
//     UINT32 i          = 0;
//     UINT32 numSinks   = 0;
//
//      status = dispAnalyzeBlankCommon(headNum);
//
//     //
//     // Iso ctx dmas in base and core channels.
//     //
//     data32 = REG_IDX_RD_DRF(_PDISP, _CHNCTL_CORE, 0, _ALLOCATION);
//     if (data32 == LW_PDISP_CHNCTL_CORE_ALLOCATION_ALLOCATE)
//     {
//         Printf(Tee::PriLow,"Iso Ctx dmas in core:\n");
//         for (i = 0; i < LW_CORE_SURFACE; i++)
//         {
//             data32 = REG_RD32(LW857D_SC_HEAD_SET_CONTEXT_DMAS_ISO(headNum, i));
//             if (!i)
//             {
//                 Printf(Tee::PriLow,"    Surf%d:                0x%08x", i, data32);
//                 if (data32)
//                 {
//                     Printf(Tee::PriLow,"     (Looks OK)\n");
//                 }
//                 else
//                 {
//                     Printf(Tee::PriLow,"     (Did you expect a NULL iso ctx dma?)\n");
//                 }
//             }
//             else if (data32)
//             {
//                 Printf(Tee::PriLow,"    Surf%d:                0x%08x");
//                 Printf(Tee::PriHigh,"         (This is bad since zaphod is not yet supported)\n", i, data32);
//             }
//          }
//
//         data32 = REG_IDX_RD_DRF(_PDISP, _CHNCTL_BASE, headNum, _ALLOCATION);
//         if (data32 == LW_PDISP_CHNCTL_BASE_ALLOCATION_ALLOCATE)
//         {
//             Printf(Tee::PriLow,"Iso Ctx dmas in base:\n");
//             for (i = 0; i < LW_BASE_SURFACE; i++)
//             {
//                 data32 = REG_RD32(LW857C_SC_SET_CONTEXT_DMAS_ISO(headNum, i));
//                 if (i <= 0)
//                 {
//                     Printf(Tee::PriLow,"    Surf%d:                0x%08x", i, data32);
//                     if (data32)
//                     {
//                         Printf(Tee::PriLow,"     (Looks OK)\n");
//                     }
//                     else
//                     {
//                         Printf(Tee::PriLow,"     (Did you expect a NULL iso ctx dma?)\n");
//                     }
//                 }
//                 else if (data32)
//                 {
//                     Printf(Tee::PriLow,"    Surf%d:                0x%08x\n", i, data32);
//                 }
//             }
//         }
//     }
//
//     Printf(Tee::PriLow,"ORs owned by the head:    ");
//
//     //
//     // Does this head own any OR at all?
//     //
//     for (i = 0; i < LW_PDISP_DACS; i++)
//     {
//         data32 = REG_IDX_RD_DRF(857D, _SC_DAC_SET_CONTROL_ARM, i, _OWNER);
//         switch (data32)
//         {
//             case LW857D_SC_DAC_SET_CONTROL_ARM_OWNER_NONE:
//                 break;
//
//             case LW857D_SC_DAC_SET_CONTROL_ARM_OWNER_HEAD0:
//                 if (headNum == 0)
//                 {
//                     ++numSinks;
//                     Printf(Tee::PriLow,"DAC%d  ", i);
//                 }
//                 break;
//             case LW857D_SC_DAC_SET_CONTROL_ARM_OWNER_HEAD1:
//                 if (headNum == 1)
//                 {
//                     ++numSinks;
//                     Printf(Tee::PriLow,"DAC%d  ", i);
//                 }
//                 break;
//
//             default:
//                 Printf(Tee::PriLow,"\n");
//                 Printf(Tee::PriLow,"ERROR: Bad owner %d for DAC%d\n", data32, i);
//                 status = LW_ERROR;
//                 break;
//         }
//     }
//
//     for (i = 0; i < LW_PDISP_SORS; i++)
//     {
//         data32 = REG_IDX_RD_DRF(857D, _SC_SOR_SET_CONTROL_ARM, i, _OWNER);
//         switch (data32)
//         {
//             case LW857D_SC_SOR_SET_CONTROL_ARM_OWNER_NONE:
//                 break;
//
//             case LW857D_SC_SOR_SET_CONTROL_ARM_OWNER_HEAD0:
//                 if (headNum == 0)
//                 {
//                     ++numSinks;
//                     Printf(Tee::PriLow,"SOR%d  ", i);
//                 }
//                 break;
//             case LW857D_SC_SOR_SET_CONTROL_ARM_OWNER_HEAD1:
//                 if (headNum == 1)
//                 {
//                     ++numSinks;
//                     Printf(Tee::PriLow,"SOR%d  ", i);
//                 }
//                 break;
//
//             default:
//                 Printf(Tee::PriLow,"\n");
//                 Printf(Tee::PriLow,"ERROR: Bad owner %d for SOR%d\n", data32, i);
//                 status = LW_ERROR;
//                 break;
//         }
//     }
//
//     for (i = 0; i < LW_PDISP_PIORS; i++)
//     {
//         data32 = REG_IDX_RD_DRF(857D, _SC_PIOR_SET_CONTROL_ARM, i, _OWNER);
//         switch (data32)
//         {
//             case LW857D_SC_PIOR_SET_CONTROL_ARM_OWNER_NONE:
//                 break;
//
//             case LW857D_SC_PIOR_SET_CONTROL_ARM_OWNER_HEAD0:
//                 if (headNum == 0)
//                 {
//                     ++numSinks;
//                     Printf(Tee::PriLow,"PIOR%d ", i);
//                 }
//                 break;
//             case LW857D_SC_PIOR_SET_CONTROL_ARM_OWNER_HEAD1:
//                 if (headNum == 1)
//                 {
//                     ++numSinks;
//                     Printf(Tee::PriLow,"PIOR%d ", i);
//                 }
//                 break;
//
//             default:
//                 Printf(Tee::PriLow,"\n");
//                 Printf(Tee::PriLow,"ERROR: Bad owner %d for PIOR%d\n", data32, i);
//                 status = LW_ERROR;
//                 break;
//         }
//     }
//
//     if (numSinks == 0)
//     {
//         Printf(Tee::PriLow,"NONE           (Did you expect no ORs to be owned by this head?)");
//     }
//     else
//     {
//         if (numSinks <= 2)
//         {
//             for (i = 2 - numSinks; i > 0; --i)
//             {
//                 Printf(Tee::PriLow,"      ");
//             }
//             Printf(Tee::PriLow,"   (Looks OK)");
//         }
//         else
//         {
//             Printf(Tee::PriLow,"   (Looks OK). Alignment/formatting will be bad");
//         }
//     }
//     Printf(Tee::PriLow,"\n");
//     Printf(Tee::PriLow,"---------------------------------------------------------------------\n");
//
//     return status;
// }
//

