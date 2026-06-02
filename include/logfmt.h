#ifndef VERIUM_LOGFMT_H
#define VERIUM_LOGFMT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Short level tag + color for applog() prefix (5-char tag, NUL-terminated). */
const char *logfmt_tag(int prio);
const char *logfmt_tag_color(int prio);

/* Human-readable uptime, e.g. "32m 16s", "2h 05m". */
void logfmt_uptime(long seconds, char *buf, size_t bufsz);

/* Middle-ellipsis for long wallet/worker names. */
void logfmt_ellipsis(const char *src, char *out, size_t outsz, size_t max_visible);

/* High-frequency miner events (use applog internally). */
void logfmt_share(bool accepted, bool block_share,
	unsigned long accepted_n, unsigned long total_n,
	const char *suppl, const char *rate_hpm, const char *extra);

void logfmt_status_panel(bool pool_ok, const char *pool_status,
	const char *worker, const char *algo,
	const char *rate_now, const char *rate_1m, const char *rate_15m,
	unsigned accepted, unsigned total, double accept_pct,
	float temp_c, int threads, int phys_cpus, long uptime_sec);

void logfmt_new_block(const char *source, const char *algo, uint32_t height,
	const char *detail);

/* Pool accepted a share that met the network (block) target. */
void logfmt_block_found(uint32_t height, double share_diff, double network_diff);

#endif /* VERIUM_LOGFMT_H */
