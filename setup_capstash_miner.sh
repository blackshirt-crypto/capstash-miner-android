#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  CapStash Miner — Termux Setup Script
#  Whirlpool-512 CPU miner for CapStash
#  github.com/CapStash/capstash-miner-android
# ============================================================

set -e

INSTALL_DIR="$HOME/capstash-miner"
REPO="https://github.com/scratcher14/capstash-miner-android"
CONFIG_FILE="$INSTALL_DIR/mining-config.txt"

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
echo -e "${GREEN}║         CAPSTASH MINER SETUP              ║${RESET}"
echo -e "${GREEN}║    Whirlpool-512 · Android CPU Miner      ║${RESET}"
echo -e "${GREEN}║    github.com/CapStash                    ║${RESET}"
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

CPU_INFO=$(cat /proc/cpuinfo | grep "Hardware" | head -1 || echo "ARM64")
echo -e "${DIM}Detected: $CPU_INFO${RESET}"

mkdir -p build
cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DANDROID_BUILD=ON \
    -DCMAKE_C_FLAGS="-O3 -march=armv8.2-a+crypto+sha3 -mtune=cortex-a78 -fomit-frame-pointer -funroll-loops" \
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

# Mining mode
echo "Mining Mode:"
echo "  1) Solo   — connect directly to your CapStash node via RPC"
echo "  2) Pool   — connect to a mining pool via stratum"
echo ""
read -p "Select mode (1/2): " MODE_CHOICE

if [ "$MODE_CHOICE" = "1" ]; then
    MINING_MODE="solo"
    echo ""
    echo -e "${DIM}Solo mining connects to your CapStash node.${RESET}"
    echo -e "${DIM}Your node must have server=1 and rpcbind set in CapStash.conf${RESET}"
    echo ""
    read -p "Node IP address (e.g. 100.x.x.x or 192.168.1.x): " NODE_IP
    read -p "RPC port [8332]: " NODE_PORT
    NODE_PORT=${NODE_PORT:-8332}
    read -p "RPC username: " RPC_USER
    read -s -p "RPC password: " RPC_PASS
    echo ""
    POOL_URL="http://${NODE_IP}:${NODE_PORT}"
    POOL_PASS="$RPC_PASS"
    STRATUM_USER="$RPC_USER"
else
    MINING_MODE="pool"
    echo ""
    echo -e "${DIM}Pool mining uses stratum protocol.${RESET}"
    echo -e "${DIM}Format: stratum+tcp://pool.address:port${RESET}"
    echo ""
    read -p "Primary pool URL: " POOL_URL
    read -p "Backup pool URL (leave blank to skip): " BACKUP_URL
    read -p "Worker name (e.g. phone-1): " WORKER_NAME
    STRATUM_USER="WALLET_ADDRESS.$WORKER_NAME"
    POOL_PASS="x"
fi

# Reward address
echo ""
read -p "CapStash reward address (cap1... or C...): " REWARD_ADDRESS

# Validate address format
if [[ ! "$REWARD_ADDRESS" =~ ^(cap1|C|8) ]]; then
    echo -e "${AMBER}Warning: Address doesn't look like a CapStash address (cap1..., C..., 8...)${RESET}"
    read -p "Continue anyway? (y/n): " ADDR_CONT
    [ "$ADDR_CONT" != "y" ] && exit 1
fi

# Thread count
CORE_COUNT=$(nproc)
OPTIMAL=$((CORE_COUNT / 2))
[ $OPTIMAL -lt 1 ] && OPTIMAL=1
echo ""
echo -e "${DIM}Your device has $CORE_COUNT CPU threads.${RESET}"
echo -e "${DIM}Recommended: $OPTIMAL threads (50% avoids thermal throttling)${RESET}"
read -p "Threads to use [$OPTIMAL]: " THREADS
THREADS=${THREADS:-$OPTIMAL}

# ── Step 6: Write config and scripts ─────────────────────────────────────
echo ""
echo -e "${AMBER}[6/6] Writing configuration...${RESET}"

# Save config
cat > "$CONFIG_FILE" << EOF
# CapStash Miner Configuration
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

# Write start.sh (solo mode)
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
    --user "$RPC_USER" \\
    --pass "$RPC_PASS" \\
    --address "$REWARD_ADDRESS" \\
    --threads $THREADS
EOF

# Write start.sh (pool mode)
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

# Backup pool start script (pool mode only)
if [ "$MINING_MODE" = "pool" ] && [ -n "$BACKUP_URL" ]; then
cat > "$INSTALL_DIR/start-backup.sh" << EOF
#!/data/data/com.termux/files/usr/bin/bash
cd "$INSTALL_DIR"
echo -e "\033[38;5;214m[capstash-miner] Starting on BACKUP pool...\033[0m"
./capstash-miner \\
    --url "$BACKUP_URL" \\
    --user "$STRATUM_USER" \\
    --pass "$POOL_PASS" \\
    --address "$REWARD_ADDRESS" \\
    --threads $THREADS
EOF
chmod +x "$INSTALL_DIR/start-backup.sh"
fi

# reconfigure.sh
cat > "$INSTALL_DIR/reconfigure.sh" << 'RECONF'
#!/data/data/com.termux/files/usr/bin/bash
echo ""
echo -e "\033[38;5;82m[capstash-miner] Reconfiguration\033[0m"
echo ""
curl -fsSL https://raw.githubusercontent.com/CapStash/capstash-miner-android/main/setup_capstash_miner.sh | bash
RECONF

# info.sh
cat > "$INSTALL_DIR/info.sh" << EOF
#!/data/data/com.termux/files/usr/bin/bash
echo ""
echo -e "\033[38;5;82m[capstash-miner] Current Configuration\033[0m"
echo -e "\033[2m────────────────────────────────────────\033[0m"
source "$CONFIG_FILE"
echo -e "  Mode:     \$MINING_MODE"
echo -e "  Pool/Node: \$POOL_URL"
echo -e "  Address:  \$REWARD_ADDRESS"
echo -e "  Threads:  \$THREADS"
echo ""
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
echo -e "  ${GREEN}Start mining:${RESET}    cd ~/capstash-miner && ./start.sh"
echo -e "  ${GREEN}View config:${RESET}     cd ~/capstash-miner && ./info.sh"
echo -e "  ${GREEN}Reconfigure:${RESET}     cd ~/capstash-miner && ./reconfigure.sh"
echo ""
echo -e "  ${AMBER}Keep mining in background:${RESET}"
echo -e "  ${DIM}  termux-wake-lock${RESET}"
echo -e "  ${DIM}  ./start.sh${RESET}"
echo -e "  ${DIM}  (close Termux — mining continues)${RESET}"
echo ""
echo -e "  ${AMBER}To stop:${RESET}"
echo -e "  ${DIM}  Press Ctrl+C in Termux${RESET}"
echo -e "  ${DIM}  termux-wake-unlock${RESET}"
echo ""
