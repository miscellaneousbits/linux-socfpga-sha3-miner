#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sha3.h"

unsigned debug = 0;

/*
 * This flag is used to configure "pure" Keccak, as opposed to NIST SHA3.
 */
#define SHA3_USE_KECCAK_FLAG 0x80000000


#ifndef SHA3_ROTL64
#define SHA3_ROTL64(x, y) (((x) << (y)) | ((x) >> ((sizeof(uint64_t) * 8) - (y))))
#endif


static const uint64_t keccakf_rndc[24] = {0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL, 0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL};

static const unsigned keccakf_rotc[24] = {
    1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44};

static const unsigned keccakf_piln[24] = {
    10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};

/* generally called after SHA3_KECCAK_SPONGE_WORDS-ctx->capacityWords words
 * are XORed into the state s
 */
static void keccakf(uint64_t s[25])
{
    int i, j, round;
    uint64_t t, bc[5];
#define KECCAK_ROUNDS 24
    for (round = 0; round < KECCAK_ROUNDS; round++)
    {
        if (debug)
        {
            char* sb = (char*)s;
            printf("round in: ");
            for (i = 0; i < 1600 / 8; i++)
                printf("%02x", sb[i]);
            printf("\n");
        }

        /* Theta */
        for (i = 0; i < 5; i++)
            bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];

        for (i = 0; i < 5; i++)
        {
            t = bc[(i + 4) % 5] ^ SHA3_ROTL64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5)
                s[j + i] ^= t;
        }

        /* Rho Pi */
        t = s[1];
        for (i = 0; i < 24; i++)
        {
            j = keccakf_piln[i];
            bc[0] = s[j];
            s[j] = SHA3_ROTL64(t, keccakf_rotc[i]);
            t = bc[0];
        }

        /* Chi */
        for (j = 0; j < 25; j += 5)
        {
            for (i = 0; i < 5; i++)
                bc[i] = s[j + i];
            for (i = 0; i < 5; i++)
                s[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }

        /* Iota */
        s[0] ^= keccakf_rndc[round];
        if (debug)
        {
            char* sb = (char*)s;
            printf("round out: ");
            for (i = 0; i < 1600 / 8; i++)
                printf("%02x", sb[i]);
            printf("\n\n");
        }
    }
}

/* *************************** Public Inteface ************************ */

/* For Init or Reset call these: */
sha3_return_t sha3_Init(sha3_context* ctx, unsigned bitSize)
{
    if (bitSize != 256 && bitSize != 384 && bitSize != 512)
        return SHA3_RETURN_BAD_PARAMS;
    memset(ctx, 0, sizeof(*ctx));
    ctx->capacityWords = 2ULL * bitSize / (8 * sizeof(uint64_t));
    return SHA3_RETURN_OK;
}

void sha3_Init256(sha3_context* ctx)
{
    sha3_Init(ctx, 256);
}

void sha3_Init384(sha3_context* ctx)
{
    sha3_Init(ctx, 384);
}

void sha3_Init512(sha3_context* ctx)
{
    sha3_Init(ctx, 512);
}

enum SHA3_FLAGS sha3_SetFlags(sha3_context* ctx, enum SHA3_FLAGS flags)
{
    flags &= SHA3_FLAGS_KECCAK;
    ctx->flags |= (flags == SHA3_FLAGS_KECCAK ? SHA3_USE_KECCAK_FLAG : 0);
    return flags;
}

