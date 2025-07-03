/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: main.c
 */

//
// Includes
//
#include <stdint.h>
#include <lwtypes.h>
#include <lwmisc.h>

#include <dev_gsp.h>

#include <lwriscv/csr.h>
#include <lwriscv/fence.h>
#include <lwriscv/peregrine.h>

#include <liblwriscv/io.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>

#include <testharness.h>
#include <stress.h>

#define ALL_SOURCE_DISABLE_NO_READ_WRITE     0xF00
#define GSP_SOURCE_ENABLE_L3_READ_WRITE      0x3F88
#define CPU_SOURCE_ENABLE_L0_READ_WRITE      0xFFFFFFFF
#define ALL_SOURCE_ENABLE_L0_READ            0xFFFFFF0F
// Default watchdog timer is 1 second (or more precisely, 1.024 seconds)
#define WATCHDOG_TIMER_TIMEOUT_1S	10000000

//DMEM Size used for debug prints
#define DMEM_END_CARVEOUT_SIZE	0xC00

// Queue used for debug prints
#define GSP_DEBUG_BUFFER_QUEUE	3

//Signature used to notify Lwgpu through MB.
volatile LwU32 global_var = 0xAAAAAAAA;

// Address of the sysmem block allocated by Lwgpu
LwU64 g_sysmem_block_addr;

#define RUN_TEST(x, y) do {\
    if ((x)(y)) \
        goto fail_with_error; \
} while (0);

#define RUN_TEST_EXPECT(x, y, z) do {\
    if ((x)(y) != (z)) \
        goto fail_with_error; \
} while (0);

static void appShutdown(void)
{
    csr_write(LW_RISCV_CSR_MIE, 0);
    riscvLwfenceIO();
    __asm__ volatile ("csrrwi zero, %0, %1" : :
					  "i"(LW_RISCV_CSR_MOPT),
					  "i"( DRF_DEF64(_RISCV, _CSR_MOPT, _CMD, _HALT)));
    while(1);
}

static void setHaltInterrupt(void)
{
	localWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_DEF(_PRGNLCL, _FALCON_IRQSSET,
			   _HALT, _SET));	
	riscvLwfenceIO();
}

static void changeSysMemCachable(LwBool bEnable)
{
	if (bEnable) {
		csr_write(LW_RISCV_CSR_MLDSTATTR,
				  (csr_read(LW_RISCV_CSR_MLDSTATTR) | (0x1 << 18)));
		csr_write(LW_RISCV_CSR_MFETCHATTR,
				  (csr_read(LW_RISCV_CSR_MFETCHATTR) | (0x1 << 18)));
	} else {
		csr_write(LW_RISCV_CSR_MLDSTATTR,
				  (csr_read(LW_RISCV_CSR_MLDSTATTR) & ~(0x1 << 18)));
		csr_write(LW_RISCV_CSR_MFETCHATTR,
				  (csr_read(LW_RISCV_CSR_MFETCHATTR) & ~(0x1 << 18)));
	}
}

/*!
 * @brief  Read SysMem PA allocated by lwgpu.
 */
static LwU64 getTestSysmemAddr(void)
{
    LwU32 addr_lo = 0;
    LwU32 addr_hi = 0;

    addr_lo = localRead(LW_PRGNLCL_FALCON_MAILBOX0);
    addr_hi = localRead(LW_PRGNLCL_FALCON_MAILBOX1);

    return ((LwU64)(addr_hi) << 32 | (LwU64)(addr_lo));
}

/*!
 * @brief Configure the debug print memory in DTCM. 0xC00 bytes at the end of
 * DMEM is used as debug print buffer. Queue 3 and SwGen1 is used in GSP for
 * communicating with the host.
 */
static void configDebugPrint(void)
{
	LwU32 dmemSize;
	LwU8 swgenNo = 1;

	dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
	// Reported value is in 256bytes, so right shift by 8.
	dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

	// reset queues
	priWrite(LW_PGSP_QUEUE_HEAD(GSP_DEBUG_BUFFER_QUEUE), 0);
	priWrite(LW_PGSP_QUEUE_TAIL(GSP_DEBUG_BUFFER_QUEUE), 0);
	riscvFenceRWIO();

	// Clean debug buffer (in case it's not clean)
	memset((LwU8*)LW_RISCV_AMAP_DMEM_START + dmemSize - DMEM_END_CARVEOUT_SIZE,
		   0, DMEM_END_CARVEOUT_SIZE);

	printInitEx((LwU8*)LW_RISCV_AMAP_DMEM_START +
				(dmemSize - DMEM_END_CARVEOUT_SIZE),
				DMEM_END_CARVEOUT_SIZE,
				LW_PGSP_QUEUE_HEAD(GSP_DEBUG_BUFFER_QUEUE),
				LW_PGSP_QUEUE_TAIL(GSP_DEBUG_BUFFER_QUEUE),
				swgenNo);

	localWrite(LW_PRGNLCL_FALCON_LOCKPMB,
            DRF_DEF(_PRGNLCL, _FALCON_LOCKPMB, _IMEM, _LOCK) |
            DRF_DEF(_PRGNLCL, _FALCON_LOCKPMB, _DMEM, _UNLOCK));
}

