# Solo mining with veriumMiner

Solo mining connects the CPU miner directly to your local **veriumd** node over
JSON-RPC. The node supplies block templates via `getblocktemplate` (with fallback
to `getwork`); found blocks are submitted back to your node and pay out to the
address you specify.

Pool mining (Stratum) is documented in the [README](../README.md#mining-modes-pool-and-solo).

## Prerequisites

1. **veriumd** installed and fully synced with the Verium network.
2. RPC enabled in `verium.conf` (typically under `~/.verium/` on Linux/macOS, or
   `%APPDATA%\Verium\` on Windows):

```ini
server=1
rpcuser=your_rpc_user
rpcpassword=your_rpc_password
rpcallowip=127.0.0.1
# rpcport=33987   # mainnet default; omit unless you changed it
```

3. Enough RAM for mining threads (~1 GB per thread for the scrypt scratchpad).

Start the node before the miner:

```sh
veriumd -daemon
# or use your wallet / systemd unit to manage veriumd
```

## Quick start (command line)

Mainnet RPC defaults to port **33987**:

```sh
./cpuminer \
  -o http://127.0.0.1:33987 \
  -O your_rpc_user:your_rpc_password \
  --coinbase-addr=VYourPayoutAddress \
  -t 0 \
  --profile dedicated
```

Windows:

```powershell
.\cpuminer.exe -o http://127.0.0.1:33987 -O rpcuser:rpcpassword `
  --coinbase-addr=VYourPayoutAddress -t 0 --profile dedicated
```

- `-O user:pass` — RPC credentials from `verium.conf` (not your wallet password).
- `--coinbase-addr` — Verium address that receives block rewards when you find a block.
- `-t 0` — auto thread count from CPU topology and RAM.

## JSON configuration

Copy [`cpuminer-conf.solo.example.json`](../cpuminer-conf.solo.example.json), edit
the values, and run:

```sh
./cpuminer -c /path/to/solo.json
```

Example:

```json
{
  "url": "http://127.0.0.1:33987",
  "user": "your_rpc_user",
  "pass": "your_rpc_password",
  "coinbase-addr": "VYourPayoutAddress",
  "threads": 0,
  "profile": "dedicated",
  "log-file": "/var/log/veriumminer/solo.log"
}
```

For pool mode, `user` is your wallet address; for solo mode, `user`/`pass` are
RPC credentials and `coinbase-addr` is the payout address.

Run `./cpuminer --setup` and enter an `http://` URL to generate a solo config
interactively.

## Windows batch helper

Edit [`contrib/windows/mine-verium-solo.bat`](../contrib/windows/mine-verium-solo.bat)
with your RPC credentials and payout address, ensure `veriumd` is running, then
double-click the batch file.

## Headless / 24/7 solo

1. Run **veriumd** as a service (must stay synced).
2. Run **cpuminer** as a separate service with a solo config file.
3. Enable `--log-file` and monitor via the local API (`127.0.0.1:4048`).

See [HEADLESS.md](HEADLESS.md) for systemd, launchd, and Windows Scheduled Task
setup. Order matters: start `veriumd` before the miner. A typical systemd layout
uses two units — one for the daemon, one for `cpuminer@solo` reading
`/etc/veriumminer/solo.json`.

Health check:

```sh
printf 'health\n' | nc -w 2 127.0.0.1 4048
```

## Testnet

If your node runs on testnet, use the RPC port and credentials from your testnet
`verium.conf`. The miner URL is still `http://127.0.0.1:<rpcport>`; only the
port and network differ.

## Troubleshooting

| Symptom                     | Fix                                                                                                                                                                                                                              |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Connection refused on 33987 | Start `veriumd`, confirm `server=1` and `rpcport` in `verium.conf`.                                                                                                                                                              |
| HTTP timeout (30 s)         | Node must listen on the miner host: set `rpcbind=0.0.0.0` and `rpcallowip=<miner-ip>/32` in `verium.conf` when mining remotely. Confirm firewall allows the RPC port. Use `-O rpcuser:rpcpassword` (not your wallet address).    |
| 401 / authorization failed  | Match `-O user:pass` to `rpcuser` / `rpcpassword` in `verium.conf`.                                                                                                                                                              |
| No work / idle hashrate     | Node must be synced; check `verium-cli getblockchaininfo`.                                                                                                                                                                       |
| Invalid address             | `--coinbase-addr` must be a valid Verium address (starts with `V`).                                                                                                                                                              |
| Out of memory               | Lower `-t`; each thread needs ~1 GB scratchpad RAM.                                                                                                                                                                              |
| `recommended <= 1` warning  | Fixed in current tree (L3 heuristic no longer caps desktop CPUs to 1). Override with explicit `-t N`.                                                                                                                            |
| Migrating from FireWorm     | Same flags restored: `--no-getwork --no-stratum --no-longpoll`, plus `--ryzen` on Ryzen rigs. Example: `cpuminer -o http://127.0.0.1:33987 -O user:pass --coinbase-addr=V… --no-getwork --no-stratum --no-longpoll -t 6 --ryzen` |

Fleet operators: see [BAREMETAL_SOLO.md](BAREMETAL_SOLO.md) for rollout, remote RPC, and monitoring.
