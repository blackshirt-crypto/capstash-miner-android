/**
 * stratum.c — Stratum client for CapStash Android miner
 * v1.1 — fixed process_line order (subscribe before notify)
 *
 * Protocol flow:
 *   1. TCP connect to pool
 *   2. Send mining.subscribe → get extranonce1 + extranonce2_size
 *   3. Send mining.authorize → authenticate wallet.worker
 *   4. Receive mining.notify → new job
 *   5. Mine → send mining.submit when share found
 */

#define _GNU_SOURCE
#include "stratum.h"
#include "whirlpool.h"
#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define LOG_INFO(fmt, ...)  fprintf(stdout, "[stratum] "      fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  fprintf(stderr, "[stratum] WARN: " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[stratum] ERR:  " fmt "\n", ##__VA_ARGS__)

#define STRATUM_TIMEOUT_SEC  30
#define RECV_BUF_SIZE        4096

// ── TCP connect ───────────────────────────────────────────────────────────
static int tcp_connect(const char *host, int port) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        LOG_ERROR("getaddrinfo(%s) failed", host);
        return -1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }

    struct timeval tv = { STRATUM_TIMEOUT_SEC, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        LOG_ERROR("connect(%s:%d) failed: %s", host, port, strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    LOG_INFO("connected to %s:%d", host, port);
    return sock;
}

// ── Send a line (appends \n) ──────────────────────────────────────────────
static int stratum_send(stratum_ctx_t *ctx, const char *msg) {
    char buf[2048];
    int  len = snprintf(buf, sizeof(buf), "%s\n", msg);
    if (send(ctx->sock, buf, len, 0) != len) {
        LOG_ERROR("send failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

// ── Receive one line (newline-delimited JSON) ─────────────────────────────
static int stratum_recv_line(stratum_ctx_t *ctx, char *out, size_t out_sz) {
    size_t pos = 0;
    char   c;
    while (pos < out_sz - 1) {
        int n = recv(ctx->sock, &c, 1, 0);
        if (n <= 0) return -1;
        if (c == '\n') break;
        out[pos++] = c;
    }
    out[pos] = '\0';
    return (int)pos;
}

// ── Parse mining.subscribe response ──────────────────────────────────────
// Response: {"result":[[["mining.notify","..."]]],"extranonce1","en2_size"}
// The result array contains "mining.notify" as a string — so we MUST
// parse subscribe by id BEFORE checking for notify by method name.
static int parse_subscribe(stratum_ctx_t *ctx, const char *line) {
    // Find extranonce1 — it follows the last ]] in the result array
    // Pattern: ]],"<extranonce1>",<size>  or  ]],{"  variations
    const char *p = strstr(line, "]],\"");
    if (!p) {
        p = strstr(line, "],\"");
        if (!p) { LOG_ERROR("subscribe parse failed: %s", line); return -1; }
        p += 3;
    } else {
        p += 4;
    }

    // p now points at start of extranonce1 value
    const char *e = strchr(p, '"');
    if (!e) return -1;
    size_t len = (size_t)(e - p);
    if (len >= sizeof(ctx->extranonce1)) return -1;
    memcpy(ctx->extranonce1, p, len);
    ctx->extranonce1[len] = '\0';

    // extranonce2_size follows: ,"<en1>",<size>
    p = e + 2; // skip closing quote and comma
    ctx->extranonce2_size = atoi(p);
    if (ctx->extranonce2_size <= 0) ctx->extranonce2_size = 4;

    LOG_INFO("subscribed — extranonce1=%s en2_size=%d",
             ctx->extranonce1, ctx->extranonce2_size);
    return 0;
}

// ── Parse mining.notify ───────────────────────────────────────────────────
// Params: [job_id, prevhash, coinb1, coinb2,
//          [merkle_branch,...], version, nbits, ntime, clean_jobs]
static int parse_notify(stratum_ctx_t *ctx, const char *line) {
    stratum_job_t *j = &ctx->job;

    const char *params = strstr(line, "\"params\":[");
    if (!params) return -1;
    params += 10;

    const char *p = params;

    // Field 0: job_id
    if (*p == '"') p++;
    const char *e = strchr(p, '"');
    if (!e) return -1;
    size_t len = (size_t)(e - p);
    if (len >= sizeof(j->job_id)) len = sizeof(j->job_id) - 1;
    memcpy(j->job_id, p, len); j->job_id[len] = '\0';
    p = e + 2;

    // Field 1: prevhash
    if (*p == '"') p++;
    e = strchr(p, '"'); if (!e) return -1;
    len = (size_t)(e - p); if (len >= sizeof(j->prev_hash)) len = sizeof(j->prev_hash)-1;
    memcpy(j->prev_hash, p, len); j->prev_hash[len] = '\0';
    p = e + 2;

    // Field 2: coinb1
    if (*p == '"') p++;
    e = strchr(p, '"'); if (!e) return -1;
    len = (size_t)(e - p); if (len >= sizeof(j->coinb1)) len = sizeof(j->coinb1)-1;
    memcpy(j->coinb1, p, len); j->coinb1[len] = '\0';
    p = e + 2;

    // Field 3: coinb2
    if (*p == '"') p++;
    e = strchr(p, '"'); if (!e) return -1;
    len = (size_t)(e - p); if (len >= sizeof(j->coinb2)) len = sizeof(j->coinb2)-1;
    memcpy(j->coinb2, p, len); j->coinb2[len] = '\0';
    p = e + 2;

    // Field 4: merkle_branch array
    j->merkle_count = 0;
    if (*p == '[') {
        p++;
        while (*p && *p != ']') {
            if (*p == '"') p++;
            e = strchr(p, '"'); if (!e) break;
            if (j->merkle_count < 8) {
                len = (size_t)(e - p);
                if (len >= 65) len = 64;
                memcpy(j->merkle_branch[j->merkle_count], p, len);
                j->merkle_branch[j->merkle_count][len] = '\0';
                j->merkle_count++;
            }
            p = e + 1;
            if (*p == ',') p++;
        }
        if (*p == ']') p++;
        if (*p == ',') p++;
    }

    // Field 5: version
    if (*p == '"') p++;
    e = strchr(p, '"'); if (!e) return -1;
    len = (size_t)(e - p); if (len >= sizeof(j->version)) len = sizeof(j->version)-1;
    memcpy(j->version, p, len); j->version[len] = '\0';
    p = e + 2;

    // Field 6: nbits
    if (*p == '"') p++;
    e = strchr(p, '"'); if (!e) return -1;
    len = (size_t)(e - p); if (len >= sizeof(j->nbits)) len = sizeof(j->nbits)-1;
    memcpy(j->nbits, p, len); j->nbits[len] = '\0';
    p = e + 2;

    // Field 7: ntime
    if (*p == '"') p++;
    e = strchr(p, '"'); if (!e) return -1;
    len = (size_t)(e - p); if (len >= sizeof(j->ntime)) len = sizeof(j->ntime)-1;
    memcpy(j->ntime, p, len); j->ntime[len] = '\0';
    p = e + 2;

    // Field 8: clean_jobs
    j->clean_jobs = (strncmp(p, "true", 4) == 0) ? 1 : 0;

    // Copy extranonce info into job
    snprintf(j->extranonce1, sizeof(j->extranonce1), "%s", ctx->extranonce1);
    j->extranonce2_size = ctx->extranonce2_size;

    LOG_INFO("new job id=%s nbits=%s clean=%d merkle=%d",
             j->job_id, j->nbits, j->clean_jobs, j->merkle_count);
    LOG_INFO("coinb1: %s", j->coinb1);
    LOG_INFO("coinb2: %s", j->coinb2);
    for (int i = 0; i < j->merkle_count; i++)
        LOG_INFO("branch[%d]: %s", i, j->merkle_branch[i]);
    return 0;
}

// ── Process one line from pool ────────────────────────────────────────────
// IMPORTANT: Check id-based responses FIRST.
// The subscribe response result array contains the string "mining.notify"
// so checking for notify by name first would misroute the subscribe response.
static int process_line(stratum_ctx_t *ctx, const char *line) {
    if (!line || !line[0]) return 0;

    // Subscribe response — id=1, has "result" array
    if (strstr(line, "\"id\":1") && strstr(line, "\"result\"")) {
        if (parse_subscribe(ctx, line) == 0)
            ctx->subscribed = 1;
        return 0;
    }

    // Authorize response — id=2
    if (strstr(line, "\"id\":2") && strstr(line, "\"result\"")) {
        if (strstr(line, "\"result\":true") || strstr(line, "\"result\": true")) {
            ctx->authorized = 1;
            LOG_INFO("authorized ✓");
        } else {
            LOG_ERROR("authorization failed — check wallet address");
        }
        return 0;
    }

    // Share submit response — has result but no method
    if ((strstr(line, "\"result\":true") || strstr(line, "\"result\":false")) &&
        !strstr(line, "\"method\"")) {
        if (strstr(line, "\"result\":true"))
            LOG_INFO("share accepted ✓");
        else
            LOG_WARN("share rejected");
        return 0;
    }

    // mining.notify — actual work notification (id:null)
    if (strstr(line, "\"mining.notify\"") && strstr(line, "\"id\":null")) {
        if (strstr(line, "\"params\"")) {
            if (parse_notify(ctx, line) == 0) {
                ctx->job_valid = 1;
                return 1; // new job
            }
        }
        return 0;
    }

    // mining.set_difficulty
    if (strstr(line, "mining.set_difficulty")) {
        LOG_INFO("set_difficulty received");
        return 0;
    }

    return 0;
}

// ── Public API ────────────────────────────────────────────────────────────

int stratum_connect(stratum_ctx_t *ctx, const stratum_config_t *cfg) {
    memset(ctx, 0, sizeof(stratum_ctx_t));
    memcpy(&ctx->cfg, cfg, sizeof(stratum_config_t));
    ctx->msg_id = 1;
    ctx->sock   = -1;

    ctx->sock = tcp_connect(cfg->host, cfg->port);
    if (ctx->sock < 0) return -1;

    char msg[512];
    char line[RECV_BUF_SIZE];

    // 1. Subscribe
    snprintf(msg, sizeof(msg),
        "{\"id\":%ld,\"method\":\"mining.subscribe\","
        "\"params\":[\"capstash-miner/1.0\"]}",
        ctx->msg_id++);
    if (stratum_send(ctx, msg) < 0) goto fail;

    // Read subscribe response — may get set_difficulty before it
    for (int i = 0; i < 5; i++) {
        if (stratum_recv_line(ctx, line, sizeof(line)) < 0) {
            LOG_ERROR("no subscribe response");
            goto fail;
        }
        process_line(ctx, line);
        if (ctx->subscribed) break;
    }
    if (!ctx->subscribed) {
        LOG_ERROR("subscribe failed");
        goto fail;
    }

    // 2. Authorize
    snprintf(msg, sizeof(msg),
        "{\"id\":%ld,\"method\":\"mining.authorize\","
        "\"params\":[\"%s\",\"%s\"]}",
        ctx->msg_id++, cfg->user, cfg->pass);
    if (stratum_send(ctx, msg) < 0) goto fail;

    // Read authorize response — may get notify/difficulty before it
    for (int i = 0; i < 10; i++) {
        if (stratum_recv_line(ctx, line, sizeof(line)) < 0) break;
        process_line(ctx, line);
        if (ctx->authorized) break;
    }
    if (!ctx->authorized) {
        LOG_ERROR("authorization failed");
        goto fail;
    }

    // Set non-blocking for poll
    int flags = fcntl(ctx->sock, F_GETFL, 0);
    fcntl(ctx->sock, F_SETFL, flags | O_NONBLOCK);

    return 0;

fail:
    close(ctx->sock);
    ctx->sock = -1;
    return -1;
}

void stratum_disconnect(stratum_ctx_t *ctx) {
    if (ctx->sock >= 0) {
        close(ctx->sock);
        ctx->sock = -1;
    }
    ctx->subscribed  = 0;
    ctx->authorized  = 0;
    ctx->job_valid   = 0;
}

int stratum_poll(stratum_ctx_t *ctx) {
    char line[RECV_BUF_SIZE];
    int  new_job = 0;
    while (1) {
        ssize_t n = recv(ctx->sock, line, 1, MSG_PEEK | MSG_DONTWAIT);
        if (n <= 0) break;
        if (stratum_recv_line(ctx, line, sizeof(line)) < 0) return -1;
        if (process_line(ctx, line) == 1) new_job = 1;
    }
    return new_job;
}

int stratum_submit(stratum_ctx_t *ctx, const char *job_id,
                   const char *ntime, const char *en2_hex,
                   const char *nonce_hex) {
    char msg[512];
    snprintf(msg, sizeof(msg),
        "{\"id\":%ld,\"method\":\"mining.submit\","
        "\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}",
        ctx->msg_id++,
        ctx->cfg.user,
        job_id, en2_hex, ntime, nonce_hex);
    return stratum_send(ctx, msg);
}

int stratum_build_template(const stratum_ctx_t *ctx,
                            uint64_t en2,
                            block_template_t *tmpl) {
    const stratum_job_t *j = &ctx->job;
    if (!ctx->job_valid) return -1;

    memset(tmpl, 0, sizeof(block_template_t));

    tmpl->version = (uint32_t)strtoul(j->version, NULL, 16);
    // Stratum sends prevhash as 8 x 4-byte words each byte-swapped — fix it
 
    // Store prevhash as-is — build_header() will reverse it
    snprintf(tmpl->prev_hash_hex, sizeof(tmpl->prev_hash_hex), "%s", j->prev_hash);
    // DEBUG
    LOG_INFO("prevhash raw: %s", j->prev_hash);
    tmpl->bits = (uint32_t)strtoul(j->nbits, NULL, 16);
    snprintf(tmpl->bits_hex, sizeof(tmpl->bits_hex), "%s", j->nbits);

    // Expand nbits to full 32-byte target (little-endian bytes to match hash compare)
    uint32_t exp  = (tmpl->bits >> 24) & 0xff;
    uint32_t mant = tmpl->bits & 0x7fffff;
    // Build target as 32 raw bytes first, then hex encode little-endian
    uint8_t target_bytes[32];
    memset(target_bytes, 0, 32);
    if (exp >= 1 && exp <= 32) {
        // Place mantissa bytes at position (exp-3) counting from LSB (index 0)
        int pos = (int)exp - 3;
        if (pos >= 0 && pos + 2 < 32) {
            target_bytes[pos + 2] = (mant >> 16) & 0xff;
            target_bytes[pos + 1] = (mant >> 8)  & 0xff;
            target_bytes[pos + 0] =  mant         & 0xff;
        }
    }
    // Hex encode as big-endian (byte 31 first) to match meets_target() comparison
    for (int i = 0; i < 32; i++)
        snprintf(tmpl->target_hex + i*2, 3, "%02x", target_bytes[31 - i]);
    tmpl->target_hex[64] = '\0';

    tmpl->curtime = (uint32_t)strtoul(j->ntime, NULL, 16);
    snprintf(tmpl->job_id,    sizeof(tmpl->job_id),    "%s", j->job_id);
    snprintf(tmpl->ntime_hex, sizeof(tmpl->ntime_hex), "%s", j->ntime);

    // Build extranonce2 hex string
    char en2_hex[32] = {0};
    int  en2_bytes   = j->extranonce2_size;
    for (int i = 0; i < en2_bytes; i++)
        snprintf(en2_hex + i*2, 3, "%02x",
                 (unsigned)(en2 >> ((en2_bytes-1-i)*8)) & 0xff);
    en2_hex[en2_bytes * 2] = '\0';

    // coinbase = coinb1 + extranonce1 + en2 + coinb2
    int cb_off = 0, cb_cap = (int)sizeof(tmpl->coinbase_hex);
    cb_off += snprintf(tmpl->coinbase_hex + cb_off, cb_cap - cb_off, "%s", j->coinb1);
    cb_off += snprintf(tmpl->coinbase_hex + cb_off, cb_cap - cb_off, "%s", j->extranonce1);
    cb_off += snprintf(tmpl->coinbase_hex + cb_off, cb_cap - cb_off, "%s", en2_hex);
              snprintf(tmpl->coinbase_hex + cb_off, cb_cap - cb_off, "%s", j->coinb2);

    // Merkle root: sha256d(coinbase) then hash with each branch
    uint8_t cb_raw[512];
    int     cb_len = hex_to_bytes(tmpl->coinbase_hex, cb_raw, sizeof(cb_raw));
    if (cb_len < 0) return -1;

    uint8_t hash[32];
    sha256d(cb_raw, cb_len, hash);

    for (int i = 0; i < j->merkle_count; i++) {
        uint8_t branch[32];
        hex_to_bytes(j->merkle_branch[i], branch, 32);
        uint8_t combined[64];
        memcpy(combined,      hash,   32);
        memcpy(combined + 32, branch, 32);
        sha256d(combined, 64, hash);
    }

    bytes_to_hex(hash, 32, tmpl->merkle_root_hex);
    return 0;
}

int stratum_is_connected(const stratum_ctx_t *ctx) {
    return (ctx->sock >= 0 && ctx->subscribed && ctx->authorized);
}
