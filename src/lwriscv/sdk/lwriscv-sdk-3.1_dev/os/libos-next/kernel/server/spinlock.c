#include "kernel.h"
#include "spinlock.h"
#include <lwmisc.h>
#include "diagnostics.h"
#include "scheduler.h"

/**
 * @brief Aquires a kernel spinlock. Interrupts will be disabled for the duration.
 * 
 *        Multiple spinlocks may be held simultaneously.
 *        Spinlocks are not re-entrant (you may not re-aquire one you already hold.)
 *  
 * @param spinner 
 */
void KernelSpinlockAcquire(KernelSpinlock * spinner) 
{
#if !LIBOS_TINY
    // Disable interrupts and save previous state
    spinner->xstatus = KernelCsrSaveAndClearImmediate(LW_RISCV_CSR_XSTATUS, REF_DEF64(LW_RISCV_CSR_XSTATUS_XIE, _ENABLE));

    // Are we already holding the spinlock? 
    KernelAssert(spinner->ownerScratch != KernelCsrRead(LW_RISCV_CSR_XSCRATCH) && "Not re-entrant");

    /**
     * @brief LIBOS is lwrrently uniprocessor.  Since interrupts are disabled
     *        while any spinlock is held, there cannot be contention.
     */
    KernelAssert(spinner->ownerScratch == 0 && "Internal error");

    // Set the owner
    spinner->ownerScratch = KernelCsrRead(LW_RISCV_CSR_XSCRATCH);    
#endif
}

/**
 * @brief Releases a kernel spinlock.
 * 
 * Interupts will be re-enabled when the last spinlock is released.
 * 
 * @param spinner 
 */
void KernelSpinlockRelease(KernelSpinlock * spinner) {
#if !LIBOS_TINY
    // Make sure we're not releasing someone elses lock
    KernelAssert(spinner->ownerScratch == KernelCsrRead(LW_RISCV_CSR_XSCRATCH));

    // Make sure interrupts are still off.
    KernelAssert(!(KernelCsrRead(LW_RISCV_CSR_XSTATUS) & REF_DEF64(LW_RISCV_CSR_XSTATUS_XIE, _ENABLE)));
    
    // Release the lock state while stil guarded by the interrupts being disabled
    spinner->ownerScratch = 0;
    
    // Restore the interrupt state
    KernelCsrWrite(LW_RISCV_CSR_XSTATUS, spinner->xstatus);  
#endif
}
