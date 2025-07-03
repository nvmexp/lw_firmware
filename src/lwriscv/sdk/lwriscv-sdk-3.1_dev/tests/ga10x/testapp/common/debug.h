/*
 * testLib.h
 *
 *  Created on: Mar 21, 2017
 *      Author: soberl
 */
#ifndef TESTLIB_H_
#define TESTLIB_H_

#include "engine.h"
#include "cache.h"
#include "io.h"

int printf(const char* fmt, ...);
int sprintf(char* str, const char* fmt, ...);
int putchar(int ch);

#define ffs __builtin_ffsl

unsigned delay(unsigned cycles);

void memcpy(void *dst, const void *src, unsigned count);
void memset(void *s, int c, unsigned n);
int memcmp(const void *s1, const void *s2, unsigned n);
void dumpHex(const char *data, unsigned size, unsigned offs);

// inlined to not use stack or anything
static inline void enterIcd(void)
{
    lwfenceAll();
    riscvWrite(LW_PRISCV_RISCV_ICD_CMD, LW_PRISCV_RISCV_ICD_CMD_OPC_STOP);
    lwfenceAll();
}

#endif /* TESTLIB_H_ */
