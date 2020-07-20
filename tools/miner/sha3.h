#pragma once

extern unsigned debug;

/* 'Words' here refers to uint64_t */
#define SHA3_KECCAK_SPONGE_WORDS (((1600) / 8 /*bits to byte*/) / sizeof(uint64_t))

typedef struct sha3_context_
{
    uint64_t saved; /* the portion of the input message that we
                     * didn't consume yet */
    union
    { /* Keccak's state */
        uint64_t s[1600 / 64];
        uint8_t sb[1600 / 8];
    };
    unsigned byteIndex;     /* 0..7--the next byte after the set one
                             * (starts from 0; 0--none are buffered) */
    unsigned wordIndex;     /* 0..24--the next word to integrate input
                             * (starts from 0) */
    unsigned capacityWords; /* the double size of the hash output in
                             * words (e.g. 16 for Keccak 512) */
    unsigned flags;
} sha3_context;

enum SHA3_FLAGS
{
    SHA3_FLAGS_NONE = 0,
    SHA3_FLAGS_KECCAK = 1
};

enum SHA3_RETURN
{
    SHA3_RETURN_OK = 0,
    SHA3_RETURN_BAD_PARAMS = 1
};
typedef enum SHA3_RETURN sha3_return_t;

/* For Init or Reset call these: */
sha3_return_t sha3_Init(sha3_context* ctx, unsigned bitSize);

void sha3_Init256(sha3_context* ctx);
void sha3_Init384(sha3_context* ctx);
void sha3_Init512(sha3_context* ctx);

enum SHA3_FLAGS sha3_SetFlags(sha3_context* ctx, enum SHA3_FLAGS);

void sha3_Update(sha3_context* ctx, void const* bufIn, size_t len);

void const* sha3_Finalize(sha3_context* ctx);

/* Single-call hashing */
sha3_return_t sha3_HashBuffer(unsigned bitSize, /* 256, 384, 512 */
    enum SHA3_FLAGS flags,                      /* SHA3_FLAGS_NONE or SHA3_FLAGS_KECCAK */
    const void* in, unsigned inBytes, void* out,
    unsigned outBytes); /* up to bitSize/8; truncation OK */
