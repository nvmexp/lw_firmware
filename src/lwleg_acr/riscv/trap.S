#include "lwriscv/asm-helpers.h"

.section ".text.trapEntry","ax",@progbits
.balign 4
FUNC trapEntry
    jal acrHandler
EFUNC trapEntry