static void configInterruptsDest(void)
{
	LwU32 irqDest = 0U;
	LwU32 irqMset = 0U;
	LwU32 irqSCMask = 0U;

	// HALT: CPU transitioned from running into halt
	irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _HALT, _HOST, irqDest);
	irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _HALT, _SET, irqMset);

	// SWGEN1: for the print buffer
	irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN1, _HOST, irqDest);
	irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN1, _SET, irqMset);

	localWrite(LW_PRGNLCL_RISCV_IRQDEST, irqDest);
	localWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
	localWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);
    
	irqSCMask = FLD_SET_DRF(_PRGNLCL, _FALCON_IRQSCMASK, _HALT, _ENABLE, irqSCMask);
	irqSCMask = FLD_SET_DRF(_PRGNLCL, _FALCON_IRQSCMASK, _SWGEN1, _ENABLE, irqSCMask);
	localWrite(LW_PRGNLCL_FALCON_IRQSCMASK, irqSCMask);
}

static void configWdTimer(void)
{
	/*
	 * TODO: Need to configure this based on platform(Pre-si or Si).
	 * Set boot watchdog limit to 2 * standard limit for Si and 10 * standard
	 * limit for Pre-Si.
	 */
    localWrite(LW_PRGNLCL_FALCON_WDTMRVAL, 10 * WATCHDOG_TIMER_TIMEOUT_1S);
	//  USE_PTIMER | ENABLE
    localWrite(LW_PRGNLCL_FALCON_WDTMRCTL, (1<<1) | (1<<0));
}

static void configSecinitPLMs(void)
{
	localWrite(LW_PRGNLCL_FALCON_AMAP_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_BOOTVEC_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_DBGCTL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_DIODT_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_DIODTA_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_DMA_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_DMEM_PRIV_LEVEL_MASK,
			   CPU_SOURCE_ENABLE_L0_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_ENGCTL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_HSCTL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_IMEM_PRIV_LEVEL_MASK,
			   GSP_SOURCE_ENABLE_L3_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_INTR_CTRL_PRIV_LEVEL_MASK(1),
			   ALL_SOURCE_ENABLE_L0_READ);
	localWrite(LW_PRGNLCL_FALCON_MTHDCTX_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_PMB_DMEM_PRIV_LEVEL_MASK(0),
			   CPU_SOURCE_ENABLE_L0_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_PMB_IMEM_PRIV_LEVEL_MASK(0),
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_SAFETY_CTRL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_SCTL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_SHA_RAL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_TMR_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_TRACEBUF_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_WDTMR_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FBIF_CTL2_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FBIF_REGIONCFG_PRIV_LEVEL_MASK,
			   ALL_SOURCE_ENABLE_L0_READ);
	localWrite(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK,
			   ALL_SOURCE_ENABLE_L0_READ);
	localWrite(LW_PRGNLCL_RISCV_BCR_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_ENABLE_L0_READ);
	localWrite(LW_PRGNLCL_RISCV_IRQ_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_RISCV_MSIP_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_RISCV_LWCONFIG_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_RISCV_TRACEBUF_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_CG2_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_CSBERR_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_EXCI_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_EXTERR_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_BRKPT_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_ICD_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_IDLESTATE_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_KFUSE_LOAD_CTL_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_KMEM_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_RSTAT0_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_SP_MIN_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_SVEC_SPR_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_RISCV_ERR_PRIV_LEVEL_MASK,
			   ALL_SOURCE_ENABLE_L0_READ);
	localWrite(LW_PRGNLCL_RISCV_PICSCMASK_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_RISCV_MSPM_PRIV_LEVEL_MASK,
			   ALL_SOURCE_DISABLE_NO_READ_WRITE);
}

