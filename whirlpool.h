/**
 * whirlpool.h — Whirlpool-512 hash engine for CapStash Android CLI miner
 * v3.0 — midstate support, no Android deps
 */

#ifndef CAPSTASH_WHIRLPOOL_H
#define CAPSTASH_WHIRLPOOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Constants ─────────────────────────────────────────────────────────────
#define WHIRLPOOL_DIGEST_SIZE  64   // 512 bits
#define WHIRLPOOL_BLOCK_SIZE   64   // 512-bit internal block
#define WHIRLPOOL_ROUNDS       10
#define CAPSTASH_HASH_SIZE     32   // XOR-fold: 64 → 32 bytes

// ── Whirlpool context ──────────────────────────────────────────────────────
// Kept minimal — miner only needs state and block, no streaming buffer
#include "sph_whirlpool.h"
typedef struct {
    sph_whirlpool_context sph;
} whirlpool_ctx;

// ── Core API ───────────────────────────────────────────────────────────────

// Initialize context to all-zero state (Whirlpool spec)
void whirlpool_init(whirlpool_ctx *ctx);

// Feed exactly one 64-byte block into context — for midstate precomputation.
// Call whirlpool_init() first, then call this once with header bytes 0-63.
// Save the resulting ctx as your midstate — reuse it for every nonce.
void whirlpool_update_block(whirlpool_ctx *ctx, const uint8_t *data64);

// Single-shot: hash exactly 80 bytes with full padding (no midstate).
// Produces 64-byte Whirlpool-512 digest.
void whirlpool512(const uint8_t *data, size_t len, uint8_t *digest);

// ── Midstate hash — hot path, called once per nonce ───────────────────────
// Resumes from saved midstate context, processes the final 16 bytes of the
// 80-byte header (curtime + bits + nonce), applies padding, and XOR-folds
// the 64-byte digest to 32 bytes.
//
// mid    — midstate context from whirlpool_update_block() — NOT modified
// tail16 — header[64..79]: curtime(4) + bits(4) + nonce(4) + 4 bytes pad
// out32  — 32-byte PoW hash output
void capstash_hash_midstate(const whirlpool_ctx *mid, const uint8_t *tail16,
                             uint8_t *out32);

// ── CapStash PoW helpers ───────────────────────────────────────────────────

// Full header hash (non-midstate path — used for validation)
// Whirlpool-512 over 80-byte header, XOR-folded to 32 bytes
void capstash_hash(const uint8_t *header, uint8_t *out);

// Returns 1 if hash <= target (valid PoW), 0 otherwise
// Compares 32 bytes MSB first — target must be big-endian encoded
int capstash_hash_meets_target(const uint8_t *hash, const uint8_t *target);

// ── Hex helpers ────────────────────────────────────────────────────────────

// Decode hex string → bytes. Returns byte count or -1 on error.
int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len);

// Encode bytes → null-terminated hex string. out must be len*2+1 bytes.
void bytes_to_hex(const uint8_t *bytes, size_t len, char *out);

// ── Inline header helpers ──────────────────────────────────────────────────

static inline void write_le32(uint8_t *buf, int off, uint32_t v) {
    buf[off]   =  v        & 0xff;
    buf[off+1] = (v >>  8) & 0xff;
    buf[off+2] = (v >> 16) & 0xff;
    buf[off+3] = (v >> 24) & 0xff;
}

static inline uint32_t read_le32(const uint8_t *buf, int off) {
    return (uint32_t)buf[off]         |
          ((uint32_t)buf[off+1] <<  8)|
          ((uint32_t)buf[off+2] << 16)|
          ((uint32_t)buf[off+3] << 24);
}

static inline void set_nonce(uint8_t *header, uint32_t nonce) {
    write_le32(header, 76, nonce);
}

#ifdef __cplusplus
}
#endif

#endif /* CAPSTASH_WHIRLPOOL_H */
