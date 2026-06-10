/*
 * 3-way scrypt lane equivalence test (aarch64 NEON path).
 *
 * Each lane of scrypt_1024_1_1_256_3way must match scrypt_1024_1_1_256 for the
 * same nonce — otherwise the miner would produce invalid shares.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

extern int scrypt_3way_equiv_selftest(void);

/* Stubs for scanhash_scrypt() symbols pulled in from scrypt.c (link only). */
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

int main(void)
{
	int failures = scrypt_3way_equiv_selftest();

	if (failures) {
		fprintf(stderr, "FAIL: scrypt 3-way equivalence: %d mismatch(es)\n", failures);
		return 1;
	}
	printf("PASS: scrypt 3-way lanes match 1-way core (or 3-way not compiled)\n");
	return 0;
}
