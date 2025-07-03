/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ga102_dev_riscv_csr_64_addendum_h__
#define __ga102_dev_riscv_csr_64_addendum_h__


/* Derivations to be used by assembly */

#define LW_RISCV_CSR_MLDSTATTR_WPR__SHIFT                                               24
#define LW_RISCV_CSR_MLDSTATTR_WPR__SHIFTMASK                                           (0xF << LW_RISCV_CSR_MLDSTATTR_WPR__SHIFT)

#define LW_RISCV_CSR_MLDSTATTR_COHERENT__SHIFT                                          19
#define LW_RISCV_CSR_MLDSTATTR_COHERENT__SHIFTMASK                                      (1 << LW_RISCV_CSR_MLDSTATTR_COHERENT__SHIFT)
#define LW_RISCV_CSR_MLDSTATTR_CACHEABLE__SHIFT                                         18
#define LW_RISCV_CSR_MLDSTATTR_CACHEABLE__SHIFTMASK                                     (1 << LW_RISCV_CSR_MLDSTATTR_CACHEABLE__SHIFT)

#define LW_RISCV_CSR_MMPUVA_VLD__SHIFT                                                  0
#define LW_RISCV_CSR_MMPUVA_VLD__SHIFTMASK                                              (1 << LW_RISCV_CSR_MMPUVA_VLD__SHIFT)

#define LW_RISCV_CSR_MMPUATTR_UR__SHIFT                                                 0
#define LW_RISCV_CSR_MMPUATTR_UR__SHIFTMASK                                             (1 << LW_RISCV_CSR_MMPUATTR_UR__SHIFT)
#define LW_RISCV_CSR_MMPUATTR_UW__SHIFT                                                 1
#define LW_RISCV_CSR_MMPUATTR_UW__SHIFTMASK                                             (1 << LW_RISCV_CSR_MMPUATTR_UW__SHIFT)
#define LW_RISCV_CSR_MMPUATTR_UX__SHIFT                                                 2
#define LW_RISCV_CSR_MMPUATTR_UX__SHIFTMASK                                             (1 << LW_RISCV_CSR_MMPUATTR_UX__SHIFT)

#define LW_RISCV_CSR_MMPUATTR_MR__SHIFT                                                 9
#define LW_RISCV_CSR_MMPUATTR_MR__SHIFTMASK                                             (1 << LW_RISCV_CSR_MMPUATTR_MR__SHIFT)
#define LW_RISCV_CSR_MMPUATTR_MW__SHIFT                                                 10
#define LW_RISCV_CSR_MMPUATTR_MW__SHIFTMASK                                             (1 << LW_RISCV_CSR_MMPUATTR_MW__SHIFT)
#define LW_RISCV_CSR_MMPUATTR_MX__SHIFT                                                 11
#define LW_RISCV_CSR_MMPUATTR_MX__SHIFTMASK                                             (1 << LW_RISCV_CSR_MMPUATTR_MX__SHIFT)

#define LW_RISCV_CSR_MCFG_BTB_EN__SHIFT                                                 1
#define LW_RISCV_CSR_MCFG_BTB_EN__SHIFTMASK                                             (1 << LW_RISCV_CSR_MCFG_BTB_EN__SHIFT)
#define LW_RISCV_CSR_MCFG_BHT_EN__SHIFT                                                 2
#define LW_RISCV_CSR_MCFG_BHT_EN__SHIFTMASK                                             (1 << LW_RISCV_CSR_MCFG_BHT_EN__SHIFT)
#define LW_RISCV_CSR_MCFG_RAS_EN__SHIFT                                                 3
#define LW_RISCV_CSR_MCFG_RAS_EN__SHIFTMASK                                             (1 << LW_RISCV_CSR_MCFG_RAS_EN__SHIFT)

#define LW_RISCV_CSR_MISA_BASE__SHIFT                                                   62
#define LW_RISCV_CSR_MISA_EXT__SHIFT                                                    0
#define LW_RISCV_CSR_MISA_RST                                                           ((LW_RISCV_CSR_MISA_BASE_RST << LW_RISCV_CSR_MISA_BASE__SHIFT) | \
                                                                                         (LW_RISCV_CSR_MISA_EXT_RST << LW_RISCV_CSR_MISA_EXT__SHIFT))

#define LW_RISCV_CSR_MVENDORID_RST                                                      ((LW_RISCV_CSR_MVENDORID_VENDORH_RST << 32) | \
                                                                                         (LW_RISCV_CSR_MVENDORID_VENDORL_RST << 0))
#endif // __ga102_dev_riscv_csr_64_addendum_h__
