/*
 * Miner statistics: EMA hashrate, accept rate, status panel, JSON export.
 */
#include "stats.h"

extern uint32_t solved_count;
extern double net_diff;

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miner.h"
#include "logfmt.h"
#include "topo.h"

time_t g_miner_start_time;
volatile int g_pool_connected;
char g_pool_status[64] = "initializing";
char g_worker_name[128];
int g_solo_mining;
uint32_t g_solo_height;

void stats_set_solo_height(uint32_t height)
{
	g_solo_height = height;
}

static int g_n_threads;
static double g_thr_hps[TOPO_MAX_CPUS];
static double g_ema_60;
static double g_ema_900;
static time_t g_last_ema_tick;
static time_t g_last_panel;
static int g_status_interval = 30;

/* Authoritative share counters owned by stats.c (lock-guarded). */
static uint32_t g_shares_acc;
static uint32_t g_shares_rej;

/* Latest memory-health snapshot (log-only telemetry). */
static size_t g_mem_sockbuf_cap;
static size_t g_mem_sockbuf_used;
static int g_mem_workio_qdepth;

extern bool opt_debug;
extern int opt_n_threads;
extern double *thr_hashrates;
extern char *rpc_user;

/* Escape a string for safe embedding inside a JSON string literal. */
static void stats_json_escape(const char *src, char *dst, size_t dstsz)
{
	size_t di = 0;

	if (!dstsz)
		return;
	if (!src) {
		dst[0] = '\0';
		return;
	}
	for (; *src && di + 1 < dstsz; src++) {
		unsigned char c = (unsigned char)*src;
		if (c == '"' || c == '\\') {
			if (di + 2 >= dstsz)
				break;
			dst[di++] = '\\';
			dst[di++] = (char)c;
		} else if (c < 0x20) {
			if (di + 6 >= dstsz)
				break;
			di += (size_t)snprintf(dst + di, dstsz - di, "\\u%04x", c);
		} else {
			dst[di++] = (char)c;
		}
	}
	dst[di] = '\0';
}

void stats_init(void)
{
	g_miner_start_time = time(NULL);
	g_last_ema_tick = g_miner_start_time;
	g_last_panel = 0;
	g_ema_60 = g_ema_900 = 0.0;
	g_pool_connected = 0;
	snprintf(g_pool_status, sizeof(g_pool_status), "connecting");
	if (rpc_user)
		snprintf(g_worker_name, sizeof(g_worker_name), "%s", rpc_user);
}

void stats_set_thread_count(int n)
{
	g_n_threads = n;
}

void stats_set_status_interval(int seconds)
{
	if (seconds > 0)
		g_status_interval = seconds;
}

int stats_log_frequency_seconds(const char *mode)
{
	if (!mode || !mode[0])
		return -1;
	if (!strcasecmp(mode, "ultra"))
		return 5;
	if (!strcasecmp(mode, "fast"))
		return 15;
	if (!strcasecmp(mode, "medium"))
		return 30;
	if (!strcasecmp(mode, "slow"))
		return 60;
	return -1;
}

static void stats_update_ema_locked(double total_hps)
{
	time_t now = time(NULL);
	double dt = (double)(now - g_last_ema_tick);
	double a60, a900;

	if (dt < 0.1) {
		g_ema_60 = total_hps;
		g_ema_900 = total_hps;
		return;
	}
	g_last_ema_tick = now;
	a60 = 1.0 - exp(-dt * log(2.0) / 60.0);
	a900 = 1.0 - exp(-dt * log(2.0) / 900.0);
	if (g_ema_60 <= 0.0)
		g_ema_60 = total_hps;
	else
		g_ema_60 += a60 * (total_hps - g_ema_60);
	if (g_ema_900 <= 0.0)
		g_ema_900 = total_hps;
	else
		g_ema_900 += a900 * (total_hps - g_ema_900);
}

