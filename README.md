# CapStash Miner for Android (Termux)

Whirlpool-512 CPU miner for CapStash — built specifically for Android via Termux. Unlike generic multi-algorithm miners, this miner is purpose-built for CapStash's Whirlpool-512 algorithm, delivering significantly higher hashrates through targeted CPU optimizations.

For information about CapStash, tokenomics, and the coin itself visit the official repository:
**[https://github.com/CapStash/CapStash-Core](https://github.com/CapStash/CapStash-Core)**

---

## 🚀 What's New in v3.0

- **T0-only Whirlpool** — replaced 8-table (16KB) lookup with single-table + ROTL64 rotations (2KB). Fits entirely in ARM L1 cache for a dramatic speedup
- **Midstate precomputation** — the first 64 bytes of the block header are hashed once per template, not once per nonce. Saves ~80% of hash work in the inner loop
- **P-core affinity** — mining threads are pinned directly to performance cores via `sched_setaffinity`, preventing the OS from migrating work to slow efficiency cores
- **Stratum pool support** — full `stratum+tcp` client with subscribe, authorize, notify, and share submission
- **Auto P-core detection** — setup script reads `/proc/cpuinfo` CPU part numbers to recommend the correct thread count for your specific chip

---

## 📱 Requirements

- Android device (ARM64 — any modern Android phone)
- [Termux](https://f-droid.org/en/packages/com.termux/) from F-Droid (not Play Store)
- Stable internet connection
- At least 500MB free storage space
- A CapStash wallet address (`cap1...`, `C...`, or `8...`)
- A running CapStash node (solo) or pool URL (pool)

---

## ⚡ Quick Start

### 1. Install Termux
Download from [F-Droid](https://f-droid.org/en/packages/com.termux/) — do **not** use the Play Store version as it is outdated and will fail to build.

### 2. Download and Run Setup Script
Open Termux and run:

```bash
curl -O https://raw.githubusercontent.com/scratcher14/capstash-miner-android/main/setup_capstash_miner.sh && chmod +x setup_capstash_miner.sh && ./setup_capstash_miner.sh
```

### 3. Configure During Setup
The interactive setup will prompt you for:

- **Mining mode**: Solo (direct to your CapStash node) or Pool (stratum)
- **Solo**: Node IP address, RPC port, RPC username, RPC password
- **Pool**: Select from known pools or enter manually, backup pool (optional), worker name
- **Reward address**: Your CapStash wallet address (`cap1...`, `C...`, or `8...`)
- **Threads**: Auto-detected from your CPU's performance core count

### 4. Start Mining

```bash
cd ~/capstash-miner
./start.sh
```

---

## 🎯 Solo vs Pool Mining

### Solo Mining
Connect directly to your own CapStash node via RPC. You keep 100% of the block reward (1 CAP per block, 60 second block times).

Your node's `CapStash.conf` must include:
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

Example setup prompt responses:
```
Node IP:  192.168.86.x  (local) or 100.x.x.x (Tailscale)
Port:     8332
User:     your_rpc_user
Pass:     your_rpc_password
```

**Pro tip:** Use your local IP (`192.168.x.x`) when mining at home for lower latency. Switch to your Tailscale IP when away using `./reconfigure.sh`.

### Pool Mining
Connect to a CapStash mining pool via stratum protocol. Rewards are split proportionally by hashrate — no node required.

```
Pool URL:   stratum+tcp://pool.address:port
Worker:     cap1qYOURADDRESS.phone-1
Password:   x
```

#### Known CapStash Pools

| Pool | Stratum URL |
|------|-------------|
| [crypto-eire.com](https://crypto-eire.com/dash.php?coin=cap) | `stratum+tcp://crypto-eire.com:3333` |
| [capspool.io](https://capspool.io/) | `stratum+tcp://capspool.io:3333` |
| [papaspool.net](https://papaspool.net/) | `stratum+tcp://papaspool.net:3333` |
| [1miner.net](https://1miner.net/main.html) | `stratum+tcp://1miner.net:3333` |

> ⚠️ **Note:** Not all pools have confirmed low difficulty settings for mobile miners. Verify your hashrate after connecting — if shares aren't being accepted try another pool.

---

## 🔄 Managing Your Miner

### Start Mining
```bash
cd ~/capstash-miner && ./start.sh
```

### Start on Backup Pool (pool mode only)
```bash
cd ~/capstash-miner && ./start-backup.sh
```

### View Current Configuration
```bash
cd ~/capstash-miner && ./info.sh
```

### Reconfigure Settings
```bash
cd ~/capstash-miner && ./reconfigure.sh
```

Reconfigure menu options:
1. Mining mode (solo/pool)
2. Node IP / Pool URL
3. RPC credentials / Pool password
4. Reward address
5. Thread count
6. Worker name
7. Full reconfigure
8. Exit

### Keep Mining After Closing Termux
```bash
termux-wake-lock
cd ~/capstash-miner && ./start.sh
# Close Termux — mining continues in background
```

### Stop Mining
```bash
# Press Ctrl+C in Termux window
termux-wake-unlock
```

---

## 📊 Thread Count Guide

This miner is optimized for ARM64 big.LITTLE architecture. The key rule: **never exceed your performance core count**.

Modern Android CPUs have two types of cores:
- **Performance cores** (A78, A76, X1, X2) — fast, ideal for mining
- **Efficiency cores** (A55, A53) — slow, adding these hurts overall hashrate

The setup script auto-detects your P-core count from `/proc/cpuinfo` and recommends the right thread count. To check manually:

```bash
cat /proc/cpuinfo | grep "CPU part" | sort | uniq -c
```

Common CPU part numbers:
- `0xd41` = Cortex-A78 (performance) ← use these
- `0xd44` = Cortex-X1 (performance) ← use these
- `0xd46` = Cortex-A76 (performance) ← use these
- `0xd05` = Cortex-A55 (efficiency) ← avoid
- `0xd03` = Cortex-A53 (efficiency) ← avoid

### Real Performance Example — v3.0 (4x A78 + 4x A55 device)

| Threads | Hashrate | Notes |
|---------|----------|-------|
| 1 | ~840 KH/s | Single A78 core |
| 2 | ~1.68 MH/s | Two A78 cores |
| 3 | ~2.52 MH/s | Three A78 cores |
| **4** | **~5.77 MH/s** | ← Sweet spot, all A78 cores |
| 5 | ~1.31 MH/s | Hits A55 cores — dramatic drop |
| 6-8 | worse | Avoid |

Performance collapses beyond the A78 core count because Whirlpool's lookup table (2KB in v3.0) gets evicted from L1 cache when E-cores join.

> v1.0 topped out at ~3.36 MH/s at 4 threads. v3.0 reaches ~5.77 MH/s on the same hardware through T0-only Whirlpool and midstate precomputation.

---

## 🛠️ Switching Between Home and Remote

At home (lower latency, better hashrate):
```bash
cd ~/capstash-miner
./reconfigure.sh  # Option 2 — set to local IP 192.168.x.x
```

Away from home (via Tailscale):
```bash
cd ~/capstash-miner
./reconfigure.sh  # Option 2 — set to Tailscale IP 100.x.x.x
```

---

## 🔗 Network Notes — Lottery Blocks & Hardforks

CapStash has undergone two consensus hardforks relevant to miners:

**Height 55,000 — Lottery Consensus v1**
Introduced cryptographically randomized lottery blocks. Previously, lottery blocks were triggered by slow block times (>2 min). After this fork, approximately 1 in 20 blocks is selected as a lottery block via a hash-based random draw from prior chain state — making lottery blocks unpredictable and manipulation-resistant.

**Height 65,000 — Lottery Consensus v2 (Permanent)**
Finalized the lottery system. Consecutive lottery blocks are no longer allowed, and the selection uses an updated domain separator for improved security. This is the permanent quarantine of the old lottery sample set.

**What this means for miners:** The PoW algorithm (Whirlpool-512 XOR/256) and block reward (1 CAP) are unchanged by either fork. Lottery blocks are a consensus feature — you mine them the same way as any other block. No changes to this miner are required.

---

## 🔧 Manual Build

If the setup script fails, build manually:

```bash
# Install dependencies
pkg update && pkg upgrade
pkg install clang cmake make git curl

# Clone repo
git clone https://github.com/scratcher14/capstash-miner-android ~/capstash-miner
cd ~/capstash-miner

# Build optimized for your device
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DANDROID_BUILD=ON \
  -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops"
make -j$(nproc)
cd ..
```

---

## 🔄 Updating

```bash
cd ~/capstash-miner
git pull
cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DANDROID_BUILD=ON \
  -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops"
make -j$(nproc)
cd ..
```

---

## ⚠️ Important Notes

### Battery & Heat Management
- Keep your phone plugged in while mining
- Monitor temperature — stop if device exceeds 45°C
- Remove phone case for better airflow
- Start with fewer threads and increase gradually
- Mining at 4 threads on A78 cores runs warm but stable on most devices

### Performance Tips
- Use local IP when on the same network as your node — reduces latency dramatically
- Close all other apps while mining
- Enable Performance mode in phone settings if available
- Disable battery optimization for Termux

### Why This Miner is Faster Than Generic Miners
cpuminer-opt and similar tools carry code for 80+ algorithms. Every build includes dead code, generic memory layouts, and compromises that serve all hardware. This miner is Whirlpool-512 only:
- T0-only lookup table (2KB) fits entirely in ARM L1 cache — no cache eviction during the hash loop
- Midstate precomputation eliminates ~80% of hash work per nonce
- P-core thread pinning prevents OS scheduling onto slow efficiency cores
- Compiler flags tuned specifically for ARM64 out-of-order execution
- No algorithm-switching overhead in the hot loop

---

## 📁 File Structure

```
~/capstash-miner/
├── capstash-miner        ← compiled binary
├── start.sh              ← start mining (primary pool or solo)
├── start-backup.sh       ← start on backup pool (pool mode only)
├── reconfigure.sh        ← change settings menu
├── info.sh               ← view current config
├── mining-config.txt     ← saved configuration
└── build/                ← cmake build directory
```

---

## 🔧 Troubleshooting

**Low hashrate after v3.0 upgrade**
Make sure you rebuilt the binary after pulling — an old binary won't have the new optimizations:
```bash
cd ~/capstash-miner && git pull
cd build && make -j$(nproc) && cd ..
```

**"Failed to start — check node connection"**
- Verify node IP and port are correct
- Test from Termux: `curl -s --user "user:pass" --data '{"method":"getblockcount","params":[],"id":1}' -H 'Content-Type: application/json' http://NODE_IP:8332/`
- Solo: confirm `rpcbind` in `CapStash.conf` includes your network IP
- Solo: confirm `rpcallowip` covers your phone's subnet

**Pool shares not being accepted**
- Some pools have high minimum difficulty — try a different pool from the list above
- Verify your worker format is `cap1qYOURADDRESS.workername`
- Check pool dashboard to confirm your worker is registering

**"Address decode failed"**
- Verify address starts with `cap1`, `C`, or `8`
- `cap1` addresses must be lowercase

**Low hashrate / drops after a few minutes**
- Thermal throttling — reduce thread count by 1
- Keep phone plugged in and cool
- Never exceed your performance core count

**Miner stops after closing Termux**
- Run `termux-wake-lock` before `./start.sh`

---

## 💡 Tips & Tricks

### Phone Farms
Each phone mines independently — in solo mode they do not combine hashrate, but all rewards land in whichever wallet address you configured regardless of which device finds the block. Point all phones at the same address and everything accumulates in one wallet.

For combined hashrate across devices, connect all phones to the same stratum pool using the same wallet address with different worker names (`cap1q...address.phone-1`, `cap1q...address.phone-2`, etc).

### Best Mining Strategy
- Test thread counts on your specific device — every chip is different
- Use local IP at home, Tailscale when away
- Monitor first session for thermal behavior before leaving unattended
- Start at P-core count and adjust from there

### Temperature Monitoring
```bash
# Install termux-api for battery/temp info
pkg install termux-api
termux-battery-status
```

---

## 📖 About CapStash

CapStash is a CPU-minable cryptocurrency using the Whirlpool-512 XOR/256 proof-of-work algorithm.

- **Block time:** 60 seconds
- **Block reward:** 1 CAP per block
- **Algorithm:** Whirlpool-512 XOR/256
- **CPU friendly:** designed for CPU mining, resistant to GPU and ASIC

For full details on tokenomics, the whitepaper, node setup, and wallet downloads visit the official repository:
**[https://github.com/CapStash/CapStash-Core](https://github.com/CapStash/CapStash-Core)**

---

## 🆘 Support & Resources

- **CapStash Core:** [https://github.com/CapStash/CapStash-Core](https://github.com/CapStash/CapStash-Core)
- **This Repo:** [https://github.com/scratcher14/capstash-miner-android](https://github.com/scratcher14/capstash-miner-android)
- **Cell Hasher:** [https://cellhasher.com/](https://cellhasher.com/) — Mobile mining community, resources, and Discord

---

## ⚖️ Disclaimer

Mining cryptocurrency generates significant heat and consumes battery. Monitor your device temperature and battery health. This software is provided as-is. Mine responsibly and never mine on devices with poor cooling or in hot environments.

---

*WALLET OF THE WASTELAND · STAY VIGILANT · STAY SAFE · STACK CAPS*
