/**
 * whirlpool.c — CapStash miner Whirlpool wrapper
 * v3.1 — uses SPHlib Whirlpool (identical to CapStash Core)
 */

#include "whirlpool.h"
#include "sph_whirlpool.h"
#include <string.h>
#include <stdio.h>

// ── Core SPHlib wrappers ──────────────────────────────────────────────────

void whirlpool_init(whirlpool_ctx *ctx) {
    sph_whirlpool_init(&ctx->sph);
}

void whirlpool_update_block(whirlpool_ctx *ctx, const uint8_t *data64) {
    sph_whirlpool(&ctx->sph, data64, 64);
}

void whirlpool512(const uint8_t *data, size_t len, uint8_t *digest) {
    sph_whirlpool_context sph;
    sph_whirlpool_init(&sph);
    sph_whirlpool(&sph, data, len);
    sph_whirlpool_close(&sph, digest);
}

// ── Midstate hash — hot path ──────────────────────────────────────────────
void capstash_hash_midstate(const whirlpool_ctx *mid,
                             const uint8_t *tail16,
                             uint8_t *out32) {
    // Copy midstate context and feed final 16 bytes + padding
    whirlpool_ctx ctx;
    memcpy(&ctx, mid, sizeof(ctx));
    sph_whirlpool(&ctx.sph, tail16, 16);
    uint8_t digest[64];
    sph_whirlpool_close(&ctx.sph, digest);
    // XOR fold 64 -> 32 bytes then reverse to match Core uint256 byte order
    for (int i = 0; i < 32; i++)
        out32[i] = digest[i] ^ digest[i + 32];
}

// ── Full header hash (validation path) ───────────────────────────────────
void capstash_hash(const uint8_t *header, uint8_t *out) {
    uint8_t digest[64];
    whirlpool512(header, 80, digest);
    for (int i = 0; i < 32; i++)
        out[i] = digest[i] ^ digest[i + 32];
}

// ── Target comparison — MSB first ─────────────────────────────────────────
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
