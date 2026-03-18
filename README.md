# CapStash Miner for Android (Termux)

Whirlpool-512 CPU miner for CapStash — built specifically for Android via Termux. Unlike generic multi-algorithm miners, this miner is purpose-built for CapStash's Whirlpool-512 algorithm, delivering significantly higher hashrates through targeted CPU optimizations.

For information about CapStash, tokenomics, and the coin itself visit the official repository:
**[https://github.com/CapStash/CapStash-Core](https://github.com/CapStash/CapStash-Core)**

---

## 📱 Requirements

- Android device (ARM64 architecture — any modern Android phone)
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
- **Pool**: Pool URL, backup pool (optional), worker name
- **Reward address**: Your CapStash wallet address (`cap1...`, `C...`, or `8...`)
- **Threads**: CPU threads to use (auto-detected optimal shown)

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
rpcbind=100.x.x.x        ← your Tailscale IP
rpcallowip=127.0.0.1
rpcallowip=192.168.x.0/24 ← your local subnet
rpcallowip=100.64.0.0/10  ← Tailscale subnet
rpcport=8332
```

Example setup prompt responses:
```
Node IP:  192.168.86.x  (local) or 100.x.x.x (Tailscale)
Port:     8332
User:     your_rpc_user
Pass:     your_rpc_password
```

**Pro tip:** Use your local IP (`192.168.x.x`) when mining at home for lower latency and higher hashrate. Switch to your Tailscale IP when away from home using `./reconfigure.sh`.

### Pool Mining
Connect to a CapStash mining pool via stratum protocol. Rewards split proportionally by hashrate.

```
Pool URL:   stratum+tcp://pool.address:port
Worker:     cap1qYOURADDRESS.phone-1
Password:   x
```

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
- **Performance cores** (A78, A76, X1, X2) — fast, great for mining
- **Efficiency cores** (A55, A53) — slow, adding these hurts overall hashrate

### Finding Your Optimal Thread Count

```bash
# See your CPU core types
cat /proc/cpuinfo | grep "CPU part" | sort | uniq -c
```

Common CPU part numbers:
- `0xd41` = Cortex-A78 (performance) ← use these
- `0xd44` = Cortex-X1 (performance) ← use these
- `0xd05` = Cortex-A55 (efficiency) ← avoid
- `0xd03` = Cortex-A53 (efficiency) ← avoid

The higher number = performance cores. Use that count as your thread limit.

### Real Performance Example (4x A78 + 4x A55 device)

| Threads | Hashrate | Notes |
|---------|----------|-------|
| 1 | ~840 KH/s | Single A78 core |
| 2 | ~1.68 MH/s | Two A78 cores |
| 3 | ~2.52 MH/s | Three A78 cores |
| **4** | **~3.36 MH/s** | ← Sweet spot, all A78 cores |
| 5 | ~1.31 MH/s | Hits A55 cores — dramatic drop |
| 6-8 | worse | Avoid |

Performance collapses beyond the A78 core count because Whirlpool's lookup tables (16KB) get evicted from L1 cache by the slower A55 cores.

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

## 🔧 Manual Build

If the setup script fails, build manually:

```bash
# Install dependencies
pkg update && pkg upgrade
pkg install clang cmake make git curl

# Clone repo
git clone https://github.com/scratcher14/capstash-miner-android ~/capstash-miner
cd ~/capstash-miner

# Build with A78 optimizations
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_C_FLAGS="-O3 -march=armv8.2-a+crypto+sha3 -mtune=cortex-a78 -fomit-frame-pointer -funroll-loops"
make -j$(nproc)
cp capstash-miner ..
cd ..
```

---

## 🔄 Updating

```bash
cd ~/capstash-miner
git pull
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_C_FLAGS="-O3 -march=armv8.2-a+crypto+sha3 -mtune=cortex-a78 -fomit-frame-pointer -funroll-loops"
make -j$(nproc)
cp capstash-miner ..
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
- Use local IP when on same network as your node — dramatically reduces latency
- Close all other apps while mining
- Enable Performance mode in phone settings if available
- Disable battery optimization for Termux

### Why This Miner is Faster Than Generic Miners
cpuminer-opt and similar tools carry code for 80+ algorithms. Every build includes dead code, generic memory layouts, and compromises that serve all hardware. This miner is Whirlpool-512 only:
- Lookup tables pre-aligned to cache line boundaries
- Compiler flags tuned specifically for A78/A76 out-of-order execution
- No algorithm-switching overhead in the hot loop
- Maintenance interval tuned for Whirlpool's table access pattern

---

## 📁 File Structure

```
~/capstash-miner/
├── capstash-miner        ← compiled binary
├── start.sh              ← start mining (primary)
├── start-backup.sh       ← start on backup pool (pool mode)
├── reconfigure.sh        ← change settings menu
├── info.sh               ← view current config
├── mining-config.txt     ← saved configuration
└── build/                ← cmake build directory
```

---

## 🔧 Troubleshooting

**Build fails with `cpu_set_t` errors**
Already fixed in current repo. If you have an old clone:
```bash
cd ~/capstash-miner && git pull
cd build && make -j$(nproc) && cp capstash-miner ..
```

**"Failed to start — check node connection"**
- Verify node IP and port are correct
- Test from Termux: `curl -s --user "user:pass" --data '{"method":"getblockcount","params":[],"id":1}' -H 'Content-Type: application/json' http://NODE_IP:8332/`
- Solo: confirm `rpcbind` in `CapStash.conf` includes your network IP
- Solo: confirm `rpcallowip` covers your phone's subnet

**"Address decode failed"**
- Verify address starts with `cap1`, `C`, or `8`
- `cap1` addresses must be lowercase

**Low hashrate / drops after a few minutes**
- Thermal throttling — reduce thread count by 1
- Keep phone plugged in and cool
- Never exceed your performance core count

**Miner stops after closing Termux**
- Run `termux-wake-lock` before `./start.sh`

**reconfigure.sh returns 404**
```bash
curl -O https://raw.githubusercontent.com/scratcher14/capstash-miner-android/main/reconfigure.sh
chmod +x reconfigure.sh && mv reconfigure.sh ~/capstash-miner/
```

---

## 💡 Tips & Tricks

### Multiple Phones
Each phone mines independently in solo mode — they do not combine hashrate. All rewards go to whichever wallet address you configured. Point multiple phones at the same address and all winnings land in one wallet regardless of which device finds the block.

To combine hashrate across devices you need a stratum pool server — watch the CapStash repository for pool announcements.

### Best Mining Strategy
- Test thread counts on your specific device — every chip is different
- Use local IP at home, Tailscale when away
- Monitor first session for thermal behavior before leaving unattended
- Start with performance core count and adjust from there

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
