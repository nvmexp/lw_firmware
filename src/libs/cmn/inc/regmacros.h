/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef REG_MACROS_H
#define REG_MACROS_H

/*!
 * @file    regmacros.h
 *
 * @brief   Set of macros for performing field DRF-operations on registers.
 *
 * @details The macros are constructed in such a way that allows them to be
 *          used across multiple buses (CSB, BAR0, etc ...) based on what is
 *          available to the engine that is using them. The first parameter
 *          to each macro defines the name of the bus the operation is to be
 *          performed on. The bus name is not used for anything other than to
 *          establish the namespace for the operation. The only requirement is
 *          that application define the following macros for each bus that it
 *          it expects to use:
 *
 *  #define  <bus>_REG_RD32(addr)              <bus-specific implementation>
 *  #define  <bus>_REG_WR32(addr, data)        <bus-specific implementation>
 *  #define  <bus>_REG_RD32_STALL(addr)        <bus-specific implementation>
 *  #define  <bus>_REG_WR32_STALL(addr, data)  <bus-specific implementation>
 *
 * @note    Not all buses are expected to have stalling forms (_STALL). It is
 *          not necesary to provide these definitons when pipeline-stalling
 *          is not supported for the bus.
 */

/*!
 * @brief   Write a constant value to a specific field of a register.
 *
 * @note    Values are automatically truncated to fit into the field.
 * @note    This operation does NOT force a pipeline stall.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  f    field name within the register to update
 * @param[in]  c    field value constant to write
 */
