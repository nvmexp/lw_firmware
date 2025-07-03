//  Copyright (c) 2020-2021 by LWPU Corp.  All rights reserved.
//
//  This material  constitutes  the trade  secrets  and confidential,
//  proprietary information of LWPU, Corp.  This material is not to
//  be  disclosed,  reproduced,  copied,  or used  in any manner  not
//  permitted  under license from LWPU, Corp.


// The implementation of this API should export a "void* GetContext()" routine
// that allows the test to recover the Ctx Pointer for the session.

///////////////////////////////////////////////
// testapicontext.h
// This header defines the SimContext structure defining the interface between 
// a Test Driver and External Tests.
// ////////////////////////////////////////////

#ifndef TESTAPICTX_INCLUDED
#define TESTAPICTX_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include "core/include/platform.h"

enum RT_MEM_TARGET
{
    RT_IMEM,
    RT_EMEM,
    RT_DATAMEM,
    RT_GART, 
    RT_EMEM_NO_CACHE,
    RT_VPRMEM,
    RT_TZMEM,
    RT_IPCMEM,
    RT_EMEM_FULL,
    RT_ARAM,
    RT_ANY,
    RT_APE_ARAM,
    RT_SYSRAM,
    RT_CVSRAM,
    RT_VIDMEM,
    RT_LAST_TARGET
};

enum RT_RC
{
    RT_OK,
    RT_ERROR
};

enum EventId
{
    StartStageEvent = 0,
    EndStageEvent = 1
};

namespace TestApi
{
    enum RT_SIM_MODE
    {
        Hardware,
        RTL,
        Fmodel,
        Amodel
    };
    using POLL_FUNC = UINT32 (*)(void *Args[]);

    // Reg Access routines (retricted to 32 bits)
    // adr : Absolute address within the System Address Map
    using REG_RD_08 = uint8_t  (*)(uintptr_t addr);
    using REG_WR_08 = void  (*)(uintptr_t addr, uint8_t data);
    using REG_RD_16 = uint16_t (*)(uintptr_t addr);
    using REG_WR_16 = void  (*)(uintptr_t addr, uint16_t data);
    using REG_RD_32 = UINT32 (*)(uintptr_t addr);
    using REG_WR_32 = void  (*)(uintptr_t addr, UINT32 data);

    using GET_GPVAR_STR = int  (*)(const char *gpstr, char *retval);
    using GET_GPVAR_DOUBLE = int  (*)(const char *gpstr, double *retval);

    using SET_GPVAR_STR = int  (*)(const char *gpstr, char *setval);
    using SET_GPVAR_DOUBLE = int  (*)(const char *gpstr, double setval);

    // Memory Access routines
    // adr : Absolute address within the System Address Map
    using MEM_RD_08 =  uint8_t  (*)(UINT64 addr);
    using MEM_RD_16 =  uint16_t (*)(UINT64 addr);
    using MEM_RD_32 =  UINT32 (*)(UINT64 addr);
    using MEM_RD_64 =  UINT64 (*)(UINT64 addr);
    using MEM_RD_BLOCK =  void  (*)(UINT64 addr, uint8_t *ptr, UINT32 len);
    using MEM_RD_BURST =  void  (*)(UINT64 addr, uint8_t *data, uint8_t burst_type, UINT32 burst_len, UINT32 burst_size);
    using MEM_CRC =  void  (*)(UINT64 addr, UINT32 startY, UINT32 height, UINT32 width, UINT32 stride,
            UINT32 fmtid, UINT32 surface_fmt, UINT32 *result, uint8_t bpp, uint8_t lineIncr);

    using MEM_WR_08 =  void (*)(UINT64 addr, uint8_t  data);
    using MEM_WR_16 =  void (*)(UINT64 addr, uint16_t data);
    using MEM_WR_32 =  void (*)(UINT64 addr, UINT32 data);
    using MEM_WR_64 =  void (*)(UINT64 addr, UINT64 data);
    using MEM_WR_BLOCK =  void (*)(UINT64 addr, const uint8_t *ptr, UINT32 len);
    using MEM_WR_BURST =  void (*)(UINT64 addr, const uint8_t *data, uint8_t burst_type, UINT32 burst_len, UINT32 burst_size);
    using HMEM_RD_32 =  UINT32 (*)(UINT64 addr);
    using HMEM_WR_32 =  void (*)(UINT64 addr, UINT32 data);

