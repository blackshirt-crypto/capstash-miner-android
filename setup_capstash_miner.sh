#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  CapStash Miner v3.0.0 — Termux Setup Script
#  Whirlpool-512 CPU miner for CapStash
#  github.com/scratcher14/capstash-miner-android
# ============================================================

set -e

INSTALL_DIR="$HOME/capstash-miner"
REPO="https://github.com/scratcher14/capstash-miner-android"
CONFIG_FILE="$INSTALL_DIR/mining-config.txt"
MINER_VERSION="3.0.0"

GREEN='\033[38;5;82m'
AMBER='\033[38;5;214m'
RED='\033[38;5;196m'
DIM='\033[2m'
RESET='\033[0m'

clear
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}║    CAPSTASH MINER v3.0.0 SETUP            ║${RESET}"
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
pkg install -y -q clang cmake make git curl pkg-config
echo -e "${GREEN}✓ Build tools installed${RESET}"

# ── Step 3: Clone / update repository ────────────────────────────────────
echo -e "${AMBER}[3/6] Downloading capstash-miner source...${RESET}"
if [ -d "$INSTALL_DIR/.git" ]; then
    echo -e "${DIM}Existing installation found — updating...${RESET}"
    cd "$INSTALL_DIR"
    git fetch origin
    git reset --hard origin/main
    git clean -fd -q
else
    git clone -q "$REPO" "$INSTALL_DIR"
    cd "$INSTALL_DIR"
fi
echo -e "${GREEN}✓ Source downloaded${RESET}"

# ── Step 4: Build ─────────────────────────────────────────────────────────
echo -e "${AMBER}[4/6] Building miner (optimized for your CPU)...${RESET}"
echo -e "${DIM}This takes 1-3 minutes on most devices...${RESET}"

CPU_INFO=$(grep "Hardware" /proc/cpuinfo 2>/dev/null | head -1 || echo "ARM64")
echo -e "${DIM}Detected: $CPU_INFO${RESET}"

rm -rf build && mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DANDROID_BUILD=ON \
    -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    > /dev/null 2>&1

make -j$(nproc) 2>&1 | tail -3
cp capstash-miner "$INSTALL_DIR/capstash-miner"
cd "$INSTALL_DIR"
echo -e "${GREEN}✓ Build successful${RESET}"

# ── Step 5: Interactive configuration ─────────────────────────────────────
echo ""
echo -e "${AMBER}[5/6] Configuration${RESET}"
echo -e "${DIM}────────────────────────────────────────────${RESET}"
echo ""

# Mining mode
echo "Mining Mode:"
echo "  1) Solo — connect directly to your CapStash node via RPC"
echo "  2) Pool — connect to a mining pool via stratum"
echo ""
read -p "Select mode (1/2): " MODE_CHOICE

