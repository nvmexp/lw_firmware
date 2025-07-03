/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file dispstate.h
//! \brief dispState Check utilty
//!
//! This file DispState Check routines.

#ifdef NO_ERROR
#undef NO_ERROR
#endif

#include "lwos.h"
#include "core/include/rc.h"
class GpuSubdevice;


class DispState
{
public:
   GpuSubdevice *m_pSubdev;
   DispState();
   void BindGpuSubdevice(GpuSubdevice* pSubdev);

   #define LW_ERROR RC::LWRM_ERROR
   #define REG_RD32 m_pSubdev->RegRd32

   #define LW_OK OK
   #define REG_RD_DRF(d,r,f) (((REG_RD32(LW ## d ## r))>>DRF_SHIFT(LW ## d ## r ## f))&DRF_MASK(LW ## d ## r ## f))
   #define REG_IDX_RD_DRF(d,r,i,f)   (((REG_RD32(LW ## d ## r(i)))>>DRF_SHIFT(LW ## d ## r ## f))&DRF_MASK(LW ## d ## r ## f))

     RC checkDispMethodFifo(void);
     RC checkDispExceptions( UINT32 chid );
     RC checkDispInterrupts( void );
     RC checkDispChnCtlCore( void );
     RC checkDispChnCtlBase(void);
     RC checkDispChnCtlOvly(void);
     RC checkDispChnCtlOvim( void );
     RC checkDispChnCtlLwrs( void );
     RC checkDispDMAState( void );

     RC checkDispUpdatePreFSM( UINT32 *pre );
     RC checkDispUpdatePostFSM( UINT32 *post );
     RC checkDispUpdateUsubFSM( UINT32 *usub );
     RC checkDispUpdateCmgrFSM( UINT32 cmgr );
     RC checkDispCoreUpdateFSM( void );

     RC checkDispDmiStatus(void);
     RC checkDispOrStatus( void );
     RC checkDispHdmiStatus( void );
     RC checkDispDpStatus( void );
     RC checkDispUnderflow( void );
     RC checkDispClocks(void);
     RC SetUnderflowState(UINT32 head,
                          UINT32 enable,
                          UINT32 clearUnderflow,
                          UINT32 mode);

    // RC dispAnalyzeBlankCommon(INT32 headNum);
    // RC dispAnalyzeBlank(INT32 headNum);

    // There are tests still need to come here
    // ImageCRC Check Utilty
    // Some more DispState Check functions

     RC dispTestDisplayState(void);

};

