#ifndef VERIUM_STATS_H
#define VERIUM_STATS_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

extern pthread_mutex_t stats_lock;

void stats_init(void);
void stats_set_thread_count(int n);
void stats_set_status_interval(int seconds);
/* log-frequency preset: ultra=5s, fast=15s, medium=30s, slow=60s; -1 if unknown */
int stats_log_frequency_seconds(const char *mode);

/* `hashes_per_sec` is instantaneous from the last scanhash batch. */
void stats_record_hashrate(int thr_id, double hashes_per_sec);
void stats_record_share(bool accepted);

double stats_total_hps(void);
double stats_ema_60s(void);
double stats_ema_900s(void);
double stats_accept_pct(void);

/* Snapshot the accepted/rejected share counters owned by stats.c. */
void stats_get_shares(uint32_t *accepted, uint32_t *rejected);

/* Format hashes/sec as human-readable hashes/min (H/m). */
void stats_format_hpm(double hps, char *buf, size_t bufsz);

/* Record lightweight memory-health signals (latest stratum receive buffer
 * capacity/usage and work I/O queue depth). Cheap; safe to call frequently. */
void stats_record_mem(size_t sockbuf_cap, size_t sockbuf_used, int workio_qdepth);

/* Periodic status panel (TUI-lite) when enabled. */
void stats_maybe_print_panel(bool force);

/* JSON blob for API /metrics (caller frees). */
char *stats_json_summary(void);

extern time_t g_miner_start_time;
extern volatile int g_pool_connected;
extern char g_pool_status[64];
extern char g_worker_name[128];
extern int g_solo_mining;
extern uint32_t g_solo_height;

void stats_set_solo_height(uint32_t height);

#endif /* VERIUM_STATS_H */