if [ "$MODE_CHOICE" = "1" ]; then
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
    MINING_MODE="pool"
    echo ""
    echo -e "${DIM}Known CapStash pools:${RESET}"
    echo ""
    echo "  1) capspool.io      — stratum+tcp://pplns.capspool.io:6333"
    echo "  2) papaspool.net    — stratum+tcp://papaspool.net:7777"
    echo "  3) crypto-eire.com  — stratum+tcp://stratum.crypto-eire.com:3340"
    echo "  4) 1miner.net       — stratum+tcp://1miner.net:3690"
    echo "  5) Enter manually"
    echo "  6) Enter manually"
    echo ""
    echo -e "${AMBER}  ⚠ Not all pools have confirmed low difficulty for mobile miners.${RESET}"
    echo -e "${AMBER}    Verify your hashrate after connecting or try another pool.${RESET}"
    echo ""
    read -p "Select pool (1-6): " POOL_CHOICE
    case $POOL_CHOICE in
        1) POOL_URL="stratum+tcp://pplns.capspool.io:6333" ;;
        2) POOL_URL="stratum+tcp://papaspool.net:7777" ;;
        3) POOL_URL="stratum+tcp://stratum.crypto-eire.com:3340" ;;
        4) POOL_URL="stratum+tcp://1miner.net:3690" ;;
        5) read -p "Pool URL (stratum+tcp://...): " POOL_URL ;;
        *) read -p "Pool URL (stratum+tcp://...): " POOL_URL ;;
    esac

    echo ""
    echo -e "${DIM}Backup pool (used by start-backup.sh):${RESET}"
    echo "  1) capspool.io      — stratum+tcp://pplns.capspool.io:6333"
    echo "  2) papaspool.net    — stratum+tcp://papaspool.net:7777"
    echo "  3) crypto-eire.com  — stratum+tcp://stratum.crypto-eire.com:3340"
    echo "  4) 1miner.net       — stratum+tcp://1miner.net:3690"
    echo "  5) Enter manually"
    echo "  6) Enter manually"
    echo "  7) Skip — no backup"
    echo ""
    read -p "Select backup (1-7): " BACKUP_CHOICE
    case $BACKUP_CHOICE in
        1) BACKUP_URL="stratum+tcp://pplns.capspool.io:6333" ;;
        2) BACKUP_URL="stratum+tcp://papaspool.net:7777" ;;
        3) BACKUP_URL="stratum+tcp://stratum.crypto-eire.com:3340" ;;
        4) BACKUP_URL="stratum+tcp://1miner.net:3690" ;;
        5) read -p "Backup pool URL: " BACKUP_URL ;;
        6) read -p "Backup pool URL: " BACKUP_URL ;;
        *) BACKUP_URL="" ;;
    esac

    echo ""
    read -p "Worker name (e.g. phone-1): " WORKER_NAME
    WORKER_NAME=${WORKER_NAME:-phone-1}
    POOL_PASS="x"
fi

# Reward address
echo ""
read -p "CapStash reward address (cap1... or C... or 8...): " REWARD_ADDRESS
if [[ ! "$REWARD_ADDRESS" =~ ^(cap1|C|8) ]]; then
    echo -e "${AMBER}Warning: Address doesn't look like a CapStash address${RESET}"
    read -p "Continue anyway? (y/n): " ADDR_CONT
    [ "$ADDR_CONT" != "y" ] && exit 1
fi

if [ "$MINING_MODE" = "pool" ]; then
    STRATUM_USER="${REWARD_ADDRESS}.${WORKER_NAME}"
    echo -e "${DIM}Pool worker: $STRATUM_USER${RESET}"
fi

# Thread count — detect P-cores
echo ""
echo -e "${DIM}Detecting CPU core layout...${RESET}"
P_CORES=0
E_CORES=0
while IFS= read -r line; do
    part=$(echo "$line" | grep -o "0x[0-9a-fA-F]*" | head -1)
    case $part in
        0xd41|0xd44|0xd46|0xd47|0xd48|0xd4b|0xd4d)
            P_CORES=$((P_CORES + 1)) ;;
        *)
            E_CORES=$((E_CORES + 1)) ;;
    esac
done < <(grep "CPU part" /proc/cpuinfo)
TOTAL_CORES=$((P_CORES + E_CORES))
[ $P_CORES -eq 0 ] && P_CORES=$((TOTAL_CORES / 2))
[ $P_CORES -lt 1 ] && P_CORES=1

echo ""
echo -e "${DIM}Total: $TOTAL_CORES  |  Performance: $P_CORES  |  Efficiency: $E_CORES${RESET}"
echo ""
echo -e "${DIM}Hashrate guide (v3.0.0 optimized):${RESET}"
echo -e "${DIM}  1 thread → ~1.47 MH/s${RESET}"
echo -e "${DIM}  2 threads → ~2.94 MH/s${RESET}"
echo -e "${DIM}  3 threads → ~4.40 MH/s${RESET}"
echo -e "${DIM}  4 threads → ~5.87 MH/s  ← safe sweet spot${RESET}"
echo -e "${DIM}  6 threads → ~8.81 MH/s  ← max (batteryless farms)${RESET}"
echo -e "${AMBER}  ⚠ Never exceed your P-core count — E-cores kill hashrate${RESET}"
echo ""
read -p "Threads to use [$P_CORES]: " THREADS
THREADS=${THREADS:-$P_CORES}

