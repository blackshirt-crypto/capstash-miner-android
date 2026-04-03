#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  CapStash Miner v4.20.69 — Reconfiguration Menu
# ============================================================

INSTALL_DIR="$HOME/capstash-miner"
CONFIG_FILE="$INSTALL_DIR/mining-config.txt"

GREEN='\033[38;5;82m'
AMBER='\033[38;5;214m'
RED='\033[38;5;196m'
DIM='\033[2m'
RESET='\033[0m'

# ── Pool list (synced with setup_capstash_miner.sh) ───────────────────────
pick_pool() {
    # NOTE: All output except the final URL goes to stderr so the
    #       caller's $(...) capture only gets the clean URL string
    echo "" >&2
    echo -e "${DIM}Known CapStash pools:${RESET}" >&2
    echo "" >&2
    echo "  1) 1miner.net      — stratum+tcp://1miner.net:3690        (CPU friendly)" >&2
    echo "  2) crypto-eire.com — stratum+tcp://stratum.crypto-eire.com:3340 (CPU friendly)" >&2
    echo "  3) capspool.io     — stratum+tcp://pplns.capspool.io:6333  (GPU difficulty)" >&2
    echo "  4) papaspool.net   — stratum+tcp://papaspool.net:7777      (GPU difficulty)" >&2
    echo "  5) Enter manually" >&2
    echo "" >&2
    echo -e "${AMBER}  ⚠ Pools 3-4 may set difficulty too high for mobile/CPU miners.${RESET}" >&2
    echo -e "${AMBER}    Verify your hashrate after connecting or try another pool.${RESET}" >&2
    echo "" >&2
    read -p "Select pool (1-5): " POOL_CHOICE >&2
    case $POOL_CHOICE in
        1) echo "stratum+tcp://1miner.net:3690" ;;
        2) echo "stratum+tcp://stratum.crypto-eire.com:3340" ;;
        3) echo "stratum+tcp://pplns.capspool.io:6333" ;;
        4) echo "stratum+tcp://papaspool.net:7777" ;;
        *)
            # Manual entry — prompt to stderr, result to stdout
            read -p "Pool URL (stratum+tcp://...): " MANUAL_URL >&2
            echo "$MANUAL_URL" ;;
    esac
}

if [ ! -f "$CONFIG_FILE" ]; then
    echo -e "${RED}No config found. Run setup first.${RESET}"
    exit 1
fi

source "$CONFIG_FILE"

clear
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}║   CAPSTASH MINER v4.20.69 — RECONFIGUR    ║${RESET}"
echo -e "${GREEN}╚═══════════════════════════════════════════╝${RESET}"
echo ""
echo -e "${DIM}Current configuration:${RESET}"
echo -e "  Mode:      $MINING_MODE"
echo -e "  Pool/Node: $POOL_URL"
echo -e "  Worker:    ${STRATUM_USER:-$WORKER_NAME}"
echo -e "  Address:   $REWARD_ADDRESS"
echo -e "  Threads:   $THREADS"
echo ""
echo -e "${AMBER}What would you like to change?${RESET}"
echo ""
echo "  1) Mining mode (solo/pool)"
echo "  2) Node IP / Pool URL"
echo "  3) RPC credentials / Pool password"
echo "  4) Reward address"
echo "  5) Thread count"
echo "  6) Worker name"
echo "  7) Full reconfigure (re-run setup)"
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
        POOL_URL=$(pick_pool)
        read -p "Wallet address: " WALLET_ADDR
        read -p "Worker name [phone-1]: " WORKER_NAME
        WORKER_NAME=${WORKER_NAME:-phone-1}
        STRATUM_USER="${WALLET_ADDR}.${WORKER_NAME}"
        POOL_PASS="x"
        echo -e "${DIM}Pool worker: $STRATUM_USER${RESET}"
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
        echo -e "${DIM}Primary pool:${RESET}"
        POOL_URL=$(pick_pool)
        echo ""
        echo -e "${DIM}Update backup pool too?${RESET}"
        read -p "(y/n): " UPD_BACKUP
        if [ "$UPD_BACKUP" = "y" ]; then
            echo -e "${DIM}Backup pool:${RESET}"
            BACKUP_URL=$(pick_pool)
        fi
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
    # Update stratum user with new address if pool mode
    if [ "$MINING_MODE" = "pool" ]; then
        STRATUM_USER="${REWARD_ADDRESS}.${WORKER_NAME:-phone-1}"
        echo -e "${DIM}Updated pool worker: $STRATUM_USER${RESET}"
    fi
    ;;

  5)
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
    TOTAL=$((P_CORES + E_CORES))
    [ $P_CORES -eq 0 ] && P_CORES=$((TOTAL / 2))
    [ $P_CORES -lt 1 ] && P_CORES=1
    echo ""
    echo -e "${DIM}Total: $TOTAL  |  Performance: $P_CORES  |  Efficiency: $E_CORES${RESET}"
    echo -e "${AMBER}  ⚠ Never exceed your P-core count — E-cores kill hashrate${RESET}"
    echo ""
    read -p "Thread count [$P_CORES]: " THREADS
    THREADS=${THREADS:-$P_CORES}
    ;;

  6)
    echo ""
    read -p "Worker name (e.g. phone-1): " WORKER_NAME
    if [ "$MINING_MODE" = "pool" ]; then
        STRATUM_USER="${REWARD_ADDRESS}.${WORKER_NAME}"
        echo -e "${DIM}Updated pool worker: $STRATUM_USER${RESET}"
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
# CapStash Miner v4.20.69 Configuration
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

# ── Rewrite start.sh ──────────────────────────────────────────────────────
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
