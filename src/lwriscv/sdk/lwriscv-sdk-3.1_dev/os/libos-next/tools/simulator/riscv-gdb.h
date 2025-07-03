/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "riscv-core.h"
#include <unordered_set>

struct riscv_gdb
{
    riscv_core *core;
    bool        ddd;
    riscv_gdb(riscv_core *core, bool use_ddd);

    void instruction_decoded(LwU64 pc, LibosRiscvInstruction * i);
    void handler_instruction_prestart();

    std::unordered_set<LwU64> rtrap, wtrap, itrap;
    int                       gdb_listener_socket = 0;
    int                       gdb_socket          = 0;
    char                      gdb_buffer_in[4096];
    char                      gdb_buffer_out[4096];
    LwU32                     gdb_buffer_in_count = 0;

    void gdb_poll();
    void gdb_poll_cmd();
    void gdb_process_message();
    void gdb_send_packet();
    void gdb_lost_connection();
    void gdb_xmit_raw(const char *p);
    char gdb_get_byte();
    void gdb_init();
    void gdb_break(const char *code);
};
