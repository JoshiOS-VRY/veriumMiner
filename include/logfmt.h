#ifndef VERIUM_LOGFMT_H
#define VERIUM_LOGFMT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define LOGFMT_THEME_AUTO  0
#define LOGFMT_THEME_DARK  1
#define LOGFMT_THEME_LIGHT 2
#define LOGFMT_THEME_OFF   3

/* Parse theme name; returns LOGFMT_THEME_* or -1 if unknown. */
int logfmt_color_theme_parse(const char *mode);
void logfmt_set_color_theme(int theme);
/* Apply theme to global use_colors (respects NO_COLOR, --log-file, -B, -S). */
void logfmt_apply_color_theme(bool force_plain);

/* Short level tag + color for applog() prefix (5-char tag, NUL-terminated). */
const char *logfmt_tag(int prio);
const char *logfmt_tag_color(int prio);
const char *logfmt_ts_color(void);
const char *logfmt_msg_color(int prio);

/* Human-readable uptime, e.g. "32m 16s", "2h 05m". */
void logfmt_uptime(long seconds, char *buf, size_t bufsz);

/* Middle-ellipsis for long wallet/worker names. */
void logfmt_ellipsis(const char *src, char *out, size_t outsz, size_t max_visible);

/* Fixed decimal difficulty (e.g. 0.00006633), never scientific notation. */
void logfmt_diff_decimal(double diff, char *buf, size_t bufsz);

/* High-frequency miner events (use applog internally). */
void logfmt_share(bool accepted, bool block_share,
	unsigned long accepted_n, unsigned long total_n,
	const char *suppl, const char *rate_hpm, const char *extra);

void logfmt_status_panel(bool solo, bool pool_ok, const char *pool_status,
	const char *worker, const char *algo,
	const char *rate_now, const char *rate_1m, const char *rate_15m,
	unsigned accepted, unsigned total, double accept_pct,
	float temp_c, int threads, int phys_cpus, long uptime_sec);

void logfmt_new_block(const char *source, const char *algo, uint32_t height,
	const char *detail);

/* Local hash meets network target; share is being submitted to the pool. */
void logfmt_block_candidate(int thr_id, uint32_t height, double share_diff,
	double network_diff);

/* Pool accepted a share that met the network (block) target. */
void logfmt_block_found(uint32_t height, double share_diff, double network_diff);

#endif /* VERIUM_LOGFMT_H */
