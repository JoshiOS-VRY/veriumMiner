/*
 * User-friendly log formatting for Verium Miner.
 */
#include "logfmt.h"

#include <stdio.h>
#include <string.h>

#include "miner.h"
#include "stats.h"

extern bool use_colors;
extern uint32_t solved_count;
extern double net_diff;

static int g_color_theme = LOGFMT_THEME_AUTO;
static int g_color_theme_eff = LOGFMT_THEME_DARK;

static bool logfmt_theme_is_light(void)
{
	return use_colors && g_color_theme_eff == LOGFMT_THEME_LIGHT;
}

int logfmt_color_theme_parse(const char *mode)
{
	if (!mode || !mode[0])
		return -1;
	if (!strcasecmp(mode, "auto"))
		return LOGFMT_THEME_AUTO;
	if (!strcasecmp(mode, "dark"))
		return LOGFMT_THEME_DARK;
	if (!strcasecmp(mode, "light"))
		return LOGFMT_THEME_LIGHT;
	if (!strcasecmp(mode, "off") || !strcasecmp(mode, "none"))
		return LOGFMT_THEME_OFF;
	return -1;
}

void logfmt_set_color_theme(int theme)
{
	if (theme >= LOGFMT_THEME_AUTO && theme <= LOGFMT_THEME_OFF)
		g_color_theme = theme;
}

void logfmt_apply_color_theme(bool force_plain)
{
	const char *no_color = getenv("NO_COLOR");
	const char *force_color = getenv("FORCE_COLOR");

	if (force_plain || g_color_theme == LOGFMT_THEME_OFF) {
		use_colors = false;
		return;
	}
	if (no_color && no_color[0] &&
	    !(force_color && force_color[0] && force_color[0] != '0')) {
		use_colors = false;
		return;
	}
	use_colors = true;
	g_color_theme_eff = (g_color_theme == LOGFMT_THEME_LIGHT)
		? LOGFMT_THEME_LIGHT : LOGFMT_THEME_DARK;
}

const char *logfmt_ts_color(void)
{
	if (!use_colors)
		return "";
	return logfmt_theme_is_light() ? CL_BL2 : CL_GRY;
}

const char *logfmt_msg_color(int prio)
{
	if (!use_colors)
		return "";
	if (prio == LOG_BLUE)
		prio = LOG_NOTICE;
	if (logfmt_theme_is_light()) {
		switch (prio) {
		case LOG_ERR:     return CL_RD2;
		case LOG_WARNING: return CL_BRW;
		case LOG_INFO:    return CL_BL2;
		case LOG_DEBUG:   return CL_BLK;
		default:          return "";
		}
	}
	switch (prio) {
	case LOG_ERR:     return CL_WHT;
	case LOG_WARNING: return CL_YL2;
	case LOG_NOTICE:  return CL_SIL;
	case LOG_INFO:    return CL_CY2;
	case LOG_DEBUG:   return CL_GRY;
	default:          return "";
	}
}

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
	if (logfmt_theme_is_light()) {
		switch (prio) {
		case LOG_ERR:     return CL_RD2;
		case LOG_WARNING: return CL_BRW;
		case LOG_INFO:    return CL_BL2;
		case LOG_DEBUG:   return CL_BLK;
		case LOG_BLUE:    return CL_BL2;
		default:          return CL_BLK;
		}
	}
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

void logfmt_diff_decimal(double diff, char *buf, size_t bufsz)
{
	size_t len;

	if (!buf || !bufsz)
		return;
	snprintf(buf, bufsz, "%.12f", diff);
	len = strlen(buf);
	while (len > 0 && buf[len - 1] == '0')
		buf[--len] = '\0';
	if (len > 0 && buf[len - 1] == '.')
		buf[--len] = '\0';
	if (len == 0)
		snprintf(buf, bufsz, "0");
}

