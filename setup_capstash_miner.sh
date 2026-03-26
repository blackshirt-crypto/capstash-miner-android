#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  CapStash Miner v3.0 — Termux Setup Script
#  Whirlpool-512 CPU miner for CapStash
#  github.com/scratcher14/capstash-miner-android
# ============================================================

set -e

INSTALL_DIR="$HOME/capstash-miner"
REPO="https://github.com/scratcher14/capstash-miner-android"
CONFIG_FILE="$INSTALL_DIR/mining-config.txt"
MINER_VERSION="3.0.0"

# ── Colors ────────────────────────────────────────────────────────────────
GREEN='\033[38;5;82m'
AMBER='\033[38;5;214m'
RED='\033[38;5;196m'
DIM='\033[2m'
RESET='\033[0m'

# ── Banner ────────────────────────────────────────────────────────────────
clear
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}║      CAPSTASH MINER v3.0 SETUP            ║${RESET}"
echo -e "${GREEN}║   Whirlpool-512 · Android CPU Miner       ║${RESET}"
echo -e "${GREEN}║   github.com/scratcher14                  ║${RESET}"
echo -e "${GREEN}╚═══════════════════════════════════════════╝${RESET}"
echo ""

# ── Check Termux ──────────────────────────────────────────────────────────
if [ ! -d "/data/data/com.termux" ]; then
    echo -e "${RED}Error: This script must be run in Termux.${RESET}"
    exit 1
fi

# ── Storage check ─────────────────────────────────────────────────────────
AVAIL=$(df "$HOME" | awk 'NR==2 {print $4}')
if [ "$AVAIL" -lt 524288 ]; then
    echo -e "${RED}Warning: Less than 512MB free storage. Build may fail.${RESET}"
    read -p "Continue anyway? (y/n): " CONT
    [ "$CONT" != "y" ] && exit 1
fi

# ── Step 1: Update packages ───────────────────────────────────────────────
echo -e "${AMBER}[1/6] Updating Termux packages...${RESET}"
pkg update -y -q
pkg upgrade -y -q

# ── Step 2: Install build dependencies ───────────────────────────────────
echo -e "${AMBER}[2/6] Installing build dependencies...${RESET}"
pkg install -y -q \
    clang \
    cmake \
    make \
    git \
    curl \
    pkg-config

echo -e "${GREEN}✓ Build tools installed${RESET}"

# ── Step 3: Clone repository ──────────────────────────────────────────────
echo -e "${AMBER}[3/6] Downloading capstash-miner source...${RESET}"

if [ -d "$INSTALL_DIR" ]; then
    echo -e "${DIM}Existing installation found — updating...${RESET}"
    cd "$INSTALL_DIR"
    git pull -q
else
    git clone -q "$REPO" "$INSTALL_DIR"
    cd "$INSTALL_DIR"
fi
echo -e "${GREEN}✓ Source downloaded${RESET}"

# ── Step 4: Detect CPU and build ─────────────────────────────────────────
echo -e "${AMBER}[4/6] Building miner (optimized for your CPU)...${RESET}"
echo -e "${DIM}This takes 1-3 minutes on most devices...${RESET}"

CPU_INFO=$(grep "Hardware" /proc/cpuinfo 2>/dev/null | head -1 || echo "ARM64")
echo -e "${DIM}Detected: $CPU_INFO${RESET}"

mkdir -p build
cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DANDROID_BUILD=ON \
    -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    > /dev/null 2>&1

make -j$(nproc) 2>&1 | tail -5
make install > /dev/null 2>&1

cd "$INSTALL_DIR"
echo -e "${GREEN}✓ Build successful${RESET}"

# ── Step 5: Interactive configuration ────────────────────────────────────
echo ""
echo -e "${AMBER}[5/6] Configuration${RESET}"
echo -e "${DIM}────────────────────────────────────────────${RESET}"
echo ""

# ── Mining mode ───────────────────────────────────────────────────────────
echo "Mining Mode:"
echo "  1) Solo   — connect directly to your CapStash node via RPC"
echo "  2) Pool   — connect to a mining pool via stratum"
echo ""
read -p "Select mode (1/2): " MODE_CHOICE