# ── Step 6: Write config and scripts ──────────────────────────────────────
echo ""
echo -e "${AMBER}[6/6] Writing configuration...${RESET}"

# Config file — unquoted EOF so variables expand
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

# start.sh — quoted 'EOF' so variables resolve at runtime from config
cat > "$INSTALL_DIR/start.sh" << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
INSTALL_DIR="$HOME/capstash-miner"
CONFIG="$INSTALL_DIR/mining-config.txt"
if [ ! -f "$CONFIG" ]; then
    echo "No config found. Run setup first."
    exit 1
fi
source "$CONFIG"
cd "$INSTALL_DIR"
echo ""
if [ "$MINING_MODE" = "solo" ]; then
    echo -e "\033[38;5;82m[capstash-miner] Starting solo mining...\033[0m"
    echo -e "\033[2mNode:    $POOL_URL\033[0m"
else
    echo -e "\033[38;5;82m[capstash-miner] Starting pool mining...\033[0m"
    echo -e "\033[2mPool:    $POOL_URL\033[0m"
    echo -e "\033[2mWorker:  $STRATUM_USER\033[0m"
fi
echo -e "\033[2mAddress: $REWARD_ADDRESS\033[0m"
echo -e "\033[2mThreads: $THREADS\033[0m"
echo ""
./capstash-miner \
    --url "$POOL_URL" \
    --user "$STRATUM_USER" \
    --pass "$POOL_PASS" \
    --address "$REWARD_ADDRESS" \
    --threads $THREADS
EOF

# start-backup.sh — quoted 'EOF' so variables resolve at runtime from config
cat > "$INSTALL_DIR/start-backup.sh" << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
INSTALL_DIR="$HOME/capstash-miner"
CONFIG="$INSTALL_DIR/mining-config.txt"
if [ ! -f "$CONFIG" ]; then
    echo "No config found. Run setup first."
    exit 1
fi
source "$CONFIG"
cd "$INSTALL_DIR"
if [ -z "$BACKUP_URL" ]; then
    echo "No backup pool configured. Run reconfigure.sh to add one."
    exit 1
fi
echo ""
echo -e "\033[38;5;214m[capstash-miner] Starting on BACKUP pool...\033[0m"
echo -e "\033[2mPool:    $BACKUP_URL\033[0m"
echo -e "\033[2mWorker:  $STRATUM_USER\033[0m"
echo -e "\033[2mThreads: $THREADS\033[0m"
echo ""
./capstash-miner \
    --url "$BACKUP_URL" \
    --user "$STRATUM_USER" \
    --pass "$POOL_PASS" \
    --address "$REWARD_ADDRESS" \
    --threads $THREADS
EOF

# info.sh — unquoted EOF so $CONFIG_FILE and $MINER_VERSION expand at write time
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

# reconfigure.sh — downloads fresh copy from GitHub
cat > "$INSTALL_DIR/reconfigure.sh" << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
curl -fsSL https://raw.githubusercontent.com/scratcher14/capstash-miner-android/main/reconfigure.sh -o "$HOME/reconfigure_new.sh"
chmod +x "$HOME/reconfigure_new.sh"
bash "$HOME/reconfigure_new.sh"
rm -f "$HOME/reconfigure_new.sh"
EOF

chmod +x \
    "$INSTALL_DIR/start.sh" \
    "$INSTALL_DIR/start-backup.sh" \
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
echo -e "  ${DIM}  (close Termux — mining continues)${RESET}"
echo ""
echo -e "  ${AMBER}To stop:${RESET}"
echo -e "  ${DIM}  Press Ctrl+C  |  termux-wake-unlock${RESET}"
echo ""
