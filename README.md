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

**The fastest way to start mining is to download a prebuilt binary** from the
[Releases page](https://github.com/JoshiOS-VRY/veriumMiner/releases) — see
[Download & run](#download--run). Building from source is the advanced path.

#### Table of contents

* [Download & run](#download--run)
* [Quick start (build from source)](#quick-start-build-from-source)
* [Running headless / as a service](#running-headless--as-a-service)
* [What's new in Tippy](#whats-new-in-tippy)
* [Supported platforms](#supported-platforms)
* [Dependencies](#dependencies)
* [Build](#build)
* [Build options](#build-options)
* [Usage](#usage)
* [Troubleshooting](#troubleshooting)
* [Hash regression tests](#hash-regression-tests)
* [Security](#security)
* [Contributing](#contributing)
* [License](#license)


Download & run
--------------

Prebuilt, checksummed binaries are published for every release. Verify the
download against `SHA256SUMS` before running.

1. Go to the [Releases page](https://github.com/JoshiOS-VRY/veriumMiner/releases)
   and download the archive for your platform:
   * `veriumminer-<ver>-windows-x86_64.zip` — single self-contained
     `cpuminer.exe` (no extra DLLs needed), plus an optional installer
     (`veriumminer-setup.exe`).
   * `veriumminer-<ver>-linux-x86_64.tar.gz` / `...-linux-arm64.tar.gz`
   * `veriumminer-<ver>-macos-arm64.tar.gz` / `...-macos-x86_64.tar.gz`
2. Extract it, open the folder, and read **`START.txt`**.

3. **First run must be the setup wizard** (creates `%APPDATA%\cpuminer\cpuminer-conf.json`):

```sh
# Windows (PowerShell): unzip, then
.\cpuminer.exe --setup
.\cpuminer.exe
```

On **Windows**, run `--setup` once before relying on double-click — `cpuminer.exe` alone
only works after you have a real config (not the example placeholder).

On **macOS**, double-click **`Verium Miner.command`** in the extracted folder (it
clears Gatekeeper quarantine and starts the miner). The first interactive run
launches the setup wizard and then starts mining; later runs use the saved config
in `~/.cpuminer/cpuminer-conf.json`. To change settings later, use **`Change
Settings.command`** or `./cpuminer --setup`.

# Linux
tar xzf veriumminer-*.tar.gz && cd veriumminer-*
./cpuminer --setup   # optional; first interactive run also runs the wizard
./cpuminer
```

Or one-shot against the official pool (run from the folder you extracted — no
`build/` path):

```powershell
# Windows (PowerShell or cmd, from the extracted folder)
.\cpuminer.exe -o stratum+tcp://mine.vericonomy.com:3333 -u VYourAddress.worker1 -p x -t 0
```

```sh
# Linux / macOS
./cpuminer -o stratum+tcp://mine.vericonomy.com:3333 -u VYourAddress.worker1 -p x -t 0
```

On Windows you can also double-click `contrib\windows\mine-verium-pool.bat`
(after editing your address inside the file).

**Verify checksums:**

```sh
# Linux/macOS
sha256sum -c SHA256SUMS            # (shasum -a 256 -c on macOS)
# Windows (PowerShell)
Get-FileHash .\cpuminer.exe -Algorithm SHA256
```

> Release binaries are currently **unsigned**; the signing pipeline is wired in
> and will be enabled once certificates are provisioned. Until then, verify with
> `SHA256SUMS` and see [Troubleshooting](#troubleshooting) for antivirus notes.
>
> Docker: `docker run --rm ghcr.io/joshios-vry/veriumminer:latest -o stratum+tcp://mine.vericonomy.com:3333 -u VYourAddress.worker1 -p x`


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


Quick start (build from source)
-------------------------------

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

**Windows (MSYS2 MinGW64)** — install [MSYS2](https://www.msys2.org/), open the
**MINGW64** shell, then:

```sh
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
                   mingw-w64-x86_64-curl mingw-w64-x86_64-jansson
cd /c/Users/you/veriumMiner
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

From **PowerShell** (after the build above):

```powershell
cd C:\Users\you\veriumMiner
.\build\cpuminer.exe --setup    # writes config, then exits
.\build\cpuminer.exe            # mines using the saved config
```

Release and default **MinGW** CMake builds use `-DSTATIC_BUILD=ON` so you do **not**
need `C:\msys64\mingw64\bin` on `PATH`. If you see `libwinpthread-1.dll` missing,
reconfigure with `cmake -B build -DSTATIC_BUILD=ON` and rebuild.

> Use **MINGW64** paths in the MSYS2 shell (`cd /c/Users/you/veriumMiner`), not
> in PowerShell (`cd /c/...` is bash-only). In PowerShell use `cd C:\Users\you\veriumMiner`.

> **No `build` folder yet?** Run `cmake -B build` and `cmake --build build` first,
> or use a [release zip](#download--run) (no compile).

Optional install (only **after** `cmake -B build` and `cmake --build build`):

```sh
cmake --install build
# Linux/macOS: installs to /usr/local/bin/cpuminer
```

```powershell
# Windows: pick a prefix you own (no admin required)
cmake --install build --prefix "$env:USERPROFILE\bin"
# then add $env:USERPROFILE\bin to PATH, or run $env:USERPROFILE\bin\cpuminer.exe
```

If `cmake --install` says `cmake_install.cmake` is missing, you have not configured the
`build` folder yet — run `cmake -B build` first.

### 3. Configure and run

**First run (wizard)** — writes `~/.cpuminer/cpuminer-conf.json` (or the Windows
path below):

```sh
# Linux / macOS / MSYS2 bash
./build/cpuminer --setup
./build/cpuminer
```

```powershell
# Windows PowerShell (use backslashes and .exe)
.\build\cpuminer.exe --setup
.\build\cpuminer.exe
```

**One-shot from the command line** (no config file):

```sh
./build/cpuminer -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x -t 0
```

```powershell
.\build\cpuminer.exe -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x -t 0
```

**Vericonomy official pool** ([mine.vericonomy.com](https://mine.vericonomy.com)):

```sh
./build/cpuminer -o stratum+tcp://mine.vericonomy.com:3333 -u VYourAddress.worker1 -p x -t 0
```

```powershell
.\build\cpuminer.exe -o stratum+tcp://mine.vericonomy.com:3333 -u VYourAddress.worker1 -p x -t 0
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

On Windows, a **source build** needs MinGW DLLs on `PATH` (see
[Windows](#windows-msys2--mingw-w64)); the **release** `.exe` is statically
linked and needs nothing extra.


Running headless / as a service
-------------------------------

For servers, SBCs (Raspberry Pi), and other non-GUI devices, run the miner in
the background and persist logs with `--log-file`:

```sh
cpuminer -c /etc/veriumminer/default.json --log-file /var/log/veriumminer/miner.log
```

Ready-to-use service definitions ship in [`contrib/`](contrib):

* **Linux (systemd):** [`contrib/systemd/cpuminer@.service`](contrib/systemd/cpuminer@.service)
  — `systemctl enable --now cpuminer@default`
* **macOS (launchd):** [`contrib/launchd/com.vericonomy.veriumminer.plist`](contrib/launchd/com.vericonomy.veriumminer.plist)
* **Windows (Scheduled Task):** [`contrib/windows/install-service.ps1`](contrib/windows/install-service.ps1)
  — run from an elevated PowerShell to auto-start at boot.

Use `--profile dedicated` on mining-only machines for higher CPU priority, or
the default `background` profile on shared/desktop machines to stay responsive.
A full walkthrough is in [`docs/HEADLESS.md`](docs/HEADLESS.md).

The miner shuts down cleanly on `Ctrl-C` / `SIGTERM` (and the `quit` API
command): it stops the worker threads, frees scrypt scratchpads, closes the
pool connection, and exits — so service restarts are graceful.


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

#### Windows Defender false positives (Bearfoos / trojan)

Unsigned CPU miners are often flagged by Microsoft Defender machine learning
(e.g. `Trojan:Win32/Bearfoos.A!ml`) even when you build this project yourself.
That is a **heuristic false positive**, not proof the binary is malicious.

**In the codebase we reduce ambiguity by:**

- Embedding proper **version resources** (Company: Vericonomy, Product: Verium
  Miner, description, copyright) and an **application manifest** (`asInvoker`)
  in Windows builds — see `res/cpuminer.rc.in` and `res/veriumminer.manifest`.
- Keeping the binary name `cpuminer` only for config compatibility; metadata
  identifies it as Verium Miner.

**What actually stops most false positives for end users:**

1. **Code signing** — Authenticode-sign release builds with your publisher cert.
2. **Microsoft submission** — Report a false positive:
   [Microsoft Security Intelligence file submission](https://www.microsoft.com/en-us/wdsi/filesubmission)
   (category: Incorrect detection / false positive).
3. **Local exclusion** — For your own dev builds, exclude only
   `veriumMiner\build` (not broad folders).

After changing Windows resources, **reconfigure and rebuild** so `cpuminer.exe`
is regenerated. If Defender quarantined the old binary, restore or rebuild, then
add the build-folder exclusion before running.

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
| `STATIC_BUILD` | `OFF`   | Statically link deps for a dependency-free distributable (used by release CI; on Windows produces a single DLL-free `cpuminer.exe`) |
| `WERROR`       | `OFF`   | Treat warnings as errors (CI gate)                       |

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

After a **release download**, run the binary from the extracted folder (add
`.\` on Windows). Only **build-from-source** workflows use `build/cpuminer`:

```powershell
# Windows (release)
.\cpuminer.exe -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x -t 0
```

```sh
# Linux / macOS (release)
./cpuminer -o stratum+tcp://POOL:PORT -u WALLET.WORKER -p x -t 0
```

**Default pool (Vericonomy):** `stratum+tcp://mine.vericonomy.com:3333` — username
`VRM_ADDRESS.workerName`, password `x` (any value). See
[Download & run](#download--run) for copy-paste examples.

Run `cpuminer --help` / `cpuminer.exe --help` for the full list of options.
Common ones:

* `-o, --url` — primary pool URL (`stratum+tcp://...`)
* `--backup-url` — comma-separated backup pools (automatic failover)
* `-u, --user` / `-p, --pass` — wallet/worker credentials
* `-t, --threads` — mining threads (`0` = auto from CPU topology and L3 cache)
* `--setup` — interactive wizard writes the default config file (see below)
* `--tune` — print recommended thread count at startup
* `--status-interval` — seconds between status summaries (hashrate, shares, temp)
* `--profile dedicated` — higher CPU priority for dedicated mining rigs
* `--log-file FILE` — also append plain-text logs to FILE (headless/service)
* `--selftest` — verify the scrypt core against the golden vector, then exit (0 = OK)
* `-B, --background` — detach and run in the background
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


Troubleshooting
===============

| Symptom | Fix |
|---------|-----|
| **Antivirus flags `cpuminer.exe`** (e.g. `Trojan:Win32/Bearfoos.A!ml`) | A heuristic false positive common to all CPU miners. Verify with `SHA256SUMS`, then add an exclusion for the miner folder, or submit a [false-positive report to Microsoft](https://www.microsoft.com/en-us/wdsi/filesubmission). Signed releases (coming) will reduce this. See the Windows section above. |
| **`error while loading shared libraries: libcurl…` (Linux)** | Install the runtime: `sudo apt-get install -y libcurl4`. Prebuilt Linux binaries link libgcc/libstdc++ statically but use the system libcurl. |
| **`scrypt buffer allocation failed` / out of memory** | Each thread needs ~1 GB for the N=1048576 scratchpad. Lower `-t` (threads) or add RAM/swap. Use `--tune` to see the recommended thread count. |
| **Low/zero hashrate on a shared machine** | Use the default `background` profile; reserve a core for the OS by lowering `-t`. For mining-only rigs use `--profile dedicated`. |
| **Large pages not used (slower hashrate)** | On Linux, grant locked-memory limits (the systemd unit sets `LimitMEMLOCK=infinity`) and enable hugepages. On Windows, run elevated once so the "Lock pages in memory" privilege can be acquired. |
| **All shares rejected** | Check the wallet address (`user`) and that the pool URL/port are correct; watch for "High reject rate" warnings in the log. |
| **macOS: “Verium Miner is damaged”** | Not corrupted — macOS blocked an unsigned download. Use **`Verium Miner.command`**, or run `xattr -dr com.apple.quarantine /path/to/extracted/folder`, then right-click the app → **Open** once. |
| **Change thread count after setup** | Double-click **`Change Settings.command`** (macOS), run `./cpuminer --setup`, edit `"threads"` in `~/.cpuminer/cpuminer-conf.json` (`0` = auto), or pass `-t N` for a one-off override. |

For headless setup details and more, see [`docs/HEADLESS.md`](docs/HEADLESS.md).


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
cpuminer --cputest          # release binary in PATH or current directory
# or, if you built from source: ./build/cpuminer --cputest
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


Security
========

To report a vulnerability, see [`SECURITY.md`](SECURITY.md). Please do **not**
open public issues for security problems. Always verify downloads against the
published `SHA256SUMS`.


Contributing
============

Contributions are welcome — see [`CONTRIBUTING.md`](CONTRIBUTING.md) for the
build/test workflow, coding conventions, and the rule that scrypt² output is
consensus-bound and must never change without golden-vector validation.


License
=======
GPLv2. See `COPYING` for details.