if [ "$MODE_CHOICE" = "1" ]; then
    # ── Solo mode ─────────────────────────────────────────────────────────
    MINING_MODE="solo"
    echo ""
    echo -e "${DIM}Solo mining connects directly to your CapStash node.${RESET}"
    echo -e "${DIM}Your node must have server=1 and rpcbind set in CapStash.conf${RESET}"
    echo ""
    read -p "Node IP address (e.g. 192.168.1.x or 100.x.x.x Tailscale): " NODE_IP
    read -p "RPC port [8332]: " NODE_PORT
    NODE_PORT=${NODE_PORT:-8332}
    read -p "RPC username: " RPC_USER
    read -s -p "RPC password: " RPC_PASS
    echo ""
    POOL_URL="http://${NODE_IP}:${NODE_PORT}"
    STRATUM_USER="$RPC_USER"
    POOL_PASS="$RPC_PASS"
    BACKUP_URL=""
    WORKER_NAME="phone"

else
    # ── Pool mode ─────────────────────────────────────────────────────────
    MINING_MODE="pool"
    echo ""
    echo -e "${DIM}Known CapStash pools:${RESET}"
    echo ""
    echo "  1) crypto-eire.com         — stratum+tcp://stratum.crypto-eire.com:3340"
    echo "  2) capspool.io PPLNS (CPU) — stratum+tcp://pplns.capspool.io:6333"
    echo "  3) papaspool.net      — stratum+tcp://papaspool.net:3333"
    echo "  4) 1miner.net         — stratum+tcp://1miner.net:3333"
    echo "  5) Enter manually"
    echo ""
    echo -e "${AMBER}  ⚠ Not all pools have confirmed low difficulty settings.${RESET}"
    echo -e "${AMBER}    Verify your hashrate after connecting or try another pool.${RESET}"
    echo ""
    read -p "Select pool (1-5): " POOL_CHOICE

    case $POOL_CHOICE in
        1) POOL_URL="stratum+tcp://stratum.crypto-eire.com:3340" ;;
        2) POOL_URL="stratum+tcp://pplns.capspool.io:6333" ;;
        3) POOL_URL="stratum+tcp://papaspool.net:3333" ;;
        4) POOL_URL="stratum+tcp://1miner.net:3333" ;;
        5)
            read -p "Primary pool URL (stratum+tcp://...): " POOL_URL
            ;;
        *)
            echo -e "${RED}Invalid selection — defaulting to manual entry${RESET}"
            read -p "Primary pool URL (stratum+tcp://...): " POOL_URL
            ;;
    esac

    # Backup pool
    echo ""
    echo -e "${DIM}Backup pool (used by start-backup.sh):${RESET}"
    echo "  1) crypto-eire.com"
    echo "  2) capspool.io"
    echo "  3) papaspool.net"
    echo "  4) 1miner.net"
    echo "  5) Enter manually"
    echo "  6) Skip — no backup"
    echo ""
    read -p "Select backup (1-6): " BACKUP_CHOICE

    case $BACKUP_CHOICE in
        1) BACKUP_URL="stratum+tcp://stratum.crypto-eire.com:3340" ;;
        2) BACKUP_URL="stratum+tcp://pplns.capspool.io:6333" ;;
        3) BACKUP_URL="stratum+tcp://papaspool.net:3333" ;;
        4) BACKUP_URL="stratum+tcp://1miner.net:3333" ;;
        5) read -p "Backup pool URL: " BACKUP_URL ;;
        *) BACKUP_URL="" ;;
    esac

    # Worker name
    echo ""
    read -p "Worker name (e.g. phone-1): " WORKER_NAME
    WORKER_NAME=${WORKER_NAME:-phone-1}
    POOL_PASS="x"
fi

# ── Reward address ────────────────────────────────────────────────────────
echo ""
read -p "CapStash reward address (cap1... or C... or 8...): " REWARD_ADDRESS

if [[ ! "$REWARD_ADDRESS" =~ ^(cap1|C|8) ]]; then
    echo -e "${AMBER}Warning: Address doesn't look like a CapStash address${RESET}"
    read -p "Continue anyway? (y/n): " ADDR_CONT
    [ "$ADDR_CONT" != "y" ] && exit 1
fi

