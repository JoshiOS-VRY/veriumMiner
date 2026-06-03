Tippy Verium Miner
==================

A multi-threaded CPU miner for **Verium** using the scrypt² ("VeriHash")
proof-of-work algorithm.

This is the **Tippy** maintained fork of
[FireWorm71/veriumMiner](https://github.com/fireworm71/veriumMiner), which
focused the original Verium miner on scrypt²-only operation. That line traces
back through Vericoin/Verium Reserve work to
[tpruvot/cpuminer-multi](https://github.com/tpruvot/cpuminer-multi) (see
`AUTHORS` for the full contributor list).

**Maintainer:** [JoshiOS-VRY/veriumMiner](https://github.com/JoshiOS-VRY/veriumMiner)
(`tippy-verium-miner` branch) — **tippy** &lt;tippytech@gmail.com&gt;

The shipped binary is still named `cpuminer` for compatibility with existing
configs and pool scripts.

There are **no prebuilt release binaries** yet — install by cloning this repo and
building with CMake (see [Quick start](#quick-start)).

#### Table of contents

* [Quick start](#quick-start)
* [What's new in Tippy](#whats-new-in-tippy)
* [Supported platforms](#supported-platforms)
* [Dependencies](#dependencies)
* [Build](#build)
* [Build options](#build-options)
* [Usage](#usage)
* [Hash regression tests](#hash-regression-tests)
* [License](#license)


What's new in Tippy
-------------------

This fork modernizes and hardens the FireWorm71 tree for day-to-day mining and
operations:

* **CMake** build on all platforms (replaces autotools + legacy Visual Studio)
* **CI** on every push: Linux (x86_64 GCC/Clang, ARM64), macOS (Apple Silicon +
  Intel via `macos-15-intel`), Windows (MSYS2), and FreeBSD — plus hash
  regression tests and `--cputest`
* **No OpenSSL** — hashing uses the in-tree scrypt² core only
* **Dead algorithms removed** — CryptoNight/Monero and other non-Verium code
  stripped out
* **CPU topology** — auto thread count (`-t 0`) from physical cores, L3 cache,
  and RAM budget; topology-aware affinity binding
* **Pool failover** — `--backup-url` with backoff and jitter
* **Monitoring API** — `summary`, `json`, `health`, and Prometheus-style
  `metrics` on port 4048
* **Setup wizard** — `--setup` writes a starter `cpuminer-conf.json`
* **Portable macOS / ARM64** — Apple Silicon and Linux ARM64 use the validated C
  scrypt core (assembly is x86/x86-64 and 32-bit ARM only)

See [`docs/AUDIT_IMPLEMENTATION.md`](docs/AUDIT_IMPLEMENTATION.md) for the full
audit-to-code mapping.


Quick start
-----------

### 1. Get the source

```sh
git clone https://github.com/JoshiOS-VRY/veriumMiner.git
cd veriumMiner
git checkout tippy-verium-miner   # Tippy fork branch (use main if that is default)
```

### 2. Build

Linux (Debian/Ubuntu example):

```sh
sudo apt-get install -y cmake build-essential libcurl4-openssl-dev libjansson-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Other platforms: see [Build](#build) below.

Optional install to `/usr/local/bin`:

```sh
cmake --install build
# then run: cpuminer ...
```

### 3. Configure and run

**First run (wizard)** — writes `~/.cpuminer/cpuminer-conf.json` (or the Windows
path below):

```sh
./build/cpuminer --setup
./build/cpuminer
```

**One-shot from the command line** (no config file):

```sh
./build/cpuminer -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x -t 0
```

**Vericonomy official pool** ([mine.vericonomy.com](https://mine.vericonomy.com)):

```sh
./build/cpuminer -o stratum+tcp://mine.vericonomy.com:3333 -u VYourAddress.worker1 -p x -t 0
```

Use your Verium address (starts with `V`) and an optional worker label after the dot.
Pool dashboard: `https://mine.vericonomy.com/miner/VYourAddress`.

`-t 0` picks a thread count from CPU topology and cache size. Use `-t 4` (etc.)
to set it manually. The miner exits with an error if no pool URL is given and no
config file is found.

**Example config** — copy and edit [`cpuminer-conf.json`](cpuminer-conf.json),
then:

```sh
./build/cpuminer -c /path/to/cpuminer-conf.json
```

On Windows, use `.\build\cpuminer.exe` and put MinGW on `PATH` as in
[Windows](#windows-msys2--mingw-w64).


Supported platforms
-------------------

| Platform | CI job | Notes |
|----------|--------|-------|
| Linux x86_64 | Linux x86_64 (GCC/Clang) | SSE2/AVX/AVX2 assembly when enabled |
| Linux ARM64 | Linux ARM64 (GCC) | Portable C core |
| macOS Apple Silicon | macOS Apple Silicon | Portable C core (`macos-14`) |
| macOS Intel | macOS Intel | Portable C core (`macos-15-intel`) |
| Windows x86_64 | windows-mingw64 | MSYS2 / MinGW-w64 |
| FreeBSD x86_64 | freebsd-x86_64 | VM-based CI |

**Version:** 1.4.0 (see `CMakeLists.txt`).


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
.\build\cpuminer.exe -o stratum+tcp://mine.vericonomy.com:3333 -u VYourAddress.worker1 -p x
```

### FreeBSD

```sh
pkg install -y cmake curl jansson
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Docker

```sh
docker build -t veriumminer .
docker run --rm veriumminer -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x
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

On x86-64 Linux and Windows, the miner detects CPU capabilities at runtime:

* SSE2 → 1-way scrypt
* AVX  → 3-way scrypt
* AVX2 → 6-way scrypt

No manual flags are needed to choose between them. macOS and ARM64 always use
the portable C implementation.


Usage
=====

```sh
./build/cpuminer -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x -t <threads>
```

**Default pool (Vericonomy):** `stratum+tcp://mine.vericonomy.com:3333` — username
`VRM_ADDRESS.workerName`, password `x` (any value). See [Quick start](#quick-start)
for a copy-paste example.

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
panel (`HPM`, `HPM_AVG60`, `HPM_AVG900` in the `summary` API).

### Config file location

| Platform | Default path |
|----------|--------------|
| Linux / macOS / FreeBSD | `~/.cpuminer/cpuminer-conf.json` |
| Windows | `%APPDATA%\cpuminer\cpuminer-conf.json` |

If no file exists at that path, the miner looks for `cpuminer-conf.json` next
to the executable. Use `--setup` or `-c` to point at a custom file.

### Monitoring API (port 4048)

Default bind: `127.0.0.1:4048` (override with `-b` / `"api-bind"` in config).
Send a command name on one line; the miner replies and closes the connection.

| Command | Format | Hashrate fields |
|---------|--------|-----------------|
| `summary` | `KEY=value;...` | `HPM`, `HPM_AVG60`, `HPM_AVG900` (hashes per **minute**) |
| `json` | JSON object | `hashrate_hps`, `hashrate_ema_60s`, `hashrate_ema_900s` (per **second**) |
| `health` | `KEY=value;...` | pool/accept/temp probe |
| `metrics` | Prometheus text | `verium_hashrate_hps`, `verium_hashrate_ema60`, … |

Examples:

```sh
printf 'summary\n' | nc -w 2 127.0.0.1 4048
printf 'json\n'    | nc -w 2 127.0.0.1 4048
```

On macOS, `nc` is available; use the same `printf` form (BSD `nc` does not accept
`echo ... | nc` the same way on all versions).

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


Fork lineage
============

```
tpruvot/cpuminer-multi
    └── Verium / Vericoin community (scrypt² focus)
            └── fireworm71/veriumMiner
                    └── JoshiOS-VRY/veriumMiner  ← Tippy Verium Miner (this tree)
```

To track upstream fixes from FireWorm71:

```sh
git remote add upstream https://github.com/fireworm71/veriumMiner.git
git fetch upstream
```


License
=======
GPLv2. See `COPYING` for details.
