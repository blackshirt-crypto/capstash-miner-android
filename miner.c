/**
 * miner.c — CapStash CPU Miner (Android/ARM64 optimized)
 * v3.0.0 — T0-only Whirlpool, midstate, P-core affinity, stratum share submission
 */

#define _GNU_SOURCE
#include "miner.h"
#include "rpc.h"
#include "whirlpool.h"
#include "sha256.h"
#include "stratum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

// ── Android / Linux affinity ──────────────────────────────────────────────
#ifdef __ANDROID__
  #include <sys/syscall.h>
  #include <linux/sched.h>
  typedef struct { unsigned long bits[1]; } cs_t;
  static inline void cs_zero(cs_t *s)         { s->bits[0] = 0; }
  static inline void cs_set(cs_t *s, int cpu)  { s->bits[0] |= (1UL << cpu); }
  static void pin_to_core(int core) {
      cs_t mask; cs_zero(&mask); cs_set(&mask, core);
      syscall(__NR_sched_setaffinity, 0, sizeof(mask), &mask);
  }
#elif defined(__linux__)
  #include <sched.h>
  static void pin_to_core(int core) {
      cpu_set_t mask; CPU_ZERO(&mask); CPU_SET(core, &mask);
      sched_setaffinity(0, sizeof(mask), &mask);
  }
#else
  static void pin_to_core(int core) { (void)core; }
#endif

// ── Logging ───────────────────────────────────────────────────────────────
#define LOG_INFO(fmt, ...)  fprintf(stdout, "[miner] "       fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  fprintf(stderr, "[miner] WARN: " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[miner] ERR:  " fmt "\n", ##__VA_ARGS__)

// ── Tuning ────────────────────────────────────────────────────────────────
#define MAINTENANCE_EVERY   524288
#define HASH_BATCH          4096
#define MAX_THREADS         64
#define TEMPLATE_RETRY_SEC  5
#define HASHRATE_WINDOW_SEC 5

// ── P-core layout ─────────────────────────────────────────────────────────
#ifndef PCORE_FIRST
  #define PCORE_FIRST 4
#endif
#ifndef PCORE_COUNT
  #define PCORE_COUNT 4
#endif

// ── Global state ──────────────────────────────────────────────────────────
static volatile int        g_running      = 0;
static miner_config_t      g_config;
static miner_callbacks_t   g_callbacks;
static pthread_t           g_threads[MAX_THREADS];
static int                 g_thread_count = 0;
static pthread_mutex_t     g_stats_mutex    = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t     g_template_mutex = PTHREAD_MUTEX_INITIALIZER;
static stratum_ctx_t       g_stratum        = {0};

static atomic_uint_least64_t g_total_hashes    = 0;
static atomic_uint_least32_t g_blocks_found    = 0;
static atomic_uint_least32_t g_shares_submitted = 0;

static block_template_t  g_template;
static volatile int      g_template_valid = 0;

// ── Per-thread data ───────────────────────────────────────────────────────
typedef struct {
    int          thread_id;
    rpc_config_t rpc;
    char         address[128];
    double       last_hashrate;
} thread_data_t;

// ── Address decode ────────────────────────────────────────────────────────
static const char BECH32_CHARSET[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static int decode_bech32(const char *addr, uint8_t *out_hash160) {
    char lower[128];
    int len = (int)strlen(addr);
    if (len < 8 || len > 90) return 0;
    for (int i = 0; i < len; i++)
        lower[i] = (addr[i] >= 'A' && addr[i] <= 'Z') ? addr[i] + 32 : addr[i];
    lower[len] = '\0';
    int sep = -1;
    for (int i = len - 1; i >= 0; i--)
        if (lower[i] == '1') { sep = i; break; }
    if (sep < 1) return 0;
    int data_len = len - sep - 1 - 6;
    if (data_len < 1) return 0;
    uint8_t data[64];
    for (int i = 0; i < data_len; i++) {
        const char *p = strchr(BECH32_CHARSET, lower[sep + 1 + i]);
        if (!p) return 0;
        data[i] = (uint8_t)(p - BECH32_CHARSET);
    }
    if (data[0] != 0) return 0;
    uint8_t bytes[32];
    int byte_count = 0, acc = 0, bits = 0;
    for (int i = 1; i < data_len; i++) {
        acc = (acc << 5) | data[i];
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            bytes[byte_count++] = (acc >> bits) & 0xff;
            if (byte_count > 20) return 0;
        }
    }
    if (byte_count != 20) return 0;
    memcpy(out_hash160, bytes, 20);
    return 1;
}