# Pool mode: build stratum user from address + worker
if [ "$MINING_MODE" = "pool" ]; then
    STRATUM_USER="${REWARD_ADDRESS}.${WORKER_NAME}"
    echo -e "${DIM}Pool worker: $STRATUM_USER${RESET}"
fi

# ── Thread count ──────────────────────────────────────────────────────────
# Detect P-cores vs E-cores from /proc/cpuinfo CPU part numbers
# High part numbers = performance cores (A76/A78/X1/X2), low = efficiency (A55/A53)
echo ""
echo -e "${DIM}Detecting CPU core layout...${RESET}"

P_CORES=0
E_CORES=0
while IFS= read -r line; do
    part=$(echo "$line" | grep -o "0x[0-9a-fA-F]*" | head -1)
    case $part in
        0xd41|0xd44|0xd46|0xd47|0xd48|0xd4b|0xd4d)
            P_CORES=$((P_CORES + 1)) ;;  # A76, X1, A78, X2, X3, X4
        *)
            E_CORES=$((E_CORES + 1)) ;;  # A55, A53, etc
    esac
done < <(grep "CPU part" /proc/cpuinfo)

TOTAL_CORES=$((P_CORES + E_CORES))
[ $P_CORES -eq 0 ] && P_CORES=$((TOTAL_CORES / 2))
[ $P_CORES -lt 1 ] && P_CORES=1

echo ""
echo -e "${DIM}Total cores: $TOTAL_CORES  |  Performance: $P_CORES  |  Efficiency: $E_CORES${RESET}"
echo ""
echo -e "${DIM}Hashrate guide (Whirlpool-512, optimized):${RESET}"
echo -e "${DIM}  1 P-core  → ~840 KH/s${RESET}"
echo -e "${DIM}  2 P-cores → ~1.68 MH/s${RESET}"
echo -e "${DIM}  3 P-cores → ~2.52 MH/s${RESET}"
echo -e "${DIM}  4 P-cores → ~5.77 MH/s  ← sweet spot${RESET}"
echo -e "${AMBER}  ⚠ Never exceed your P-core count — E-cores kill hashrate${RESET}"
echo ""
read -p "Threads to use [$P_CORES]: " THREADS
THREADS=${THREADS:-$P_CORES}

# ── Step 6: Write config and scripts ─────────────────────────────────────
echo ""
echo -e "${AMBER}[6/6] Writing configuration...${RESET}"

# Save config
cat > "$CONFIG_FILE" << EOF
# CapStash Miner v${MINER_VERSION} Configuration
# Generated: $(date)

MINING_MODE=$MINING_MODE
POOL_URL=$POOL_URL
BACKUP_URL=${BACKUP_URL:-}
STRATUM_USER=$STRATUM_USER
POOL_PASS=$POOL_PASS
REWARD_ADDRESS=$REWARD_ADDRESS
THREADS=$THREADS
WORKER_NAME=${WORKER_NAME:-phone}
EOF

# ── Write start.sh ────────────────────────────────────────────────────────
if [ "$MINING_MODE" = "solo" ]; then
cat > "$INSTALL_DIR/start.sh" << EOF
#!/data/data/com.termux/files/usr/bin/bash
cd "$INSTALL_DIR"
echo ""
echo -e "\033[38;5;82m[capstash-miner] Starting solo mining...\033[0m"
echo -e "\033[2mNode:    $POOL_URL\033[0m"
echo -e "\033[2mAddress: $REWARD_ADDRESS\033[0m"
echo -e "\033[2mThreads: $THREADS\033[0m"
echo ""
./capstash-miner \\
    --url "$POOL_URL" \\
    --user "$STRATUM_USER" \\
    --pass "$POOL_PASS" \\
    --address "$REWARD_ADDRESS" \\
    --threads $THREADS
EOF
else
cat > "$INSTALL_DIR/start.sh" << EOF
#!/data/data/com.termux/files/usr/bin/bash
cd "$INSTALL_DIR"
echo ""
echo -e "\033[38;5;82m[capstash-miner] Starting pool mining...\033[0m"
echo -e "\033[2mPool:    $POOL_URL\033[0m"
echo -e "\033[2mWorker:  $STRATUM_USER\033[0m"
echo -e "\033[2mThreads: $THREADS\033[0m"
echo ""
./capstash-miner \\
    --url "$POOL_URL" \\
    --user "$STRATUM_USER" \\
    --pass "$POOL_PASS" \\
    --address "$REWARD_ADDRESS" \\
    --threads $THREADS
