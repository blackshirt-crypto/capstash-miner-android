#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  CapStash Miner — Reconfiguration Menu
# ============================================================

INSTALL_DIR="$HOME/capstash-miner"
CONFIG_FILE="$INSTALL_DIR/mining-config.txt"

GREEN='\033[38;5;82m'
AMBER='\033[38;5;214m'
RED='\033[38;5;196m'
DIM='\033[2m'
RESET='\033[0m'

if [ ! -f "$CONFIG_FILE" ]; then
    echo -e "${RED}No config found. Run setup first.${RESET}"
    exit 1
fi

source "$CONFIG_FILE"

clear
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}║     CAPSTASH MINER — RECONFIGURE          ║${RESET}"
echo -e "${GREEN}╚═══════════════════════════════════════════╝${RESET}"
echo ""
echo -e "${DIM}Current configuration:${RESET}"
echo -e "  Mode:     $MINING_MODE"
echo -e "  Node/Pool: $POOL_URL"
echo -e "  Address:  $REWARD_ADDRESS"
echo -e "  Threads:  $THREADS"
echo -e "  Worker:   ${WORKER_NAME:-phone}"
echo ""
echo -e "${AMBER}What would you like to change?${RESET}"
echo ""
echo "  1) Mining mode (solo/pool)"
echo "  2) Node IP / Pool URL"
echo "  3) RPC credentials / Pool password"
echo "  4) Reward address"
echo "  5) Thread count"
echo "  6) Worker name"
echo "  7) Change everything (full reconfigure)"
echo "  8) Exit — no changes"
echo ""
read -p "Select option (1-8): " CHOICE

case $CHOICE in

  1)
    echo ""
    echo "Mining Mode:"
    echo "  1) Solo — direct RPC to your node"
    echo "  2) Pool — stratum protocol"
    read -p "Select (1/2): " MODE_CHOICE
    if [ "$MODE_CHOICE" = "1" ]; then
        MINING_MODE="solo"
        read -p "Node IP: " NODE_IP
        read -p "RPC port [8332]: " NODE_PORT
        NODE_PORT=${NODE_PORT:-8332}
        read -p "RPC username: " RPC_USER
        read -s -p "RPC password: " RPC_PASS
        echo ""
        POOL_URL="http://${NODE_IP}:${NODE_PORT}"
        STRATUM_USER="$RPC_USER"
        POOL_PASS="$RPC_PASS"
    else
        MINING_MODE="pool"
        read -p "Pool URL (stratum+tcp://...): " POOL_URL
        read -p "Wallet.worker: " STRATUM_USER
        POOL_PASS="x"
    fi
    ;;

  2)
    echo ""
    if [ "$MINING_MODE" = "solo" ]; then
        read -p "New node IP: " NODE_IP
        read -p "RPC port [8332]: " NODE_PORT
        NODE_PORT=${NODE_PORT:-8332}
        POOL_URL="http://${NODE_IP}:${NODE_PORT}"
    else
        read -p "New pool URL (stratum+tcp://...): " POOL_URL
        read -p "Backup pool URL (leave blank to skip): " BACKUP_URL
    fi
    ;;

  3)
    echo ""
    if [ "$MINING_MODE" = "solo" ]; then
        read -p "RPC username: " STRATUM_USER
        read -s -p "RPC password: " POOL_PASS
        echo ""
    else
        read -p "Pool password [x]: " POOL_PASS
        POOL_PASS=${POOL_PASS:-x}
    fi
    ;;

  4)
    echo ""
    read -p "New reward address (cap1... or C... or 8...): " REWARD_ADDRESS
    if [[ ! "$REWARD_ADDRESS" =~ ^(cap1|C|8) ]]; then
        echo -e "${AMBER}Warning: doesn't look like a CapStash address${RESET}"
        read -p "Continue? (y/n): " CONT
        [ "$CONT" != "y" ] && exit 1
    fi
    ;;

  5)
    echo ""
    CORE_COUNT=$(nproc)
    OPTIMAL=$((CORE_COUNT / 2))
    echo -e "${DIM}Device has $CORE_COUNT threads. Recommended: $OPTIMAL${RESET}"
    read -p "Thread count [$OPTIMAL]: " THREADS
    THREADS=${THREADS:-$OPTIMAL}
    ;;

  6)
    echo ""
    read -p "Worker name (e.g. phone-1): " WORKER_NAME
    if [ "$MINING_MODE" = "pool" ]; then
        read -p "Wallet address for pool user: " WALLET_ADDR
        STRATUM_USER="${WALLET_ADDR}.${WORKER_NAME}"
    fi
    ;;

  7)
    curl -fsSL https://raw.githubusercontent.com/scratcher14/capstash-miner-android/main/setup_capstash_miner.sh | bash
    exit 0
    ;;

  8)
    echo -e "${DIM}No changes made.${RESET}"
    exit 0
    ;;

  *)
    echo -e "${RED}Invalid option.${RESET}"
    exit 1
    ;;
esac

# ── Save updated config ───────────────────────────────────────────────────
cat > "$CONFIG_FILE" << EOF
# CapStash Miner Configuration
# Updated: $(date)

MINING_MODE=$MINING_MODE
POOL_URL=$POOL_URL
BACKUP_URL=${BACKUP_URL:-}
STRATUM_USER=$STRATUM_USER
POOL_PASS=$POOL_PASS
REWARD_ADDRESS=$REWARD_ADDRESS
THREADS=$THREADS
WORKER_NAME=${WORKER_NAME:-phone}
EOF

# ── Rewrite start.sh with new settings ───────────────────────────────────
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

chmod +x "$INSTALL_DIR/start.sh"

echo ""
echo -e "${GREEN}✓ Configuration updated${RESET}"
echo ""
echo -e "  ${DIM}Start mining: cd ~/capstash-miner && ./start.sh${RESET}"
echo ""
