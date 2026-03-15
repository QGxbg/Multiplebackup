#!/bin/bash

# ==============================================================================
# Configuration Parameters
# You can modify these parameters to change the test setup
# ==============================================================================

# Cluster and environment settings
CLUSTER="lab2"                   # cluster name
BLOCK_SOURCE="standalone"       # standalone or hdfs

# Block and packet sizes
BLK_MB=1                        # block size in MiB
PKT_KB=4                       # packet size in KiB

# Erasure coding parameters
CODE="Clay"                     # name of the code (e.g., Clay/RSPIPE/RSCONV)
ECN=4                           # n (total number of nodes)
ECK=2                           # k (data nodes)
ECW=4                           # w

# System parameters
BATCH_SIZE=2                    # batch size
NSTRIPES=10                     # number of stripes

# Failure scenario
# FAIL_IDS: Node IDs that will fail
# MUST_EXIST_IDS: Node IDs that must be in the stripe but will NOT fail
FAIL_IDS="0 1"
MUST_EXIST_IDS="2 3"
AVOID_IDS=""

# ==============================================================================
# Test Execution Steps
# ==============================================================================

# Paths
PROJ_DIR="/home/lx/Multiple_ParaRC"
SCRIPTS_DIR="${PROJ_DIR}/scripts"

echo ">>> Step 1: Generating configuration (conf/createconf.py) ..."
cd ${PROJ_DIR}
python3 ${SCRIPTS_DIR}/conf/createconf.py \
    ${CLUSTER} ${BLOCK_SOURCE} ${BLK_MB} ${PKT_KB} ${CODE} ${ECN} ${ECK} \
    ${ECW} ${BATCH_SIZE}

if [ $? -ne 0 ]; then
    echo "Error in Step 1. Exiting..."
    exit 1
fi
echo "Configuration generated successfully."
echo ""

echo ">>> Step 2: Cleaning up existing standalone data ..."
python3 ${SCRIPTS_DIR}/data/clean_standalone_data.py ${CLUSTER}

if [ $? -ne 0 ]; then
    echo "Error in Step 2. Exiting..."
    exit 1
fi
echo "Data cleanup finished."
echo ""

echo ">>> Step 3: Generating data blocks (data/gen_standalone_data_multiple.py) ..."
# Combine MUST_EXIST_IDS and FAIL_IDS for the generator
# If AVOID_IDS is set, add a comma separator
if [ -n "$AVOID_IDS" ]; then
    python3 ${SCRIPTS_DIR}/data/gen_standalone_data_multiple.py \
        ${CLUSTER} ${NSTRIPES} ${CODE} ${ECN} ${ECK} ${ECW} ${BLK_MB} ${MUST_EXIST_IDS} ${FAIL_IDS} , ${AVOID_IDS}
else
    python3 ${SCRIPTS_DIR}/data/gen_standalone_data_multiple.py \
        ${CLUSTER} ${NSTRIPES} ${CODE} ${ECN} ${ECK} ${ECW} ${BLK_MB} ${MUST_EXIST_IDS} ${FAIL_IDS}
fi

if [ $? -ne 0 ]; then
    echo "Error in Step 2. Exiting..."
    exit 1
fi
echo "Data generated successfully."
echo ""

echo ">>> Step 4: Starting cluster nodes (start.py) ..."
python3 ${SCRIPTS_DIR}/start.py

if [ $? -ne 0 ]; then
    echo "Error in Step 4. Exiting..."
    exit 1
fi
echo "Cluster started successfully."
echo ""

# Additional MultipleCoor Parameters
METHOD="parallel"                # centralize, offline, parallel
SCENARIO="scatter"              # standby, scatter
FAIL_NODE_NUM=$(echo $FAIL_IDS | wc -w)

echo ">>> Step 5: Running MultipleCoor ..."
cd ${PROJ_DIR}
./MultipleCoor ${METHOD} ${SCENARIO} ${FAIL_NODE_NUM} ${FAIL_IDS}

if [ $? -ne 0 ]; then
    echo "Error in Step 5. Exiting..."
    exit 1
fi
echo "MultipleCoor finished successfully."
echo ""
echo "=== Test script finished successfully ==="
