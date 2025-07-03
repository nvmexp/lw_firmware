#include "riscv-gdb.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile bool ctrl_c_pressed = false;

static void ctrl_c_break(int sig) { ctrl_c_pressed = true; }

riscv_gdb::riscv_gdb(riscv_core *core, bool use_ddd) : core(core), ddd(use_ddd)
{
    core->event_ebreak.connect(&riscv_gdb::handler_ebreak, this);
    core->event_store.connect(&riscv_gdb::handler_store, this);
    core->event_load.connect(&riscv_gdb::handler_load, this);
    core->event_instruction_prestart.connect(&riscv_gdb::handler_instruction_prestart, this);

    gdb_init();
}

void riscv_gdb::handler_ebreak()
{
    if (gdb_socket)
        gdb_break("T05");
}

void riscv_gdb::handler_store(store_kind kind, LwU64 rs1, LwU64 rs2, LwS64 imm)
{
    if (wtrap.find(imm + core->regs[rs1]) != wtrap.end())
        gdb_break("T05");
}

void riscv_gdb::handler_load(load_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm)
{
    if (rtrap.find(imm + core->regs[rs1]) != rtrap.end())
        gdb_break("T05");
}

void riscv_gdb::handler_instruction_prestart()
{
    if (ctrl_c_pressed)
    {
        printf("Halting core...\n");
        gdb_break("T05");
        ctrl_c_pressed = false;
    }

    if (itrap.find(core->lwrrent_instruction) != itrap.end())
        gdb_break("T05");

    gdb_poll();
}

void riscv_gdb::gdb_lost_connection()
{
    fprintf(stderr, "GDB stub disconnected\n");
    close(gdb_socket);
    gdb_buffer_in_count = 0;
    gdb_socket          = 0;
}

char riscv_gdb::gdb_get_byte()
{
    int ch = 0;
    fcntl(gdb_socket, F_SETFL, fcntl(gdb_socket, F_GETFL, 0) & ~O_NONBLOCK);
    int r = recv(gdb_socket, &ch, 1, 0);
    if (r < 0)
        return 0;
    return ch;
}

void riscv_gdb::gdb_xmit_raw(const char *p)
{
    // printf("Sending: %s\n", p);
    fcntl(gdb_socket, F_SETFL, fcntl(gdb_socket, F_GETFL, 0) & ~O_NONBLOCK);
    while (*p)
    {
        int n = send(gdb_socket, p, strlen(p), MSG_NOSIGNAL);
        if (n == -1)
        {
            gdb_lost_connection();
            return;
        }
        p += n;
    }
}

void riscv_gdb::gdb_send_packet()
{
    // printf("send_packet: %s\n", gdb_buffer_out);
    // Compute checksum
    LwU8 checksum = 0;
    int  i        = 0;
    
    for (i = 0; gdb_buffer_out[i]; i++)
        checksum += (LwU8)gdb_buffer_out[i];
    sprintf(&gdb_buffer_out[i], "#%02x", checksum);

    // Send command prefix
    gdb_xmit_raw("$");

    // Send body
    gdb_xmit_raw(gdb_buffer_out);

    // Wait for ack
    char response = gdb_get_byte();
    if (response != '+')
        printf("remote nacked: %s\n", gdb_buffer_out);
    gdb_buffer_out[0] = 0;
}

