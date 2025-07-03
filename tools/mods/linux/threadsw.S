        .text
        .align  4
/*
  _SwitchThreads(pOldStackPtr, NewStackPtr)
 
  This function pushes the value of all registers and saves the 
  current value of esp into pOldStackPtr.  It then writes esp with
  the value of NewStackPtr and pops the value of all the registers
  using the new stack pointer and then returns using the new stack
  pointer 
 
  Parameters:
      eax/rdi : Pointer to storage for the current (old thread)
                stack pointer
      edx/rsi : Value of the new stack pointer
*/
        .globl  _SwitchThreads
_SwitchThreads:
#ifdef  __x86_64__
        /* don't save all the registers, only the callee-saved ones */
        pushq   %rbx         /* Store registers onto old stack. */
        pushq   %rbp
        pushq   %r12
        pushq   %r13
        pushq   %r14
        pushq   %r15
        pushq   %rdi          /* doesn't actually need to be saved, but used for initial launch */
        movq    %rsp, (%rdi)  /* store esp in old Thread object. */
        movq    %rsi, %rsp    /* esp = newStack  *** STACK MOVES HERE *** */
        popq    %rdi          /* arg for initial launch */
        popq    %r15
        popq    %r14
        popq    %r13
        popq    %r12
        popq    %rbp
        popq    %rbx
        retq
#else // __x86_64__
        pushal                   
        movl    %esp, (%eax)
        movl    %edx, %esp
        popal            
        ret
#endif // __x86_64__