    // AXI AxUser DBB Field Routines
    using SET_STREAM_ID = void(*) (LwU16 streamid);
    using GET_STREAM_ID = LwU16(*) ();
    using SET_SELWRE_CONTEXT = void(*) (bool sc);
    using GET_SELWRE_CONTEXT = bool(*) ();
    using SET_SUB_STREAM_ID = void(*) (LwU32 ssid);
    using GET_SUB_STREAM_ID = LwU32(*) ();
    using SET_IO_COH_HINT = void(*) (bool iocoh);
    using GET_IO_COH_HINT = bool(*) ();
    using SET_VPR_HINT = void(*) (LwU32 vpr);
    using GET_VPR_HINT = LwU32(*) ();
    using SET_ATS_TRANSLATION_HINT = void(*) (bool atshint);
    using GET_ATS_TRANSLATION_HINT = bool(*) ();
    using SET_GSC_REQUEST = void(*) (LwU32 gsc_aid, LwU32 gsc_al);
    using GET_GSC_REQUEST = void(*) (LwU32 &gsc_aid, LwU32 &gsc_al);
    using SET_ATOMIC_REQUEST = void(*) (LwU32 atomic_cmd, LwU32 atomic_op_sz, LwU8 *atomic_data, LwU32 atomic_sign);
    using GET_ATOMIC_REQUEST = void(*) (LwU32 &atomic_cmd, LwU32 &atomic_op_sz, LwU8 *atomic_data, LwU32 &atomic_sign);

    // AXI AxUser CBB Field Routines
    using SET_CBB_MASTER_ID = void(*) (LwU32 mstr_id);
    using GET_CBB_MASTER_ID = LwU32(*) ();
    using SET_CBB_VQC = void(*) (LwU32 vqc);
    using GET_CBB_VQC = LwU32(*) ();
    using SET_CBB_GROUP_SEC = void(*) (LwU32 grp_sec);
    using GET_CBB_GROUP_SEC = LwU32(*) ();
    using SET_CBB_FALCON_SEC = void(*) (LwU32 lsec);
    using GET_CBB_FALCON_SEC = LwU32(*) ();

    using SYNC_PT =  void   (*)(UINT32 uid);
    // Memory Allocation routines
    // Target = IMEM, EMEM, ANY
    // Need to clarify the ENUM values for the Target field.
    using MEM_ALLOC =  UINT64  (*)(RT_MEM_TARGET target, UINT32 size, UINT32 Alignment);
    using MEM_FREE =  UINT32  (*)(UINT64 ptr);
    using MEM_FREE_TARGET =  void (*)(RT_MEM_TARGET type, UINT64 ptr);
    using MEM_SIZE =  UINT64 (*)(RT_MEM_TARGET target);
    using MEM_INIT =  void   (*)(UINT32 sz);
    // Synchronization routine between CPU and EXT thread.
    // This is a blocking call. 
    // It can return successfully with a 0 return code.
    // In that case, the "synchronized" value (provided by the EXT thread) 
    // is stored in value_ptr.
    // If the call fails due to a Timeout, the return code is -1.
    using RD_SYNC_32 =  RT_RC  (*)(UINT32 *value_ptr);

    using WR_SYNC_32 =  RT_RC  (*)(UINT32 value);

    // Routines to Register/Unregister Interrupt Service routines by the Test
    // intr_id is the ID defined in project.h for the interrupt of interest.
    using REGISTER_INTR =  RT_RC  (*)(UINT32 intr_id, ISR callback);
    using UNREGISTER_INTR =  RT_RC  (*)(UINT32 intr_id);

    // Test Run Status return check. This is sent to the EXT thread.
    // If status is not RT_OK, exit.  Else do nothing.
    // Status is the return code.
    using RETURN =  void   (*)(RT_RC status);

    // Test Exit.  status is the return code.
    using EXIT =  void   (*)(int status);

