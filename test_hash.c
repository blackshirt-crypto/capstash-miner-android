/**
 * test_hash.c — verify hash function against known block
 * Block 69792 — known powhash: 00000000000182999b1b499aed8875cfe0bba36f06c1967f606576f7814794e6
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "whirlpool.h"
#include "sha256.h"

int main(void) {
    // ── Test 1: known block 69792 ─────────────────────────────────────────
    uint32_t version = 536870912;
    const char *prevhash_hex = "a86f2c94ec894f39b2a09cd0ae53e4037db7bb975b7e32a9bd7334a4b91eee20";
    const char *merkle_hex   = "afc7aaa70fa5358a73a348f96f815ebe396f0e0036a0eda7ef1b5d3f08ce7ee4";
    uint32_t ntime = 1774701401;
    uint32_t bits  = 0x1b18bcd6;
    uint32_t nonce = 1618377884;
    const char *expected = "00000000000182999b1b499aed8875cfe0bba36f06c1967f606576f7814794e6";

    uint8_t header[80];
    uint8_t prev[32], merkle[32];
    uint8_t prev_rev[32], merkle_rev[32];
    hex_to_bytes(prevhash_hex, prev, 32);
    hex_to_bytes(merkle_hex, merkle, 32);
    for (int i = 0; i < 32; i++) { prev_rev[i] = prev[31-i]; merkle_rev[i] = merkle[31-i]; }
    write_le32(header, 0, version);
    memcpy(header + 4,  prev_rev,   32);
    memcpy(header + 36, merkle_rev, 32);
    write_le32(header, 68, ntime);
    write_le32(header, 72, bits);
    write_le32(header, 76, nonce);

    uint8_t out[32], out_rev[32];
    capstash_hash(header, out);
    for (int i = 0; i < 32; i++) out_rev[i] = out[31-i];
    char result[65];
    bytes_to_hex(out_rev, 32, result);
    printf("Expected: %s\n", expected);
    printf("Got:      %s\n", result);
    printf("Match:    %s\n\n", strcmp(result, expected) == 0 ? "YES ✓" : "NO ✗");

    // ── Test 2: verify coinbase merkle from session ───────────────────────
    const char *coinb1  = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff2e030c11010435e5c76900";
    const char *en1     = "8000000000001323";
    const char *en2_str = "0000000000000000";
    const char *coinb2  = "134d696e656420427920314d696e65722e4e657400000000020000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf900e1f505000000001976a9141dbec89f1504d794f310bde57c008b0f1e271b1b88ac00000000";
    const char *actual_merkle = "8b5072309ae16f0d9383a0ae28f17a1c03751d371465a2789c7ef36cd01f3c36";

    char full_cb[2048];
    snprintf(full_cb, sizeof(full_cb), "%s%s%s%s", coinb1, en1, en2_str, coinb2);
    uint8_t cb_raw[1024];
    int cb_len = hex_to_bytes(full_cb, cb_raw, sizeof(cb_raw));
    uint8_t cb_txid[32];
    sha256d(cb_raw, (size_t)cb_len, cb_txid);
    char txid_hex[65];
    bytes_to_hex(cb_txid, 32, txid_hex);
    printf("Coinbase txid:    %s\n", txid_hex);

    uint8_t merkle_rev2[32];
    for (int i = 0; i < 32; i++) merkle_rev2[i] = cb_txid[31-i];
    char merkle_hex2[65];
    bytes_to_hex(merkle_rev2, 32, merkle_hex2);
    printf("Merkle computed:  %s\n", merkle_hex2);
    printf("Merkle in header: %s\n", actual_merkle);
    printf("Merkle match:     %s\n", strcmp(merkle_hex2, actual_merkle) == 0 ? "YES ✓" : "NO ✗");

    return 0;
}
