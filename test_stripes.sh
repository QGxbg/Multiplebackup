#!/bin/bash

# Test script for MulSim_parallel with Clay(14,10), 40 nodes
# Tests different stripe counts: 100, 200, 500, 1000
# Tests BATCH_METHOD: 0, 1, 2
# Failure scenario: nodes 0,1,2,3 fail (4 failures)

BINARY="./MulSim_parallel"
N=14
K=10
ECW=256
AGENTS=40
STANDBY=0
BATCHSIZE=4
METHOD=1
FAILURE_SIZE=4
# GEN_FAILIDS：生成 placement 时每条条带必须包含的节点
#   只需包含 0，确保每条条带都受节点0故障影响即可
GEN_FAILIDS="0"
# FAILURES：模拟时实际故障的节点集合（传给 MulSim_parallel）
FAILURES="0 1 2 3"
# AVOID_NODES：条带中必须不存在的节点（逗号后传给 gen 脚本）
# 留空表示无限制；如需排除某些节点，写成 "4 5"
AVOID_NODES=""
SCENARIO="scatter"
TOLERANCE=4

STRIPE_COUNTS=(100 200 300 400 500 600 700 800 900 1000)
BATCH_METHODS=(0 1 2)

echo "==================================================="
echo "  MulSim_parallel Stripe Scale Test"
echo "  Code: Clay($N, $K), Agents: $AGENTS"
echo "  Failure nodes: $FAILURES (count=$FAILURE_SIZE)"
echo "  BATCH_METHOD: ${BATCH_METHODS[*]}"
echo "==================================================="
echo ""

# --- Step 1: Generate any missing placement files ---
for STRIPES in "${STRIPE_COUNTS[@]}"; do
    PLACEMENT="stripeStore/simulation_${AGENTS}_${STRIPES}_${N}_${K}_${TOLERANCE}"
    if [ ! -f "$PLACEMENT" ]; then
        echo "[INFO] Generating placement: $PLACEMENT ..."
        # 参数格式：... FAILIDS , AVOIDIDS（逗号作为分隔符，避免节点可为空）
        python3 scripts/data/gen_simulation_data_multiple_failure.py \
            $AGENTS $STRIPES $N $K 4 $GEN_FAILIDS , $AVOID_NODES
        if [ $? -ne 0 ]; then
            echo "[ERROR] Failed to generate $PLACEMENT"
        fi
    fi
done

echo ""

# --- Step 2: Run MulSim_parallel for each (stripe count, batch method) ---
printf "%-12s  %-14s  %-20s  %-20s  %-14s\n" \
       "Stripes" "BatchMethod" "Overall Load (blks)" "Overall Bdwt (blks)" "Duration (μs)"
echo "--------------------------------------------------------------------------"

for STRIPES in "${STRIPE_COUNTS[@]}"; do
    PLACEMENT="stripeStore/simulation_${AGENTS}_${STRIPES}_${N}_${K}_${TOLERANCE}"

    if [ ! -f "$PLACEMENT" ]; then
        printf "%-12s  %-14s  %-20s  %-20s  %-14s\n" \
               "$STRIPES" "-" "MISSING FILE" "-" "-"
        continue
    fi

    for BM in "${BATCH_METHODS[@]}"; do
        OUTPUT=$("$BINARY" \
            "$PLACEMENT" \
            "$AGENTS" \
            "$STRIPES" \
            "Clay" \
            "$N" "$K" "$ECW" \
            "$SCENARIO" "$STANDBY" \
            "$BATCHSIZE" "$METHOD" "$BM" \
            "$FAILURE_SIZE" $FAILURES 2>/dev/null)

        LOAD=$(echo "$OUTPUT" | grep "\[RET\] overall load:" | awk '{print $4}')
        BDWT=$(echo "$OUTPUT" | grep "\[RET\] overall bdwt:" | awk '{print $4}')
        DUR=$(echo  "$OUTPUT" | grep "^duration ="          | awk '{print $3}')

        if [ -z "$LOAD" ]; then
            printf "%-12s  %-14s  %-20s  %-20s  %-14s\n" \
                   "$STRIPES" "$BM" "ERROR" "-" "-"
        else
            printf "%-12s  %-14s  %-20s  %-20s  %-14s\n" \
                   "$STRIPES" "$BM" "$LOAD" "$BDWT" "$DUR"
        fi
    done
    echo ""   # blank line between stripe groups
done

echo "Done."
