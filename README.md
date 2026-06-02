veriumMiner
===========

A multi-threaded CPU miner for **Verium** using the scrypt² ("VeriHash")
proof-of-work algorithm. Fork of [tpruvot](https://github.com/tpruvot)'s
cpuminer-multi (see `AUTHORS` for contributors).

This tree has been modernized for 2026: a single **CMake** build replaces the
old autotools/Visual Studio setup, the dead CryptoNight/Monero code has been
removed, the OpenSSL dependency is gone, and CI builds and hash-tests every
supported platform (x86-64, ARM64, Apple Silicon, Intel Mac, Windows, FreeBSD).

#### Table of contents

* [Dependencies](#dependencies)
* [Build](#build)
* [Build options](#build-options)
* [Usage](#usage)
* [Hash regression tests](#hash-regression-tests)
* [License](#license)


Dependencies
============
* A C11/C++11 compiler (GCC, Clang, AppleClang, or MinGW-w64)
* [CMake](https://cmake.org/) ≥ 3.16
* [libcurl](https://curl.se/libcurl/)
* [jansson](https://github.com/akheron/jansson) (auto-downloaded if not found)
* pthreads (provided by the toolchain on every supported platform)

OpenSSL is **not** required.


Build
=====

The build is the same on every platform:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The miner binary is `build/cpuminer` (`build/cpuminer.exe` on Windows).

### Linux (Debian/Ubuntu)

```sh
sudo apt-get install -y cmake build-essential libcurl4-openssl-dev libjansson-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The same commands work on ARMv7/ARMv8 and ARM64 (aarch64) Linux.

### macOS (Apple Silicon and Intel)

```sh
brew install cmake jansson curl
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$(brew --prefix jansson);$(brew --prefix curl)"
cmake --build build -j
```

This builds natively on both Apple Silicon (arm64) and Intel (x86_64). macOS
uses the portable C scrypt cores on all Macs (assembly is disabled on Apple
platforms). Runtime SIMD selection (SSE2/AVX/AVX2) applies on Linux and
Windows x86-64 builds only.

### Windows (MSYS2 / MinGW-w64)

Install [MSYS2](https://www.msys2.org/), then in the **MINGW64** shell:

```sh
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
                   mingw-w64-x86_64-ninja mingw-w64-x86_64-curl \
                   mingw-w64-x86_64-jansson
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

From PowerShell (without opening the MSYS2 shell):

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/mingw64/bin:`$PATH && cd /path/to/veriumMiner && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
```

Run the miner with MinGW DLLs on `PATH`:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
.\build\cpuminer.exe -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x
```

### FreeBSD

```sh
pkg install -y cmake curl jansson
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```


Build options
=============

Pass these with `-D<option>=ON|OFF` at configure time:

| Option         | Default | Description                                              |
|----------------|---------|----------------------------------------------------------|
| `USE_ASM`      | `ON`    | Use hand-written assembly cores (x86/x86-64/ARM32)       |
| `MARCH_NATIVE` | `OFF`   | Build with `-march=native` for a single specific machine |
| `ENABLE_LTO`   | `ON`    | Link-time optimization for Release builds                |
| `BUILD_TESTS`  | `ON`    | Build the hash regression tests                          |

Example, tuned for the local machine:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMARCH_NATIVE=ON
cmake --build build -j
```

### CPU feature selection

The miner detects CPU capabilities at runtime, so one binary runs everywhere:

* SSE2 → 1-way scrypt
* AVX  → 3-way scrypt
* AVX2 → 6-way scrypt

No manual flags are needed to choose between them.


Usage
=====

```sh
./build/cpuminer -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x -t <threads>
```

Run `./build/cpuminer --help` for the full list of options. Common ones:

* `-o, --url` — primary pool URL (`stratum+tcp://...`)
* `--backup-url` — comma-separated backup pools (automatic failover)
* `-u, --user` / `-p, --pass` — wallet/worker credentials
* `-t, --threads` — mining threads (`0` = auto from CPU topology and L3 cache)
* `--setup` — interactive wizard writes the default config file (see below)
* `--tune` — print recommended thread count at startup
* `--status-interval` — seconds between status summaries (hashrate, shares, temp)
* `--profile dedicated` — higher CPU priority for dedicated mining rigs
* `-c, --config` — JSON config file (see `cpuminer-conf.json`)

Hashrate is reported in **hashes per minute (H/m)** in logs and the status
panel. The monitoring API also exposes raw **hashes per second (H/s)** as
`hashrate_hps` for integrations.

### Config file location

| Platform | Default path |
|----------|--------------|
| Linux / macOS / FreeBSD | `~/.cpuminer/cpuminer-conf.json` |
| Windows | `%APPDATA%\cpuminer\cpuminer-conf.json` |

If no file exists at that path, the miner looks for `cpuminer-conf.json` next
to the executable. Use `--setup` or `-c` to point at a custom file.

### Monitoring API (port 4048)

The built-in API exposes:

* `summary` — classic key/value stats (includes 1m and 15m average hashrate)
* `json` — JSON snapshot for dashboards
* `health` — quick health probe
* `metrics` — Prometheus-style text metrics

Example: `echo summary | nc 127.0.0.1 4048`

### Connecting through a proxy

Use `--proxy`. To use a SOCKS proxy add a `socks4://` or `socks5://` prefix to
the host. With no prefix an HTTP proxy is assumed; when `--proxy` is not used,
the `http_proxy` / `all_proxy` environment variables are honored.


Hash regression tests
======================

scrypt² output is consensus-bound and must never change. The test harness
links the real hashing core and verifies determinism, non-null output, and a
locked golden digest:

```sh
ctest --test-dir build --output-on-failure
```

The empty-buffer test allocates about 3 GiB of scratch memory (N=1048576).
Ensure the machine has enough free RAM before running it.

You can also print the canonical empty-buffer digest directly:

```sh
./build/cpuminer --cputest
```

To lock the golden vector for your build, run `test_hash` once, copy the value
from its `RECORD:` line into [`tests/golden.h`](tests/golden.h), and rebuild.


License
=======
GPLv2. See `COPYING` for details.
