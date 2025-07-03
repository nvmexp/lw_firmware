/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#define LW_PARAM_CONCAT(x) .lw.constant0.x
#define LW_PARAM_NAME(x) param ## x

#define LW_INFO(test, attr, val) \
                    .infofmt EIFMT_SVAL \
                    .infoattr attr \
                    .align  4 \
                    .data \
                    { \
                        .b16 8 \
                        .b32 index@test \
                        .b32 val \
                    }

#define LW_INFO_BLOCK(test) LW_INFO(test, EIATTR_REGCOUNT, 0x100) \
                            LW_INFO(test, EIATTR_MIN_STACK_SIZE, 0x0) \
                            LW_INFO(test, EIATTR_FRAME_SIZE, 0x0)

#define LWI_MAXREG_COUNT(val) \
                    .infofmt EIFMT_HVAL \
                    .infoattr EIATTR_MAXREG_COUNT \
                    .align 4 \
                    .data \
                    { \
                        .b16 val \
                    }

#define LWI_PARAM_CBANK(test, val1, val2, val3) \
                    .infofmt EIFMT_SVAL \
                    .infoattr EIATTR_PARAM_CBANK \
                    .align 4 \
                    .data \
                    { \
                        .b16 val1 \
                        .b32 index@LW_PARAM_CONCAT(test) \
                        .b16 val2 \
                        .b16 val3 \
                    }

#define LWI_CBANK_PARAM_SIZE(val) \
                    .infofmt EIFMT_HVAL \
                    .infoattr EIATTR_CBANK_PARAM_SIZE \
                    .align 4 \
                    .data \
                    { \
                        .b16 val \
                    }

#define LWI_KPARAM_INFO(v1, v2, v3) \
                    .infofmt EIFMT_SVAL \
                    .infoattr EIATTR_KPARAM_INFO \
                    .align 4 \
                    .data \
                    { \
                        .b16 v1 \
                        .b32 0x0 \
                        .b16 0 \
                        .b16 0 \
                        .b8 0, v2, v3, 0 \
                    }

#define LW_PARAM(test, size) \
                .constant LW_PARAM_NAME(test) \
                .section LW_PARAM_CONCAT(test) \
                { \
                    LW_PARAM_NAME(test): \
                    { \
                        .zero size \
                    } \
                }

#define ASM_BLOCK(f, a) f(a##0) f(a##1) f(a##2) f(a##3) f(a##4) \
                        f(a##5) f(a##6) f(a##7) f(a##8) f(a##9)

#define GEN_LABEL(x, n) x ## _ ## n
#define GEN_LABEL3(x, y, z) x ## _ ## y ## _ ## z