void riscv_gdb::gdb_process_message()
{
    gdb_buffer_in[gdb_buffer_in_count] = 0;
    // printf("GDB: %s\n", gdb_buffer_in);

    // printf("received: %s\n", gdb_buffer_in);

    // Truncate buffer early to just the command (no arguments)
    char *p = (char *)&gdb_buffer_in[1];
    for (char *tail = p; *tail; tail++)
        if (*tail == '#' || *tail == ':')
            *tail = 0;

    // Dispatch command
    switch (*p++)
    {
    case 'p': {
        char *end;
        LwU64 res, reg = strtoull(p, &end, 16);
        if (reg == 897 /* xscratch */)
            res = core->xscratch;
        else if (reg == 0x20 /* pc */)
            res = core->pc;
        else if (reg < 0x20)
            res = core->regs[reg];
        else
        {
            printf("riscv-gdb: Unsupported register: %lld\n", reg);
            break;
        }

        sprintf(gdb_buffer_out, "%016lX", __builtin_bswap64(res));
        break;
    }

    case 'Z': {
        char *comma = p;
        LwU64 kind  = strtoull(p, &comma, 16);
        LwU64 addr  = strtoull(comma + 1, &comma, 16);

        if (kind == 0 || kind == 1)
            itrap.insert(addr);
        else if (kind == 2)
            wtrap.insert(addr);
        else if (kind == 3)
            rtrap.insert(addr);
        else if (kind == 4)
        {
            wtrap.insert(addr);
            rtrap.insert(addr);
        }

        sprintf(gdb_buffer_out, "OK");
        break;
    }

    case 'z': {
        char *comma = p;
        LwU64 kind  = strtoull(p, &comma, 16);
        LwU64 addr  = strtoull(comma + 1, &comma, 16);

        if (kind == 0 || kind == 1)
            itrap.erase(addr);
        else if (kind == 2)
            wtrap.erase(addr);
        else if (kind == 3)
            rtrap.erase(addr);
        else if (kind == 4)
        {
            wtrap.erase(addr);
            rtrap.erase(addr);
        }

        sprintf(gdb_buffer_out, "OK");
        break;
    }

    case 'm': {
        char *comma = p;
        LwU64 addr  = strtoull(p, &comma, 16);
        LwU64 size  = strtoull(comma + 1, &comma, 16);
        for (LwU64 r = 0; r < size; r++)
        {
            LwU64          physaddr;
            lwstom_memory *memory;
            if (core->translate_address_HAL(addr + r, physaddr, LW_FALSE, LW_FALSE, LW_TRUE) &&
                (memory = core->memory_for_address(physaddr)))
                sprintf(&gdb_buffer_out[r * 2], "%02x", memory->read8(physaddr));
            else
                sprintf(&gdb_buffer_out[r * 2], "00");
        }
    }
    break;

    case 's':
        core->stopped          = LW_FALSE;
        core->single_step_once = LW_TRUE;
        strcpy(gdb_buffer_out, "T05");
        break;

    case 'c':
        core->stopped = LW_FALSE;
        return;

    case '?':
        core->stopped = LW_TRUE;
        strcpy(gdb_buffer_out, "S05");
        break;

    case 'H':
        strcpy(gdb_buffer_out, "OK");
        break;

    case 'g':
        for (int r = 0; r < 32; r++)
            sprintf(&gdb_buffer_out[r * 16], "%016lx", __builtin_bswap64(core->regs[r]));
        break;

    case 'q':                /* qString Get value of String */
        if (!strcmp("C", p)) // Thread 1 (single-core)
            strcpy(gdb_buffer_out, "00");
        else if (!strcmp("HostInfo", p))
            strcpy(gdb_buffer_out, "cputype:243;cpusubtype:0;endian:little;ptrsize:8;");
        else if (!strcmp(p, "Supported"))
            strcpy(gdb_buffer_out, "hwbreak+;swbreak-;PacketSize=FF0");
        break;
    default:
        break;
    }
    gdb_send_packet();
}

void riscv_gdb::gdb_poll_cmd()
{
    while (LW_TRUE)
    {
        if (gdb_buffer_in_count >= sizeof(gdb_buffer_in))
        {
            printf("Dropping incomplete buffer: %s\n", gdb_buffer_in);
            gdb_buffer_in_count = 0;
            return;
        }
        fcntl(gdb_socket, F_SETFL, fcntl(gdb_socket, F_GETFL, 0) | O_NONBLOCK);
        int r = recv(gdb_socket, &gdb_buffer_in[gdb_buffer_in_count], 1, 0);
        if (r < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;

            gdb_lost_connection();
            return;
        }

        if (!r)
            return;
        gdb_buffer_in_count++;

        // Ignore characters until start of message
        if (gdb_buffer_in_count == 1 && gdb_buffer_in[gdb_buffer_in_count - 1] != '$')
            gdb_buffer_in_count = 0;

        // Look for end of message sequence and ignore checksum
        if (gdb_buffer_in_count > 3 && gdb_buffer_in[gdb_buffer_in_count - 3] == '#')
        {
            // Ack the message without verifying the checksum
            // We're over a CRC channel (no serial) so it's not likely to fail
            gdb_xmit_raw("+");

            gdb_process_message();
            gdb_buffer_in_count = 0;
        }
    }
}