EOF
fi

# ── Backup pool script (pool mode only) ───────────────────────────────────
if [ "$MINING_MODE" = "pool" ] && [ -n "$BACKUP_URL" ]; then
cat > "$INSTALL_DIR/start-backup.sh" << EOF
#!/data/data/com.termux/files/usr/bin/bash
cd "$INSTALL_DIR"
echo ""
echo -e "\033[38;5;214m[capstash-miner] Starting on BACKUP pool...\033[0m"
echo -e "\033[2mPool:    $BACKUP_URL\033[0m"
echo -e "\033[2mWorker:  $STRATUM_USER\033[0m"
echo ""
./capstash-miner \\
    --url "$BACKUP_URL" \\
    --user "$STRATUM_USER" \\
    --pass "$POOL_PASS" \\
    --address "$REWARD_ADDRESS" \\
    --threads $THREADS
EOF
chmod +x "$INSTALL_DIR/start-backup.sh"
fi

# ── info.sh ───────────────────────────────────────────────────────────────
cat > "$INSTALL_DIR/info.sh" << EOF
#!/data/data/com.termux/files/usr/bin/bash
echo ""
echo -e "\033[38;5;82m[capstash-miner v${MINER_VERSION}] Current Configuration\033[0m"
echo -e "\033[2m────────────────────────────────────────\033[0m"
source "$CONFIG_FILE"
echo -e "  Mode:      \$MINING_MODE"
echo -e "  Pool/Node: \$POOL_URL"
echo -e "  Worker:    \$STRATUM_USER"
echo -e "  Address:   \$REWARD_ADDRESS"
echo -e "  Threads:   \$THREADS"
echo ""
EOF

# ── reconfigure.sh ────────────────────────────────────────────────────────
cat > "$INSTALL_DIR/reconfigure.sh" << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
curl -fsSL https://raw.githubusercontent.com/scratcher14/capstash-miner-android/main/reconfigure.sh -o "$HOME/reconfigure_new.sh"
chmod +x "$HOME/reconfigure_new.sh"
bash "$HOME/reconfigure_new.sh"
rm -f "$HOME/reconfigure_new.sh"
EOF

# Make all scripts executable
chmod +x \
    "$INSTALL_DIR/start.sh" \
    "$INSTALL_DIR/reconfigure.sh" \
    "$INSTALL_DIR/info.sh"

# ── Done ──────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}║         SETUP COMPLETE ✓                  ║${RESET}"
echo -e "${GREEN}╚═══════════════════════════════════════════╝${RESET}"
echo ""
echo -e "  Mode:     ${GREEN}$MINING_MODE${RESET}"
if [ "$MINING_MODE" = "pool" ]; then
echo -e "  Pool:     ${DIM}$POOL_URL${RESET}"
echo -e "  Worker:   ${DIM}$STRATUM_USER${RESET}"
else
echo -e "  Node:     ${DIM}$POOL_URL${RESET}"
fi
echo -e "  Address:  ${DIM}$REWARD_ADDRESS${RESET}"
echo -e "  Threads:  ${DIM}$THREADS${RESET}"
echo ""
echo -e "  ${GREEN}Start mining:${RESET}  cd ~/capstash-miner && ./start.sh"
echo -e "  ${GREEN}View config:${RESET}   cd ~/capstash-miner && ./info.sh"
echo -e "  ${GREEN}Reconfigure:${RESET}   cd ~/capstash-miner && ./reconfigure.sh"
echo ""
echo -e "  ${AMBER}Keep mining after closing Termux:${RESET}"
echo -e "  ${DIM}  termux-wake-lock${RESET}"
echo -e "  ${DIM}  ./start.sh${RESET}"
echo -e "  ${DIM}  (close Termux window — mining continues)${RESET}"
echo ""
echo -e "  ${AMBER}To stop:${RESET}"
echo -e "  ${DIM}  Press Ctrl+C  |  termux-wake-unlock${RESET}"
echo ""