void logfmt_share(bool accepted, bool block_share,
	unsigned long accepted_n, unsigned long total_n,
	const char *suppl, const char *rate_hpm, const char *extra)
{
	const char *kind;
	const char *verb = accepted ? "accepted" : "rejected";

	if (g_solo_mining) {
		if (block_share)
			return; /* logfmt_block_found covers solo block submits */
		kind = "Submit";
	} else {
		kind = block_share ? "Block" : "Share";
	}

	if (use_colors) {
		const char *ok = logfmt_theme_is_light() ? CL_GR2 : CL_LGR;
		const char *bad = logfmt_theme_is_light() ? CL_RD2 : CL_LRD;
		const char *rate = logfmt_theme_is_light() ? CL_BLK : CL_WHT;

		if (extra && extra[0])
			applog(LOG_NOTICE, "%s%s %s%s · %lu/%lu (%s)%s · %s%s%s",
				accepted ? ok : bad, kind, verb, CL_N,
				accepted_n, total_n, suppl ? suppl : "-", extra,
				rate, rate_hpm ? rate_hpm : "-", CL_N);
		else
			applog(LOG_NOTICE, "%s%s %s%s · %lu/%lu (%s) · %s%s%s",
				accepted ? ok : bad, kind, verb, CL_N,
				accepted_n, total_n, suppl ? suppl : "-",
				rate, rate_hpm ? rate_hpm : "-", CL_N);
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

void logfmt_status_panel(bool solo, bool pool_ok, const char *pool_status,
	const char *worker, const char *algo,
	const char *rate_now, const char *rate_1m, const char *rate_15m,
	unsigned accepted, unsigned total, double accept_pct,
	float temp_c, int threads, int phys_cpus, long uptime_sec)
{
	char worker_short[96];
	char uptime[32];
	const char *kind = solo ? "Solo" : "Pool";
	const char *who = solo ? "payout" : "worker";
	const char *conn_label = pool_ok ? "connected" : pool_status;
	const char *conn_dot = pool_ok
		? (use_colors ? (logfmt_theme_is_light() ? CL_GR2 "●" CL_N : CL_LGR "●" CL_N) : "*")
		: (use_colors ? (logfmt_theme_is_light() ? CL_BRW "○" CL_N : CL_YL2 "○" CL_N) : "o");

	logfmt_ellipsis(worker && worker[0] ? worker : "-", worker_short, sizeof(worker_short), 28);
	logfmt_uptime(uptime_sec, uptime, sizeof(uptime));

	if (solo) {
		char diffbuf[32];

		logfmt_diff_decimal(net_diff, diffbuf, sizeof(diffbuf));
		if (temp_c < 0.0f) {
			applog(LOG_BLUE,
				"%s %s %s · %s · %s %s · height %u",
				kind, conn_dot, conn_label, algo, who, worker_short,
				(unsigned)g_solo_height);
			if (net_diff > 0.)
				applog(LOG_BLUE,
					"%s now · 1m %s · 15m %s · blocks found %u · net diff %s · workers %d · %d CPUs · up %s",
					rate_now, rate_1m, rate_15m,
					(unsigned)solved_count, diffbuf,
					threads, phys_cpus, uptime);
			else
				applog(LOG_BLUE,
					"%s now · 1m %s · 15m %s · blocks found %u · workers %d · %d CPUs · up %s",
					rate_now, rate_1m, rate_15m,
					(unsigned)solved_count,
					threads, phys_cpus, uptime);
		} else {
			applog(LOG_BLUE,
				"%s %s %s · %s · %s %s · height %u · %.0f°C",
				kind, conn_dot, conn_label, algo, who, worker_short,
				(unsigned)g_solo_height, (double)temp_c);
			if (net_diff > 0.)
				applog(LOG_BLUE,
					"%s now · 1m %s · 15m %s · blocks found %u · net diff %s · workers %d · %d CPUs · up %s",
					rate_now, rate_1m, rate_15m,
					(unsigned)solved_count, diffbuf,
					threads, phys_cpus, uptime);
			else
				applog(LOG_BLUE,
					"%s now · 1m %s · 15m %s · blocks found %u · workers %d · %d CPUs · up %s",
					rate_now, rate_1m, rate_15m,
					(unsigned)solved_count,
					threads, phys_cpus, uptime);
		}
		return;
	}

	if (temp_c < 0.0f) {
		applog(LOG_BLUE,
			"%s %s %s · %s · %s %s",
			kind, conn_dot, conn_label, algo, who, worker_short);
		applog(LOG_BLUE,
			"%s now · 1m %s · 15m %s · shares %u/%u (%.1f%%) · workers %d · %d CPUs · up %s",
			rate_now, rate_1m, rate_15m,
			accepted, total, accept_pct,
			threads, phys_cpus, uptime);
		return;
	}

	applog(LOG_BLUE,
		"%s %s %s · %s · %s %s · %.0f°C",
		kind, conn_dot, conn_label, algo, who, worker_short, (double)temp_c);
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
	const char *dest = g_solo_mining ? "node" : "pool";
	char sharebuf[32], netbuf[32];

	logfmt_diff_decimal(share_diff, sharebuf, sizeof(sharebuf));
	logfmt_diff_decimal(network_diff, netbuf, sizeof(netbuf));

	if (use_colors) {
		const char *banner = logfmt_theme_is_light() ? CL_GR2 : CL_LGR;

		applog(LOG_NOTICE,
			"%s━━━ BLOCK CANDIDATE ━━━%s  Thread %d found a network-target hash%s",
			banner, CL_N, thr_id, h);
		if (height)
			applog(LOG_NOTICE,
				"    Height %u · share diff %s · network diff %s · submitting to %s",
				(unsigned)height, sharebuf, netbuf, dest);
		else
			applog(LOG_NOTICE,
				"    Share diff %s · network diff %s · submitting to %s",
				sharebuf, netbuf, dest);
	} else {
		applog(LOG_NOTICE,
			"BLOCK CANDIDATE — thread %d found a network-target hash%s",
			thr_id, h);
		if (height)
			applog(LOG_NOTICE,
				"Height %u · share diff %s · network diff %s · submitting to %s",
				(unsigned)height, sharebuf, netbuf, dest);
		else
			applog(LOG_NOTICE,
				"Share diff %s · network diff %s · submitting to %s",
				sharebuf, netbuf, dest);
	}
}

void logfmt_block_found(uint32_t height, double share_diff, double network_diff)
{
	const char *h = height ? "" : " (height pending)";
	char sharebuf[32], netbuf[32];

	logfmt_diff_decimal(share_diff, sharebuf, sizeof(sharebuf));
	logfmt_diff_decimal(network_diff, netbuf, sizeof(netbuf));

	if (g_solo_mining) {
		if (use_colors) {
			const char *banner = logfmt_theme_is_light() ? CL_GR2 : CL_LGR;

			applog(LOG_NOTICE,
				"%s━━━ BLOCK FOUND ━━━%s  Node accepted your block%s",
				banner, CL_N, h);
			if (height)
				applog(LOG_NOTICE,
					"    Height %u · share diff %s · network diff %s",
					(unsigned)height, sharebuf, netbuf);
			else
				applog(LOG_NOTICE,
					"    Share diff %s · network diff %s",
					sharebuf, netbuf);
			applog(LOG_NOTICE,
				"    Reward pays to your --coinbase-addr; matures in your wallet after confirmations.");
		} else {
			applog(LOG_NOTICE,
				"BLOCK FOUND — node accepted your block%s", h);
			if (height)
				applog(LOG_NOTICE,
					"Height %u · share diff %s · network diff %s",
					(unsigned)height, sharebuf, netbuf);
			else
				applog(LOG_NOTICE,
					"Share diff %s · network diff %s",
					sharebuf, netbuf);
			applog(LOG_NOTICE,
				"Reward pays to your --coinbase-addr; matures in your wallet after confirmations.");
		}
		return;
	}

	if (use_colors) {
		const char *banner = logfmt_theme_is_light() ? CL_GR2 : CL_LGR;

		applog(LOG_NOTICE,
			"%s━━━ BLOCK SHARE ACCEPTED ━━━%s  Pool will submit this block to the network%s",
			banner, CL_N, h);
		if (height)
			applog(LOG_NOTICE,
				"    Height %u · share diff %s · network diff %s",
				(unsigned)height, sharebuf, netbuf);
		else
			applog(LOG_NOTICE,
				"    Share diff %s · network diff %s",
				sharebuf, netbuf);
		applog(LOG_NOTICE,
			"    Reward matures after pool confirmations; track status on the pool blocks page.");
	} else {
		applog(LOG_NOTICE,
			"BLOCK SHARE ACCEPTED — pool will submit this block to the network%s",
			h);
		if (height)
			applog(LOG_NOTICE,
				"Height %u · share diff %s · network diff %s",
				(unsigned)height, sharebuf, netbuf);
		else
			applog(LOG_NOTICE,
				"Share diff %s · network diff %s",
				sharebuf, netbuf);
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
	if (g_solo_mining) {
		if (extra && extra[0])
			applog(LOG_BLUE, "New template #%u · %s · %s · %s",
				(unsigned)height, algo, source ? source : "-", extra);
		else
			applog(LOG_BLUE, "New template #%u · %s · %s",
				(unsigned)height, algo, source ? source : "-");
		return;
	}
	if (extra && extra[0])
		applog(LOG_BLUE, "New block #%u · %s · %s · %s",
			(unsigned)height, algo, source ? source : "-", extra);
	else
		applog(LOG_BLUE, "New block #%u · %s · %s",
			(unsigned)height, algo, source ? source : "-");
}
