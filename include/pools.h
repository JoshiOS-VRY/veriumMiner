#ifndef VERIUM_POOLS_H
#define VERIUM_POOLS_H

#include <stdbool.h>

#define POOLS_MAX 8

void pools_clear(void);
void pools_add(const char *url);
void pools_set_primary(const char *url);
int pools_count(void);
const char *pools_current_url(void);

/* Advance to next pool after failure; returns false if no more pools. */
bool pools_failover(void);

/* Seconds to wait before reconnect (exponential backoff with jitter). */
int pools_backoff_seconds(int attempt);

void pools_reset_backoff(void);
int pools_fail_attempts(void);

#endif /* VERIUM_POOLS_H */
