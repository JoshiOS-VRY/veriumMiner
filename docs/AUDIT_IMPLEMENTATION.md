# Audit implementation status

This document maps the Verium Miner audit findings to the code changes that
implement them. (The original audit canvas lived outside the repository; this
file is the in-tree record of what was addressed.)

## Completed

| ID | Area | Implementation |
|----|------|----------------|
| P1/P2 | Performance | `topo.c`: physical-core detection, L3/RAM budget, auto `-t 0`, oversubscription warning |
| P3/P4 | Performance | Topology-aware `topo_bind_worker()`, `topo_bind_process_mask()` (replaces `unsigned long` per-thread mask) |
| P5 | Performance | Windows `MEM_LARGE_PAGES`, macOS `posix_memalign`, Linux THP retained |
| P7/P8 | Performance | `--profile dedicated` skips idle scheduler policy |
| U1 | UX | `--setup` wizard (`onboard.c`), sample `cpuminer-conf.json` |
| U2 | UX | Help text trimmed; dead neoscrypt/gbt options removed from help |
| U3 | UX | `stats_format_khs()` / unified kH/s in console and API |
| U4 | UX | EMA hashrate 60s and 900s in `stats.c` |
| U5/U7 | UX | Periodic status panel (`stats_maybe_print_panel`), API `json`/`health`/`metrics` |
| U6 | UX | Linux hwmon scan across `/sys/class/hwmon/*` |
| U8 | UX | Reject-rate warning in `share_result()` |
| R1 | Reliability | Pool list + failover (`pools.c`, `--backup-url`) |
| R2 | Reliability | Backoff+jitter (`pools_backoff_seconds`), default pause 30s |
| R3/R4 | Reliability | Worker survives alloc/get_work failures (retry/disable vs `exit(1)`) |
| R5 | Reliability | `watchdog_thread`: stall detection + status tick |
| R6/R7 | Reliability | API `json`, `health`, `metrics`; structured summary fields |

## Deferred (consensus or scope)

| ID | Reason |
|----|--------|
| P6 | ARM64 NEON multi-way scrypt core requires golden-vector validation before merge; portable C core remains default on Apple/arm64 |
| AVX-512 | Extension point documented in `scrypt.c`; not enabled without validated core |
| U5 earnings | Pool-specific payout APIs differ; use pool web UI or future pool-plugin |
| Full TUI/ncurses | Status panel + API JSON cover MVP; dedicated TUI can build on `stats_json_summary()` |

## New files

- `src/topo.c`, `include/topo.h`
- `src/stats.c`, `include/stats.h`
- `src/pools.c`, `include/pools.h`
- `src/onboard.c`, `include/onboard.h`
