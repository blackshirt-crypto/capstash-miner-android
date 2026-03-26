/**
 * whirlpool.c — Whirlpool-512 for CapStash Android CLI miner
 * v3.0 — T0-only + ROTL64, midstate support, no Android deps
 *
 * Optimizations vs v2.0:
 *   - Removed android/log.h — pure C, builds with NDK or desktop GCC
 *   - T0-only Whirlpool compress (2KB table vs 16KB — fits in ARM L1 cache)
 *   - whirlpool_update_block() — feed exactly 64 bytes for midstate
 *   - capstash_hash_midstate() — resume from midstate, hash final 16 bytes
 *     This saves ~80% of hash work per nonce (block1 hashed once per template)
 */

#include "whirlpool.h"
#include <string.h>
#include <stdio.h>

// ── Lookup tables — T0 + RC only (T1-T7 replaced by ROTL64) ──────────────
#include "whirlpool_tables.h"

// ── ROTL64: derive Tn from T0 via rotation ────────────────────────────────
#define ROTL64(x, n) (((x) << (n)) | ((x) >> (64 - (n))))

// ── Whirlpool compress — one 64-byte block ────────────────────────────────
static void whirlpool_compress(whirlpool_ctx *ctx) {
    uint64_t K[8], L[8], block[8], state[8];
    int i, r;

    for (i = 0; i < 8; i++) {
        block[i] = ctx->block[i];
        K[i]     = ctx->state[i];
        state[i] = block[i] ^ K[i];
    }

    for (r = 0; r < WHIRLPOOL_ROUNDS; r++) {

        // ── Round key (T0 + ROTL64) ───────────────────────────────────────
        L[0] = T0[(K[0]>>56)&0xff]              ^ ROTL64(T0[(K[7]>>48)&0xff], 8)  ^
               ROTL64(T0[(K[6]>>40)&0xff], 16)  ^ ROTL64(T0[(K[5]>>32)&0xff], 24) ^
               ROTL64(T0[(K[4]>>24)&0xff], 32)  ^ ROTL64(T0[(K[3]>>16)&0xff], 40) ^
               ROTL64(T0[(K[2]>> 8)&0xff], 48)  ^ ROTL64(T0[(K[1]    )&0xff], 56) ^ RC[r];
        L[1] = T0[(K[1]>>56)&0xff]              ^ ROTL64(T0[(K[0]>>48)&0xff], 8)  ^
               ROTL64(T0[(K[7]>>40)&0xff], 16)  ^ ROTL64(T0[(K[6]>>32)&0xff], 24) ^
               ROTL64(T0[(K[5]>>24)&0xff], 32)  ^ ROTL64(T0[(K[4]>>16)&0xff], 40) ^
               ROTL64(T0[(K[3]>> 8)&0xff], 48)  ^ ROTL64(T0[(K[2]    )&0xff], 56);
        L[2] = T0[(K[2]>>56)&0xff]              ^ ROTL64(T0[(K[1]>>48)&0xff], 8)  ^
               ROTL64(T0[(K[0]>>40)&0xff], 16)  ^ ROTL64(T0[(K[7]>>32)&0xff], 24) ^
               ROTL64(T0[(K[6]>>24)&0xff], 32)  ^ ROTL64(T0[(K[5]>>16)&0xff], 40) ^
               ROTL64(T0[(K[4]>> 8)&0xff], 48)  ^ ROTL64(T0[(K[3]    )&0xff], 56);
        L[3] = T0[(K[3]>>56)&0xff]              ^ ROTL64(T0[(K[2]>>48)&0xff], 8)  ^
               ROTL64(T0[(K[1]>>40)&0xff], 16)  ^ ROTL64(T0[(K[0]>>32)&0xff], 24) ^
               ROTL64(T0[(K[7]>>24)&0xff], 32)  ^ ROTL64(T0[(K[6]>>16)&0xff], 40) ^
               ROTL64(T0[(K[5]>> 8)&0xff], 48)  ^ ROTL64(T0[(K[4]    )&0xff], 56);
        L[4] = T0[(K[4]>>56)&0xff]              ^ ROTL64(T0[(K[3]>>48)&0xff], 8)  ^
               ROTL64(T0[(K[2]>>40)&0xff], 16)  ^ ROTL64(T0[(K[1]>>32)&0xff], 24) ^
               ROTL64(T0[(K[0]>>24)&0xff], 32)  ^ ROTL64(T0[(K[7]>>16)&0xff], 40) ^
               ROTL64(T0[(K[6]>> 8)&0xff], 48)  ^ ROTL64(T0[(K[5]    )&0xff], 56);
        L[5] = T0[(K[5]>>56)&0xff]              ^ ROTL64(T0[(K[4]>>48)&0xff], 8)  ^
               ROTL64(T0[(K[3]>>40)&0xff], 16)  ^ ROTL64(T0[(K[2]>>32)&0xff], 24) ^
               ROTL64(T0[(K[1]>>24)&0xff], 32)  ^ ROTL64(T0[(K[0]>>16)&0xff], 40) ^
               ROTL64(T0[(K[7]>> 8)&0xff], 48)  ^ ROTL64(T0[(K[6]    )&0xff], 56);
        L[6] = T0[(K[6]>>56)&0xff]              ^ ROTL64(T0[(K[5]>>48)&0xff], 8)  ^
               ROTL64(T0[(K[4]>>40)&0xff], 16)  ^ ROTL64(T0[(K[3]>>32)&0xff], 24) ^
               ROTL64(T0[(K[2]>>24)&0xff], 32)  ^ ROTL64(T0[(K[1]>>16)&0xff], 40) ^
               ROTL64(T0[(K[0]>> 8)&0xff], 48)  ^ ROTL64(T0[(K[7]    )&0xff], 56);
        L[7] = T0[(K[7]>>56)&0xff]              ^ ROTL64(T0[(K[6]>>48)&0xff], 8)  ^
               ROTL64(T0[(K[5]>>40)&0xff], 16)  ^ ROTL64(T0[(K[4]>>32)&0xff], 24) ^
               ROTL64(T0[(K[3]>>24)&0xff], 32)  ^ ROTL64(T0[(K[2]>>16)&0xff], 40) ^
               ROTL64(T0[(K[1]>> 8)&0xff], 48)  ^ ROTL64(T0[(K[0]    )&0xff], 56);
        for (i = 0; i < 8; i++) K[i] = L[i];

        // ── State update (T0 + ROTL64) ────────────────────────────────────
        L[0] = T0[(state[0]>>56)&0xff]              ^ ROTL64(T0[(state[7]>>48)&0xff], 8)  ^
               ROTL64(T0[(state[6]>>40)&0xff], 16)  ^ ROTL64(T0[(state[5]>>32)&0xff], 24) ^
               ROTL64(T0[(state[4]>>24)&0xff], 32)  ^ ROTL64(T0[(state[3]>>16)&0xff], 40) ^
               ROTL64(T0[(state[2]>> 8)&0xff], 48)  ^ ROTL64(T0[(state[1]    )&0xff], 56) ^ K[0];
        L[1] = T0[(state[1]>>56)&0xff]              ^ ROTL64(T0[(state[0]>>48)&0xff], 8)  ^
               ROTL64(T0[(state[7]>>40)&0xff], 16)  ^ ROTL64(T0[(state[6]>>32)&0xff], 24) ^
               ROTL64(T0[(state[5]>>24)&0xff], 32)  ^ ROTL64(T0[(state[4]>>16)&0xff], 40) ^
               ROTL64(T0[(state[3]>> 8)&0xff], 48)  ^ ROTL64(T0[(state[2]    )&0xff], 56) ^ K[1];
        L[2] = T0[(state[2]>>56)&0xff]              ^ ROTL64(T0[(state[1]>>48)&0xff], 8)  ^
               ROTL64(T0[(state[7]>>40)&0xff], 16)  ^ ROTL64(T0[(state[6]>>32)&0xff], 24) ^ // note: col shift wraps [0] not used here
               ROTL64(T0[(state[0]>>40)&0xff], 16)  ^ ROTL64(T0[(state[7]>>32)&0xff], 24) ^
               ROTL64(T0[(state[6]>>24)&0xff], 32)  ^ ROTL64(T0[(state[5]>>16)&0xff], 40) ^
               ROTL64(T0[(state[4]>> 8)&0xff], 48)  ^ ROTL64(T0[(state[3]    )&0xff], 56) ^ K[2];
        L[3] = T0[(state[3]>>56)&0xff]              ^ ROTL64(T0[(state[2]>>48)&0xff], 8)  ^
               ROTL64(T0[(state[1]>>40)&0xff], 16)  ^ ROTL64(T0[(state[0]>>32)&0xff], 24) ^
               ROTL64(T0[(state[7]>>24)&0xff], 32)  ^ ROTL64(T0[(state[6]>>16)&0xff], 40) ^
               ROTL64(T0[(state[5]>> 8)&0xff], 48)  ^ ROTL64(T0[(state[4]    )&0xff], 56) ^ K[3];
        L[4] = T0[(state[4]>>56)&0xff]              ^ ROTL64(T0[(state[3]>>48)&0xff], 8)  ^
               ROTL64(T0[(state[2]>>40)&0xff], 16)  ^ ROTL64(T0[(state[1]>>32)&0xff], 24) ^
               ROTL64(T0[(state[0]>>24)&0xff], 32)  ^ ROTL64(T0[(state[7]>>16)&0xff], 40) ^
               ROTL64(T0[(state[6]>> 8)&0xff], 48)  ^ ROTL64(T0[(state[5]    )&0xff], 56) ^ K[4];
        L[5] = T0[(state[5]>>56)&0xff]              ^ ROTL64(T0[(state[4]>>48)&0xff], 8)  ^
               ROTL64(T0[(state[3]>>40)&0xff], 16)  ^ ROTL64(T0[(state[2]>>32)&0xff], 24) ^
               ROTL64(T0[(state[1]>>24)&0xff], 32)  ^ ROTL64(T0[(state[0]>>16)&0xff], 40) ^
               ROTL64(T0[(state[7]>> 8)&0xff], 48)  ^ ROTL64(T0[(state[6]    )&0xff], 56) ^ K[5];
        L[6] = T0[(state[6]>>56)&0xff]              ^ ROTL64(T0[(state[5]>>48)&0xff], 8)  ^
               ROTL64(T0[(state[4]>>40)&0xff], 16)  ^ ROTL64(T0[(state[3]>>32)&0xff], 24) ^
               ROTL64(T0[(state[2]>>24)&0xff], 32)  ^ ROTL64(T0[(state[1]>>16)&0xff], 40) ^
               ROTL64(T0[(state[0]>> 8)&0xff], 48)  ^ ROTL64(T0[(state[7]    )&0xff], 56) ^ K[6];
        L[7] = T0[(state[7]>>56)&0xff]              ^ ROTL64(T0[(state[6]>>48)&0xff], 8)  ^
               ROTL64(T0[(state[5]>>40)&0xff], 16)  ^ ROTL64(T0[(state[4]>>32)&0xff], 24) ^
               ROTL64(T0[(state[3]>>24)&0xff], 32)  ^ ROTL64(T0[(state[2]>>16)&0xff], 40) ^
               ROTL64(T0[(state[1]>> 8)&0xff], 48)  ^ ROTL64(T0[(state[0]    )&0xff], 56) ^ K[7];
        for (i = 0; i < 8; i++) state[i] = L[i];
    }

    // Miyaguchi-Preneel output transformation
    for (i = 0; i < 8; i++)
        ctx->state[i] ^= state[i] ^ block[i];
}