void stats_record_hashrate(int thr_id, double hashes_per_sec)
{
	double total = 0.0;
	int i;

	if (thr_id < 0 || thr_id >= TOPO_MAX_CPUS)
		return;

	pthread_mutex_lock(&stats_lock);
	g_thr_hps[thr_id] = hashes_per_sec;
	for (i = 0; i < g_n_threads; i++)
		total += g_thr_hps[i];
	stats_update_ema_locked(total);
	pthread_mutex_unlock(&stats_lock);
}

void stats_record_share(bool accepted)
{
	pthread_mutex_lock(&stats_lock);
	if (accepted)
		g_shares_acc++;
	else
		g_shares_rej++;
	pthread_mutex_unlock(&stats_lock);
}

void stats_record_mem(size_t sockbuf_cap, size_t sockbuf_used, int workio_qdepth)
{
	pthread_mutex_lock(&stats_lock);
	g_mem_sockbuf_cap = sockbuf_cap;
	g_mem_sockbuf_used = sockbuf_used;
	g_mem_workio_qdepth = workio_qdepth;
	pthread_mutex_unlock(&stats_lock);
}

double stats_total_hps(void)
{
	double total = 0.0;
	int i;
	pthread_mutex_lock(&stats_lock);
	for (i = 0; i < g_n_threads; i++)
		total += g_thr_hps[i];
	pthread_mutex_unlock(&stats_lock);
	return total;
}

double stats_ema_60s(void)
{
	double v;
	pthread_mutex_lock(&stats_lock);
	v = g_ema_60;
	pthread_mutex_unlock(&stats_lock);
	return v;
}

double stats_ema_900s(void)
{
	double v;
	pthread_mutex_lock(&stats_lock);
	v = g_ema_900;
	pthread_mutex_unlock(&stats_lock);
	return v;
}

double stats_accept_pct(void)
{
	uint32_t a, r;
	pthread_mutex_lock(&stats_lock);
	a = g_shares_acc;
	r = g_shares_rej;
	pthread_mutex_unlock(&stats_lock);
	if (a + r == 0)
		return 100.0;
	return 100.0 * (double)a / (double)(a + r);
}

void stats_get_shares(uint32_t *accepted, uint32_t *rejected)
{
	pthread_mutex_lock(&stats_lock);
	if (accepted)
		*accepted = g_shares_acc;
	if (rejected)
		*rejected = g_shares_rej;
	pthread_mutex_unlock(&stats_lock);
}

static void format_whole_commas(long long whole, char *out, size_t outsz)
{
	char digits[24];
	int len, i, out_i = 0;
	int next_comma;

	snprintf(digits, sizeof(digits), "%lld", whole);
	len = (int)strlen(digits);
	next_comma = len % 3;
	if (next_comma == 0)
		next_comma = 3;

	for (i = 0; i < len && out_i + 2 < (int)outsz; i++) {
		if (i == next_comma) {
			next_comma += 3;
			out[out_i++] = ',';
		}
		out[out_i++] = digits[i];
	}
	out[out_i] = '\0';
}

void stats_format_hpm(double hps, char *buf, size_t bufsz)
{
	char whole_buf[32];
	double hpm = hps * 60.0;
	long long whole;
	int cents;

	if (hpm >= 10000.0) {
		snprintf(buf, bufsz, "%.1fk H/m", hpm / 1000.0);
		return;
	}

	whole = (long long)hpm;
	cents = (int)floor((hpm - (double)whole) * 100.0 + 0.5);
	if (cents >= 100) {
		whole++;
		cents = 0;
	}

	format_whole_commas(whole, whole_buf, sizeof(whole_buf));
	snprintf(buf, bufsz, "%s.%02d H/m", whole_buf, cents);
}

