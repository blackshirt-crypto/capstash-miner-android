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
    // Block 69792 fields
    uint32_t version  = 536870912;
    // previousblockhash — as stored in Core (little-endian internal)
    const char *prevhash_hex = "a86f2c94ec894f39b2a09cd0ae53e4037db7bb975b7e32a9bd7334a4b91eee20";
    const char *merkle_hex   = "afc7aaa70fa5358a73a348f96f815ebe396f0e0036a0eda7ef1b5d3f08ce7ee4";
    uint32_t ntime  = 1774701401;
    uint32_t bits   = 0x1b18bcd6;
    uint32_t nonce  = 1618377884;

    const char *expected = "00000000000182999b1b499aed8875cfe0bba36f06c1967f606576f7814794e6";

    uint8_t header[80];
    uint8_t prev[32], merkle[32];

    hex_to_bytes(prevhash_hex, prev, 32);
hex_to_bytes(merkle_hex,   merkle, 32);

// Reverse both — Core uint256 is stored reversed internally
uint8_t prev_rev[32], merkle_rev[32];
for (int i = 0; i < 32; i++) {
    prev_rev[i]   = prev[31-i];
    merkle_rev[i] = merkle[31-i];
}

write_le32(header, 0, version);
memcpy(header + 4,  prev_rev,   32);
memcpy(header + 36, merkle_rev, 32);
    write_le32(header, 68, ntime);
    write_le32(header, 72, bits);
    write_le32(header, 76, nonce);

    uint8_t out[32], out_rev[32];
capstash_hash(header, out);

// Core returns uint256 which displays reversed
for (int i = 0; i < 32; i++) out_rev[i] = out[31-i];

char result[65];
bytes_to_hex(out_rev, 32, result);

    printf("Expected: %s\n", expected);
    printf("Got:      %s\n", result);
    printf("Match:    %s\n", strcmp(result, expected) == 0 ? "YES ✓" : "NO ✗");

    // Test with live header from miner log
    const char *live_header_hex = "0000002003b900a828908596c573811cefc3f7d5aef304bf6267a6c6fb15b57ab384adace2d83e1d59091418477ee1def15dd6b375df4f7d77fd27f3f274b769f325496b27dbc7699d2e161b00000000";
    uint8_t live_hdr[80];
    hex_to_bytes(live_header_hex, live_hdr, 80);
    uint8_t live_out[32];
    capstash_hash(live_hdr, live_out);
    char live_result[65];
    bytes_to_hex(live_out, 32, live_result);
    printf("Live header hash: %s\n", live_result);

// Print merkle root from live header (bytes 36-67)
    char merkle_from_hdr[65];
    bytes_to_hex(live_hdr + 36, 32, merkle_from_hdr);
    printf("Merkle in header: %s\n", merkle_from_hdr);

// Verify merkle root from coinbase (job 00001adf, merkle=0, thread 0)
    printf("Actual in header: 2f209d3963f08c4ed3b47bbe8d3a584cad6e299697f24fe12766e48122c18213\n");
    printf("Merkle match:     %s\n", strcmp(merkle_hex2, "2f209d3963f08c4ed3b47bbe8d3a584cad6e299697f24fe12766e48122c18213") == 0 ? "YES ✓" : "NO ✗");char full_cb[2048];
    snprintf(full_cb, sizeof(full_cb), "%s%s%s%s", coinb1, en1, en2_str, coinb2);
    uint8_t cb_raw[1024];
    int cb_len = hex_to_bytes(full_cb, cb_raw, sizeof(cb_raw));
    uint8_t cb_txid[32];
    sha256d(cb_raw, cb_len, cb_txid);
    char txid_hex[65];
    bytes_to_hex(cb_txid, 32, txid_hex);
    printf("Coinbase txid:    %s\n", txid_hex);
    // Reverse for merkle root in header
    uint8_t merkle_rev2[32];
    for (int i = 0; i < 32; i++) merkle_rev2[i] = cb_txid[31-i];
    char merkle_hex2[65];
    bytes_to_hex(merkle_rev2, 32, merkle_hex2);
    printf("Merkle in header: %s\n", merkle_hex2);
    printf("Actual in header: e2d83e1d59091418477ee1def15dd6b375df4f7d77fd27f3f274b769f325496b\n");
    printf("Merkle match:     %s\n", strcmp(merkle_hex2, "e2d83e1d59091418477ee1def15dd6b375df4f7d77fd27f3f274b769f325496b") == 0 ? "YES ✓" : "NO ✗");

    return 0;
}