    // Print Message - replaces Printf in the tests
    using PRINT_MSG =  void   (*)(const char * msg ...);
    // Sets thread number ref in the test
    using SET_THREADID_REF =  void   (*)(const UINT32 &thread_id_ref);

    // Enter/exit Critical Region Function
    using ENTER_CRITICAL =  void   (*)(void);
    using EXIT_CRITICAL =  void   (*)(void);

    // Core Polling Function
    using POLL = RT_RC  (*)(POLL_FUNC poll_routine, void *args[], UINT32 timeout);

    // Args processing/handling routines
    using GET_ARGS =  RT_RC  (*)(void *args_s);

    using PARSE_INT_ARG =  RT_RC  (*)(const char *arg_name, UINT32 *var);

    using PARSE_CHAR_ARG =  RT_RC  (*)(const char *arg_name, char *var);

    using ALLOCATE_BBC_MEM_REGIONS =  void   (*)(const uint8_t ipcmem_size, const uint8_t privmem_size, uintptr_t ipc_mem_addr_base, uintptr_t priv_mem_addr_base);
    // Enable / disable status of the back door path
    using MEM_BD_ENABLED =  bool (*)(void);

    using SET_PROT =  void  (*)(UINT32 prot);
    using GET_PROT =  UINT32 (*)(void);

    using SET_KEYS =  void  (*)(uint8_t key_sel, uint8_t *key);
    using VOID_P =  void * (*)(void);
    using ENCR_DECR =  void  (*)(bool encrypt, uint64_t addr, int key_sel, uint8_t *data_in, uint8_t *data_out);

    using IS_MUTEX_FREE =  bool (*)(void* ptr);
    using MUTEX_LOCK_BLOCKING =  void (*)(void* ptr);
    using MUTEX_LOCK_NONBLOCKING =  bool (*)(void* ptr);
    using MUTEX_WAIT =  void (*)(void* ptr);
    using MUTEX_UNLOCK =  bool (*)(void* ptr);
    using HOST_BARRIER =  void (*)(void);
    using WAITNS = void (*)(UINT64 ns);
    using YIELD = void (*)(void);
    using GET_TIME = UINT64 (*)(void);
    using SET_EVENT = void (*)(EventId);
    using WAIT_ON_EVENT = bool (*)(EventId, UINT64);
    using ESCAPE_READ_BUFFER = int (*)(UINT32 gpuId, const char *path, UINT32 index, size_t size, void *buffer); 
    using ESCAPE_WRITE_BUFFER = int (*)(UINT32 gpuId, const char *path, UINT32 index, size_t size, void* buffer); 
    using GET_SIMULATION_MODE = RT_SIM_MODE (*)(void);