void stats_maybe_print_panel(bool force)
{
	time_t now = time(NULL);
	char rate_now[32], rate_avg[32], rate_15m[32];
	char algo[64];
	float temp;
	const struct topo_info *tp;

	if (opt_quiet && !force)
		return;
	if (!force && g_last_panel && (now - g_last_panel) < g_status_interval)
		return;
	g_last_panel = now;

	size_t mem_cap, mem_used;
	int mem_qdepth;

	pthread_mutex_lock(&stats_lock);
	{
		double total = 0.0;
		int i;
		for (i = 0; i < g_n_threads; i++)
			total += g_thr_hps[i];
		stats_format_hpm(total, rate_now, sizeof(rate_now));
		stats_format_hpm(g_ema_60, rate_avg, sizeof(rate_avg));
		stats_format_hpm(g_ema_900, rate_15m, sizeof(rate_15m));
	}
	mem_cap = g_mem_sockbuf_cap;
	mem_used = g_mem_sockbuf_used;
	mem_qdepth = g_mem_workio_qdepth;
	pthread_mutex_unlock(&stats_lock);

	/* Memory-health telemetry stays on the debug channel to keep the normal
	 * panel uncluttered while still surfacing long-run buffer/queue drift. */
	if (opt_debug && mem_cap)
		applog(LOG_DEBUG,
			"mem: sockbuf %zu/%zu bytes, workio queue depth %d",
			mem_used, mem_cap, mem_qdepth);

	get_currentalgo(algo, sizeof(algo));
	temp = cpu_temp(0);
	tp = topo_get();

	uint32_t acc = 0, rej = 0;
	stats_get_shares(&acc, &rej);

	logfmt_status_panel(
		g_solo_mining != 0,
		g_pool_connected != 0,
		g_pool_status,
		g_worker_name[0] ? g_worker_name : "-",
		algo,
		rate_now, rate_avg, rate_15m,
		(unsigned)acc,
		(unsigned)(acc + rej),
		stats_accept_pct(),
		temp,
		g_n_threads,
		tp ? tp->physical_cpus : g_n_threads,
		(long)(now - g_miner_start_time));
}

char *stats_json_summary(void)
{
	char *buf;
	double total = stats_total_hps();
	double ema60 = stats_ema_60s();
	double ema900 = stats_ema_900s();
	time_t up = time(NULL) - g_miner_start_time;

	buf = (char *)malloc(2048);
	if (!buf)
		return NULL;
	{
		float temp = cpu_temp(0);
		uint32_t acc = 0, rej = 0;
		char pool_esc[128], worker_esc[256];

		stats_get_shares(&acc, &rej);
		stats_json_escape(g_pool_status, pool_esc, sizeof(pool_esc));
		stats_json_escape(g_worker_name, worker_esc, sizeof(worker_esc));

		if (temp < 0.0f) {
			snprintf(buf, 2048,
				"{\"hashrate_hps\":%.2f,\"hashrate_ema_60s\":%.2f,"
				"\"hashrate_ema_900s\":%.2f,\"accepted\":%u,\"rejected\":%u,"
				"\"accept_pct\":%.2f,\"uptime_sec\":%ld,\"pool_connected\":%d,"
				"\"pool_status\":\"%s\",\"worker\":\"%s\",\"temp_c\":null,"
				"\"threads\":%d}",
				total, ema60, ema900,
				(unsigned)acc, (unsigned)rej,
				stats_accept_pct(), (long)up, g_pool_connected,
				pool_esc, worker_esc, g_n_threads);
			return buf;
		}
		snprintf(buf, 2048,
			"{\"hashrate_hps\":%.2f,\"hashrate_ema_60s\":%.2f,"
			"\"hashrate_ema_900s\":%.2f,\"accepted\":%u,\"rejected\":%u,"
			"\"accept_pct\":%.2f,\"uptime_sec\":%ld,\"pool_connected\":%d,"
			"\"pool_status\":\"%s\",\"worker\":\"%s\",\"temp_c\":%.1f,"
			"\"threads\":%d}",
			total, ema60, ema900,
			(unsigned)acc, (unsigned)rej,
			stats_accept_pct(), (long)up, g_pool_connected,
			pool_esc, worker_esc, (double)temp, g_n_threads);
	}
	return buf;
}