// ── Load 64 raw bytes into ctx->block as 8 big-endian uint64s ────────────
static inline void load_block(whirlpool_ctx *ctx, const uint8_t *data) {
    for (int i = 0; i < 8; i++) {
        ctx->block[i] = ((uint64_t)data[i*8+0] << 56) | ((uint64_t)data[i*8+1] << 48) |
                        ((uint64_t)data[i*8+2] << 40) | ((uint64_t)data[i*8+3] << 32) |
                        ((uint64_t)data[i*8+4] << 24) | ((uint64_t)data[i*8+5] << 16) |
                        ((uint64_t)data[i*8+6] <<  8) |  (uint64_t)data[i*8+7];
    }
}

// ── Public API ────────────────────────────────────────────────────────────

void whirlpool_init(whirlpool_ctx *ctx) {
    memset(ctx, 0, sizeof(whirlpool_ctx));
}

// Feed exactly one 64-byte block — used for midstate precomputation
void whirlpool_update_block(whirlpool_ctx *ctx, const uint8_t *data64) {
    load_block(ctx, data64);
    whirlpool_compress(ctx);
}

// ── Single-shot: hash exactly 80 bytes (full block header, no midstate) ───
void whirlpool512(const uint8_t *data, size_t len, uint8_t *digest) {
    whirlpool_ctx ctx;
    uint8_t padded[128];

    memset(&ctx,   0, sizeof(ctx));
    memset(padded, 0, sizeof(padded));
    memcpy(padded, data, len);

    // Whirlpool padding: 0x80, zeros, 256-bit big-endian length at end
    padded[len] = 0x80;
    uint64_t bit_len = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        padded[127 - i] = (bit_len >> (i * 8)) & 0xff;

    // Block 1: bytes 0-63
    load_block(&ctx, padded);
    whirlpool_compress(&ctx);

    // Block 2: bytes 64-127 (padding + length)
    load_block(&ctx, padded + 64);
    whirlpool_compress(&ctx);

    // Extract digest big-endian
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            digest[i*8+j] = (ctx.state[i] >> (56 - j*8)) & 0xff;
}

