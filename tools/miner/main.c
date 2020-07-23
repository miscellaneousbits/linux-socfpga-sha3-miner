#define _GNU_SOURCE

#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <miner.h>

#define catreg(r) MINER_##r##_REG
#define catmask(r, f) MINER_##r##_##f##_MASK
#define catshift(r, f) MINER_##r##_##f##_SHIFT
#define bitfield(r, f) ((read_reg32(catreg(r)) & catmask(r, f)) >> catshift(r, f))

#include "sha3.h"

static char pass = 1;
static int fd;

static void sigintHandler(int sig, siginfo_t* si, void* unused)
{
    // ioctl(fd, MINER_IOCTL_ABORT);
}

static inline uint32_t swap32(uint32_t v)
{
    return (v << 24) | (v >> 24) | ((v & 0x00ff0000) >> 8) | ((v & 0x0000ff00) << 8);
}

static void write_reg32(uint32_t i, uint32_t v)
{
    lseek(fd, i * sizeof(uint32_t), SEEK_SET);
    write(fd, &v, sizeof(v));
}

static void write_reg64_be(uint32_t i, uint64_t v)
{
    uint32_t h = v >> 32;
    uint32_t l = v;
    lseek(fd, i * sizeof(uint32_t), SEEK_SET);
    write(fd, &h, sizeof(h));
    write(fd, &l, sizeof(l));
}

static uint32_t read_reg32(uint32_t i)
{
    lseek(fd, i * sizeof(uint32_t), SEEK_SET);
    uint32_t u32;
    read(fd, &u32, sizeof(u32));
    return u32;
}

static uint64_t read_reg64_le(uint32_t i)
{
    uint32_t h, l;
    lseek(fd, i * sizeof(uint32_t), SEEK_SET);
    read(fd, &l, sizeof(l));
    read(fd, &h, sizeof(h));
    return ((uint64_t)h << 32) | l;
}

static void test(int i, uint64_t start, uint32_t poll)
{
    write_reg32(MINER_CTL_REG, 0);
    uint32_t block[10];
    uint32_t hash[8];

    for (int i = 0; i < 8; i++)
        block[i] = rand();
    for (int i = 0; i < 8; i++)
        write_reg32(MINER_HDR_REG + i, swap32(block[i]));

    uint64_t t = start - i;
    block[8] = swap32(start >> 32);
    block[9] = swap32(start);
    write_reg64_be(MINER_START_REG, t);

    sha3_HashBuffer(256, SHA3_FLAGS_KECCAK, block, sizeof(block), hash, sizeof(hash));
    for (int i = 0; i < 8; i++)
        write_reg32(MINER_DIFF_REG + i, swap32(hash[i]));

    write_reg32(MINER_CTL_REG, (0x01 << MINER_CTL_FIRST_SHIFT) | (0x80 << MINER_CTL_LAST_SHIFT) |
                                   MINER_CTL_RUN_MASK | MINER_CTL_TEST_MASK);

    char timeout = 0;
    if (poll)
    {
        usleep(2);
        if (!bitfield(STATUS, FOUND))
            timeout = 1;
    }
    else
    {
        struct pollfd rfds[1];
        int ret;
        rfds[0].fd = fd;
        rfds[0].events = POLLIN;
        ret = ppoll(rfds, 1, NULL, 0);
        if (ret > 0)
        {
            if (!(rfds[0].revents & POLLIN))
                timeout = 1;
        }
        else
            timeout = 1;
    }
    uint64_t solution = read_reg64_le(MINER_SOLN_REG);
    if (timeout)
    {
        printf("t");
        pass = 0;
    }
    else
    {
        if (solution != start)
        {
            printf("x");
            pass = 0;
        }
        else
            printf(".");
    }
    write_reg32(MINER_CTL_REG, 0);
    fflush(stdout);
    usleep(1);
}