    typedef struct TestApiCtx_s
    {
        REG_RD_08 RegRead08;
        REG_RD_16 RegRead16;
        REG_RD_32 RegRead32;

        REG_WR_08 RegWrite08; 
        REG_WR_16 RegWrite16; 
        REG_WR_32 RegWrite32; 

        GET_GPVAR_STR GetGPVarStr;
        SET_GPVAR_STR SetGPVarStr;
        GET_GPVAR_DOUBLE GetGPVarDouble;
        SET_GPVAR_DOUBLE SetGPVarDouble;

        MEM_RD_08 MemRead08; 
        MEM_RD_16 MemRead16; 
        MEM_RD_32 MemRead32; 
        MEM_RD_64 MemRead64;
        MEM_RD_BLOCK MemReadBlock;
        MEM_RD_BURST MemReadBurst;
        MEM_WR_BURST MemWriteBurst;
        MEM_CRC   MemCrc;


        MEM_WR_08 MemWrite08; 
        MEM_WR_16 MemWrite16; 
        MEM_WR_32 MemWrite32; 
        MEM_WR_64 MemWrite64;
        MEM_WR_BLOCK MemWriteBlock;

        MEM_RD_32 MemBdRead32; 
        MEM_RD_32 MemBdReadPl32; 
        MEM_RD_64 MemBdRead64;
        MEM_RD_BLOCK MemBdReadBlock;
        MEM_WR_32 MemBdWrite32;
        MEM_WR_32 MemBdWritePl32;
        MEM_WR_64 MemBdWrite64;
        MEM_WR_BLOCK MemBdWriteBlock;
        MEM_BD_ENABLED IsMemBdEnabled;

        HMEM_RD_32 HMemBdRead32; 
        HMEM_WR_32 HMemBdWrite32;

        MEM_ALLOC Malloc;
        MEM_FREE  Free;
        MEM_FREE_TARGET FreeTargetMemory;
        MEM_SIZE  GetMemSize;
        MEM_INIT  InitVprMem;
        MEM_INIT  InitTzMem;

        RD_SYNC_32 RdCPUSync32;
        WR_SYNC_32 WrCPUSync32;

        SYNC_PT SyncPoint;

        REGISTER_INTR RegisterIntr;
        UNREGISTER_INTR UnRegisterIntr;

        EXIT      Exit;
        POLL      Poll;

        PRINT_MSG PrintMsg;
        SET_THREADID_REF SetThreadIdRef;
        ENTER_CRITICAL SimEnterCritical;
        EXIT_CRITICAL SimExitCritical;
        SET_PROT SetProt;
        GET_PROT GetProt;

        GET_ARGS  GetArgs;

        PARSE_INT_ARG  ParseIntArg;
        PARSE_CHAR_ARG ParseCharArg;

        ALLOCATE_BBC_MEM_REGIONS AllocateBbcMemRegions;
        UINT32   rtprintlevel;
        bool    bDumpTransaction;

        SET_KEYS MemBdSetEncrAddrKey;
        SET_KEYS MemBdSetEncrDataKey;
        VOID_P MemBdGetEmcRegCalcPtr;
        ENCR_DECR EncrDecrData;
        IS_MUTEX_FREE          isMutexFree;
        MUTEX_LOCK_BLOCKING    mutexLockBlocking;
        MUTEX_LOCK_NONBLOCKING mutexLockNonblocking;
        MUTEX_WAIT             mutexWait;
        MUTEX_UNLOCK           mutexUnlock;
        HOST_BARRIER HostBarrier;
        WAITNS WaitNS;
        YIELD  Yield;
        GET_TIME GetTime;
        SET_EVENT SetEvent;
        WAIT_ON_EVENT WaitOnEvent;

        SET_STREAM_ID SetStreamID;
        GET_STREAM_ID GetStreamID;
        SET_SELWRE_CONTEXT SetSelwreContext;
        GET_SELWRE_CONTEXT GetSelwreContext;
        SET_SUB_STREAM_ID SetSubStreamID;
        GET_SUB_STREAM_ID GetSubStreamID;
        SET_IO_COH_HINT SetIOCohHint;
        GET_IO_COH_HINT GetIOCohHint;
        SET_VPR_HINT SetVPRHint;
        GET_VPR_HINT GetVPRHint;
        SET_ATS_TRANSLATION_HINT SetATSTranslationHint;
        GET_ATS_TRANSLATION_HINT GetATSTranslationHint;
        SET_GSC_REQUEST SetGSCRequest;
        GET_GSC_REQUEST GetGSCRequest;
        SET_ATOMIC_REQUEST SetAtomicRequest;
        GET_ATOMIC_REQUEST GetAtomicRequest;
        SET_CBB_MASTER_ID SetCBBMasterID;
        GET_CBB_MASTER_ID GetCBBMasterID;
        SET_CBB_VQC SetCBBVQC;
        GET_CBB_VQC GetCBBVQC;
        SET_CBB_GROUP_SEC SetCBBGroupSec;
        GET_CBB_GROUP_SEC GetCBBGroupSec;
        SET_CBB_FALCON_SEC SetCBBFalconSec;
        GET_CBB_FALCON_SEC GetCBBFalconSec;

        ESCAPE_READ_BUFFER EscapeReadBuffer;
        ESCAPE_WRITE_BUFFER EscapeWriteBuffer;
        GET_SIMULATION_MODE GetSimulationMode;
    } TestApiCtx;

    extern void* TestApiGetContext();
}
#endif /* TESTAPICONTEXT_DEFINED */
