-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Ucode SCRATCH definitions for debug.
--
--  @description
--  This package defines all registers that will be used for RM Falcon ucode debug.
package Ucode_Post_Codes
with SPARK_Mode => On
is

   -- NOTE : Below DEBUGI error codes are only for local debug purpose and will not be part of any PROD code
   --  Error codes for RM Falcons.
   --  @value UCODE_ERR_CODE_NOERROR                Denotes No error.
   type LW_UCODE_UNIFIED_ERROR_TYPE is (UCODE_ERR_CODE_NOERROR,                                  --0
                                        UCODE_ERROR_BIN_STARTED_BUT_NOT_FINISHED,                --1
                                        UCODE_ERROR_SSP_STACK_CHECK_FAILED,                      --2
                                        UCODE_DESCRIPTOR_ILWALID,                                --3
                                        ILWALID_FALCON_REV,                                      --4
                                        CHIP_ID_ILWALID,                                         --5
                                        ILWALID_UCODE_REVISION,                                  --6
                                        UNEXPECTED_FALCON_ENGID,                                 --7
                                        BAD_CSBERRORSTATE,                                       --8
                                        SEBUS_GET_DATA_TIMEOUT,                                  --9
                                        SEBUS_GET_DATA_BUS_REQUEST_FAIL,                         --10
                                        SEBUS_SEND_REQUEST_TIMEOUT_CHANNEL_EMPTY,                --11
                                        SEBUS_SEND_REQUEST_TIMEOUT_WRITE_COMPLETE,               --12
                                        SEBUS_DEFAULT_TIMEOUT_ILWALID,                           --13
                                        SEBUS_ERR_MUTEX_ACQUIRE,                                 --14
                                        SEBUS_VHR_CHECK_FAILED,                                  --15
                                        FLCN_ERROR_REG_ACCESS,                                   --16
                                        FLCN_BAR0_WAIT,                                          --17
                                        FLCN_PRI_ERROR,                                          --18
                                        BAD_VHRCFG_BASE_ADDR,                                    --19
                                        ILWALID_IMEM_BLOCK_COUNT,                                --20
                                        SE_COMMON_MUTEX_ACQUIRE_FAILED,                          --21
                                        SE_COMMON_MUTEX_OWNERSHIP_MATCH_FAILED,                  --22
                                        SE_COMMON_MUTEX_INDEX_ILWALID,                           --23
                                        UCODE_PUB_ERR_PMU_RESET,                                 --4097
                                        DEV_FBPA_WAR_FAILED,                                     --4098
                                        UCODE_PUB_ERR_READBACK_FAILED                            --4099
                                       )
     with Size => 32, Object_Size => 32;
   for LW_UCODE_UNIFIED_ERROR_TYPE use (
                                        UCODE_ERR_CODE_NOERROR                                => 0,
                                        UCODE_ERROR_BIN_STARTED_BUT_NOT_FINISHED              => 1,
                                        UCODE_ERROR_SSP_STACK_CHECK_FAILED                    => 2,
                                        UCODE_DESCRIPTOR_ILWALID                              => 3,
                                        ILWALID_FALCON_REV                                    => 4,
                                        CHIP_ID_ILWALID                                       => 5,
                                        ILWALID_UCODE_REVISION                                => 6,
                                        UNEXPECTED_FALCON_ENGID                               => 7,
                                        BAD_CSBERRORSTATE                                     => 8,
                                        SEBUS_GET_DATA_TIMEOUT                                => 9,
                                        SEBUS_GET_DATA_BUS_REQUEST_FAIL                       => 10,
                                        SEBUS_SEND_REQUEST_TIMEOUT_CHANNEL_EMPTY              => 11,
                                        SEBUS_SEND_REQUEST_TIMEOUT_WRITE_COMPLETE             => 12,
                                        SEBUS_DEFAULT_TIMEOUT_ILWALID                         => 13,
                                        SEBUS_ERR_MUTEX_ACQUIRE                               => 14,
                                        SEBUS_VHR_CHECK_FAILED                                => 15,
                                        FLCN_ERROR_REG_ACCESS                                 => 16,
                                        FLCN_BAR0_WAIT                                        => 17,
                                        FLCN_PRI_ERROR                                        => 18,
                                        BAD_VHRCFG_BASE_ADDR                                  => 19,
                                        ILWALID_IMEM_BLOCK_COUNT                              => 20,
                                        SE_COMMON_MUTEX_ACQUIRE_FAILED                        => 21,
                                        SE_COMMON_MUTEX_OWNERSHIP_MATCH_FAILED                => 22,
                                        SE_COMMON_MUTEX_INDEX_ILWALID                         => 23,
                                        UCODE_PUB_ERR_PMU_RESET                               => 4097,
                                        DEV_FBPA_WAR_FAILED                                   => 4098,
                                        UCODE_PUB_ERR_READBACK_FAILED                         => 4099
                                        );

end Ucode_Post_Codes;
