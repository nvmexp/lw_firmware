/*
 * Copyright (c) 2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

#ifndef SECHUB_SELITE_ENGINE_ACCESSORS_H
#define SECHUB_SELITE_ENGINE_ACCESSORS_H

#include <stdint.h>

void     dioSeWriteWrapper(volatile void* addr, uint32_t val);
uint32_t dioSeReadWrapper(volatile void* addr);
void     writeWrapper(volatile void* addr, uint32_t val);
uint32_t readWrapper(volatile void* addr);
uint32_t ccc_device_get_reg_normal_redirect(uint32_t *dev, uint32_t reg);
void     ccc_device_set_reg_normal_redirect(uint32_t *dev, uint32_t reg, uint32_t data);

#define LWRNG_SET_REG(baseaddr, reg, data)      dioSeWriteWrapper(&(baseaddr)[reg], (data))
#define LWRNG_GET_REG(baseaddr, reg)	        dioSeReadWrapper(&(baseaddr)[reg])
#define LWRNG_GET_REG_NOTRACE(baseaddr, reg)	dioSeReadWrapper(&(baseaddr)[reg])

#endif // SECHUB_SELITE_ENGINE_ACCESSORS_H
