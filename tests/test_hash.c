/*
 * Hash regression test for veriumMiner.
 *
 * Links the real scrypt^2 hashing core (algo/scrypt.c + algo/sha2.c + the
 * per-arch assembly) and verifies that scrypthash() is:
 *   1. deterministic (same input -> same output),
 *   2. non-null (catches scratchpad allocation failures), and
 *   3. bit-identical to a recorded golden digest (when one is present).
 *
 * This is the safety net that lets the rest of the codebase be refactored
 * without silently changing consensus-critical output.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "golden.h"

/* Implemented in algo/scrypt.c */
extern void scrypthash(void *output, const void *input, uint32_t N);

/*
 * Stubs for symbols referenced by scanhash_scrypt() (pulled in from
 * scrypt.c) but never executed by this test. They only need to resolve at
 * link time so we can test the pure hashing path without the network stack.
 */
struct work_restart { volatile unsigned char restart; char pad[127]; };
struct work_restart *work_restart = 0;

struct work;

bool fulltest(const uint32_t *hash, const uint32_t *target)
{
	(void)hash; (void)target;
	return false;
}

void work_set_target_ratio(struct work *work, uint32_t *hash)
{
	(void)work; (void)hash;
}

bool have_stratum = false;

void applog(int prio, const char *fmt, ...)
{
	(void)prio; (void)fmt;
}

static void to_hex(const uint8_t *in, size_t len, char *out)
{
	static const char hexd[] = "0123456789abcdef";
	size_t i;
	for (i = 0; i < len; i++) {
		out[2 * i]     = hexd[in[i] >> 4];
		out[2 * i + 1] = hexd[in[i] & 0x0f];
	}
	out[2 * len] = '\0';
}

int main(void)
{
	const uint32_t N = 1048576;
	uint8_t input[80];
	uint8_t hash_a[32], hash_b[32];
	char hex[65];
	int failures = 0;
	int nonzero = 0;
	size_t i;

	/* Canonical vector: 80-byte all-zero buffer, matching --cputest. */
	memset(input, 0, sizeof(input));

	scrypthash(hash_a, input, N);
	scrypthash(hash_b, input, N);

	to_hex(hash_a, 32, hex);
	printf("RECORD: scrypt:1048576 empty-buffer = %s\n", hex);

	/* 1. determinism */
	if (memcmp(hash_a, hash_b, 32) != 0) {
		fprintf(stderr, "FAIL: scrypthash is not deterministic\n");
		failures++;
	}

	/* 2. non-null (scratchpad allocation / hashing actually ran) */
	for (i = 0; i < 32; i++)
		nonzero |= hash_a[i];
	if (!nonzero) {
		fprintf(stderr, "FAIL: scrypthash returned an all-zero digest\n");
		failures++;
	}

	/* 3. golden lock (only enforced once a value has been recorded) */
	if (sizeof(VERIUM_GOLDEN_EMPTY_N1048576) - 1 == 64) {
		if (strcmp(hex, VERIUM_GOLDEN_EMPTY_N1048576) != 0) {
			fprintf(stderr,
				"FAIL: golden mismatch!\n  expected %s\n  got      %s\n",
				VERIUM_GOLDEN_EMPTY_N1048576, hex);
			failures++;
		} else {
			printf("PASS: golden vector matches\n");
		}
	} else {
		printf("NOTE: golden vector not recorded yet (see tests/golden.h)\n");
	}

	if (failures) {
		fprintf(stderr, "%d hash test(s) failed\n", failures);
		return 1;
	}
	printf("All hash tests passed\n");
	return 0;
}
