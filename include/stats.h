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

/* `hashes_per_sec` is instantaneous from the last scanhash batch. */
void stats_record_hashrate(int thr_id, double hashes_per_sec);
void stats_record_share(bool accepted);

double stats_total_hps(void);
double stats_ema_60s(void);
double stats_ema_900s(void);
double stats_accept_pct(void);

/* Format hashes/sec as human-readable hashes/min (H/m). */
void stats_format_hpm(double hps, char *buf, size_t bufsz);

/* Periodic status panel (TUI-lite) when enabled. */
void stats_maybe_print_panel(bool force);

/* JSON blob for API /metrics (caller frees). */
char *stats_json_summary(void);

extern time_t g_miner_start_time;
extern volatile int g_pool_connected;
extern char g_pool_status[64];
extern char g_worker_name[128];

#endif /* VERIUM_STATS_H */
