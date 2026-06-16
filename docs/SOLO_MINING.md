# Solo mining with veriumMiner

Solo mining connects the CPU miner directly to your local **veriumd** node (or
Vericonomy wallet vault) over JSON-RPC. The node supplies block templates via
`getblocktemplate`; found blocks are submitted back to your node and pay out to
the address you specify with `--coinbase-addr`.

Pool mining (Stratum) is documented in the [README](../README.md#mining-modes-pool-and-solo).

**Recommended release for vault solo:** [v1.4.19](https://github.com/JoshiOS-VRY/veriumMiner/releases/tag/v1.4.19)
or newer.

## Prerequisites

1. **veriumd** installed and fully synced with the Verium network (or Vericonomy
   wallet with vault running).
2. RPC enabled in `verium.conf` / `vericonomy.conf`:
   - Linux: `~/.verium/verium.conf`
   - macOS (Vericonomy wallet): `~/Library/Application Support/Verium/vericonomy.conf`
   - Windows: `%APPDATA%\Verium\vericonomy.conf`

```ini
[verium]
server=1
rpcuser=your_rpc_user
rpcpassword=your_rpc_password
rpcallowip=127.0.0.1
# rpcport=33987   # mainnet default; omit unless you changed it
```

3. Enough RAM for mining threads (~384–768 MB per thread depending on CPU core).

Start the node before the miner:

```sh
veriumd -daemon
# or use your wallet / systemd unit to manage veriumd
```

## Quick start (command line)

Mainnet RPC defaults to port **33987**. Use FireWorm-style flags against veriumd:

```sh
./cpuminer \
  -o http://127.0.0.1:33987 \
  -O your_rpc_user:your_rpc_password \
  --coinbase-addr=VYourPayoutAddress \
  --no-getwork --no-stratum --no-longpoll \
  -t 0 \
  --profile dedicated
```

Windows:

```powershell
.\cpuminer.exe -o http://127.0.0.1:33987 -O rpcuser:rpcpassword `
  --coinbase-addr=VYourPayoutAddress `
  --no-getwork --no-stratum --no-longpoll -t 0 --profile dedicated
```

- `-O user:pass` — RPC credentials from `verium.conf` / `vericonomy.conf` (**not**
  your wallet address or encryption password).
- `--coinbase-addr` — Verium address that receives block rewards when you find a block.
- `--no-getwork --no-stratum --no-longpoll` — GBT-only solo against veriumd (matches
  legacy FireWorm cpuminer).
- `-t 0` — auto thread count from CPU topology and RAM.

Optional UX flags:

```sh
  --log-frequency=fast      # status panel every 15s (good for solo)
  --color-theme=light       # readable on macOS light terminal backgrounds
```

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
  "no-getwork": true,
  "no-stratum": true,
  "no-longpoll": true,
  "threads": 0,
  "profile": "dedicated",
  "log-frequency": "fast",
  "color-theme": "light",
  "log-file": "/var/log/veriumminer/solo.log"
}
```

For pool mode, `user` is your wallet address; for solo mode, `user`/`pass` are
RPC credentials and `coinbase-addr` is the payout address.

Run `./cpuminer --setup` and enter an `http://` URL to generate a solo config
interactively.

## What you see in the logs

Solo mode uses a dedicated status panel (no pool shares):

```text
STAT  Solo ● connected · scrypt^2 · payout VYour…Address · height 1104101
STAT  2,432 H/m now · … · blocks found 0 · net diff 0.000066332811 · workers 8 · …
```

- **Network difficulty** and **net diff** use fixed decimal (`0.0xxxx`), not
  scientific notation.
- **Block candidate / block found** lines appear only when a hash meets network
  difficulty (rare on CPU).
- Pool-style share accepted/rejected lines are not used in solo mode.

## Windows batch helper

Edit [`contrib/windows/mine-verium-solo.bat`](../contrib/windows/mine-verium-solo.bat)
with your RPC credentials and payout address, ensure `veriumd` is running, then
double-click the batch file.

## Headless / 24/7 solo

1. Run **veriumd** as a service (must stay synced).
2. Run **cpuminer** as a separate service with a solo config file.
3. Enable `"log-file"` and monitor via the local API (`127.0.0.1:4048`).

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

| Symptom                         | Fix                                                                                                                                                                                                                     |
| ------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Connection refused on 33987     | Start `veriumd`, confirm `server=1` and `rpcport` in config.                                                                                                                                                            |
| HTTP timeout (30 s)             | Node must listen on the miner host: set `rpcbind=0.0.0.0` and `rpcallowip=<miner-ip>/32` when mining remotely. Confirm firewall allows the RPC port. Use `-O rpcuser:rpcpassword` (not your wallet address).            |
| 401 / authorization failed      | Match `-O user:pass` to `rpcuser` / `rpcpassword` in config. Wallet address as RPC user will fail.                                                                                                                      |
| `Unrecognized block version: 7` | Upgrade to **v1.4.19+**; veriumd GBT returns version 7. Use `--no-getwork` so GBT is required.                                                                                                                          |
| `invalid URL -- ''`             | Upgrade to **v1.4.19+**; empty `"url"` in JSON no longer breaks CLI `-o` overrides. Re-run `--setup` or fix config.                                                                                                     |
| No work / idle hashrate         | Node must be synced; check `verium-cli getblockchaininfo`.                                                                                                                                                              |
| Invalid address                 | `--coinbase-addr` must be a valid Verium address (starts with `V`).                                                                                                                                                     |
| Out of memory                   | Lower `-t`; each thread needs hundreds of MB scratchpad RAM.                                                                                                                                                            |
| Log text hard to read (Mac)     | `--color-theme=light` or `"color-theme": "light"` in config.                                                                                                                                                            |
| `recommended <= 1` warning      | Fixed in 1.4.11+ (L3 heuristic no longer caps desktop CPUs to 1). Override with explicit `-t N`.                                                                                                                        |
| Migrating from FireWorm         | Same flags: `--no-getwork --no-stratum --no-longpoll`, plus `--ryzen` on Ryzen rigs. Example: `cpuminer -o http://127.0.0.1:33987 -O user:pass --coinbase-addr=V… --no-getwork --no-stratum --no-longpoll -t 6 --ryzen` |

Fleet operators: see [BAREMETAL_SOLO.md](BAREMETAL_SOLO.md) for rollout, remote RPC, and monitoring.
