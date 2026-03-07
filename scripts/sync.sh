#!/bin/bash

# ==============================================================================
# Sync Script
# This script synchronizes the compiled Multiple_ParaRC folder from the controller
# to all agents and newnodes defined in the cluster setup.
# ==============================================================================

CLUSTER=${1:-"lab"}

PROJ_DIR="/home/lx/Multiple_ParaRC"
SCRIPTS_DIR="${PROJ_DIR}/scripts"
CLUSTER_DIR="${SCRIPTS_DIR}/cluster/${CLUSTER}"

if [ ! -d "${CLUSTER_DIR}" ]; then
    echo "Error: Cluster directory not found at ${CLUSTER_DIR}"
    exit 1
fi

# Read all agent and newnode IPs
AGENTS=$(cat "${CLUSTER_DIR}/agents" 2>/dev/null)
NEWNODES=$(cat "${CLUSTER_DIR}/newnodes" 2>/dev/null)

# Combine them into one list
ALL_NODES="${AGENTS} ${NEWNODES}"

echo ">>> Starting sync for cluster '${CLUSTER}'..."
echo "Target nodes:"
echo "${ALL_NODES}"
echo "--------------------------------------------------------"

for node in ${ALL_NODES}; do
    if [ -z "$node" ]; then
        continue
    fi
    echo "Syncing to $node..."
    # rsync will synchronize the folder. We exclude data/log folders so it transfers faster.
    # Note: --delete will remove files on destination that don't exist on source.
    rsync -avz --exclude '.git' \
               --exclude 'blkDir' \
               --exclude 'stripeStore' \
               --exclude 'offline' \
               --exclude 'build' \
               --exclude 'CMakeFiles' \
               --exclude 'CMakeCache.txt' \
               --exclude 'core.*' \
               "${PROJ_DIR}/" "$node:${PROJ_DIR}/"
done

echo "--------------------------------------------------------"
echo "=== Sync finished successfully ==="
