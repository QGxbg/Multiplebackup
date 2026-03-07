#!/bin/bash

# ==============================================================================
# Sync Script
# This script synchronizes the compiled Multiple_ParaRC folder from the controller
# to all agents and newnodes defined in the cluster setup using scp.
# ==============================================================================

CLUSTER=${1:-"lab2"}

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
    echo "==============================="
    echo "Syncing to $node..."
    
    # Delete the directory first on remote
    echo "Removing remote directory ${PROJ_DIR}/ ..."
    ssh $node "rm -rf ${PROJ_DIR}/"

    # We use scp instead of rsync because rsync is hanging on the connection
    # Note: scp doesn't have an easy --exclude flag like rsync. If you want to transfer
    # faster, you might need to clean the local build/ directories before running this script,
    # or ignore the time it takes to transfer them.
    echo "Copying local directory with scp..."
    scp -r "${PROJ_DIR}/" "$node:${PROJ_DIR}/"
    echo "Done for $node."
done

echo "--------------------------------------------------------"
echo "=== Sync finished successfully ==="
