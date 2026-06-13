# Raspberry Pi and other AArch64 SBCs

Community-tested on **Raspberry Pi 5** (Verium Miner 1.4.7, arm64 build from source).

## Expected performance

| Metric            | Pi 5 (1 thread, NEON 3-way)                             |
| ----------------- | ------------------------------------------------------- |
| Hashrate          | Benchmark on device — DRAM bus often caps gain vs 1-way |
| Scrypt scratchpad | ~384 MB per default thread (NEON 3-way)                 |
| Temperature       | ~50 °C with light airflow                               |
| Pool shares       | Accepted at min difficulty 1e-8                         |

Extra mining threads **do not** increase hashrate on Pi 5: the shared DRAM bus
saturates with a single scrypt worker. Auto threads (`-t 0`) recommend **1**
on low-core AArch64 SBCs when scratchpad exceeds ~200 MB.

Use **`-t 1`** (or `"threads": 1` in config). If NEON 3-way does not beat the
portable core on your board, stay at one thread regardless.

## Build from source

```sh
git clone https://github.com/JoshiOS-VRY/veriumMiner.git
cd veriumMiner
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$(nproc)"
./build/cpuminer -V          # expect "ARMV8 NEON"
./build/cpuminer --selftest
./build/cpuminer --benchmark -t 1
ctest --test-dir build --output-on-failure
```

## Docker

Build the image on the Pi (or pull a published arm64 tag when available):

```sh
docker build -t veriumminer:local .
```

**Important:** add a `.dockerignore` (included in this repo) so a local `build/`
directory is not copied into the image. Host `CMakeCache.txt` paths break the
in-container CMake run.

Run once:

```sh
docker run --rm veriumminer:local \
  -o stratum+tcp://mine.vericonomy.com:3333 \
  -u VYourAddress.worker1 -p x -t 1
```

### Docker Compose (Portainer)

See [`contrib/docker/docker-compose.yml`](../contrib/docker/docker-compose.yml).
Set credentials via environment variables (`.env` or Portainer UI), not in the
compose file:

```env
MINER_POOL_URL=stratum+tcp://mine.vericonomy.com:3333
MINER_WALLET=VYourAddress.worker1
MINER_PASSWORD=x
MINER_THREADS=1
```

Resource limits used in community testing:

- **cpus:** `1.0` — matches the memory-bus bottleneck
- **memory:** `512M` — enough for one ~384 MB NEON scratchpad plus overhead

## Pool difficulty

The public pool minimum difficulty is `1e-8`. A Pi submitting ~150 H/m finds
shares on that order without issue. If a pool flags very slow workers as stale,
adjust pool-side minimum difficulty rather than raising Pi thread count.

## Not commercially competitive

A Pi is fine for learning, community participation, and low-power background
mining. Dedicated x86_64 servers with wide memory bandwidth are orders of
magnitude faster for serious hashrate.
