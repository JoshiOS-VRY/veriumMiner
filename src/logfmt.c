/*
 * User-friendly log formatting for Verium Miner.
 */
#include "logfmt.h"

#include <stdio.h>
#include <string.h>

#include "miner.h"

extern bool use_colors;

const char *logfmt_tag(int prio)
{
	switch (prio) {
	case LOG_ERR:     return " ERR ";
	case LOG_WARNING: return "WARN ";
	case LOG_INFO:    return "INFO ";
	case LOG_DEBUG:   return "DBG  ";
	case LOG_BLUE:    return "STAT ";
	default:          return "     ";
	}
}

const char *logfmt_tag_color(int prio)
{
	if (!use_colors)
		return "";
	switch (prio) {
	case LOG_ERR:     return CL_LRD;
	case LOG_WARNING: return CL_YL2;
	case LOG_INFO:    return CL_LCY;
	case LOG_DEBUG:   return CL_GRY;
	case LOG_BLUE:    return CL_LBL;
	default:          return CL_SIL;
	}
}

void logfmt_uptime(long seconds, char *buf, size_t bufsz)
{
	long h, m, s;

	if (seconds < 0)
		seconds = 0;
	h = seconds / 3600;
	m = (seconds % 3600) / 60;
	s = seconds % 60;

	if (h > 0)
		snprintf(buf, bufsz, "%ldh %02ldm", (long)h, (long)m);
	else if (m > 0)
		snprintf(buf, bufsz, "%ldm %02lds", (long)m, (long)s);
	else
		snprintf(buf, bufsz, "%lds", (long)s);
}

void logfmt_ellipsis(const char *src, char *out, size_t outsz, size_t max_visible)
{
	size_t len, head, tail;

	if (!src || !src[0]) {
		snprintf(out, outsz, "-");
		return;
	}
	len = strlen(src);
	if (len <= max_visible || max_visible < 7) {
		snprintf(out, outsz, "%s", src);
		return;
	}
	head = (max_visible - 3) / 2;
	tail = max_visible - 3 - head;
	snprintf(out, outsz, "%.*s...%s", (int)head, src, src + len - tail);
}

void logfmt_share(bool accepted, bool block_share,
	unsigned long accepted_n, unsigned long total_n,
	const char *suppl, const char *rate_hpm, const char *extra)
{
	const char *kind = block_share ? "Block" : "Share";
	const char *verb = accepted ? "accepted" : "rejected";

	if (use_colors) {
		if (extra && extra[0])
			applog(LOG_NOTICE, "%s%s %s%s · %lu/%lu (%s)%s · %s%s%s",
				accepted ? CL_LGR : CL_LRD, kind, verb, CL_N,
				accepted_n, total_n, suppl ? suppl : "-", extra,
				CL_WHT, rate_hpm ? rate_hpm : "-", CL_N);
		else
			applog(LOG_NOTICE, "%s%s %s%s · %lu/%lu (%s) · %s%s%s",
				accepted ? CL_LGR : CL_LRD, kind, verb, CL_N,
				accepted_n, total_n, suppl ? suppl : "-",
				CL_WHT, rate_hpm ? rate_hpm : "-", CL_N);
	} else {
		if (extra && extra[0])
			applog(LOG_NOTICE, "%s %s · %lu/%lu (%s)%s · %s",
				kind, verb, accepted_n, total_n,
				suppl ? suppl : "-", extra, rate_hpm ? rate_hpm : "-");
		else
			applog(LOG_NOTICE, "%s %s · %lu/%lu (%s) · %s",
				kind, verb, accepted_n, total_n,
				suppl ? suppl : "-", rate_hpm ? rate_hpm : "-");
	}
}