void sha3_Update(sha3_context* ctx, void const* bufIn, size_t len)
{
    /* 0...7 -- how much is needed to have a word */
    unsigned old_tail = (8 - ctx->byteIndex) & 7;

    size_t words;
    unsigned tail;
    size_t i;

    const uint8_t* buf = bufIn;

    if (len < old_tail)
    { /* have no complete word or haven't started
       * the word yet */
        /* endian-independent code follows: */
        while (len--)
            ctx->saved |= (uint64_t)(*(buf++)) << ((ctx->byteIndex++) * 8);
        return;
    }

    if (old_tail)
    { /* will have one word to process */
        /* endian-independent code follows: */
        len -= old_tail;
        while (old_tail--)
            ctx->saved |= (uint64_t)(*(buf++)) << ((ctx->byteIndex++) * 8);

        /* now ready to add saved to the sponge */
        ctx->s[ctx->wordIndex] ^= ctx->saved;
        ctx->byteIndex = 0;
        ctx->saved = 0;
        if (++ctx->wordIndex == (SHA3_KECCAK_SPONGE_WORDS - ctx->capacityWords))
        {
            keccakf(ctx->s);
            ctx->wordIndex = 0;
        }
    }

    /* now work in full words directly from input */

    words = len / sizeof(uint64_t);
    tail = (uint32_t)(len - words * sizeof(uint64_t));

    for (i = 0; i < words; i++, buf += sizeof(uint64_t))
    {
        const uint64_t t = ((uint64_t)(buf[0]) << 8 * 0) | ((uint64_t)(buf[1]) << 8 * 1) |
                           ((uint64_t)(buf[2]) << 8 * 2) | ((uint64_t)(buf[3]) << 8 * 3) |
                           ((uint64_t)(buf[4]) << 8 * 4) | ((uint64_t)(buf[5]) << 8 * 5) |
                           ((uint64_t)(buf[6]) << 8 * 6) | ((uint64_t)(buf[7]) << 8 * 7);
        ctx->s[ctx->wordIndex] ^= t;
        if (++ctx->wordIndex == (SHA3_KECCAK_SPONGE_WORDS - ctx->capacityWords))
        {
            keccakf(ctx->s);
            ctx->wordIndex = 0;
        }
    }

    /* finally, save the partial word */
    while (tail--)
    {
        ctx->saved |= (uint64_t)(*(buf++)) << ((ctx->byteIndex++) * 8);
    }
}

/* This is simply the 'update' with the padding block.
 * The padding block is 0x01 || 0x00* || 0x80. First 0x01 and last 0x80
 * bytes are always present, but they can be the same byte.
 */
void const* sha3_Finalize(sha3_context* ctx)
{
    /* Append 2-bit suffix 01, per SHA-3 spec. Instead of 1 for padding we
     * use 1<<2 below. The 0x02 below corresponds to the suffix 01.
     * Overall, we feed 0, then 1, and finally 1 to start padding. Without
     * M || 01, we would simply use 1 to start padding. */

    uint64_t t;

    if (ctx->flags & SHA3_USE_KECCAK_FLAG) /* Keccak version */
        t = (uint64_t)(((uint64_t)1) << (ctx->byteIndex * 8));
    else /* SHA3 version */
        t = (uint64_t)(((uint64_t)(0x02 | (1 << 2))) << ((ctx->byteIndex) * 8));

    ctx->s[ctx->wordIndex] ^= ctx->saved ^ t;
    ctx->s[SHA3_KECCAK_SPONGE_WORDS - ctx->capacityWords - 1] ^= 0x8000000000000000ULL;
    if (debug)
    {
        printf("pad: ");
        for (unsigned i = 0; i < (SHA3_KECCAK_SPONGE_WORDS - ctx->capacityWords) * sizeof(uint64_t);
             i++)
            printf("%02x", ctx->sb[i]);
        printf("\n");
    }
    keccakf(ctx->s);

    for (unsigned i = 0; i < SHA3_KECCAK_SPONGE_WORDS - ctx->capacityWords; i++)
    {
        const unsigned t1 = (uint32_t)ctx->s[i];
        const unsigned t2 = (uint32_t)(ctx->s[i] >> 32);
        ctx->sb[i * 8 + 0] = (uint8_t)(t1);
        ctx->sb[i * 8 + 1] = (uint8_t)(t1 >> 8);
        ctx->sb[i * 8 + 2] = (uint8_t)(t1 >> 16);
        ctx->sb[i * 8 + 3] = (uint8_t)(t1 >> 24);
        ctx->sb[i * 8 + 4] = (uint8_t)(t2);
        ctx->sb[i * 8 + 5] = (uint8_t)(t2 >> 8);
        ctx->sb[i * 8 + 6] = (uint8_t)(t2 >> 16);
        ctx->sb[i * 8 + 7] = (uint8_t)(t2 >> 24);
    }

    return (ctx->sb);
}

sha3_return_t sha3_HashBuffer(unsigned bitSize, enum SHA3_FLAGS flags, const void* in,
    unsigned inBytes, void* out, unsigned outBytes)
{
    sha3_return_t err;
    sha3_context c;

    err = sha3_Init(&c, bitSize);
    if (err != SHA3_RETURN_OK)
        return err;
    if (sha3_SetFlags(&c, flags) != flags)
        return SHA3_RETURN_BAD_PARAMS;
    sha3_Update(&c, in, inBytes);
    const void* h = sha3_Finalize(&c);

    if (outBytes > bitSize / 8)
        outBytes = bitSize / 8;
    memcpy(out, h, outBytes);
    return SHA3_RETURN_OK;
}