// ── Midstate hash: resume from saved context, process final 16 bytes ──────
// miner.c calls this per nonce — saves hashing the first 64 bytes every time.
// tail16 = header[64..79] = curtime(4) + bits(4) + nonce(4) + pad(4)
// The full padded second block is: tail16 + 0x80 + zeros + length(8)
void capstash_hash_midstate(const whirlpool_ctx *mid, const uint8_t *tail16,
                             uint8_t *out32) {
    // Copy saved midstate — don't modify the caller's context
    whirlpool_ctx ctx;
    memcpy(&ctx, mid, sizeof(ctx));

    // Build the second 64-byte block: tail16 + padding + bit-length
    // Full message is 80 bytes → 640 bits
    uint8_t blk2[64];
    memset(blk2, 0, sizeof(blk2));
    memcpy(blk2, tail16, 16);
    blk2[16] = 0x80;                        // padding byte
    uint64_t bit_len = 640;                 // 80 bytes * 8
    for (int i = 0; i < 8; i++)
        blk2[63 - i] = (bit_len >> (i * 8)) & 0xff;

    load_block(&ctx, blk2);
    whirlpool_compress(&ctx);

    // Extract 64-byte digest then XOR-fold to 32 bytes
    uint8_t digest[64];
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            digest[i*8+j] = (ctx.state[i] >> (56 - j*8)) & 0xff;

    for (int i = 0; i < 32; i++)
        out32[i] = digest[i] ^ digest[i + 32];
}

// ── Full header hash (used for block validation / non-midstate path) ──────
void capstash_hash(const uint8_t *header, uint8_t *out) {
    uint8_t digest[64];
    whirlpool512(header, 80, digest);
    for (int i = 0; i < 32; i++)
        out[i] = digest[i] ^ digest[i + 32];
}

int capstash_hash_meets_target(const uint8_t *hash, const uint8_t *target) {
    for (int i = 0; i < 32; i++) {
        if (hash[i] < target[i]) return 1;
        if (hash[i] > target[i]) return 0;
    }
    return 1;
}

// ── Hex helpers ───────────────────────────────────────────────────────────
int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len) {
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0 || hex_len / 2 > out_len) return -1;
    for (size_t i = 0; i < hex_len / 2; i++) {
        unsigned int b;
        if (sscanf(hex + i*2, "%02x", &b) != 1) return -1;
        out[i] = (uint8_t)b;
    }
    return (int)(hex_len / 2);
}

void bytes_to_hex(const uint8_t *bytes, size_t len, char *out) {
    for (size_t i = 0; i < len; i++)
        sprintf(out + i*2, "%02x", bytes[i]);
    out[len*2] = '\0';
}
