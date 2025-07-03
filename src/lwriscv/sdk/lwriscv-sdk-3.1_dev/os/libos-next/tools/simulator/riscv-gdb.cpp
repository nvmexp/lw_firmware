#include "riscv-gdb.h"
#include <fcntl.h>
#ifdef __MINGW64__
#   include <winsock2.h>
#else
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <poll.h>
#   include <signal.h>
#   include <sys/select.h>
#   include <sys/socket.h>
#   include <sys/wait.h>
#   include <unistd.h>
#endif

static void socketSetNonblocking(int socket)
{
#ifdef __MINGW64__
    unsigned long ul = 1;
    ioctlsocket(socket,FIONBIO,(unsigned long *)&ul);
#else
    fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
#endif
}

static void socketSetBlocking(int socket)
{
#ifdef __MINGW64__
    unsigned long ul = 0;
    ioctlsocket(socket,FIONBIO,(unsigned long *)&ul);
#else
    fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) & ~O_NONBLOCK);
#endif
}

static bool wouldBlock() {
#ifndef __MINGW64__
    return errno == EWOULDBLOCK;
#else
    return WSAGetLastError() == WSAEWOULDBLOCK;
#endif
}

#ifdef __MINGW64__
#define MSG_NOSIGNAL 0
typedef int socklen_t;
#endif

static volatile bool ctrl_c_pressed = false;

static void ctrl_c_break(int sig) { ctrl_c_pressed = true; }

riscv_gdb::riscv_gdb(riscv_core *core, bool use_ddd) : core(core), ddd(use_ddd)
{
    core->event_instruction_decoded.connect(&riscv_gdb::instruction_decoded, this);
    core->event_instruction_prestart.connect(&riscv_gdb::handler_instruction_prestart, this);

    gdb_init();
    printf("GDB stub initialized.\n");
}

void riscv_gdb::instruction_decoded(LwU64 pc, LibosRiscvInstruction * i)
{
    switch(i->opcode)
    {
    case RISCV_LB: case RISCV_LH: case RISCV_LW: case RISCV_LD: case RISCV_LBU: case RISCV_LHU: case RISCV_LWU:
        if (rtrap.find(i->imm + core->regs[i->rs1]) != rtrap.end())
            gdb_break("T05");
        break;

    case RISCV_SB: case RISCV_SH: case RISCV_SW: case RISCV_SD:
        if (wtrap.find(i->imm + core->regs[i->rs1]) != wtrap.end())
            gdb_break("T05");
        break;

    case RISCV_EBREAK:
        if (gdb_socket)
            gdb_break("T05");    
        break;
    }
}

void riscv_gdb::handler_instruction_prestart()
{
    if (ctrl_c_pressed)
    {
        printf("Halting core...\n");
        gdb_break("T05");
        ctrl_c_pressed = false;
    }

    if (itrap.find(core->programCounter) != itrap.end())
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
    char ch = 0;

    socketSetBlocking(gdb_socket);
    int r = recv(gdb_socket, (char *)&ch, 1, 0);
    if (r < 0)
        return 0;
    return ch;
}

void riscv_gdb::gdb_xmit_raw(const char *p)
{
    //printf("xmit: %s\n", p);
    socketSetBlocking(gdb_socket);

    while (*p)
    {
        int n = send(gdb_socket, p, strlen(p), MSG_NOSIGNAL);
        if (n == -1 )
        {
            gdb_lost_connection();
            return;
        }
        p += n;
    }
}

void riscv_gdb::gdb_send_packet()
{
    //printf("send_packet: %s\n", gdb_buffer_out);
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
            riscv_core::TranslationKind kind;
            if (core->translate_address(addr + r, physaddr, kind) &&
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
        socketSetNonblocking(gdb_socket);
        int r = recv(gdb_socket, &gdb_buffer_in[gdb_buffer_in_count], 1, 0);
        if (r < 0)
        {
            if (wouldBlock())
                return;

            if (errno == EAGAIN)
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

#ifndef __MINGW64__
static void startDebugger(const char * cmd)
{
    if (int child_pid = fork())
    {
        printf("%s\n",cmd);
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
#else
static void startDebugger(const char * cmd)
{
    ShellExelwte(NULL, NULL, "cmd.exe", cmd, NULL, SW_SHOW);
}
#endif

void riscv_gdb::gdb_init()
{
#ifdef __MINGW64__
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
        throw std::runtime_error("Unable to initialize winsock2.");
#endif

    // start breakpointed
    core->stopped = LW_TRUE;

    int r;

    gdb_listener_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (gdb_listener_socket == -1)
        throw std::runtime_error("Unable to create socket");
    socketSetNonblocking(gdb_listener_socket);

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


    char cmd[2048];
#if !LIBOS_TINY
#   define ELF "libos-kernel.elf"
#else
#   define ELF "firmware.elf"
#endif


#ifndef __MINGW64__
    if (ddd)
        sprintf(
            cmd,
            "/bin/sh -c \"ddd --debugger "
            "\\\"$LW_TOOLS/riscv/toolchain/linux/release/20190625/bin/riscv64-linux-gdb "
            ELF " -x $LIBOS_PATH/.gdbinit -ex 'target remote localhost:%d' -ex 'b "
            "KernelInit' \\\"\"",
            ntohs(sockaddr.sin_port));
    else
#endif
        sprintf(
            cmd,
#ifndef __MINGW64__
            "/bin/sh -c "
            "\"$LW_TOOLS/riscv/toolchain/linux/release/20190625/bin/riscv64-linux-gdb "
            ELF " -x $LIBOS_PATH/.gdbinit -ex 'target remote localhost:%d' -ex 'b "
            "KernelInit' \"",
#else
            "/c"
            "\"%P4ROOT%\\sw\\tools\\win64\\msys2\\20210303\\mingw64\\bin\\gdb-multiarch  "
            ELF " -ex \"target remote localhost:%d\" -ex \"b KernelInit\" \"",
#endif
            ntohs(sockaddr.sin_port));
    printf("%s\n",cmd);
    startDebugger(cmd);
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
        socketSetNonblocking(gdb_socket);

        // disable nagling algorithm (packet delay and coalescing)
        int on = 1;
        ret = setsockopt(gdb_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
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
