# Baremetal solo mining — operator notes

Fleet checklist for CPU miners on dedicated baremetal boxes connecting to a
**veriumd** node (local wallet, headless node, or remote full node). Applies to
**veriumMiner 1.4.10+** (replaces legacy FireWorm cpuminer for solo).

Pool mining is unchanged — see the [README](../README.md).

## What changed in 1.4.9 (vs 1.4.8 and FireWorm)

| Issue (1.4.8) | Fix (1.4.9) |
| --- | --- |
| `-t 0` picked **1 thread** on 12-core Ryzen; warning `recommended <= 1` | L3 heuristic fixed — auto threads use core count, RAM, and bandwidth again |
| `--no-getwork`, `--no-gbt` silently ignored | Flags restored in CLI and JSON config |
| `--ryzen` missing | Restored — forces AVX 3-way (often faster on Ryzen 1xxx/2xxx) |
| Solo looked like pool in logs (`Pool ○ connecting`) | Still cosmetic; use `http://` URL and RPC credentials (below) |

## Architecture

```text
  [ baremetal miner ]                    [ node / wallet host ]
  cpuminer ── JSON-RPC (HTTP) ───────► veriumd :33987
           getblocktemplate / submitblock
           --coinbase-addr = payout
```

- Miner never uses Stratum for solo (`http://` URL).
- Block rewards go to `--coinbase-addr`, not the RPC username.
- RPC user/pass come from **verium.conf**, not the wallet encryption password.

## Node prerequisites (wallet or headless veriumd)

On the machine running **veriumd** (must be synced):

```ini
# ~/.verium/verium.conf  (Linux)  or  %APPDATA%\Verium\verium.conf  (Windows)
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

Verify from the **miner host**:

```sh
curl -s --user fleet_rpc_user:strong_random_password \
  --data '{"jsonrpc":"1.0","id":"t","method":"getblockchaininfo","params":[]}' \
  http://NODE_IP:33987/
```

Expect JSON with `"result"` and no connection timeout.

## Recommended miner command (baremetal)

Replace placeholders. Use **explicit** `-t` until you confirm auto threads on
each box (`--tune` prints the recommendation).

```sh
./cpuminer \
  -o http://NODE_IP:33987 \
  -O fleet_rpc_user:strong_random_password \
  --coinbase-addr=VYourPayoutAddress \
  --no-getwork --no-stratum --no-longpoll \
  -t 6 \
  --ryzen \
  --profile dedicated \
  --log-file=/var/log/veriumminer/solo.log
```

| Flag | Purpose |
| --- | --- |
| `-o http://…` | Solo mode (not `stratum+tcp://`) |
| `-O user:pass` | **RPC** credentials from verium.conf |
| `--coinbase-addr` | Verium address (`V…`) that receives block rewards |
| `--no-getwork` | getblocktemplate only (matches legacy FireWorm) |
| `--no-stratum` | Never negotiate Stratum on HTTP |
| `--no-longpoll` | Poll on scantime instead of long-poll hang |
| `-t N` | Thread count; `-t 0` = auto (OK on 1.4.9+) |
| `--ryzen` | AVX 3-way on AMD Ryzen (try with/without on Zen 3+) |
| `--profile dedicated` | Higher CPU priority for mining rigs |

### JSON config (systemd / fleet deploy)

Copy [`cpuminer-conf.solo.example.json`](../cpuminer-conf.solo.example.json):

```json
{
  "url": "http://203.0.113.10:33987",
  "user": "fleet_rpc_user",
  "pass": "strong_random_password",
  "coinbase-addr": "VYourPayoutAddress",
  "threads": 6,
  "profile": "dedicated",
  "no-getwork": true,
  "no-stratum": true,
  "no-longpoll": true,
  "ryzen": true,
  "log-file": "/var/log/veriumminer/solo.log"
}
```

Run: `./cpuminer -c /etc/veriumminer/solo.json`

## Rollout from legacy FireWorm cpuminer

1. Stop the old service: `systemctl stop verium-miner` (or your unit name).
2. Install **1.4.10** binary from [GitHub Releases](https://github.com/JoshiOS-VRY/veriumMiner/releases/tag/v1.4.10) or build from tag `v1.4.10`.
3. Keep the same `-o`, `-O`, `--coinbase-addr`, and `--no-*` flags as before.
4. Remove any pool-style `-u VAddress.worker` — solo uses `-O rpcuser:rpcpass`.
5. Start and verify hashrate > 0 within one scantime (default 5 s):

```sh
printf 'summary\n' | nc -w 2 127.0.0.1 4048
```

## Troubleshooting

| Symptom | Likely cause | Action |
| --- | --- | --- |
| `HTTP request failed: Connection timed out after 30002 ms` | RPC not reachable | Check `veriumd` running, `rpcbind`, `rpcallowip`, firewall, correct IP/port |
| `401` / authorization failed | Wrong RPC creds | Match `-O` to `rpcuser`/`rpcpassword` in verium.conf |
| `0.00 H/m`, shares 0/0 | Node not synced or no work | `verium-cli getblockchaininfo` — `blocks` near network height |
| `invalid address` | Bad coinbase | `--coinbase-addr` must be valid `V…` address |
| `recommended <= 1` warning on `-t 6` | **1.4.8 bug** | Upgrade to **1.4.10+** or ignore warning and use explicit `-t` |
| `No usable protocol` | Disabled all work sources | Do not pass both `--no-getwork` and `--no-gbt` unless one path works |
| `scrypt buffer allocation failed` | RAM | ~768 MB–1 GB per thread; lower `-t` or add swap |

## Health monitoring

Local API (default `127.0.0.1:4048`):

```sh
printf 'health\n' | nc -w 2 127.0.0.1 4048
printf 'summary\n' | nc -w 2 127.0.0.1 4048
```

Log file (if `--log-file` set): plain text, suitable for central log collection.

## Download

- **Release:** `v1.4.10` — https://github.com/JoshiOS-VRY/veriumMiner/releases/tag/v1.4.10  
- **Verify:** compare `SHA256SUMS` from the release assets with `sha256sum -c SHA256SUMS`.

## Related docs

- [SOLO_MINING.md](SOLO_MINING.md) — full solo walkthrough  
- [HEADLESS.md](HEADLESS.md) — systemd / 24×7 layout  