static const char BASE58_ALPHA[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static int decode_base58check(const char *addr, uint8_t *out_hash160) {
    int len = (int)strlen(addr);
    if (len < 25 || len > 35) return 0;
    uint8_t dec[64];
    memset(dec, 0, sizeof(dec));
    for (int i = 0; i < len; i++) {
        const char *p = strchr(BASE58_ALPHA, addr[i]);
        if (!p) return 0;
        int carry = (int)(p - BASE58_ALPHA);
        for (int j = 24; j >= 0; j--) {
            carry += 58 * dec[j];
            dec[j] = carry & 0xff;
            carry >>= 8;
        }
        if (carry) return 0;
    }
    uint8_t check[32];
    sha256d(dec, 21, check);
    if (dec[21] != check[0] || dec[22] != check[1] ||
        dec[23] != check[2] || dec[24] != check[3]) {
        LOG_WARN("base58check checksum failed for %s", addr);
        return 0;
    }
    memcpy(out_hash160, dec + 1, 20);
    return 1;
}

static int build_output_script(const char *addr, uint8_t *out) {
    uint8_t h160[20];
    if (strncmp(addr, "cap1", 4) == 0) {
        if (!decode_bech32(addr, h160)) { LOG_ERROR("bech32 decode failed: %s", addr); return -1; }
        out[0]=0x00; out[1]=0x14;
        memcpy(out+2, h160, 20);
        return 22;
    } else if (addr[0]=='C' || addr[0]=='8') {
        if (!decode_base58check(addr, h160)) { LOG_ERROR("base58 decode failed: %s", addr); return -1; }
        out[0]=0x76; out[1]=0xa9; out[2]=0x14;
        memcpy(out+3, h160, 20);
        out[23]=0x88; out[24]=0xac;
        return 25;
    }
    LOG_ERROR("unrecognized address: %s", addr);
    return -1;
}

// ── Coinbase builder ──────────────────────────────────────────────────────
static int build_coinbase(const block_template_t *tmpl,
                           int en1, uint64_t en2,
                           const char *addr,
                           uint8_t *out, size_t out_sz) {
    uint8_t *p = out;
    write_le32(p, 0, 1); p += 4;
    *p++ = 0x01;
    memset(p, 0x00, 32); p += 32;
    write_le32(p, 0, 0xffffffff); p += 4;

    uint8_t ss[64], *sp = ss;
    uint32_t h = tmpl->height;
    if      (h == 0)       { *sp++=0x01; *sp++=(uint8_t)h; }
    else if (h < 0x80)     { *sp++=0x01; *sp++=(uint8_t)h; }
    else if (h < 0x8000)   { *sp++=0x02; *sp++=h&0xff; *sp++=(h>>8)&0xff; }
    else if (h < 0x800000) { *sp++=0x03; *sp++=h&0xff; *sp++=(h>>8)&0xff; *sp++=(h>>16)&0xff; }
    else { *sp++=0x04; *sp++=h&0xff; *sp++=(h>>8)&0xff; *sp++=(h>>16)&0xff; *sp++=(h>>24)&0xff; }
    *sp++=0x04; write_le32(sp,0,(uint32_t)en1);                    sp+=4;
    *sp++=0x04; write_le32(sp,0,(uint32_t)(en2&0xffffffff));       sp+=4;
    *sp++=0x04; write_le32(sp,0,(uint32_t)((en2>>32)&0xffffffff)); sp+=4;

    int ss_len = (int)(sp - ss);
    *p++ = (uint8_t)ss_len;
    memcpy(p, ss, ss_len); p += ss_len;
    write_le32(p, 0, 0xffffffff); p += 4;

    *p++ = 0x01;
    uint64_t val = (uint64_t)tmpl->coinbase_value;
    for (int i = 0; i < 8; i++) *p++ = (val >> (i*8)) & 0xff;

    uint8_t oscript[25];
    int slen = build_output_script(addr, oscript);
    if (slen < 0) return -1;
    *p++ = (uint8_t)slen;
    memcpy(p, oscript, slen); p += slen;
    write_le32(p, 0, 0); p += 4;
    (void)out_sz;
    return (int)(p - out);
}

static void compute_txid(const uint8_t *tx, int len, uint8_t *txid) {
    sha256d(tx, (size_t)len, txid);
}

// ── Header builder ────────────────────────────────────────────────────────
static void build_header(const block_template_t *tmpl,
                          const uint8_t *merkle_root,
                          uint32_t nonce, uint8_t *hdr) {
    write_le32(hdr, 0, tmpl->version);
    // Stratum prevhash is 8x4-byte groups, each group internally byte-swapped
    // Un-swizzle: reverse bytes within each 4-byte group, keep group order
    uint8_t prev[32], prev_fix[32];
    hex_to_bytes(tmpl->prev_hash_hex, prev, 32);
    for (int i = 0; i < 32; i += 4) {
        prev_fix[i+0] = prev[i+3];
        prev_fix[i+1] = prev[i+2];
        prev_fix[i+2] = prev[i+1];
        prev_fix[i+3] = prev[i+0];
    }
    memcpy(hdr+4, prev_fix, 32);
    // Merkle root: use raw bytes directly — no reversal needed
    // sha256d output is already in correct internal byte order for the header
    memcpy(hdr+36, merkle_root, 32);
    write_le32(hdr, 68, tmpl->curtime);
    write_le32(hdr, 72, tmpl->bits);
    write_le32(hdr, 76, nonce);
}

// ── Block serializer (solo mode) ──────────────────────────────────────────
static int serialize_block(const uint8_t *hdr,
                            const uint8_t *cb, int cb_len,
                            char *out_hex, size_t hex_sz) {
    uint8_t raw[80 + 1 + 512];
    uint8_t *p = raw;
    memcpy(p, hdr, 80); p += 80;
    *p++ = 0x01;
    if (cb_len > 512) { LOG_ERROR("coinbase too large: %d", cb_len); return -1; }
    memcpy(p, cb, cb_len); p += cb_len;
    int tot = (int)(p - raw);
    if ((size_t)(tot*2+1) > hex_sz) { LOG_ERROR("hex buf too small"); return -1; }
    bytes_to_hex(raw, tot, out_hex);
    return tot;
}

// ── Template refresh (solo mode) ──────────────────────────────────────────
static int refresh_template(const rpc_config_t *rpc) {
    block_template_t t;
    if (rpc_getblocktemplate(rpc, &t) != 0) {
        LOG_WARN("getblocktemplate failed — retry in %ds", TEMPLATE_RETRY_SEC);
        return -1;
    }
    pthread_mutex_lock(&g_template_mutex);
    memcpy(&g_template, &t, sizeof(t));
    g_template_valid = 1;
    pthread_mutex_unlock(&g_template_mutex);
    return 0;
}

// ── Midstate precomputation ───────────────────────────────────────────────
static void compute_midstate(const uint8_t *hdr64, whirlpool_ctx *mid_ctx) {
    whirlpool_init(mid_ctx);
    whirlpool_update_block(mid_ctx, hdr64);
}

// ── Mining thread ─────────────────────────────────────────────────────────
static void *mining_thread(void *arg) {
    thread_data_t *td = (thread_data_t *)arg;

    int core = PCORE_FIRST + (td->thread_id % PCORE_COUNT);
    pin_to_core(core);
    LOG_INFO("thread %d pinned to core %d", td->thread_id, core);

    uint8_t  header[80];
    uint8_t  hash[32];
    uint8_t  target[32];
    uint8_t  coinbase[512];
    uint8_t  cb_txid[32];
    uint8_t  merkle_root[32];
    uint64_t en2    = 0;  // All threads start at 0 — matches template built with en2=0
    uint32_t nonce;
    uint64_t hashes = 0;
    char     tip_local[65] = {0};
    char     tip_check[65];
    int      cb_len;

    block_template_t ltmpl;
    whirlpool_ctx    mid_ctx;
    uint8_t          last_merkle[32];
    memset(last_merkle, 0, 32);

    time_t   hr_t0 = time(NULL);
    uint64_t hr_h0 = 0;

    LOG_INFO("thread %d started", td->thread_id);

    while (g_running) {
        // ── Grab template ─────────────────────────────────────────────────
        pthread_mutex_lock(&g_template_mutex);
        int valid = g_template_valid;
        if (valid) memcpy(&ltmpl, &g_template, sizeof(ltmpl));
        pthread_mutex_unlock(&g_template_mutex);

        if (!valid) {
            if (td->thread_id == 0 && g_config.pool_mode == 0)
                refresh_template(&td->rpc);
            sleep(1);
            continue;
        }

        hex_to_bytes(ltmpl.target_hex, target, 32);
        // DEBUG — remove after share submission confirmed
        if (td->thread_id == 0) LOG_INFO("target: %s", ltmpl.target_hex);

        // ── Build coinbase + merkle root ──────────────────────────────────
        if (g_config.pool_mode == 1) {
            // Pool mode — use stratum coinbase directly
            cb_len = hex_to_bytes(ltmpl.coinbase_hex, coinbase, sizeof(coinbase));
            if (cb_len < 0) {
                LOG_ERROR("coinbase hex decode failed");
                g_running = 0;
                break;
            }
        } else {
            // Solo mode — build coinbase from template
            cb_len = build_coinbase(&ltmpl, td->thread_id, en2,
                                    td->address, coinbase, sizeof(coinbase));
            if (cb_len < 0) {
                if (g_callbacks.on_error)
                    g_callbacks.on_error("address decode failed", g_callbacks.userdata);
                g_running = 0;
                break;
            }
        }
        compute_txid(coinbase, cb_len, cb_txid);
        if (g_config.pool_mode == 1) {
            // Pool mode — merkle root already computed (with branches) by stratum_build_template
            // Do NOT recompute from coinbase alone — that ignores the merkle branch hashing
            hex_to_bytes(ltmpl.merkle_root_hex, merkle_root, 32);
        } else {
            // Solo mode — no branches, merkle root IS the coinbase txid
            memcpy(merkle_root, cb_txid, 32);
        }

        // ── Build header + midstate ───────────────────────────────────────
        nonce = (uint32_t)td->thread_id;
        build_header(&ltmpl, merkle_root, nonce, header);

        // Compute midstate from first 64 bytes of header
        compute_midstate(header, &mid_ctx);

        // ── Inner nonce loop ──────────────────────────────────────────────
        while (g_running) {
            write_le32(header, 76, nonce);
            capstash_hash_midstate(&mid_ctx, header + 64, hash);
            // Reverse hash to big-endian for target comparison
            uint8_t hash_be[32];
            for (int _i = 0; _i < 32; _i++) hash_be[_i] = hash[31 - _i];
            if (hash_be[0] <= target[0]) {
            if (capstash_hash_meets_target(hash_be, target)) {
            
                    // ── SHARE / BLOCK FOUND ───────────────────────────────
                    char hash_hex[65];
                    bytes_to_hex(hash, 32, hash_hex);
                    LOG_INFO("thread %d ★ FOUND nonce=%u", td->thread_id, nonce);

                    if (g_config.pool_mode == 1) {
                        // Pool mode — rate limit to 1 submit per 500ms
                        static struct timespec last_submit = {0, 0};
                        struct timespec now;
                        clock_gettime(CLOCK_MONOTONIC, &now);
                        long elapsed_ms = (now.tv_sec - last_submit.tv_sec) * 1000
                                        + (now.tv_nsec - last_submit.tv_nsec) / 1000000;
                        if (elapsed_ms < 500) goto skip_submit;
                        last_submit = now;
                        char nonce_hex[9];
                        // en2_hex must be exactly extranonce2_size bytes wide (pool rejects wrong width)
                        // Build it the same way stratum_build_template() does
                        int en2_size = g_stratum.extranonce2_size;
                        if (en2_size <= 0) en2_size = 4;
                        char en2_hex[17] = {0};
                        for (int ei = 0; ei < en2_size; ei++)
                            snprintf(en2_hex + ei*2, 3, "%02x",
                                     (unsigned)((en2 >> ((en2_size-1-ei)*8)) & 0xff));
                        snprintf(nonce_hex, sizeof(nonce_hex), "%08x", nonce);
                        int sub = stratum_submit(&g_stratum,
                                                  ltmpl.job_id,
                                                  ltmpl.ntime_hex,
                                                  en2_hex,
                                                  nonce_hex);
                        if (sub == 0) {
                            LOG_INFO("share submitted ✓");
                            atomic_fetch_add(&g_shares_submitted, 1);
                        } else {
                            LOG_WARN("share submit failed");
                        }
                    } else {
                        // Solo mode — submit full block via RPC
                        char blk_hex[2048];
                        int  blk_len = serialize_block(header, coinbase, cb_len,
                                                       blk_hex, sizeof(blk_hex));
                        if (blk_len > 0) {
                            int sub = rpc_submitblock(&td->rpc, blk_hex);
                            if (sub == 0) LOG_INFO("block accepted!");
                            else          LOG_WARN("block rejected");
                        }
                        atomic_fetch_add(&g_blocks_found, 1);
                        if (g_callbacks.on_block)
                            g_callbacks.on_block(ltmpl.height, hash_hex,
                                                  g_callbacks.userdata);
                        g_template_valid = 0;
                        break;
                    }
                }
            }
            skip_submit:;

            hashes++;
            if (hashes % HASH_BATCH == 0)
                atomic_fetch_add(&g_total_hashes, HASH_BATCH);

            // ── Periodic maintenance ──────────────────────────────────────
            if (hashes % MAINTENANCE_EVERY == 0) {
                time_t  now     = time(NULL);
                double  elapsed = difftime(now, hr_t0);
                if (elapsed >= HASHRATE_WINDOW_SEC) {
                    double hr = (double)(hashes - hr_h0) / elapsed;
                    td->last_hashrate = hr;
                    if (td->thread_id == 0 && g_callbacks.on_hashrate)
                        g_callbacks.on_hashrate(hr * g_thread_count,
                                                 g_callbacks.userdata);
                    hr_t0 = now;
                    hr_h0 = hashes;
                }

                if (td->thread_id == 0 && g_config.pool_mode == 0) {
                    if (rpc_getbestblockhash(&td->rpc, tip_check) == 0) {
                        if (strcmp(tip_check, tip_local) != 0) {
                            LOG_INFO("new block detected — refreshing template");
                            strncpy(tip_local, tip_check, 64);
                            tip_local[64] = '\0';
                            refresh_template(&td->rpc);
                            break;
                        }
                    }
                }

                // Pool mode — poll for new jobs
                if (td->thread_id == 0 && g_config.pool_mode == 1) {
                    if (stratum_poll(&g_stratum) == 1) {
                        block_template_t stmpl;
                        if (stratum_build_template(&g_stratum, en2, &stmpl) == 0) {
                            pthread_mutex_lock(&g_template_mutex);
                            memcpy(&g_template, &stmpl, sizeof(stmpl));
                            g_template_valid = 1;
                            pthread_mutex_unlock(&g_template_mutex);
                            LOG_INFO("new stratum job received");
                            break;
                        }
                    }
                }

                pthread_mutex_lock(&g_template_mutex);
                if (g_template_valid)
                    ltmpl.curtime = g_template.curtime;
                pthread_mutex_unlock(&g_template_mutex);
            }

            nonce += (uint32_t)g_thread_count;
             if (nonce < (uint32_t)td->thread_id) {
                en2 += (uint64_t)g_thread_count;
                LOG_INFO("thread %d nonce wrap — en2=%llu",
                         td->thread_id, (unsigned long long)en2);
                // Rebuild template with new en2 for pool mode
                if (g_config.pool_mode == 1) {
                    block_template_t stmpl;
                    pthread_mutex_lock(&g_template_mutex);
                    if (stratum_build_template(&g_stratum, en2, &stmpl) == 0) {
                        memcpy(&g_template, &stmpl, sizeof(stmpl));
                        g_template_valid = 1;
                    }
                    pthread_mutex_unlock(&g_template_mutex);
                }
                break;
            }
        }

        if (g_running &&
            g_config.duty_cycle_on > 0 && g_config.duty_cycle_off > 0)
            sleep(g_config.duty_cycle_off);
    }

    LOG_INFO("thread %d stopped — %llu hashes",
             td->thread_id, (unsigned long long)hashes);
    free(td);
    return NULL;
}

// ── Public API ────────────────────────────────────────────────────────────

int miner_start(const miner_config_t *cfg, const miner_callbacks_t *cbs) {
    if (g_running) { LOG_WARN("already running"); return -1; }
    if (!cfg || !cfg->address[0]) { LOG_ERROR("no address"); return -1; }

    uint8_t h160[20];
    int ok = 0;
    if      (strncmp(cfg->address, "cap1", 4) == 0) ok = decode_bech32(cfg->address, h160);
    else if (cfg->address[0]=='C'||cfg->address[0]=='8') ok = decode_base58check(cfg->address, h160);
    if (!ok) { LOG_ERROR("address validation failed: %s", cfg->address); return -1; }
    LOG_INFO("address validated: %s", cfg->address);

    memcpy(&g_config, cfg, sizeof(g_config));
    if (cbs) memcpy(&g_callbacks, cbs, sizeof(g_callbacks));
    else     memset(&g_callbacks, 0,   sizeof(g_callbacks));

    atomic_store(&g_total_hashes,      0);
    atomic_store(&g_blocks_found,      0);
    atomic_store(&g_shares_submitted,  0);
    g_template_valid = 0;
    g_running        = 1;

    int threads = cfg->threads > 0 ? cfg->threads : 4;
    if (threads > MAX_THREADS) threads = MAX_THREADS;
    g_thread_count = threads;

    rpc_config_t rpc = {0};
    snprintf(rpc.host, sizeof(rpc.host), "%s", cfg->host);
    rpc.port = cfg->port;
    snprintf(rpc.user, sizeof(rpc.user), "%s", cfg->user);
    snprintf(rpc.pass, sizeof(rpc.pass), "%s", cfg->pass);

    if (cfg->pool_mode == 0 && refresh_template(&rpc) != 0) {
        LOG_ERROR("initial getblocktemplate failed — check node");
        g_running = 0;
        return -1;
    }

    if (cfg->pool_mode == 1) {
        stratum_config_t scfg = {0};
        snprintf(scfg.host, sizeof(scfg.host), "%s", cfg->host);
        scfg.port = cfg->port;
        snprintf(scfg.user, sizeof(scfg.user), "%s", cfg->user);
        snprintf(scfg.pass, sizeof(scfg.pass), "%s", cfg->pass);
        if (stratum_connect(&g_stratum, &scfg) != 0) {
            LOG_ERROR("stratum connect failed — check pool URL");
            g_running = 0;
            return -1;
        }
        // Wait for first job (up to 10 seconds)
        int waited = 0;
        while (!g_stratum.job_valid && waited < 10) {
            stratum_poll(&g_stratum);
            sleep(1);
            waited++;
        }
        if (!g_stratum.job_valid) {
            LOG_ERROR("no job received from pool");
            stratum_disconnect(&g_stratum);
            g_running = 0;
            return -1;
        }
        block_template_t stmpl;
        if (stratum_build_template(&g_stratum, 0, &stmpl) == 0) {
            pthread_mutex_lock(&g_template_mutex);
            memcpy(&g_template, &stmpl, sizeof(stmpl));
            g_template_valid = 1;
            pthread_mutex_unlock(&g_template_mutex);
            LOG_INFO("stratum connected — first job received");
        }
    }

    LOG_INFO("starting %d threads → %s (P-cores %d-%d)",
             threads, cfg->address, PCORE_FIRST, PCORE_FIRST + PCORE_COUNT - 1);

    for (int i = 0; i < threads; i++) {
        thread_data_t *td = (thread_data_t *)calloc(1, sizeof(thread_data_t));
        td->thread_id = i;
        memcpy(&td->rpc, &rpc, sizeof(rpc));
        snprintf(td->address, sizeof(td->address), "%s", cfg->address);
        if (pthread_create(&g_threads[i], NULL, mining_thread, td) != 0) {
            LOG_ERROR("failed to spawn thread %d", i);
            free(td);
        }
    }
    return 0;
}

void miner_stop(void) {
    if (!g_running) return;
    LOG_INFO("stopping...");
    g_running = 0;
    for (int i = 0; i < g_thread_count; i++)
        pthread_join(g_threads[i], NULL);
    if (g_config.pool_mode == 1)
        stratum_disconnect(&g_stratum);
    g_thread_count = 0;
    LOG_INFO("all threads stopped");
}

void miner_get_stats(miner_stats_t *stats) {
    pthread_mutex_lock(&g_stats_mutex);
    stats->total_hashes     = atomic_load(&g_total_hashes);
    stats->blocks_found     = atomic_load(&g_blocks_found);
    stats->shares_submitted = atomic_load(&g_shares_submitted);
    stats->running          = g_running;
    stats->thread_count     = g_thread_count;
    stats->hashrate         = 0;
    pthread_mutex_unlock(&g_stats_mutex);
}

int  miner_is_running(void) { return g_running; }

void miner_set_threads(int threads) {
    if (!g_running) return;
    miner_stop();
    g_config.threads = threads;
    miner_start(&g_config, &g_callbacks);
}
