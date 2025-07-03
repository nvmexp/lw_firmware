
void BakerShutdown()
{
    register unsigned long long a0 __asm__("a0") = 0;

    // Caution! This SBI is non-compliant and clears GP and TP
    //          This works out as TP is guaranteed to be 0 while in the kernel.
    //          We don't lwrrently use GP in the kernel.

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : : "r"(a0):);
}

unsigned long long zzz = 0;
unsigned long long aaa = 0xDEADBEEF;

__attribute__((used)) void bake() { 
    zzz = 1;
    BakerShutdown();
}