#pragma once

#include "kernel.h"

typedef struct {
    /**
     * @brief We restore the entire xstatus when we release the spinlock.
     *        This ensures we correctly restore the previous interrupt enabled state.
     */
    LwU64 xstatus;

#if LIBOS_DEBUG
    // debug only: snapshot of the holding tasks xscratch register
    //             this is used to assert we own the lock.
    LwU64 ownerScratch;       
#endif
} KernelSpinlock;

void   KernelSpinlockAcquire(KernelSpinlock * spinner);
void   KernelSpinlockRelease(KernelSpinlock * spinner);

/**
 * @brief Asserts the current task holds the spinlock
 *  
 * @param spinner 
 */
#if !LIBOS_TINY
#define KernelAssertSpinlockHeld(spinner)                                                                   \
    /* Make sure we're the owner. */                                                                        \
    KernelAssert((spinner)->ownerScratch == KernelCsrRead(LW_RISCV_CSR_XSCRATCH));                          \
                                                                                                            \
    /* Make sure interrupts are still disabled */                                                           \
    KernelAssert(!(KernelCsrRead(LW_RISCV_CSR_XSTATUS) & REF_DEF64(LW_RISCV_CSR_XSTATUS_XIE, _ENABLE)));
#else
#define KernelAssertSpinlockHeld(spinner)
#endif