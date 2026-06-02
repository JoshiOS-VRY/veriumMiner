/*
 * Minimal SHA-1 implementation (public domain).
 * Based on the widely used reference by Steve Reid <steve@edmweb.com>.
 *
 * Used only by the optional API websocket handshake so that the miner does
 * not need to link against OpenSSL/libcrypto for a single SHA-1 digest.
 */
#ifndef VERIUM_SHA1_H
#define VERIUM_SHA1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t state[5];
	uint32_t count[2];
	uint8_t  buffer[64];
} vsha1_ctx;

void vsha1_init(vsha1_ctx *ctx);
void vsha1_update(vsha1_ctx *ctx, const void *data, size_t len);
void vsha1_final(vsha1_ctx *ctx, uint8_t digest[20]);

#ifdef __cplusplus
}
#endif

#endif /* VERIUM_SHA1_H */
