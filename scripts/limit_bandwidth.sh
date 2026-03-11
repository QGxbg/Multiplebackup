#!/bin/bash

# Default values
INTERFACE="eth0"
RATE="100mbit"
ACTION="start"

usage() {
    echo "Usage: $0 [start|stop|status] [-i interface] [-r rate]"
    echo "  start  : Start bandwidth limitation"
    echo "  stop   : Stop bandwidth limitation"
    echo "  status : Show current bandwidth limit status"
    echo "  -i     : Network interface (default: eth0, check 'ip addr' or 'ifconfig' for your actual interface)"
    echo "  -r     : Rate limit (default: 100mbit, e.g., 10mbit, 1gbit, 500kbit)"
    exit 1
}

# Parse command line arguments
if [ $# -eq 0 ]; then
    usage
fi

ACTION=$1
shift

while getopts "i:r:h" opt; do
    case ${opt} in
        i )
            INTERFACE=$OPTARG
            ;;
        r )
            RATE=$OPTARG
            ;;
        h )
            usage
            ;;
        \? )
            usage
            ;;
    esac
done

case "$ACTION" in
    start)
        echo "Starting bandwidth limitation on $INTERFACE to $RATE..."
        # Clear existing rules first to avoid errors
        sudo tc qdisc del dev $INTERFACE root 2>/dev/null
        
        # Add a Token Bucket Filter (tbf) rule
        # burst is typically around rate / 100 to buffer bursts, latency is max delay
        sudo tc qdisc add dev $INTERFACE root tbf rate $RATE burst 1mbit latency 50ms
        
        if [ $? -eq 0 ]; then
            echo "Successfully limited $INTERFACE bandwidth to $RATE."
        else
            echo "Failed to apply bandwidth limit on $INTERFACE (does the interface exist?)."
        fi
        ;;
    stop)
        echo "Stopping bandwidth limitation on $INTERFACE..."
        sudo tc qdisc del dev $INTERFACE root 2>/dev/null
        
        if [ $? -eq 0 ]; then
            echo "Successfully removed bandwidth limits on $INTERFACE."
        else
            echo "Failed to remove limit or no limit existed on $INTERFACE."
        fi
        ;;
    status)
        echo "Current tc qdisc rules on $INTERFACE:"
        sudo tc qdisc show dev $INTERFACE
        ;;
    *)
        usage
        ;;
esac
