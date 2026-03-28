/**
 * test_hash.c — verify hash function against known block
 * Block 69792 — known powhash: 00000000000182999b1b499aed8875cfe0bba36f06c1967f606576f7814794e6
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "whirlpool.h"


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

    write_le32(header, 0, version);
    memcpy(header + 4,  prev,   32);
    memcpy(header + 36, merkle, 32);
    write_le32(header, 68, ntime);
    write_le32(header, 72, bits);
    write_le32(header, 76, nonce);

    uint8_t out[32];
    capstash_hash(header, out);

    char result[65];
    bytes_to_hex(out, 32, result);

    printf("Expected: %s\n", expected);
    printf("Got:      %s\n", result);
    printf("Match:    %s\n", strcmp(result, expected) == 0 ? "YES ✓" : "NO ✗");

    return 0;
}
