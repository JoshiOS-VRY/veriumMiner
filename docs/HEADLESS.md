# Running Verium Miner on headless / non-GUI devices

This guide covers running the miner unattended on servers, VPSes, single-board
computers (Raspberry Pi and similar), and any machine without a desktop.

## 1. Get the binary

Download the prebuilt archive for your platform from the
[Releases page](https://github.com/JoshiOS-VRY/veriumMiner/releases) and extract
it, or build from source (see the [README](../README.md)). The Windows release
is a single self-contained `cpuminer.exe`.

## 2. Create a config

Run the wizard once (interactive), or copy and edit the example config. On a
desktop Mac, double-clicking **Verium Miner.app** runs the wizard on first
launch and then starts mining; on servers without a TTY, use `--setup` or a
hand-written config file.

```sh
# From the release folder (or /usr/local/bin after cmake --install)
./cpuminer --setup
# Windows: .\cpuminer.exe --setup
# or
cp cpuminer-conf.json /etc/veriumminer/default.json && nano /etc/veriumminer/default.json
```

Set at least `url` and `user` (your Verium address, optionally `.worker`). For
unattended runs, enable file logging and pick a profile:

```json
{
  "url": "stratum+tcp://mine.vericonomy.com:3333",
  "user": "VYourAddress.worker1",
  "pass": "x",
  "threads": 0,
  "profile": "dedicated",
  "log-file": "/var/log/veriumminer/miner.log"
}
```

- `profile`: `background` (default, keeps the machine responsive) or
  `dedicated` (higher CPU priority for mining-only rigs).
- `threads`: `0` auto-selects from CPU topology, cache, and RAM. Each thread
  needs ~1 GB for the scrypt scratchpad.

## 3. Run it as a service

### Linux (systemd)

```sh
sudo useradd --system --no-create-home --shell /usr/sbin/nologin veriumminer
sudo install -m755 cpuminer /usr/local/bin/
sudo mkdir -p /etc/veriumminer /var/log/veriumminer
sudo chown veriumminer:veriumminer /var/log/veriumminer
sudo cp cpuminer-conf.json /etc/veriumminer/default.json   # edit wallet/pool
sudo cp contrib/systemd/cpuminer@.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now cpuminer@default
journalctl -u cpuminer@default -f
```

The instance name selects the config: `cpuminer@default` reads
`/etc/veriumminer/default.json`, so you can run several configs.

### macOS (launchd)

```sh
cp cpuminer /usr/local/bin/
mkdir -p "$HOME/Library/Application Support/cpuminer"
cp cpuminer-conf.json "$HOME/Library/Application Support/cpuminer/cpuminer-conf.json"
cp contrib/launchd/com.vericonomy.veriumminer.plist "$HOME/Library/LaunchAgents/"
launchctl load "$HOME/Library/LaunchAgents/com.vericonomy.veriumminer.plist"
```

### Windows (Scheduled Task)

From an **elevated** PowerShell:

```powershell
.\contrib\windows\install-service.ps1 -ExePath "C:\veriumminer\cpuminer.exe" -Config "C:\veriumminer\cpuminer-conf.json"
```

This registers a task that starts the miner minimized at boot and restarts it on
failure. Remove it with `.\install-service.ps1 -Uninstall`. For a true Windows
service, [NSSM](https://nssm.cc) also works.

## 4. Monitor

The miner serves a localhost API on port 4048:

```sh
printf 'summary\n' | nc -w 2 127.0.0.1 4048
printf 'json\n'    | nc -w 2 127.0.0.1 4048
```

Or tail the log file you configured. The status panel prints hashrate (H/m),
accepted/total shares, accept %, temperature, and uptime.

## 5. Shutdown behavior

The miner stops cleanly on `Ctrl-C`, `SIGTERM` (what systemd/launchd send), and
the `quit` API command: it signals the worker threads, frees the scrypt
scratchpads, closes the pool connection, and exits 0. A second `Ctrl-C` forces
an immediate exit. This makes service restarts safe.

## Performance tips

- Use `--tune` (or run once and read the startup log) to see the recommended
  thread count for your CPU/RAM.
- Enable huge/large pages for a throughput boost. On Linux the systemd unit
  grants `LimitMEMLOCK=infinity`; also configure hugepages at the OS level.
- On big.LITTLE / hybrid CPUs, workers are pinned to performance cores
  automatically.