static void configDeviceMap(void)
{
	LwU32 group;
	LwU32 fld;
	LwU32 reg;

	group = LW_PRGNLCL_DEVICE_MAP_GROUP_MMODE /
				LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;

	fld = LW_PRGNLCL_DEVICE_MAP_GROUP_MMODE %
			LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;

	reg = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group));

	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _WRITE, fld, _DISABLE, reg);
	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _READ, fld, _DISABLE, reg);
	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _LOCK, fld, _LOCKED, reg);
	localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);

	riscvLwfenceRWIO();

	group = LW_PRGNLCL_DEVICE_MAP_GROUP_PLM /
				LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;

	fld = LW_PRGNLCL_DEVICE_MAP_GROUP_PLM %
			LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;

	reg = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group));

	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _WRITE, fld, _DISABLE, reg);
	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _READ, fld, _DISABLE, reg);
	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _LOCK, fld, _LOCKED, reg);
	localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);

	riscvLwfenceRWIO();

	group = LW_PRGNLCL_DEVICE_MAP_GROUP_PRGN_CTL /
				LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;

	fld = LW_PRGNLCL_DEVICE_MAP_GROUP_PRGN_CTL %
			LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;

	reg = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group));

	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _WRITE, fld, _DISABLE, reg);
	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _READ, fld, _DISABLE, reg);
	reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE,
							  _LOCK, fld, _LOCKED, reg);
	localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);

	riscvLwfenceRWIO();
}

// Define used to set the WD Interval before starting a WD test. Pre-Si - 1000,
// Si - 5000
#define JUST_IN_TIME_INITIAL_TIMEOUT 5000
// Max WD timer interval to wait before petting the WD again. A value of 100 is
// what is needed in Pre-Si.
// TODO Need to modify this according to the platform. Pre-si - 100 Si - 10
#define JUST_IN_TIME_WD_MARGIN 100
// Max PTimer interval to wait before petting the WD again. A value of 1000 is
// what is needed in Pre-Si.
// TODO Need to modify this according to the platform. Pre-Si - 1000 Si - 
#define JUST_IN_TIME_PTIMER_MARGIN 1000
// Max RDCYCLE interval to wait before petting the WD again. A value of 30 is
// what is needed in Pre-Si.
// TODO Need to modify this according to the platform. Pre-si - 30 Si -
#define JUST_IN_TIME_RDCYCLE_MARGIN 30
#define JUST_IN_TIME_RESET_MARGIN 10
// Static variable used to keep a note of different WDT tests run.
// A WD IRQSTAT is checked only for the first run of each WD test.
static LwU8 wdtTestHistory = 0;

/*!
 * @brief Watchdog tests.
 *
 * Right now the function supports three different tests for watchdog selected
 * using the param \a flags. Following are the different tests,
 * - \a flags == 0, uses the current WDTMRVAL to decide on when to pet the WD
 * again.
 * - \a flags == 1, uses the current PTIMER value to decide on when to pet the
 * WD again.
 * - \a flags == 2, uses the current HPMCOUNTER TIME to decide on when to pet
 * the WD again.
 *
 * @param flags: Indicate which test to run.
 */
LwU64 jitWatchdogTest(LwU8 flags)
{
	LwU64 ptimer_ns, ptimer_ns_initial;
	LwU64 ptimer, ptimer_initial;

	/*
	 * Check for WDTimer Interrupt status for only the first runs of WDT test.
	 * Rationale: First time we need to fully test WDT.
	 * After the first iteration we are only using this test to stress PRI.
	 */
	if (wdtTestHistory != 7) {
		if (FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _WDTMR, _TRUE,
			localRead(LW_PRGNLCL_FALCON_IRQSTAT))) {
			// WDTMR fired already, this test is invalid.
			return 1;
		}
	}

	if (flags == 0)
	{
		PET_WATCHDOG(JUST_IN_TIME_INITIAL_TIMEOUT); // 1.024 ms
		while (localRead(LW_PRGNLCL_FALCON_WDTMRVAL) > JUST_IN_TIME_WD_MARGIN);
		PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_1S);
	}
	else if (flags == 1)
	{
		PET_WATCHDOG(JUST_IN_TIME_INITIAL_TIMEOUT); // 1.024 ms
		ptimer_ns = localRead(LW_PRGNLCL_FALCON_PTIMER0) |
						(LwU64)(localRead(LW_PRGNLCL_FALCON_PTIMER1)) << 32ull;
		ptimer_ns_initial = ptimer_ns;
		//each WDTMR tick = 1024 PTIMER units.
		//A PTIMER unit is 1ns (increments by 32 every 32ns.)
		// For Pre-Si a value of 1000 is used for the check.
		do {
			ptimer_ns = localRead(LW_PRGNLCL_FALCON_PTIMER0) |
						(LwU64)(localRead(LW_PRGNLCL_FALCON_PTIMER1)) << 32ull;
		} while ((ptimer_ns - ptimer_ns_initial) <
				 ((32 * 32 * JUST_IN_TIME_INITIAL_TIMEOUT -
				  JUST_IN_TIME_RESET_MARGIN * 32*32)));
		PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_1S);
	}
	else if (flags == 2)
	{
		PET_WATCHDOG(JUST_IN_TIME_INITIAL_TIMEOUT); // 1.024 ms
		ptimer = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
		ptimer_initial = ptimer;
		// each WDTMR tick = 32 RDCYCLE units.
		// A RDCYCLE unit is 32ns (increments by 1 every 32 ns.)
		// For Pre-Si a value of 30 is used for the check.
		do {
			ptimer = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
		} while ((ptimer - ptimer_initial) <
				 ((32 * JUST_IN_TIME_INITIAL_TIMEOUT) -
				 (JUST_IN_TIME_RESET_MARGIN * 32)));
		PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_1S);
	}

	/* Check for WDTimer Interrupt status. */
	if (wdtTestHistory != 7) {
		if (FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _WDTMR, _TRUE,
			localRead(LW_PRGNLCL_FALCON_IRQSTAT))) {
			return 1;
		}
	}

	wdtTestHistory |= (LwU8)(1<<flags);

	return 0;
}

