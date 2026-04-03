#!/bin/bash
# CapStash Miner — Pool Start Script (MacAir1)
# Usage: ./start-pool.sh [threads]

THREADS=${1:-1}
POOL="stratum+tcp://1miner.net:3690"
WALLET="cap1q0dt3gpxx6dvzy540ygyv0mhun4gsd4x64pc5wy"
WORKER="macair"

cd ~/Documents/capstash-miner-android/build && make -j$(nproc) 2>&1 | tail -3

cd ~/Documents/capstash-miner-android
./build/capstash-miner \
  --url "$POOL" \
  --user "${WALLET}.${WORKER}" \
  --pass x \
  --address "$WALLET" \
  --threads "$THREADS"