void logfmt_status_panel(bool pool_ok, const char *pool_status,
	const char *worker, const char *algo,
	const char *rate_now, const char *rate_1m, const char *rate_15m,
	unsigned accepted, unsigned total, double accept_pct,
	float temp_c, int threads, int phys_cpus, long uptime_sec)
{
	char worker_short[96];
	char uptime[32];
	const char *pool_label = pool_ok ? "connected" : pool_status;
	const char *pool_dot = pool_ok ? (use_colors ? CL_LGR "●" CL_N : "*") : (use_colors ? CL_YL2 "○" CL_N : "o");

	logfmt_ellipsis(worker && worker[0] ? worker : "-", worker_short, sizeof(worker_short), 28);
	logfmt_uptime(uptime_sec, uptime, sizeof(uptime));

	if (temp_c < 0.0f) {
		applog(LOG_BLUE,
			"Pool %s %s · %s · worker %s",
			pool_dot, pool_label, algo, worker_short);
		applog(LOG_BLUE,
			"%s now · 1m %s · 15m %s · shares %u/%u (%.1f%%) · workers %d · %d CPUs · up %s",
			rate_now, rate_1m, rate_15m,
			accepted, total, accept_pct,
			threads, phys_cpus, uptime);
		return;
	}

	applog(LOG_BLUE,
		"Pool %s %s · %s · worker %s · %.0f°C",
		pool_dot, pool_label, algo, worker_short, (double)temp_c);
	applog(LOG_BLUE,
		"%s now · 1m %s · 15m %s · shares %u/%u (%.1f%%) · workers %d · %d CPUs · up %s",
		rate_now, rate_1m, rate_15m,
		accepted, total, accept_pct,
		threads, phys_cpus, uptime);
}

void logfmt_block_candidate(int thr_id, uint32_t height, double share_diff,
	double network_diff)
{
	const char *h = height ? "" : " (height pending)";

	if (use_colors) {
		applog(LOG_NOTICE,
			"%s━━━ BLOCK CANDIDATE ━━━%s  Thread %d found a network-target hash%s",
			CL_LGR, CL_N, thr_id, h);
		if (height)
			applog(LOG_NOTICE,
				"    Height %u · share diff %.6g · network diff %.6g · submitting to pool",
				(unsigned)height, share_diff, network_diff);
		else
			applog(LOG_NOTICE,
				"    Share diff %.6g · network diff %.6g · submitting to pool",
				share_diff, network_diff);
	} else {
		applog(LOG_NOTICE,
			"BLOCK CANDIDATE — thread %d found a network-target hash%s",
			thr_id, h);
		if (height)
			applog(LOG_NOTICE,
				"Height %u · share diff %.6g · network diff %.6g · submitting to pool",
				(unsigned)height, share_diff, network_diff);
		else
			applog(LOG_NOTICE,
				"Share diff %.6g · network diff %.6g · submitting to pool",
				share_diff, network_diff);
	}
}

void logfmt_block_found(uint32_t height, double share_diff, double network_diff)
{
	const char *h = height ? "" : " (height pending)";

	if (use_colors) {
		applog(LOG_NOTICE,
			"%s━━━ BLOCK SHARE ACCEPTED ━━━%s  Pool will submit this block to the network%s",
			CL_LGR, CL_N, h);
		if (height)
			applog(LOG_NOTICE,
				"    Height %u · share diff %.6g · network diff %.6g",
				(unsigned)height, share_diff, network_diff);
		else
			applog(LOG_NOTICE,
				"    Share diff %.6g · network diff %.6g",
				share_diff, network_diff);
		applog(LOG_NOTICE,
			"    Reward matures after pool confirmations; track status on the pool blocks page.");
	} else {
		applog(LOG_NOTICE,
			"BLOCK SHARE ACCEPTED — pool will submit this block to the network%s",
			h);
		if (height)
			applog(LOG_NOTICE,
				"Height %u · share diff %.6g · network diff %.6g",
				(unsigned)height, share_diff, network_diff);
		else
			applog(LOG_NOTICE,
				"Share diff %.6g · network diff %.6g",
				share_diff, network_diff);
		applog(LOG_NOTICE,
			"Reward matures after pool confirmations; track status on the pool blocks page.");
	}
}

void logfmt_new_block(const char *source, const char *algo, uint32_t height,
	const char *detail)
{
	const char *extra = detail;

	if (extra) {
		while (*extra) {
			if (*extra == ' ' || *extra == ',')
				extra++;
			else if ((unsigned char)extra[0] == 0xC2 &&
			    (unsigned char)extra[1] == 0xB7)
				extra += 2;
			else
				break;
		}
	}
	if (extra && extra[0])
		applog(LOG_BLUE, "New block #%u · %s · %s · %s",
			(unsigned)height, algo, source ? source : "-", extra);
	else
		applog(LOG_BLUE, "New block #%u · %s · %s",
			(unsigned)height, algo, source ? source : "-");
}
