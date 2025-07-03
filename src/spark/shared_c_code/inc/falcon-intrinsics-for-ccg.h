#ifndef _FALCON_INTRINSICS_FOR_CCG_H
#define _FALCON_INTRINSICS_FOR_CCG_H

#include <falcon-intrinsics.h>

// Extern functions
extern void exception_handler_selwre_hang(void);

// Wrapper implementation to call falc_jmp. This is needed because it is not implemented
// in <falcon-intrinsics.h>.
__attribute__((always_inline)) static inline
void falc_jmp( unsigned int addr ){
    asm volatile( " jmp %0; "::"r"(addr) : "memory" );
}

// Wrapper implementation to call falc_ldx_i. This is needed because the 
// <falcon-intrinsics.h> of falc_ldx_i accepts unsigned int * as parameter whereas we
// can pass unsigned int as parameter from ADA.
__attribute__((always_inline)) static inline
unsigned int falc_ldx_i_wrapper(unsigned int csbOffset, unsigned int bitNum){
    return falc_ldx_i((unsigned int*)(csbOffset), bitNum);
}

// Wrapper implementation to call falc_stx. This is needed because the 
// <falcon-intrinsics.h> of falc_stx accepts unsigned int * as parameter whereas we
// can pass unsigned int as parameter from ADA.
__attribute__((always_inline)) static inline
void falc_stx_wrapper(unsigned int csbOffset, unsigned int val){
    falc_stx((unsigned int*)(csbOffset),(val));
}

// Wrapper implementation to call falc_stxb. This is needed because the 
// <falcon-intrinsics.h> of falc_stxb accepts unsigned int * as parameter whereas we
// can pass unsigned int as parameter from ADA.
__attribute__((always_inline)) static inline
void falc_stxb_wrapper(unsigned int csbOffset, unsigned int val){
    falc_stxb((unsigned int*)(csbOffset),(val));
}

// Implementation set SP to last addr of DMEM.
__attribute__((always_inline)) static inline
void
Hs_Entry_Set_Sp(unsigned int dmemEndAddr)
{
    falc_wspr(SP, dmemEndAddr);
}

// Wrapper implementation to install EV = exception_handler.
__attribute__((always_inline)) static inline
void
install_exception_handler_hang_to_ev(void)
{    falc_wspr(EV, exception_handler_selwre_hang);
}

__attribute__((always_inline)) static inline

unsigned int falc_rspr_sec_spr_wrapper(void){

    unsigned int secSpr;

    falc_rspr(secSpr, SEC);

    return secSpr;

}

__attribute__((always_inline)) static inline

void falc_wspr_sec_spr_wrapper(unsigned int secSpr){

    falc_wspr(SEC, secSpr);

}

__attribute__((always_inline)) static inline
unsigned int falc_rspr_csw_spr_wrapper(void){

	unsigned int cswSpr;

	falc_rspr(cswSpr, CSW);

	return cswSpr;

}

__attribute__((always_inline)) static inline

void falc_wspr_csw_spr_wrapper(unsigned int cswSpr){

	falc_wspr(CSW, cswSpr);

}

__attribute__((always_inline)) static inline
unsigned int falc_rspr_sp_spr_wrapper(void){

	unsigned int spSpr;

	falc_rspr(spSpr, SP);

	return spSpr;

}

__attribute__((always_inline)) static inline
void clearFalconGprs(void)
{
    //  clear all Falcon GPR's.
    asm volatile ( "clr.w a0;");
    asm volatile ( "clr.w a1;");
    asm volatile ( "clr.w a2;");
    asm volatile ( "clr.w a3;");
    asm volatile ( "clr.w a4;");
    asm volatile ( "clr.w a5;");
    asm volatile ( "clr.w a6;");
    asm volatile ( "clr.w a7;");
    asm volatile ( "clr.w a8;");
    asm volatile ( "clr.w a9;");
    asm volatile ( "clr.w a10;");
    asm volatile ( "clr.w a11;");
    asm volatile ( "clr.w a12;");
    asm volatile ( "clr.w a13;");
    asm volatile ( "clr.w a14;");
    asm volatile ( "clr.w a15;");
}

__attribute__((always_inline)) static inline
unsigned int get_return_addr_from_linkervar(void)
{
	unsigned int* returnAddr = __builtin_return_address(0);
    return (unsigned int)returnAddr;
}
/*
// variable to store canary for SSP
void * __stack_chk_guard;
GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_NOINLINE()
void __canary_setup(void)
{
    __stack_chk_guard = (void *)RANDOM_CANARY_NS;
}
*/

#endif // _FALCON_INTRINSICS_FOR_CCG_H
