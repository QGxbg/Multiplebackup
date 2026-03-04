# 将节点 IP 或主机名放入列表
NODES=("datanode1" "datanode2" "datanode3" "datanode4")
TARGET_DIR="/home/lx/Multiple_ParaRC"

for node in "${NODES[@]}"; do
    echo "正在处理节点: $node"
    # -t 参数强制分配伪终端，防止 sudo 报错
    ssh -t lx@$node "sudo rm -rf $TARGET_DIR"
done