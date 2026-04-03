```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  PIP-BOY 3000 Mk VI  ·  MINING INTERFACE  ·  VAULT-TEC CORP  ☢    ║
║                                                                      ║
║        C A P S T A S H   A N D R O I D   M I N E R                   ║
║                    v4.20.69  ·  WHIRLPOOL-512                        ║
║                                                                      ║
║  [ STATS ]   [ POOLS ]   [ INSTALL ]   [ DATA ]   [ RADIO ]          ║
╚══════════════════════════════════════════════════════════════════════╝
```

> *"The Pip-Boy never lies, Vault Dweller. Your hashrate is your survival."*
> — Vault-Tec Mining Division, 2161

**VAULT-TEC CERTIFIED** CPU miner for the CapStash network. Purpose-built for Android ARM64 via Termux. Unlike bloated multi-algorithm miners carrying dead code for 80+ algorithms, this unit runs **Whirlpool-512 only** — lean, fast, and optimized for the wasteland's most abundant hardware: the humble mobile phone.

For intel on CapStash, tokenomics, and the coin itself, tune your Pip-Boy to:
**[https://github.com/CapStash/CapStash-Core](https://github.com/CapStash/CapStash-Core)**

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  STATS  ·  v4.20.69 UPGRADE LOG                                   ║
╚══════════════════════════════════════════════════════════════════════╝
```

| SYSTEM | STATUS | NOTES |
|--------|--------|-------|
| Whirlpool T0-only | ✅ ONLINE | 2KB table vs 16KB — fits ARM L1 cache entirely |
| Midstate precompute | ✅ ONLINE | Block header hashed once per template, not per nonce |
| P-core affinity | ✅ ONLINE | Threads pinned to performance cores via sched_setaffinity |
| Stratum pool client | ✅ ONLINE | Full subscribe → authorize → notify → submit pipeline |
| Auto P-core detect | ✅ ONLINE | Reads /proc/cpuinfo CPU part numbers at setup |

> *Previous field units (v3.0.0) reported 5.87 MH/s. v4.20.69 units report 10+ MH/s on identical hardware with clean display and accepted share tracking. Vault-Tec approves.*

| Clean status display | ✅ ONLINE | Hashrate · accepted · rejected · diff · temp on one line |
| Dev fee 0.5% | ✅ ONLINE | 1 in 200 shares · disable with `-DDEV_FEE_DISABLED` |

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  INSTALL  ·  QUICK DEPLOYMENT PROTOCOL                           ║
╚══════════════════════════════════════════════════════════════════════╝
```

### STEP 1 — ACQUIRE TERMUX

Download from **[F-Droid](https://f-droid.org/en/packages/com.termux/)** only.

> ⚠️ **VAULT-TEC WARNING:** The Play Store version is corrupted pre-war software. It will fail to build. Do not use it.

### STEP 2 — DEPLOY MINING UNIT

Open Termux and transmit the following:

```bash
curl -O https://raw.githubusercontent.com/scratcher14/capstash-miner-android/main/setup_capstash_miner.sh && chmod +x setup_capstash_miner.sh && ./setup_capstash_miner.sh
```

### STEP 3 — CONFIGURE YOUR UNIT

The interactive setup will interrogate you for:

- **Mining mode** — Solo (direct RPC to your node) or Pool (stratum)
- **Pool selection** — Choose from known wasteland pools or enter manually
- **Reward address** — Your CapStash wallet (`cap1...`, `C...`, or `8...`)
- **Thread count** — Auto-detected from your CPU's performance core layout

### STEP 4 — BEGIN OPERATIONS

```bash
cd ~/capstash-miner && ./start.sh
```

*Vault-Tec estimates deployment time: 2-4 minutes including compilation.*

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  RADIO  ·  KNOWN WASTELAND MINING POOLS                          ║
╚══════════════════════════════════════════════════════════════════════╝
```

*Tune your Pip-Boy to one of the following verified signals. Pool operators
are fellow survivors — check their dashboards for current status and
difficulty settings before committing your hashrate.*

```
┌─────────────────────────────────────────────────────────────────────┐
│  STATION          SIGNAL                              TYPE           │
├─────────────────────────────────────────────────────────────────────┤
│  capspool.io      stratum+tcp://pplns.capspool.io:6333   PPLNS      │
│  papaspool.net    stratum+tcp://papaspool.net:7777        PPLNS      │
│  crypto-eire.com  stratum+tcp://stratum.crypto-eire.com:3340  PPLNS │
│  1miner.net       stratum+tcp://1miner.net:3690           PPLNS      │
└─────────────────────────────────────────────────────────────────────┘
```

> ⚠️ **SIGNAL ADVISORY:** Pool addresses and ports may shift as the wasteland evolves.
> Always verify connection details at the pool's dashboard before deploying.
> Not all pools have confirmed low-difficulty settings for CPU mobile miners —
> if shares aren't registering, switch stations and try again.

**Worker format:** `cap1qYOURADDRESS.your-worker-name`
**Password:** `x`

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  SOLO MINING PROTOCOL                                   ║
╚══════════════════════════════════════════════════════════════════════╝
```

Connect directly to your own CapStash node. You keep **100% of the block reward** (1 CAP per block, ~60 second block times). Requires a synced node on your network or Tailscale VPN.

Your node's `CapStash.conf` must contain:

```
server=1
rpcuser=youruser
rpcpassword=yourpass
rpcbind=127.0.0.1
rpcbind=100.x.x.x         ← your Tailscale IP
rpcallowip=127.0.0.1
rpcallowip=192.168.x.0/24  ← your local subnet
rpcallowip=100.64.0.0/10   ← Tailscale subnet
rpcport=8332
```

**Field tip:** Use your local IP (`192.168.x.x`) at home for lower latency. Switch to Tailscale (`100.x.x.x`) when operating remotely via `./reconfigure.sh`.

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  STATS  ·  PERFORMANCE READOUT  ·  MOTO EDGE FIELD TEST         ║
╚══════════════════════════════════════════════════════════════════════╝
```

*Benchmark conducted on Motorola Edge 5G UW · Snapdragon 778G · 4x A78 + 4x A55*
*Pool: pplns.capspool.io:6333 · Algorithm: Whirlpool-512*

```
THREADS  CORES        STABLE      MAX         FIELD NOTES
───────  ───────────  ──────────  ──────────  ─────────────────────────────
1        core 4       1.47 MH/s   1.57 MH/s   Single A78 — rock steady
2        cores 4-5    2.94 MH/s   3.15 MH/s   Linear scaling confirmed
3        cores 4-6    4.40 MH/s   4.72 MH/s   Solid, recommended minimum
4 ★      cores 4-7    5.87 MH/s   6.29 MH/s   ★ SAFE SWEET SPOT — all P-cores
5        cores 4-7+   7.34 MH/s   7.86 MH/s   Shares core 4, still efficient
6 ⚡     cores 4-7+   8.81 MH/s   9.44 MH/s   ⚡ MAX OUTPUT — batteryless rigs
7        cores 4-7+   ERRATIC     9.54 MH/s   Thermal throttle suspected
8        cores 4-7+   7.13 MH/s   7.55 MH/s   Contention — worse than 6
```

> **★ VAULT-TEC RECOMMENDATION:**
> 4 threads for daily drivers and hot environments.
> 6 threads for batteryless farm units — rock solid 8.81 MH/s sustained.
> Never run E-cores (A55/A53) — Whirlpool's table evicts from their tiny L1 cache.

### IDENTIFY YOUR CPU CORES

```bash
cat /proc/cpuinfo | grep "CPU part" | sort | uniq -c
```

```
PART NUMBER   CORE TYPE        VERDICT
──────────    ──────────────   ───────
0xd41         Cortex-A78       ✅ MINE WITH THESE
0xd44         Cortex-X1        ✅ MINE WITH THESE
0xd46         Cortex-A76       ✅ MINE WITH THESE
0xd47         Cortex-A710      ✅ MINE WITH THESE
0xd05         Cortex-A55       ☠ AVOID — cache too small
0xd03         Cortex-A53       ☠ AVOID — cache too small
```

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  FIELD OPERATIONS MANUAL                                ║
╚══════════════════════════════════════════════════════════════════════╝
```

**Start primary pool:**
```bash
cd ~/capstash-miner && ./start.sh
```

**Start backup pool:**
```bash
cd ~/capstash-miner && ./start-backup.sh
```

**View current config:**
```bash
cd ~/capstash-miner && ./info.sh
```

**Reconfigure unit:**
```bash
cd ~/capstash-miner && ./reconfigure.sh
```

*Reconfigure options: mode · pool URL · credentials · address · threads · worker name · full reset*

**Keep mining after closing Termux:**
```bash
termux-wake-lock
cd ~/capstash-miner && ./start.sh
# Close Termux — unit continues operating
```

**Cease operations:**
```bash
# Ctrl+C in Termux
termux-wake-unlock
```

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  NETWORK INTELLIGENCE — LOTTERY BLOCKS & HARDFORKS     ║
╚══════════════════════════════════════════════════════════════════════╝
```

**HEIGHT 55,000 — LOTTERY CONSENSUS v1**
Lottery blocks are no longer triggered by slow block times (>2 min). Selection is now cryptographically random — approximately 1 in 20 blocks is designated a lottery block via hash-based draw from prior chain state. Unpredictable. Manipulation-resistant.

**HEIGHT 65,000 — LOTTERY CONSENSUS v2 (PERMANENT)**
Consecutive lottery blocks prohibited. Updated domain separator. The old lottery sample set is permanently quarantined.

**WHAT THIS MEANS FOR YOUR UNIT:** Nothing changes. Same Whirlpool-512 PoW. Same 1 CAP reward. Lottery blocks are mined identically to regular blocks. No firmware update required.

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  PHONE FARM DEPLOYMENT                                  ║
╚══════════════════════════════════════════════════════════════════════╝
```

Each unit mines independently. Point all phones at the same wallet address — rewards accumulate regardless of which unit finds the block. For combined hashrate, connect all units to the same stratum pool with unique worker names:

```
cap1qYOURADDRESS.phone-01
cap1qYOURADDRESS.phone-02
cap1qYOURADDRESS.phone-03
...
cap1qYOURADDRESS.phone-56
```

Monitor your fleet on the pool dashboard using your wallet address.

> *The Cell Hasher deployment protocol supports mass installation across your fleet.
> One command. All units operational.*

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  MANUAL BUILD PROTOCOL                                  ║
╚══════════════════════════════════════════════════════════════════════╝
```

*If automated setup fails, build manually:*

```bash
pkg update && pkg upgrade
pkg install clang cmake make git curl

git clone https://github.com/scratcher14/capstash-miner-android ~/capstash-miner
cd ~/capstash-miner

mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DANDROID_BUILD=ON \
  -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops"
make -j$(nproc)
cp capstash-miner ..
cd ..
```

**Field update protocol:**
```bash
cd ~/capstash-miner && git pull
cd build && make -j$(nproc) && cp capstash-miner .. && cd ..
```

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  FILE MANIFEST                                          ║
╚══════════════════════════════════════════════════════════════════════╝
```

```
~/capstash-miner/
├── capstash-miner        ← compiled field unit binary
├── start.sh              ← deploy primary pool
├── start-backup.sh       ← deploy backup pool
├── reconfigure.sh        ← field reconfiguration menu
├── info.sh               ← unit status readout
├── mining-config.txt     ← mission parameters
└── build/                ← compilation bunker
```

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  TROUBLESHOOTING — FIELD DIAGNOSTICS                   ║
╚══════════════════════════════════════════════════════════════════════╝
```

**Unit reports old hashrate after update**
Binary needs recompilation after source update:
```bash
cd ~/capstash-miner && git pull
cd build && rm -rf * && cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DANDROID_BUILD=ON \
  -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops" \
  > /dev/null && make -j$(nproc) && cp capstash-miner .. && cd ..
```

**"Failed to start — check node connection"**
- Verify node IP and RPC port
- Test signal from Termux:
```bash
curl -s --user "user:pass" --data '{"method":"getblockcount","params":[],"id":1}' \
  -H 'Content-Type: application/json' http://NODE_IP:8332/
```
- Confirm `rpcbind` and `rpcallowip` cover your phone's subnet

**Pool shares not registering**
- Some pools run high minimum difficulty — mobile hashrate may not meet threshold
- Switch to another pool station and retry
- Verify worker format: `cap1qYOURADDRESS.workername`
- Confirm your address on the pool dashboard

**"Address decode failed"**
- Address must start with `cap1`, `C`, or `8`
- `cap1` addresses must be fully lowercase

**Hashrate drops after a few minutes**
- Thermal throttle detected — reduce thread count by 1
- Keep unit plugged in and remove case for airflow
- Never exceed P-core count

**Unit goes dark after closing Termux**
- Run `termux-wake-lock` before `./start.sh`

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  TEMPERATURE MONITORING                                 ║
╚══════════════════════════════════════════════════════════════════════╝
```

```bash
pkg install termux-api
termux-battery-status
```

> ⚠️ **VAULT-TEC THERMAL ADVISORY:** Cease operations if unit exceeds 45°C.
> Remove protective casing for improved heat dissipation.
> Batteryless units in the Cell Hasher have no battery thermal risk —
> push the cores harder, Vault Dweller.

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  INTELLIGENCE BRIEF — ABOUT CAPSTASH                   ║
╚══════════════════════════════════════════════════════════════════════╝
```

```
DESIGNATION:    CapStash (CAPS)
ALGORITHM:      Whirlpool-512 XOR/256
BLOCK TIME:     ~60 seconds
BLOCK REWARD:   1 CAP
RESISTANCE:     GPU-resistant · ASIC-resistant · CPU-optimized
MAX SUPPLY:     90,000,000,000 CAP
STATUS:         ACTIVE — chain height 65,000+
```

Full intelligence dossier, whitepaper, node firmware, and wallet downloads:
**[https://github.com/CapStash/CapStash-Core](https://github.com/CapStash/CapStash-Core)**

---

```
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  RADIO  ·  SUPPORT & RESOURCES                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

| STATION | FREQUENCY |
|---------|-----------|
| CapStash Official | [capstash.org](https://capstash.org) — Lore, chain spec, network status |
| CapStash Core | [github.com/CapStash/CapStash-Core](https://github.com/CapStash/CapStash-Core) |
| Explorer | [capstashmempool.codefalcon.dev](https://capstashmempool.codefalcon.dev/) |
| Discord | [discord.gg/zrzmkwAM7G](https://discord.gg/zrzmkwAM7G) |
| This Repo | [github.com/scratcher14/capstash-miner-android](https://github.com/scratcher14/capstash-miner-android) |
| Cell Hasher | [cellhasher.com](https://cellhasher.com/) — Mobile mining community & Discord |

---
╔══════════════════════════════════════════════════════════════════════╗
║  ☢  DATA  ·  ACKNOWLEDGEMENTS                                        ║
╚══════════════════════════════════════════════════════════════════════╝
```
Special thanks to **1miner** (dankminer v3.0.0) — pool operator at 1miner.net and
the first outside contributor to this project. His stratum expertise, bug fixes,
and PR #1 were instrumental in getting shares accepted. If you're looking for a
CPU-friendly pool with solid support, point your Pip-Boy to `1miner.net:3690`.

---

╔══════════════════════════════════════════════════════════════════════╗
║  ☢  VAULT-TEC LEGAL DISCLAIMER                                       ║
╚══════════════════════════════════════════════════════════════════════╝
```

*Mining operations generate significant thermal output and consume power reserves.
Monitor unit temperature. Vault-Tec Corporation accepts no liability for melted
circuits, battery bloat, or losses sustained in the wasteland. This firmware is
provided as-is. Mine responsibly. The wasteland is unforgiving.*

*Vault-Tec reminds you: a warm phone is a working phone — until it isn't.*

---

```
╔══════════════════════════════════════════════════════════════════════╗
║                                                                      ║
║   ☢  WALLET OF THE WASTELAND  ·  STAY VIGILANT  ·  STACK CAPS  ☢   ║
║                                                                      ║
║              War never changes. But hashrates do.                    ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```
