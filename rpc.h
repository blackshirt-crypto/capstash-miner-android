/**
 * rpc.h — RPC client header for CapStash miner
 * v3.0.0
 */

#ifndef CAPSTASH_RPC_H
#define CAPSTASH_RPC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── RPC connection config ─────────────────────────────────────────────────
typedef struct {
    char host[64];
    int  port;
    char user[64];
    char pass[64];
} rpc_config_t;

// ── Block template — shared by solo (RPC) and pool (stratum) modes ────────
typedef struct {
    uint32_t version;
    uint32_t height;
    uint32_t curtime;
    uint32_t bits;
    int64_t  coinbase_value;       // satoshis
    char     prev_hash_hex[65];    // 32 bytes as 64 hex chars
    char     bits_hex[16];         // compact target hex
    char     target_hex[65];       // full 32-byte target hex
    char     coinbase_hex[512];    // serialized coinbase tx hex
    char     merkle_root_hex[65];  // computed merkle root
    // Pool mode fields — populated by stratum_build_template()
    char     job_id[64];           // stratum job id
    char     ntime_hex[16];        // current time hex (for share submission)
    uint64_t coinbase_en2;         // en2 value baked into coinbase_hex (pool mode)
} block_template_t;

// ── Public API ────────────────────────────────────────────────────────────

// Fetch a new block template from the node (solo mode)
// Returns 0 on success, -1 on error
int rpc_getblocktemplate(const rpc_config_t *cfg, block_template_t *tmpl);

// Submit a solved block (solo mode)
// Returns 0 if accepted, -1 if rejected or error
int rpc_submitblock(const rpc_config_t *cfg, const char *block_hex);

// Get current chain tip hash — used to detect new blocks (solo mode)
// out_hash_hex must be at least 65 bytes
// Returns 0 on success, -1 on error
int rpc_getbestblockhash(const rpc_config_t *cfg, char *out_hash_hex);

// Parse getblocktemplate JSON response
int rpc_parse_template(const char *json, block_template_t *tmpl);

#ifdef __cplusplus
}
#endif

#endif /* CAPSTASH_RPC_H */
