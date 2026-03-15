#!/bin/bash

# Test script for MulSim across different node counts
# Tests 30, 40, 50 nodes with 1000 stripes
# Compares Centralize vs Parallel (BatchMethod 0, 1, 2)
# Code: Clay(14,10), fail nodes 0, 1, 2, 3

BIN_CENT="./MulSim_centralize"
BIN_PARA="./MulSim_parallel"
N=14
K=10
ECW=256
STRIPES=1000
STANDBY=0
BATCHSIZE=4
METHOD=1
FAILURE_SIZE=4
GEN_FAILIDS="0"
FAILURES="0 1 2 3"
AVOID_NODES=""
SCENARIO="scatter"
TOLERANCE=4

NODES_LIST=(30 40 50)
BATCH_METHODS=(0 1 2)

echo "==================================================="
echo "  MulSim Node Scale Benchmark (1000 stripes)"
echo "  Code: Clay($N, $K), Failures: $FAILURES"
echo "==================================================="
echo ""

# --- Step 1: Generate missing placements ---
for AGENTS in "${NODES_LIST[@]}"; do
    PLACEMENT="stripeStore/simulation_${AGENTS}_${STRIPES}_${N}_${K}_${TOLERANCE}"
    if [ ! -f "$PLACEMENT" ]; then
        echo "[INFO] Generating placement for $AGENTS nodes..."
        python3 scripts/data/gen_simulation_data_multiple_failure.py \
            $AGENTS $STRIPES $N $K $TOLERANCE $GEN_FAILIDS , $AVOID_NODES
    fi
done

echo ""
printf "%-8s  %-12s  %-14s  %-20s  %-20s  %-14s\n" \
       "Nodes" "Mode" "BatchMethod" "Overall Load (blks)" "Overall Bdwt (blks)" "Time/Lat"
echo "------------------------------------------------------------------------------------------------------"

for AGENTS in "${NODES_LIST[@]}"; do
    PLACEMENT="stripeStore/simulation_${AGENTS}_${STRIPES}_${N}_${K}_${TOLERANCE}"
    
    # 1. Centralize
    OUTPUT_C=$("$BIN_CENT" "$PLACEMENT" "$AGENTS" "$STRIPES" "Clay" "$N" "$K" "$ECW" \
               "$SCENARIO" "$STANDBY" "$BATCHSIZE" "$FAILURE_SIZE" $FAILURES 2>/dev/null)
    
    LOAD_C=$(echo "$OUTPUT_C" | grep "\[RET\] overall load:" | awk '{print $4}')
    BDWT_C=$(echo "$OUTPUT_C" | grep "\[RET\] overall bdwt:" | awk '{print $4}')
    LAT_C=$(echo  "$OUTPUT_C" | grep "get repair batch latency:" | awk '{print $5}')
    
    printf "%-8s  %-12s  %-14s  %-20s  %-20s  %-14s\n" \
           "$AGENTS" "Centralize" "-" "$LOAD_C" "$BDWT_C" "${LAT_C}ms"

    # 2. Parallel (BM 0, 1, 2)
    for BM in "${BATCH_METHODS[@]}"; do
        OUTPUT_P=$("$BIN_PARA" "$PLACEMENT" "$AGENTS" "$STRIPES" "Clay" "$N" "$K" "$ECW" \
                   "$SCENARIO" "$STANDBY" "$BATCHSIZE" "$METHOD" "$BM" \
                   "$FAILURE_SIZE" $FAILURES 2>/dev/null)
        
        LOAD_P=$(echo "$OUTPUT_P" | grep "\[RET\] overall load:" | awk '{print $4}')
        BDWT_P=$(echo "$OUTPUT_P" | grep "\[RET\] overall bdwt:" | awk '{print $4}')
        DUR_P=$(echo  "$OUTPUT_P" | grep "^duration ="          | awk '{print $3}')
        
        printf "%-8s  %-12s  %-14s  %-20s  %-20s  %-14s\n" \
               "$AGENTS" "Parallel" "$BM" "$LOAD_P" "$BDWT_P" "${DUR_P}μs"
    done
    echo "------------------------------------------------------------------------------------------------------"
done

echo "Done."
