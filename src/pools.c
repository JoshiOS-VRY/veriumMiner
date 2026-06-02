/*
 * Pool URL list with failover and reconnect backoff.
 */
#include "pools.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *g_urls[POOLS_MAX];
static int g_count;
static int g_current;
static int g_fail_attempts;

void pools_clear(void)
{
	int i;
	for (i = 0; i < g_count; i++) {
		free(g_urls[i]);
		g_urls[i] = NULL;
	}
	g_count = 0;
	g_current = 0;
	g_fail_attempts = 0;
}

void pools_add(const char *url)
{
	if (!url || !*url || g_count >= POOLS_MAX)
		return;
	for (int i = 0; i < g_count; i++) {
		if (g_urls[i] && !strcmp(g_urls[i], url))
			return;
	}
	g_urls[g_count++] = strdup(url);
}

void pools_set_primary(const char *url)
{
	pools_clear();
	pools_add(url);
}

int pools_count(void)
{
	return g_count;
}

const char *pools_current_url(void)
{
	if (g_count <= 0)
		return NULL;
	return g_urls[g_current];
}

bool pools_failover(void)
{
	g_fail_attempts++;
	if (g_count <= 1)
		return false;
	g_current = (g_current + 1) % g_count;
	return true;
}

int pools_backoff_seconds(int attempt)
{
	int base = 10;
	int cap = 300;
	int delay;
	if (attempt < 1)
		attempt = 1;
	delay = base;
	while (attempt > 1 && delay < cap) {
		delay *= 2;
		attempt--;
	}
	if (delay > cap)
		delay = cap;
	/* jitter 0..25% */
	delay += (int)(delay * ((rand() % 26) / 100.0));
	return delay;
}

void pools_reset_backoff(void)
{
	g_fail_attempts = 0;
}

int pools_fail_attempts(void)
{
	return g_fail_attempts;
}
