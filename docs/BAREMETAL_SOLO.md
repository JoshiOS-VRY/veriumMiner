# Baremetal solo mining — operator notes

Fleet checklist for CPU miners on dedicated baremetal boxes connecting to a
**veriumd** node (local wallet, headless node, or remote full node).

**Upgrade target: veriumMiner 1.4.19** (replaces legacy FireWorm cpuminer for solo).
Includes vault/GBT solo fixes, decimal difficulty logging, `--log-frequency`, and
`--color-theme`.

Pool mining is unchanged — see the [README](../README.md).

## Why upgrade from FireWorm (1.4.8 and earlier Tippy)

| FireWorm / 1.4.8 symptom                               | Fixed in 1.4.19                                           |
| ------------------------------------------------------ | --------------------------------------------------------- |
| `recommended <= 1` on multi-core Ryzen/baremetal       | Yes — RAM + logical CPU auto-tune (since 1.4.11)        |
| `--no-getwork` / `--no-gbt` silently ignored           | Yes — flags in CLI, JSON, and `--help`                    |
| `--ryzen` missing                                      | Yes — AVX 3-way scrypt path restored                      |
| Solo HTTP timeouts with pool-style `VAddr.worker` user | Ops: use `-O rpcuser:rpcpass` + `http://` URL (see below) |
| `Unrecognized block version: 7`                        | Yes — GBT version 7 from veriumd (1.4.19)                 |
| CLI `-O` / `--coinbase-addr` ignored after config load | Yes — macOS optreset + deferred secret scrub (1.4.19)     |
| Hybrid Intel “recommended <= 8” on 16-thread rigs      | Yes — OS core-type scheduling, no 24/16 hardcode          |

## Architecture

```text
  [ baremetal miner ]                    [ node / wallet host ]
  cpuminer ── JSON-RPC (HTTP) ───────► veriumd :33987
           getblocktemplate / submitblock
           --coinbase-addr = payout
```

- Miner never uses Stratum for solo (`http://` URL).
- Block rewards go to `--coinbase-addr`, not the RPC username.
- RPC user/pass come from **verium.conf** / **vericonomy.conf**, not the wallet
  encryption password.

## Node prerequisites (wallet or headless veriumd)

On the machine running **veriumd** (must be synced):

```ini
# ~/.verium/verium.conf  (Linux)  or  %APPDATA%\Verium\verium.conf  (Windows)
# macOS Vericonomy wallet: ~/Library/Application Support/Verium/vericonomy.conf
server=1
rpcuser=fleet_rpc_user
rpcpassword=strong_random_password
rpcport=33987

# Local miner only:
rpcallowip=127.0.0.1

# Remote baremetal miner(s) — replace with miner IP or subnet:
rpcbind=0.0.0.0
rpcallowip=203.0.113.50/32
```

Restart `veriumd` after editing. Open **TCP 33987** on the node firewall for
remote miners.

**Mandatory smoke test from the baremetal host** (must pass before starting cpuminer):

```sh
curl -s --user fleet_rpc_user:strong_random_password \
  --data '{"jsonrpc":"1.0","id":"t","method":"getblockchaininfo","params":[]}' \
  http://NODE_IP:33987/
```

Expect JSON with `"result"`. If this **times out**, fix RPC/network first — no miner
version will solo mine until this works.

## Recommended miner command (baremetal)

```sh
./cpuminer \
  -o http://NODE_IP:33987 \
  -O fleet_rpc_user:strong_random_password \
  --coinbase-addr=VYourPayoutAddress \
  --no-getwork --no-stratum --no-longpoll \
  -t 0 \
  --ryzen \
  --profile dedicated \
  --log-frequency=fast \
  --log-file=/var/log/veriumminer/solo.log
```

| Flag                  | Purpose                                                                 |
| --------------------- | ----------------------------------------------------------------------- |
| `-o http://…`         | Solo mode (not `stratum+tcp://`)                                        |
| `-O user:pass`        | **RPC** credentials from verium.conf                                    |
| `--coinbase-addr`     | Verium address (`V…`) that receives block rewards                       |
| `--no-getwork`        | getblocktemplate only (matches legacy FireWorm)                         |
| `--no-stratum`        | Never negotiate Stratum on HTTP                                         |
| `--no-longpoll`       | Poll on scantime instead of long-poll hang                              |
| `-t 0`                | Auto: min(logical CPUs, free RAM ÷ scratchpad) — or set explicit `-t N` |
| `--ryzen`             | AVX 3-way on AMD Ryzen (try with/without on Zen 3+)                     |
| `--profile dedicated` | Higher CPU priority for mining rigs                                     |
| `--log-frequency`     | Status cadence: `fast` (15s) recommended for solo headless              |