static void enableWatchdogInterrupt(LwBool bEnable)
{
	LwU32 wdtBitMask = 0;
	wdtBitMask = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET,
							 _WDTMR, _SET, wdtBitMask);

	if (bEnable)
	{
		localWrite(LW_PRGNLCL_RISCV_IRQMSET, wdtBitMask);
	}
	else
	{
		localWrite(LW_PRGNLCL_RISCV_IRQMCLR, wdtBitMask);
	}
}

void stressTestLoop(void)
{
	struct lz4CompressionTestConfig lz4TestConfig;
	register LwU64 iterations = 0;
	LwU32 testIdx = 1;
	LwU64 ptimer0, ptimer1;

	LZ4_malloc_init((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 20480);

	//Wait for LwGPU to update the MAILBOX to start the stress loop.
	while (localRead(LW_PRGNLCL_FALCON_MAILBOX1) != 0xFFFFFFFF) {
	}

	// This is the MB0 register used to pass the current runnning test.
	localWrite(LW_PRGNLCL_FALCON_MAILBOX0, 0);
	// This is the MB1 register used to pass the current iteration.
	localWrite(LW_PRGNLCL_FALCON_MAILBOX1, iterations);

	printf("Stress Loop started\n");

	configWdTimer();

	enableWatchdogInterrupt(0);

	//PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_1S);

	while (true)
	{
		ptimer0 = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
		testIdx = 1;
		//Stress test loop.

		// PRI polling test:
		RUN_TEST(jitWatchdogTest,0); // Test 1
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		RUN_TEST(jitWatchdogTest,1); // Test 2
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		RUN_TEST(jitWatchdogTest,2); // Test 3
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		// Test 4 - cached LFSR test (CRC table in cache)
		// 60KiB DTCM truncated LFSR
		lfsrTest(0x7800, (void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 0);
		verify((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 61440, 0xaca75c7a);
		fastAlignedMemSet((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K),
						  0xa5, 61440);
		verify((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 61440, 0x64490a48);
		fastAlignedMemSet((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K),
						  0, 61440);
		verify((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 61440, 0xec1c6272);
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		changeSysMemCachable(LW_FALSE);
		__asm__ __volatile__("fence.i");

		// Test 5
		// uncached LFSR test in SysMem. First 4k in SysMem has the 
		// CRC tables, so skip first 4k. 
		// TODO: Will the CRC table be already in cache? Should cache be
		// ilwalidated?
		lfsrTest(0x7800,(void*)(g_sysmem_block_addr + 0x1000), 0);
		verify((void*)(g_sysmem_block_addr + 0x1000), 61440, 0xaca75c7a);
		fastAlignedMemSet((void*)(g_sysmem_block_addr + 0x1000), 0xa5, 61440);
		verify((void*)(g_sysmem_block_addr + 0x1000), 61440, 0x64490a48);
		fastAlignedMemSet((void*)(g_sysmem_block_addr + 0x1000), 0, 61440);
		verify((void*)(g_sysmem_block_addr + 0x1000), 61440, 0xec1c6272);
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		changeSysMemCachable(LW_TRUE);
		__asm__ __volatile__("fence.i");

		// Test 6 - MUL test
		RUN_TEST(pipelinedMulTest, 0);
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		// Test 7 - LZ4 test
		lz4TestConfig.src_size = 0x4000;
		lz4TestConfig.src = (void*)LW_RISCV_AMAP_IMEM_START;
		lz4TestConfig.src2 = (void*)LW_RISCV_AMAP_DMEM_START + SIZE_32K;
		// SysMem compress destination TODO Change the location to DTCM
		lz4TestConfig.dest = (void*)(g_sysmem_block_addr + 0x1000);
		lz4TestConfig.dest_size = 0x4000;
		RUN_TEST(lz4CompressionTest, &lz4TestConfig);
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		// Test 8 - BP torture test
		RUN_TEST(branchPredictorTortureTest, 131072);
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		// Test 9 - BP torture test with MUL
		RUN_TEST(branchPredictorMultiplyTortureTest, 131072);
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		// Test 10 - Pi callwlation test
		// AKA pi value we want to halt on
		RUN_TEST_EXPECT(callwlatePiAndVerify, 0x40490fec, 0x555555);
		localWrite(MEMRW_GADGET_CTRL, testIdx++);

		if (!(iterations % 100)) {
			printf("Iteration %d, IRQSTAT=%x WDTMR=%d\n", iterations,
					localRead(LW_PRGNLCL_FALCON_IRQSTAT),
					localRead(LW_PRGNLCL_FALCON_WDTMRVAL));
		}

		iterations++;
		localWrite(MEMRW_GADGET_STATUS, iterations); // iterations completed
		localWrite(MEMRW_GADGET_CTRL, 0); // reset to zero

		// Pet the watchdog.
		PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_1S);

		ptimer1 = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
		if ((iterations % 100) == 20)
		{
			printf("> Iteration takes %u ticks (%u WDTMR us)\n",
				   ptimer1 - ptimer0, (ptimer1 - ptimer0) / 32);
		}
	}

fail_with_error:
	return;
}

static void runLfsrSanity(void)
{
    printf("Running LFSR sanity in DTCM...\n");
	// 60KiB DTCM truncated LFSR
    lfsrTest(0x7800, (void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 1);

    printf("Verifying LFSR output...\n");
    verify((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 61440, 0xaca75c7a);

    printf("Clearing DMEM...\n");
    fastAlignedMemSet((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 0xa5, 61440);
    verify((void*)(LW_RISCV_AMAP_DMEM_START + SIZE_1K), 61440, 0x64490a48);

    printf("LFSR sanity passed.\n");
}

static void configRiscvPerf(void)
{
	/*
	 * Enable FPU.
	 */
	csr_write(LW_RISCV_CSR_MSTATUS,
			  (csr_read(LW_RISCV_CSR_MSTATUS)  & 0xFFFFFFFFFFFDE7FFULL) |
			  0x0000000000002000ULL);

	//turn on all perf features
	//Write CSR Flush Pipeline is disabled for M and S mode, this test is
	//running in M mode. Spelwlative load beyond dCache is enabled for U, S
	//and M mode. Posted write also enabled for all modes.
	csr_write(LW_RISCV_CSR_MCFG, 0xbb0a);

	
	// Enabled BTB, BHT and RAS BP for all modes.
	csr_write(LW_RISCV_CSR_MBPCFG, 0x7);

	__asm__ __volatile__("fence.i");

	riscvFenceRWIO();
}

static void releasePriLockdown(void)
{
	localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
			DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));
	
	riscvLwfenceRWIO();
}

/*!
 * @brief  Stress Test ucode Entry.
 */
int main(void)
{
	g_sysmem_block_addr = getTestSysmemAddr();

	g_sysmem_block_addr += LW_RISCV_AMAP_EXTMEM2_START;

	configSecinitPLMs();

	releasePriLockdown();
	
	configDeviceMap();

	//Configure the debug prints.
	configDebugPrint();

	// Configure the Interrupts Destination.
	configInterruptsDest();

	// Init CRC table in SysMem.
	init_crc_table();

	printf("CRC initialized in SysMem\n");

	localWrite(LW_PRGNLCL_FALCON_MAILBOX0, global_var);

	printf("Mailbox Updated.\n");

	// Run LFSR Sanity test on DMEM. 60K from DMEM_START + 1K.
	runLfsrSanity();

	configRiscvPerf();

	/* Run LFSR Sanity test on DMEM after CSR settings. */
	runLfsrSanity();

	//PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_1S);

	stressTestLoop();

	localWrite(LW_PRGNLCL_FALCON_WDTMRCTL, 0);

	setHaltInterrupt();

	appShutdown();

	return 0;
}
