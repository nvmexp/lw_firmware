/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

.section ".text.startup","ax",@progbits
.global startup
startup:

    /**************************************************************************
     * *** SP setup
    **************************************************************************/ 

    la sp, _sepkern_stack_top

    /**************************************************************************
     * *** 'mtvec' setup deferred to Initialize_SK_State
     * We do not want to setup mtvec before HW state is verified.
    **************************************************************************/ 
    
    /**************************************************************************
     * *** Call to 'adainit' omitted
     * Before the first call to an Ada subprogram a call to 'adainit' would be
     * required in order for exelwtion environment to be setup by calling
     * elaboration routines. 
     * Since 'No_Elaboration_Code' restriction is used across whole SK project, 
     * it ensures no elaboration code is generated.
     * Therefore, we do not need to call 'adainit' here as that routine would 
     * only contain single 'ret' instruction and nothing else.
    **************************************************************************/

    j _ada_sk_init