Use `--tune` once per host to print the auto recommendation, or keep explicit `-t`
if you already benchmarked FireWorm settings.

### JSON config (systemd / fleet deploy)

Copy [`cpuminer-conf.solo.example.json`](../cpuminer-conf.solo.example.json):

```json
{
  "url": "http://203.0.113.10:33987",
  "user": "fleet_rpc_user",
  "pass": "strong_random_password",
  "coinbase-addr": "VYourPayoutAddress",
  "threads": 0,
  "profile": "dedicated",
  "no-getwork": true,
  "no-stratum": true,
  "no-longpoll": true,
  "ryzen": true,
  "log-frequency": "fast",
  "log-file": "/var/log/veriumminer/solo.log"
}
```

Run: `./cpuminer -c /etc/veriumminer/solo.json`

## Fleet rollout from legacy FireWorm (checklist)

1. **Node:** Confirm `veriumd` synced; `rpcbind` + `rpcallowip` for each baremetal IP; firewall open on **33987**.
2. **Smoke test:** Run the `curl getblockchaininfo` command above from **each** baremetal host.
3. **Stop** legacy miner: `systemctl stop verium-miner` (or your unit name).
4. **Download** [v1.4.19](https://github.com/JoshiOS-VRY/veriumMiner/releases/tag/v1.4.19) — pick `linux-x86_64` (or build from tag `v1.4.19`).
5. **Verify** binary: `sha256sum -c SHA256SUMS` against release assets.
6. **Replace** binary; keep FireWorm `-o`, `-O`, `--coinbase-addr`, `--no-*`, `--ryzen` flags.
7. **Remove** pool config mistakes:
   - No `stratum+tcp://` URL for solo
   - No `-u VAddress.worker` — solo uses `-O rpcuser:rpcpass` only
8. **Start** miner; within ~15 s (with `--log-frequency=fast`) hashrate should be > 0:

```sh
printf 'summary\n' | nc -w 2 127.0.0.1 4048
```

9. **Rollback plan:** keep FireWorm binary as `cpuminer-fireworm.bak` until 24 h stable hashrate.

## Troubleshooting

| Symptom                                                    | Likely cause               | Action                                                                |
| ---------------------------------------------------------- | -------------------------- | --------------------------------------------------------------------- |
| `HTTP request failed: Connection timed out after 30002 ms` | RPC not reachable          | `curl` test first; fix `veriumd`, `rpcbind`, `rpcallowip`, firewall   |
| `401` / authorization failed                               | Wrong RPC creds            | Match `-O` to `rpcuser`/`rpcpassword` in verium.conf                  |
| `Unrecognized block version: 7`                            | Pre-1.4.19 binary          | Upgrade to **v1.4.19**                                                |
| `0.00 H/m`, worker name like `minerocpu`                   | Pool config on solo URL    | Use `-O rpcuser:pass`, not wallet.worker; URL must be `http://`       |
| `0.00 H/m`, shares 0/0                                     | Node not synced            | `verium-cli getblockchaininfo`                                        |
| `invalid address`                                          | Bad coinbase               | `--coinbase-addr` must be valid `V…` address                          |
| `recommended <= 1` on `-t 6`                               | Running **1.4.8 or older** | Upgrade to **1.4.19**                                                 |
| `No usable protocol`                                       | All work sources disabled  | Do not pass both `--no-getwork` and `--no-gbt` without a working path |
| `scrypt buffer allocation failed`                          | RAM                        | ~768 MB per thread; lower `-t` or add RAM/swap                        |

## Health monitoring

Local API (default `127.0.0.1:4048`):

```sh
printf 'health\n' | nc -w 2 127.0.0.1 4048
printf 'summary\n' | nc -w 2 127.0.0.1 4048
```

## Download

- **Release:** https://github.com/JoshiOS-VRY/veriumMiner/releases/tag/v1.4.19
- **Verify:** `SHA256SUMS` on the release page — `sha256sum -c SHA256SUMS`

## Related docs

- [SOLO_MINING.md](SOLO_MINING.md) — full solo walkthrough
- [HEADLESS.md](HEADLESS.md) — systemd / 24×7 layout