#define REG_FLD_WR_DRF_DEF(bus,d,r,f,c)                                        \
    REG_WR32(bus, LW##d##r,                                                    \
        FLD_SET_DRF(d, r, f, c,                                                \
            REG_RD32(bus, LW##d##r)))

/*!
 * @brief   Write a constant value to specific field of a register
 *
 * @note    Values are automatically truncated to fit into the field.
 * @note    This is the pipeline stalling version of REG_FLD_WR_DRF_DEF
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  f    field name within the register to update
 * @param[in]  c    field value constant to write
 */
#define REG_FLD_WR_DRF_DEF_STALL(bus,d,r,f,c)                                  \
    REG_WR32_STALL(bus, LW##d##r,                                              \
        FLD_SET_DRF(d, r, f, c,                                                \
            REG_RD32_STALL(bus, LW##d##r)))  // NJ-TODO: Is this a bug?

/*!
 * @brief   Write a value to specific field of a register.
 *
 * @note    Values are automatically truncated to fit into the field.
 * @note    This operation does NOT force a pipeline stall.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  f    field name within the register to update
 * @param[in]  n    value of the field to write
 */
#define REG_FLD_WR_DRF_NUM(bus,d,r,f,n)                                        \
    REG_WR32(bus, LW##d##r,                                                    \
        FLD_SET_DRF_NUM(d, r, f, n,                                            \
            REG_RD32(bus, LW##d##r)))

/*!
 * @brief   Write a value to specific field of a register.
 * 
 * @note    Values are automatically truncated to fit into the field.
 * @note    This is the pipeline-stalling version of REG_FLD_WR_DRF_NUM.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  f    field name within the register to update
 * @param[in]  n    value of the field to write
 */
#define REG_FLD_WR_DRF_NUM_STALL(bus,d,r,f,n)                                  \
    REG_WR32_STALL(bus, LW##d##r,                                              \
        FLD_SET_DRF_NUM(d, r, f, n,                                            \
            REG_RD32(bus, LW##d##r)))  // NJ-TODO: Is this a bug?

/*!
 * @brief   Performs a 32-bit read operation on the given register
 *          and subsequently extracts data from a specific field.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  f    field name within the register to read
 *
 * @return  The field value which was read
 */
#define REG_FLD_RD_DRF(bus,d,r,f)                                              \
    DRF_VAL(d, r, f,                                                           \
        REG_RD32(bus, LW##d##r))

/*!
 * @brief   Test a register field for equality with a field constant.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  f    field name within the register to test
 * @param[in]  c    field definition value
 *
 * @return  LW_TRUE     if register field is equal to c
 * @return  LW_FALSE    otherwise
 */
#define REG_FLD_TEST_DRF_DEF(bus,d,r,f,c)                                      \
    FLD_TEST_DRF(d, r, f, c,                                                   \
        REG_RD32(bus, LW##d##r))

/*!
 * @brief   Test a register field for equality with a specified value.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  f    field name within the register to test
 * @param[in]  n    value to compare field with
 *
 * @return  LW_TRUE     if register field is equal to n
 * @return  LW_FALSE    otherwise
 */
#define REG_FLD_TEST_DRF_NUM(bus,d,r,f,n)                                      \
    FLD_TEST_DRF_NUM(d, r, f, n,                                               \
        REG_RD32(bus, LW##d##r))

/*!
 * @brief   Test an indexed register field for equality with a field constant.
 *
 * @note    Indexed version of REG_FLD_TEST_DRF_DEF macro.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  i    register-index
 * @param[in]  f    field name within the register to test
 * @param[in]  c    field definition value
 *
 * @return  LW_TRUE     if register field is equal to c
 * @return  LW_FALSE    otherwise
 */
#define REG_FLD_IDX_TEST_DRF_DEF(bus,d,r,i,f,c)                                \
    FLD_TEST_DRF(d, r, f, c,                                                   \
        REG_RD32(bus, LW##d##r(i)))

/*!
 * @brief   Performs a 32-bit read operation on the given indexed register
 *          and subsequently extracts data from a specific field.
 *
 * @note    Indexed version of REG_FLD_RD_DRF macro.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  i    register-index
 * @param[in]  f    field name within the register to read
 *
 * @return  The value which was read
 */
#define REG_FLD_IDX_RD_DRF(bus,d,r,i,f)                                        \
    DRF_VAL(d, r, f,                                                           \
        REG_RD32(bus, LW##d##r(i)))

/*!
 * @brief   Write a constant value to a specific field of an indexed register.
 *
 * @note    Values are automatically truncated to fit into the field.
 * @note    This operation does NOT force a pipeline stall.
 * @note    Indexed version of REG_FLD_WR_DRF_DEF macro.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  i    register-index
 * @param[in]  f    field name within the register to update
 * @param[in]  c    field value constant to write
 */
#define REG_FLD_IDX_WR_DRF_DEF(bus,d,r,i,f,c)                                  \
    REG_WR32(bus, LW##d##r(i),                                                 \
        FLD_SET_DRF(d, r, f, c,                                                \
            REG_RD32(bus, LW##d##r(i))))

/*!
 * @brief   Write a value to specific field of an indexed register.
 *
 * @note    Values are automatically truncated to fit into the field.
 * @note    This operation does NOT force a pipeline stall.
 * @note    Indexed version of REG_FLD_WR_DRF_NUM macro.
 *
 * @param[in]  bus  bus on which to request the operation
 * @param[in]  d    device-portion of the register name
 * @param[in]  r    register-portion of the register name
 * @param[in]  i    register-index
 * @param[in]  f    field name within the register to update
 * @param[in]  n    value of the field to write
 */
#define REG_FLD_IDX_WR_DRF_NUM(bus,d,r,i,f,n)                                  \
    REG_WR32(bus, LW##d##r(i),                                                 \
        FLD_SET_DRF_NUM(d, r, f, n,                                            \
            REG_RD32(bus, LW##d##r(i))))

/*!
 * @brief   Read a 32-bit register.
 *
 * @param[in]   bus     the bus on which to request the operation
 * @param[in]   addr    the address to read
 *
 * @return  The value which was read
 */
#define REG_RD32(bus,addr)                  bus##_REG_RD32(addr)

/*!
 * @brief   Write a 32-bit register.
 *
 * @param[in]   bus     the bus on which to request the operation
 * @param[in]   addr    the address to write
 * @param[in]   data    the data to write
 */
#define REG_WR32(bus,addr,data)             bus##_REG_WR32(addr, data)

/*!
 * @brief   Perform a stalling read of a 32-bit register.
 *
 * @param[in]   bus     the bus on which to request the operation
 * @param[in]   addr    the address to read
 *
 * @return  The value which was read
 */
#define REG_RD32_STALL(bus,addr)            bus##_REG_RD32_STALL(addr)

/*!
 * @brief   Perform a stalling write of a 32-bit register.
 *
 * @param[in]   bus     the bus on which to request the operation
 * @param[in]   addr    the address to write
 * @param[in]   data    the data to write
 */
#define REG_WR32_STALL(bus,addr,data)       bus##_REG_WR32_STALL(addr, data)

/*!
 * @brief Mask for External interface. Details in wiki at [1].
 * HW Bug: 200198584, to get the MASK and BAD Value for CSB and EXT Interface.
 * Once it is solved, remove this and use HW defines.
 */
//! [1] https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
#define EXT_INTERFACE_MASK_VALUE            0xfff00000U

/*!
 * @brief Bad value for External interface. Details in wiki at [1].
 * HW Bug: 200198584, to get the MASK and BAD Value for CSB and EXT Interface.
 * Once it is solved, remove this and use HW defines.
 */
//! [1] https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
#define EXT_INTERFACE_BAD_VALUE             0xbad00000U

/*!
 * @brief Mask for CSB interface. Details in wiki at [1].
 * HW Bug: 200198584, to get the MASK and BAD Value for CSB and EXT Interface.
 * Once it is solved, remove this and use HW defines.
 */
//! [1] https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
#define CSB_INTERFACE_MASK_VALUE            0xffff0000U

/*!
 * @brief Bad value for CSB interface. Details in wiki at [1].
 * HW Bug: 200198584, to get the MASK and BAD Value for CSB and EXT Interface.
 * Once it is solved, remove this and use HW defines.
 */
//! [1] https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
#define CSB_INTERFACE_BAD_VALUE             0xbadf0000U

#endif // REG_MACROS_H
