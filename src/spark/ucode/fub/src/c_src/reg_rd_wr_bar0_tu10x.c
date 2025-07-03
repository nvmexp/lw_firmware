#define IN_ADA_BODY
#include "reg_rd_wr_bar0_tu10x.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
reg_rd_wr_bar0_tu10x__bar0_wait_idle(void) {
  boolean bdone = false;
  dev_sec_csb_ada__lw_csec_bar0_csr_register csecbar0csr;
  while (true) {
    if (!(bdone == false)) {
      break;
    }
    csb_reg_rd_wr_instances__csb_reg_rd32_bar0csr(&csecbar0csr, 114688U);
    {
      dev_sec_csb_ada__lw_csec_bar0_csr_status_field T4b = csecbar0csr.f_status;
      if (!(((interfaces__unsigned_32)(T4b)) >= ((interfaces__unsigned_32)(0)) && ((interfaces__unsigned_32)(T4b)) <= ((interfaces__unsigned_32)(
        4))))
        GNAT_LAST_CHANCE_HANDLER(__FILE__, __LINE__);
      switch (T4b) {
        case 0: {
          bdone = true; }
          break;
        case 1:
          break;
        default: {
          falc_halt(); }
          break;
      }

    }}
  loop_until: ;
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
reg_rd_wr_bar0_tu10x__bar0_reg_rd32_private(lw_types__lwu32 *val, lw_types__reg_addr_types__bar0_addr addr) {
  dev_sec_csb_ada__lw_csec_bar0_csr_register csecbar0csr;
  typedef lw_types__Tlwu32B reg_rd_wr_bar0_tu10x__bar0_reg_rd32_private__generictolwu32GP3346__source;

  reg_rd_wr_bar0_tu10x__bar0_wait_idle();
  csb_reg_rd_wr_instances__csb_reg_wr32_lwu32((lw_types__lwu32)addr, 114944U);
  *((reg_rd_wr_bar0_tu10x__bar0_reg_rd32_private__generictolwu32GP3346__source *)&csecbar0csr) = 0U;
  csecbar0csr.f_cmd = 1;
  csecbar0csr.f_byte_enable = 15U;
  csecbar0csr.f_trig = 1;
  csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csr(&csecbar0csr, 114688U);
  reg_rd_wr_bar0_tu10x__bar0_wait_idle();
  csb_reg_rd_wr_instances__csb_reg_rd32_lwu32(&(*val), 115200U);
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
reg_rd_wr_bar0_tu10x__bar0_reg_wr32_private(lw_types__lwu32 val, lw_types__reg_addr_types__bar0_addr addr) {
  dev_sec_csb_ada__lw_csec_bar0_csr_register csecbar0csr;
  typedef lw_types__Tlwu32B reg_rd_wr_bar0_tu10x__bar0_reg_wr32_private__generictolwu32GP4534__source;

  reg_rd_wr_bar0_tu10x__bar0_wait_idle();
  csb_reg_rd_wr_instances__csb_reg_wr32_lwu32((lw_types__lwu32)addr, 114944U);
  csb_reg_rd_wr_instances__csb_reg_wr32_lwu32(val, 115200U);
  *((reg_rd_wr_bar0_tu10x__bar0_reg_wr32_private__generictolwu32GP4534__source *)&csecbar0csr) = 0U;
  csecbar0csr.f_cmd = 2;
  csecbar0csr.f_byte_enable = 15U;
  csecbar0csr.f_trig = 1;
  csb_reg_rd_wr_instances__csb_reg_wr32_stall_bar0csr(&csecbar0csr, 114688U);
  reg_rd_wr_bar0_tu10x__bar0_wait_idle();
  return;
}

