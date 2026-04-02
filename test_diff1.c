#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "whirlpool.h"
#include "sha256.h"

int main(void) {
    const char *hdr_hex = "00000020c6ee8caf1ab3adda5b5f291bf1f03d7f096c2a5810952bf888e60e70693c1da5196a5551113eaafff160db679666e969a50d8beadaec9c6f01c3e78154a96f11f5ddcd699e820f1b00000000";
    uint8_t header[80];
    hex_to_bytes(hdr_hex, header, 80);
    uint32_t nonce = 1972;
    write_le32(header, 76, nonce);
    uint8_t hash[32];
    capstash_hash(header, hash);
    char raw_hex[65];
    bytes_to_hex(hash, 32, raw_hex);
    printf("Raw hash:      %s\n", raw_hex);
    uint8_t hash_be[32];
    for (int i = 0; i < 32; i++) hash_be[i] = hash[31-i];
    char be_hex[65];
    bytes_to_hex(hash_be, 32, be_hex);
    printf("Reversed hash: %s\n", be_hex);
    printf("hash_be[0]: %02x\n", hash_be[0]);
    printf("Nonce LE bytes: %02x%02x%02x%02x\n",
           header[76], header[77], header[78], header[79]);
    return 0;
}
