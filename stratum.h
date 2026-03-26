/**
 * stratum.h — Stratum client for CapStash Android miner
 * v1.0 — TCP JSON stratum+tcp protocol
 */

#ifndef CAPSTASH_STRATUM_H
#define CAPSTASH_STRATUM_H

#include <stdint.h>
#include <stddef.h>
#include "rpc.h"   // block_template_t

#ifdef __cplusplus
extern "C" {
#endif

// ── Stratum connection config ─────────────────────────────────────────────
typedef struct {
    char host[128];       // pool hostname or IP
    int  port;            // pool port
    char user[256];       // wallet.worker  e.g. cap1q....phone1
    char pass[64];        // usually "x"
} stratum_config_t;

// ── Stratum job — work unit from pool ─────────────────────────────────────
typedef struct {
    char     job_id[64];
    char     prev_hash[65];      // 32 bytes hex
    char     coinb1[512];        // coinbase part 1 hex
    char     coinb2[256];        // coinbase part 2 hex
    char     merkle_branch[8][65]; // up to 8 merkle branches
    int      merkle_count;
    char     version[16];        // block version hex
    char     nbits[16];          // compact target hex
    char     ntime[16];          // current time hex
    int      clean_jobs;         // 1 = discard old work
    // Derived fields filled by stratum_build_template()
    char     extranonce1[32];    // assigned by pool at subscribe
    int      extranonce2_size;   // bytes for extranonce2
} stratum_job_t;

// ── Stratum connection state ──────────────────────────────────────────────
typedef struct {
    int              sock;                // TCP socket fd
    stratum_config_t cfg;
    stratum_job_t    job;                 // current job
    int              job_valid;           // 1 if job is ready to mine
    char             extranonce1[32];     // assigned by pool
    int              extranonce2_size;
    int              subscribed;
    int              authorized;
    long             msg_id;             // JSON-RPC message id counter
} stratum_ctx_t;

// ── Public API ────────────────────────────────────────────────────────────

// Connect, subscribe, and authorize with pool
// Returns 0 on success, -1 on error
int stratum_connect(stratum_ctx_t *ctx, const stratum_config_t *cfg);

// Disconnect and close socket
void stratum_disconnect(stratum_ctx_t *ctx);

// Check for new work from pool (non-blocking poll)
// Returns 1 if a new job arrived, 0 if no new data, -1 on error/disconnect
int stratum_poll(stratum_ctx_t *ctx);

// Submit a share to the pool
// nonce is the winning nonce as a hex string (8 chars)
// en2 is the extranonce2 value used
// Returns 0 if accepted, -1 if rejected or error
int stratum_submit(stratum_ctx_t *ctx, const char *job_id,
                   const char *ntime, const char *en2_hex,
                   const char *nonce_hex);

// Build a block_template_t from the current stratum job
// so miner.c can use the same mining loop for both solo and pool
// en2 = extranonce2 value to embed (thread-specific)
int stratum_build_template(const stratum_ctx_t *ctx,
                            uint64_t en2,
                            block_template_t *tmpl);

// Returns 1 if connected and authorized
int stratum_is_connected(const stratum_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CAPSTASH_STRATUM_H */
