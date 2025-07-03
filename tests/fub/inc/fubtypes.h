/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: fubtypes.h
 * @brief Main header file defining types that are used in the FUB.
 */
typedef struct
{
    LwU32 fuseBar0Address;         // Will have Priv address of fuse 
    LwU32 fuseIntendedVal;         // Will have New value to be burnt in Fuse
    LwU32 fusePriAddress;          // Will have row number of Primary bits of Fuse
    LwU32 fuseRedAddress;          // Will have row number of Redundant bits of Fuse
    LwU32 fuseAdjustedBurlwalue;   // Will have Fuse value adjusted as per location of Fuse in row
}FPF_FUSE_INFO, *PFPF_FUSE_INFO;

typedef struct
{
    LwU32  gc6BsiCtrlMask;         // Will be used for updating/restoring LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK
    LwU32  gc6SciMastMask;         // Will be used for updating/restoring LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK
    LwU32  gc6SciSecTimerMask;     // Will be used for updating/restoring LW_PGC6_SCI_SEC_TIMER_PRIV_LEVEL_MASK 
    LwU32  fpfFuseCtrlMask;        // Will be used for updating/restoring LW_FPF_FUSECTRL_PRIV_LEVEL_MASK
    LwU32  thermVidCtrlMask;       // Will be used for updating/restoring LW_THERM_VIDCTRL_PRIV_LEVEL_MASK
    LwBool bAreMasksRaised;        // Will be set only if all the PLMs are raised.
}FUB_PRIV_MASKS_RESTORE, *PFUB_PRIV_MASKS_RESTORE;

typedef enum _enum_vqps_operation
{
    ENABLE_VQPS,
    DISABLE_VQPS
}FUB_VQPS_OPERATION, *PFUB_VQPS_OPERATION;

typedef enum _enum_decode_trap_set
{
    ENABLE_DECODE_TRAP,
    CLEAR_DECODE_TRAP
}FUB_DECODE_TRAP_OPERATION_TO_PERFORM, *PFUB_DECODE_TRAP_OPERATION_TO_PERFORM;