static int mine(void)
{
    write_reg32(MINER_CTL_REG, 0);
    uint32_t block[10];
    uint32_t hash[8];

    printf("Header:   ");
    for (int i = 0; i < 8; i++)
    {
        block[i] = rand();
        printf("%08x", swap32(block[i]));
        write_reg32(MINER_HDR_REG + i, swap32(block[i]));
    }
    printf("\n");

    uint64_t start = ((uint64_t)rand() << 32) | rand();
    write_reg64_be(MINER_START_REG, start);
    printf("Start:    %016llx\n", start);

    for (int i = 0; i < 8; i++)
        write_reg32(MINER_DIFF_REG + i, 0xffffffff);
    write_reg32(MINER_DIFF_REG + 0, 0x0000000f);

    write_reg32(MINER_CTL_REG,
        (0x01 << MINER_CTL_FIRST_SHIFT) | (0x80 << MINER_CTL_LAST_SHIFT) | MINER_CTL_RUN_MASK);

    char timeout = 0;
    struct pollfd rfds[1];
    int ret;
    rfds[0].fd = fd;
    rfds[0].events = POLLIN;
    ret = ppoll(rfds, 1, NULL, 0);
    if (ret > 0)
    {
        if (!(rfds[0].revents & POLLIN))
            timeout = 1;
    }
    else
        timeout = 1;

    uint64_t solution = read_reg64_le(MINER_SOLN_REG);
    write_reg32(MINER_CTL_REG, 0);

    printf("Solution: %016llx (+%'llu)\n", solution, solution - start);

    if (timeout)
    {
        printf("FAIL\n");
        return -1;
    }

    block[8] = swap32(solution >> 32);
    block[9] = swap32(solution);

    sha3_HashBuffer(256, SHA3_FLAGS_KECCAK, block, sizeof(block), hash, sizeof(hash));
    printf("Hash:     ");
    for (int i = 0; i < 8; i++)
        printf("%08x", swap32(hash[i]));
    printf("\n");
    printf("Diff:     ");
    for (int i = 0; i < 8; i++)
        printf("%08x", read_reg32(MINER_DIFF_REG + i));
    printf("\n");


    for (int i = 0; i < 8; i++)
    {
        if (swap32(hash[i]) < read_reg32(MINER_DIFF_REG + i))
            break;
        if (swap32(hash[i]) > read_reg32(MINER_DIFF_REG + i))
        {
            timeout = 1;
            break;
        }
    }

    printf("%s\n", timeout ? "FAIL" : "PASS");
    return timeout;
}

static uint64_t rate(void)
{
    for (int i = 0; i < 8; i++)
        write_reg32(MINER_DIFF_REG + i, 0);
    uint64_t start = read_reg64_le(MINER_SOLN_REG);
    write_reg32(MINER_CTL_REG,
        (0x01 << MINER_CTL_FIRST_SHIFT) | (0x80 << MINER_CTL_LAST_SHIFT) | MINER_CTL_RUN_MASK);
    sleep(3);
    uint64_t stop = read_reg64_le(MINER_SOLN_REG);
    write_reg32(MINER_CTL_REG, 0);
    return (stop - start) / 3;
}

int main()
{
    setlocale(LC_NUMERIC, "");
    fd = open("/dev/miner0", O_RDWR | O_NONBLOCK);

    if (fd < 0)
    {
        printf("miner open\n");
        exit(EXIT_FAILURE);
    }
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sigintHandler;
    sigaction(SIGINT, &sa, NULL);

    srand(time(NULL));

    write_reg32(MINER_CTL_REG, 0);
    usleep(1);
    unsigned sha3;
    memcpy(&sha3, "SHA3", sizeof(sha3));
    if (read_reg32(MINER_SHA3_REG) != swap32(sha3))
    {
        printf("FPGA not found %08x %08x\n", read_reg32(MINER_SHA3_REG), sha3);
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("\nPoll test\n");
    for (int t = 0; t < 10; t++)
    {
        uint64_t start = (uint64_t)rand() << 32 | rand();
        printf("Test %d %016llx ", t, start);
        fflush(stdout);
        for (int i = 0; i < 48; i++)
            test(i, start, 1);
        printf("\n");
        fflush(stdout);
    }
    printf("%s\n", pass ? "PASS" : "FAIL");

    if (!pass)
        return -1;

    printf("\nInterrupt test\n");
    for (int t = 0; t < 10; t++)
    {
        uint64_t start;
        start = (uint64_t)rand() << 32 | rand();
        printf("Test %d %016llx ", t, start);
        fflush(stdout);
        for (int i = 0; i < 48; i++)
            test(i, start, 0);
        printf("\n");
        fflush(stdout);
    }
    printf("%s\n\n", pass ? "PASS" : "FAIL");

    if (!pass)
        return -1;

    printf("Checking hash rate\n");
    uint32_t h = bitfield(STATUS, MHZ);
    double expected = h / 3.0;
    printf("Miner clock %'u MHz, expected hash rate %'.2f MH/S\n", h, expected);
    double r = rate() / 1.0e6;
    int pass = fabs(r - expected) < (expected * .001);
    printf("Measured %'.2f MH/S, %s\n", r, pass ? "PASS" : "FAIL");
    if (!pass)
        return -1;

    printf("\nMining test\n");

    for (int i = 1; i <= 10; i++)
    {
        printf("\nSearch %d\n", i);
        if (mine())
            break;
    }
    printf("\n");

    return 0;
}
