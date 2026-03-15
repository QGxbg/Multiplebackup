#!/bin/bash

# Test script for MulSim_centralize with Clay(14,10), 40 nodes
# Tests different stripe counts: 100 to 1000
# Failure scenario: nodes 0,1,2,3 fail (4 failures)

BINARY="./MulSim_centralize"
N=14
K=10
ECW=256
AGENTS=40
STANDBY=0
BATCHSIZE=4
FAILURE_SIZE=4
# GEN_FAILIDS：生成 placement 时每条条带必须包含的节点
GEN_FAILIDS="0"
# FAILURES：模拟时实际故障的节点集合
FAILURES="0 1 2 3"
# AVOID_NODES：条带中必须不存在的节点
AVOID_NODES=""
SCENARIO="scatter"
TOLERANCE=4

STRIPE_COUNTS=(100 200 300 400 500 600 700 800 900 1000)

echo "==================================================="
echo "  MulSim_centralize Stripe Scale Test"
echo "  Code: Clay($N, $K), Agents: $AGENTS"
echo "  Failure nodes: $FAILURES (count=$FAILURE_SIZE)"
echo "==================================================="
echo ""

# --- Step 1: Generate any missing placement files ---
for STRIPES in "${STRIPE_COUNTS[@]}"; do
    PLACEMENT="stripeStore/simulation_${AGENTS}_${STRIPES}_${N}_${K}_${TOLERANCE}"
    if [ ! -f "$PLACEMENT" ]; then
        echo "[INFO] Generating placement: $PLACEMENT ..."
        python3 scripts/data/gen_simulation_data_multiple_failure.py \
            $AGENTS $STRIPES $N $K 1 $GEN_FAILIDS , $AVOID_NODES
        if [ $? -ne 0 ]; then
            echo "[ERROR] Failed to generate $PLACEMENT"
        fi
    fi
done

echo ""
echo "--- Results ---"
printf "%-12s  %-20s  %-20s  %-20s\n" \
       "Stripes" "Overall Load (blks)" "Overall Bdwt (blks)" "Latency (ms)"
echo "--------------------------------------------------------------------------------"

# --- Step 2: Run MulSim_centralize for each stripe count ---
for STRIPES in "${STRIPE_COUNTS[@]}"; do
    PLACEMENT="stripeStore/simulation_${AGENTS}_${STRIPES}_${N}_${K}_${TOLERANCE}"

    if [ ! -f "$PLACEMENT" ]; then
        printf "%-12s  %-20s  %-20s  %-20s\n" "$STRIPES" "MISSING FILE" "-" "-"
        continue
    fi

    # MulSim_centralize arguments: 
    # placement agents stripes code n k w scenario standby batchsize fail_size fail_ids
    OUTPUT=$("$BINARY" \
        "$PLACEMENT" \
        "$AGENTS" \
        "$STRIPES" \
        "Clay" \
        "$N" "$K" "$ECW" \
        "$SCENARIO" "$STANDBY" \
        "$BATCHSIZE" \
        "$FAILURE_SIZE" $FAILURES 2>/dev/null)

    LOAD=$(echo "$OUTPUT" | grep "\[RET\] overall load:" | awk '{print $4}')
    BDWT=$(echo "$OUTPUT" | grep "\[RET\] overall bdwt:" | awk '{print $4}')
    LAT=$(echo  "$OUTPUT" | grep "get repair batch latency:" | awk '{print $5}')

    if [ -z "$LOAD" ]; then
        printf "%-12s  %-20s  %-20s  %-20s\n" "$STRIPES" "ERROR" "-" "-"
    else
        printf "%-12s  %-20s  %-20s  %-20s\n" "$STRIPES" "$LOAD" "$BDWT" "$LAT"
    fi
done

echo ""
echo "Done."
