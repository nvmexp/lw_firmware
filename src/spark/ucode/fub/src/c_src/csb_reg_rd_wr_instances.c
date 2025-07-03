#define IN_ADA_BODY
#include "csb_reg_rd_wr_instances.h"

typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_lwu32GP203__generic_register;
extern void csb_reg_rd_wr_instances__csb_reg_rd32_lwu32(csb_reg_rd_wr_instances__csb_reg_rd32_lwu32GP203__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
typedef lw_types__Tlwu32B pri_err_handling__uc_csberrstatGP208__sourceX;
void csb_reg_rd_wr_instances__csb_reg_rd32_lwu32(csb_reg_rd_wr_instances__csb_reg_rd32_lwu32GP203__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_lwu32__uc_generic_registerGP731__source;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_lwu32__uc_generic_registerGP731__target;
  {
    const lw_types__lwu32 C56s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C56s, 0U);
    }
    (*reg) = (csb_reg_rd_wr_instances__csb_reg_rd32_lwu32__uc_generic_registerGP731__source)val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_cpuctl_register csb_reg_rd_wr_instances__csb_reg_rd32_cpuctlGP357__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_cpuctl(csb_reg_rd_wr_instances__csb_reg_rd32_cpuctlGP357__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_cpuctl(csb_reg_rd_wr_instances__csb_reg_rd32_cpuctlGP357__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_cpuctl__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_cpuctl_register csb_reg_rd_wr_instances__csb_reg_rd32_cpuctl__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C65s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C65s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_cpuctl_register csb_reg_rd_wr_instances__csb_reg_wr32_cpuctlGP473__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_wr32_cpuctl(const csb_reg_rd_wr_instances__csb_reg_wr32_cpuctlGP473__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_wr32_cpuctl(const csb_reg_rd_wr_instances__csb_reg_wr32_cpuctlGP473__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  typedef dev_sec_csb_ada__lw_csec_falcon_cpuctl_register csb_reg_rd_wr_instances__csb_reg_wr32_cpuctl__uc_lwu32GP1354__source
    GNAT_ALIGN(4);
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_wr32_cpuctl__uc_lwu32GP1354__target;

  const lw_types__lwu32 val = (*(csb_reg_rd_wr_instances__csb_reg_wr32_cpuctl__uc_lwu32GP1354__target*)(&(*reg)));
  {
    const lw_types__lwu32 C74s = (lw_types__lwu32)addr;
    {
      extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

      falc_stxb_wrapper(C74s, val);
    }
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_hwcfg1_register csb_reg_rd_wr_instances__csb_reg_rd32_hwcfg1GP589__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_hwcfg1(csb_reg_rd_wr_instances__csb_reg_rd32_hwcfg1GP589__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_hwcfg1(csb_reg_rd_wr_instances__csb_reg_rd32_hwcfg1GP589__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_hwcfg1__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_hwcfg1_register csb_reg_rd_wr_instances__csb_reg_rd32_hwcfg1__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C82s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C82s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_engid_register csb_reg_rd_wr_instances__csb_reg_rd32_engineidGP705__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_engineid(csb_reg_rd_wr_instances__csb_reg_rd32_engineidGP705__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_engineid(csb_reg_rd_wr_instances__csb_reg_rd32_engineidGP705__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_engineid__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_engid_register csb_reg_rd_wr_instances__csb_reg_rd32_engineid__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C91s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C91s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_ptimer0_register csb_reg_rd_wr_instances__csb_reg_rd32_timestampGP822__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_timestamp(csb_reg_rd_wr_instances__csb_reg_rd32_timestampGP822__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_timestamp(csb_reg_rd_wr_instances__csb_reg_rd32_timestampGP822__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_timestamp__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_ptimer0_register csb_reg_rd_wr_instances__csb_reg_rd32_timestamp__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C100s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C100s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_ptimer0_register csb_reg_rd_wr_instances__csb_reg_rd32_ptimer0GP941__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_ptimer0(csb_reg_rd_wr_instances__csb_reg_rd32_ptimer0GP941__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_ptimer0(csb_reg_rd_wr_instances__csb_reg_rd32_ptimer0GP941__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_ptimer0__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_ptimer0_register csb_reg_rd_wr_instances__csb_reg_rd32_ptimer0__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C109s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C109s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_doc_ctrl_register csb_reg_rd_wr_instances__csb_reg_rd32_doc_ctrlGP1059__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_doc_ctrl(csb_reg_rd_wr_instances__csb_reg_rd32_doc_ctrlGP1059__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_doc_ctrl(csb_reg_rd_wr_instances__csb_reg_rd32_doc_ctrlGP1059__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_doc_ctrl__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_doc_ctrl_register csb_reg_rd_wr_instances__csb_reg_rd32_doc_ctrl__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C118s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C118s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_wr32_lwu32GP1179__generic_register;
extern void csb_reg_rd_wr_instances__csb_reg_wr32_lwu32(csb_reg_rd_wr_instances__csb_reg_wr32_lwu32GP1179__generic_register reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_wr32_lwu32(csb_reg_rd_wr_instances__csb_reg_wr32_lwu32GP1179__generic_register reg, lw_types__reg_addr_types__csb_addr addr) {
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_wr32_lwu32__uc_lwu32GP1900__source;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_wr32_lwu32__uc_lwu32GP1900__target;

  const lw_types__lwu32 val = (csb_reg_rd_wr_instances__csb_reg_wr32_lwu32__uc_lwu32GP1900__source)reg;
  {
    const lw_types__lwu32 C127s = (lw_types__lwu32)addr;
    {
      extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

      falc_stxb_wrapper(C127s, val);
    }
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_dic_ctrl_register csb_reg_rd_wr_instances__csb_reg_rd32_dic_ctrlGP1275__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_dic_ctrl(csb_reg_rd_wr_instances__csb_reg_rd32_dic_ctrlGP1275__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_dic_ctrl(csb_reg_rd_wr_instances__csb_reg_rd32_dic_ctrlGP1275__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_dic_ctrl__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_dic_ctrl_register csb_reg_rd_wr_instances__csb_reg_rd32_dic_ctrl__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C135s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C135s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_dic_ctrl_register csb_reg_rd_wr_instances__csb_reg_wr32_dic_ctrlGP1395__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_wr32_dic_ctrl(const csb_reg_rd_wr_instances__csb_reg_wr32_dic_ctrlGP1395__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_wr32_dic_ctrl(const csb_reg_rd_wr_instances__csb_reg_wr32_dic_ctrlGP1395__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  typedef dev_sec_csb_ada__lw_csec_falcon_dic_ctrl_register csb_reg_rd_wr_instances__csb_reg_wr32_dic_ctrl__uc_lwu32GP1900__source
    GNAT_ALIGN(4);
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_wr32_dic_ctrl__uc_lwu32GP1900__target;

  const lw_types__lwu32 val = (*(csb_reg_rd_wr_instances__csb_reg_wr32_dic_ctrl__uc_lwu32GP1900__target*)(&(*reg)));
  {
    const lw_types__lwu32 C144s = (lw_types__lwu32)addr;
    {
      extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

      falc_stxb_wrapper(C144s, val);
    }
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_reset_priv_level_mask_register csb_reg_rd_wr_instances__csb_reg_rd32_flcn_reset_plmGP1595__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_flcn_reset_plm(csb_reg_rd_wr_instances__csb_reg_rd32_flcn_reset_plmGP1595__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_flcn_reset_plm(csb_reg_rd_wr_instances__csb_reg_rd32_flcn_reset_plmGP1595__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_flcn_reset_plm__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_reset_priv_level_mask_register csb_reg_rd_wr_instances__csb_reg_rd32_flcn_reset_plm__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C152s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C152s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_reset_priv_level_mask_register csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plmGP1743__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plm(const csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plmGP1743__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plm(const csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plmGP1743__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  typedef dev_sec_csb_ada__lw_csec_falcon_reset_priv_level_mask_register csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plm__uc_lwu32GP1354__source
    GNAT_ALIGN(4);
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plm__uc_lwu32GP1354__target;

  const lw_types__lwu32 val = (*(csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plm__uc_lwu32GP1354__target*)(&(*reg)));
  {
    const lw_types__lwu32 C161s = (lw_types__lwu32)addr;
    {
      extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

      falc_stxb_wrapper(C161s, val);
    }
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csb_reg_rd_wr_instances__csb_reg_rd32_csberrstatGP1890__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_csberrstat(csb_reg_rd_wr_instances__csb_reg_rd32_csberrstatGP1890__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_csberrstat(csb_reg_rd_wr_instances__csb_reg_rd32_csberrstatGP1890__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_csberrstat__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csb_reg_rd_wr_instances__csb_reg_rd32_csberrstat__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C169s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C169s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_bar0_csr_register csb_reg_rd_wr_instances__csb_reg_rd32_bar0csrGP2022__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_bar0csr(csb_reg_rd_wr_instances__csb_reg_rd32_bar0csrGP2022__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_bar0csr(csb_reg_rd_wr_instances__csb_reg_rd32_bar0csrGP2022__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_bar0csr__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_bar0_csr_register csb_reg_rd_wr_instances__csb_reg_rd32_bar0csr__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C178s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C178s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_bar0_csr_register csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csrGP2142__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csr(const csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csrGP2142__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csr(const csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csrGP2142__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  typedef dev_sec_csb_ada__lw_csec_bar0_csr_register csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csr__uc_lwu32GP1900__source
    GNAT_ALIGN(4);
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csr__uc_lwu32GP1900__target;

  const lw_types__lwu32 val = (*(csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csr__uc_lwu32GP1900__target*)(&(*reg)));
  {
    const lw_types__lwu32 C187s = (lw_types__lwu32)addr;
    {
      extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

      falc_stxb_wrapper(C187s, val);
    }
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_mailbox0_register csb_reg_rd_wr_instances__csb_reg_wr32_mailboxGP2274__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_wr32_mailbox(const csb_reg_rd_wr_instances__csb_reg_wr32_mailboxGP2274__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_wr32_mailbox(const csb_reg_rd_wr_instances__csb_reg_wr32_mailboxGP2274__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  typedef dev_sec_csb_ada__lw_csec_falcon_mailbox0_register csb_reg_rd_wr_instances__csb_reg_wr32_mailbox__uc_lwu32GP1354__source
    GNAT_ALIGN(4);
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_wr32_mailbox__uc_lwu32GP1354__target;

  const lw_types__lwu32 val = (*(csb_reg_rd_wr_instances__csb_reg_wr32_mailbox__uc_lwu32GP1354__target*)(&(*reg)));
  {
    const lw_types__lwu32 C195s = (lw_types__lwu32)addr;
    {
      extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

      falc_stxb_wrapper(C195s, val);
    }
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */
typedef dev_sec_csb_ada__lw_csec_falcon_vhrcfg_entnum_register csb_reg_rd_wr_instances__csb_reg_rd32_vhrcfg_entnumGP2401__generic_register
  GNAT_ALIGN(4);
extern void csb_reg_rd_wr_instances__csb_reg_rd32_vhrcfg_entnum(csb_reg_rd_wr_instances__csb_reg_rd32_vhrcfg_entnumGP2401__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
#ifdef IN_ADA_BODY
void csb_reg_rd_wr_instances__csb_reg_rd32_vhrcfg_entnum(csb_reg_rd_wr_instances__csb_reg_rd32_vhrcfg_entnumGP2401__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  lw_types__lwu32 val;
  typedef lw_types__Tlwu32B csb_reg_rd_wr_instances__csb_reg_rd32_vhrcfg_entnum__uc_generic_registerGP731__source;
  typedef dev_sec_csb_ada__lw_csec_falcon_vhrcfg_entnum_register csb_reg_rd_wr_instances__csb_reg_rd32_vhrcfg_entnum__uc_generic_registerGP731__target
    GNAT_ALIGN(4);
  {
    const lw_types__lwu32 C203s = (lw_types__lwu32)addr;
    {
      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

      val = falc_ldx_i_wrapper(C203s, 0U);
    }
    *((lw_types__lwu32 *)&(*reg)) = val;
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
#endif /* IN_ADA_BODY */

