#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "sha256.h"
#include "rpc.h"

// GPU accepted share data from tcpdump
// job=00003e7c, en2=000000000000001b, ntime=69cdf0c5, nonce=00b3081b
// en1=30000000000034f7

int main() {
    // Stratum fields
    const char *coinb1 = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff2e03ad2a0104c5f0cd6900";
    const char *en1    = "30000000000034fb";
    const char *en2    = "000000000000001b";
    const char *coinb2 = "134d696e656420427920314d696e65722e4e657400000000020000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf900e1f505000000001976a9141dbec89f1504d794f310bde57c008b0f1e271b1b88ac00000000";
    const char *prevhash_stratum = "6bd27a13a3643da6e33388d22aa1024bf1e351fb5eb93b36c5b5969e835335eb";
    const char *ntime  = "69cdf0c5";
    const char *nbits  = "1b106323";  // from GPU tcpdump job 00003e7c
    uint32_t nonce     = 0x00b3081b;
    uint32_t version   = 0x20000000;

    // 1. Build coinbase hex
    char coinbase_hex[1024];
    snprintf(coinbase_hex, sizeof(coinbase_hex), "%s%s%s%s", coinb1, en1, en2, coinb2);
    printf("coinbase_hex: %s\n\n", coinbase_hex);

    // 2. Decode coinbase to bytes and sha256d
    uint8_t cb_raw[512];
    int cb_len = hex_to_bytes(coinbase_hex, cb_raw, sizeof(cb_raw));
    uint8_t merkle_root[32];
    sha256d(cb_raw, cb_len, merkle_root);

    char merkle_hex[65];
    bytes_to_hex(merkle_root, 32, merkle_hex);
    printf("merkle_root (raw): %s\n", merkle_hex);

    // 3. Build header — try different prevhash/merkle byte orders
    uint8_t prev[32], header[80];

    // Version (little-endian)
    header[0] = version & 0xff;
    header[1] = (version >> 8) & 0xff;
    header[2] = (version >> 16) & 0xff;
    header[3] = (version >> 24) & 0xff;

    // Prevhash — METHOD A: 4-byte chunk swizzle (what we do now)
    hex_to_bytes(prevhash_stratum, prev, 32);
    uint8_t prev_swiz[32];
    for (int i = 0; i < 32; i += 4) {
        prev_swiz[i+0] = prev[i+3];
        prev_swiz[i+1] = prev[i+2];
        prev_swiz[i+2] = prev[i+1];
        prev_swiz[i+3] = prev[i+0];
    }

    // Prevhash — METHOD B: full 32-byte reversal (what we used to do)
    uint8_t prev_rev[32];
    for (int i = 0; i < 32; i++) prev_rev[i] = prev[31-i];

    // Prevhash — METHOD C: no transformation (raw as-is)
    // just use prev directly

    printf("\nprevhash swizzle:  ");
    for (int i = 0; i < 32; i++) printf("%02x", prev_swiz[i]);
    printf("\nprevhash reverse:  ");
    for (int i = 0; i < 32; i++) printf("%02x", prev_rev[i]);
    printf("\nprevhash raw:      %s\n", prevhash_stratum);

    // Merkle — METHOD 1: no reversal (raw sha256d output)
    // Merkle — METHOD 2: full reversal
    uint8_t merkle_rev[32];
    for (int i = 0; i < 32; i++) merkle_rev[i] = merkle_root[31-i];

    printf("\nmerkle raw:     %s\n", merkle_hex);
    printf("merkle reversed: ");
    for (int i = 0; i < 32; i++) printf("%02x", merkle_rev[i]);
    printf("\n");

    // ntime and nbits
    uint32_t ntime_val = (uint32_t)strtoul(ntime, NULL, 16);
    uint32_t nbits_val = (uint32_t)strtoul(nbits, NULL, 16);

    // Try all 6 combinations: (3 prevhash methods) x (2 merkle methods)
    uint8_t *prev_methods[3] = { prev_swiz, prev_rev, prev };
    const char *prev_names[3] = { "swizzle", "reverse", "raw" };
    uint8_t *merk_methods[2] = { merkle_root, merkle_rev };
    const char *merk_names[2] = { "raw", "reversed" };

    printf("\n=== Testing all header combinations ===\n");
    for (int pi = 0; pi < 3; pi++) {
        for (int mi = 0; mi < 2; mi++) {
            memcpy(header + 4, prev_methods[pi], 32);
            memcpy(header + 36, merk_methods[mi], 32);
            header[68] = ntime_val & 0xff;
            header[69] = (ntime_val >> 8) & 0xff;
            header[70] = (ntime_val >> 16) & 0xff;
            header[71] = (ntime_val >> 24) & 0xff;
            header[72] = nbits_val & 0xff;
            header[73] = (nbits_val >> 8) & 0xff;
            header[74] = (nbits_val >> 16) & 0xff;
            header[75] = (nbits_val >> 24) & 0xff;
            header[76] = nonce & 0xff;
            header[77] = (nonce >> 8) & 0xff;
            header[78] = (nonce >> 16) & 0xff;
            header[79] = (nonce >> 24) & 0xff;

            // Hash it with Whirlpool
            uint8_t hash[32];
            capstash_hash(header, hash);

            // Reverse for display (big-endian)
            uint8_t hash_be[32];
            for (int i = 0; i < 32; i++) hash_be[i] = hash[31-i];

            // Compute difficulty from hash
            // diff = diff1 / hash_value
            // Quick check: count leading zero bytes
            int leading_zeros = 0;
            for (int i = 0; i < 32; i++) {
                if (hash_be[i] == 0) leading_zeros++;
                else break;
            }

            printf("\nprev=%s merkle=%s:\n  hash_be=", prev_names[pi], merk_names[mi]);
            for (int i = 0; i < 8; i++) printf("%02x", hash_be[i]);
            printf("...  leading_zeros=%d %s\n",
                   leading_zeros,
                   leading_zeros >= 2 ? "← POSSIBLE MATCH" : "");
        }
    }

    return 0;
}