void riscv_gdb::gdb_break(const char *code)
{
    if (core->single_step_once)
        return;
    if (core->stopped)
        return;

    strcpy(gdb_buffer_out, code);
    gdb_send_packet();
    core->stopped = LW_TRUE;
}

void riscv_gdb::gdb_init()
{
    // start breakpointed
    core->stopped = LW_TRUE;

    int r;

    gdb_listener_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (gdb_listener_socket == -1)
        throw std::runtime_error("Unable to create socket");
    fcntl(gdb_listener_socket, F_SETFL, fcntl(gdb_listener_socket, F_GETFL, 0) | O_NONBLOCK);

    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family         = AF_INET;
    sockaddr.sin_addr.s_addr    = htonl(INADDR_LOOPBACK);
    sockaddr.sin_port           = htons(0);
    r                           = bind(gdb_listener_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (r == -1)
        throw std::runtime_error("Unable to create socket");
    socklen_t nl = sizeof(sockaddr);
    getsockname(gdb_listener_socket, (struct sockaddr *)&sockaddr, &nl);

    if (r == -1)
        throw std::runtime_error("Unable to bind a port");

    r = listen(gdb_listener_socket, 0);
    if (r == -1)
        throw std::runtime_error("Unable to listen on socket");

    if (int child_pid = fork())
    {
        // Parent process trucks on as GDB

        char cmd[2048];
        if (ddd)
            sprintf(
                cmd,
                "/bin/sh -c \"ddd --debugger "
                "\\\"$LW_TOOLS/riscv/toolchain/linux/release/20190625/bin/riscv64-linux-gdb "
                "firmware.elf -x $LIBOS_PATH/.gdbinit -ex 'target remote localhost:%d' -ex 'b "
                "kernel_init' -ex 'c'\\\"\"",
                ntohs(sockaddr.sin_port));
        else
            sprintf(
                cmd,
                "/bin/sh -c "
                "\"$LW_TOOLS/riscv/toolchain/linux/release/20190625/bin/riscv64-linux-gdb "
                "firmware.elf -x $LIBOS_PATH/.gdbinit -ex 'target remote localhost:%d' -ex 'b "
                "kernel_init' -ex 'c'\"",
                ntohs(sockaddr.sin_port));
        system(cmd);
        printf("GDB has exited... (killing simulator)");
        kill(child_pid, SIGTERM);
        wait(NULL);
        exit(0);
    }
    else
    {
        signal(SIGINT, ctrl_c_break);
    }
}

void riscv_gdb::gdb_poll()
{
    if (!gdb_socket)
    {
        int ret = accept(gdb_listener_socket, NULL, NULL);
        if (ret == -1)
            return;
        gdb_socket = ret;

        // set non-blocking
        fcntl(gdb_socket, F_SETFL, fcntl(gdb_socket, F_GETFL, 0) | O_NONBLOCK);

        // disable nagling algorithm (packet delay and coalescing)
        int on = 1;
        ret    = setsockopt(gdb_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
        if (ret == -1)
            throw std::runtime_error("Unable to disable nagling.");

        printf("GDB has connected to simulator.\n");
    }

    // Add the client socket to the set
    fd_set fdset = {0};
    FD_SET((unsigned)gdb_socket, &fdset);

    // Wait on the socket
    timeval to  = {0, 0};
    int     ret = select(gdb_socket + 1, &fdset, NULL, NULL, &to);

    // Do we need the disconnec the socket?
    if (ret == -1 && errno == EBADF)
    {
        gdb_lost_connection();
    }
    // Do we have data?
    else if (ret)
    {
        gdb_poll_cmd();
    }
}
