import os
import random
import sys
import subprocess
import time

def count_stripes_with_nodes(filepath, node_combinations):
    # 统计包含特定节点组合的条带数目
    counts = {tuple(nodes): 0 for nodes in node_combinations}  # 初始化计数

    with open(filepath, "r") as file:
        for line in file:
            # 获取当前条带的节点列表
            nodes = list(map(int, line.split()))

            # 遍历每个需要匹配的节点组合
            for nodes_to_check in node_combinations:
                if all(node in nodes for node in nodes_to_check):  # 如果当前条带包含这些节点
                    counts[tuple(nodes_to_check)] += 1

    # os.remove(filepath)
    return counts

def remove_elements(a, b):
    # 使用列表推导式筛选出a中不包含在b中的元素
    for x in b:
        if x in a:
            a.remove(x)
    return a

def usage():
    print(''' 
            #   1. number of agents [10|20|30|40]
            #   2. number of stripes [100]
            #   3. ecn [6]
            #   4. eck [4]
            #   5. fail node nums [2]
            #   6. fail node ids [0,1] 条带中必须存在的节点
            #   7. avoid node ids [2]  条带中必须不存在的节点
    ''')

def run_script_and_get_counts(NAGENTS, NSTRIPES, ECN, ECK, FAILNUM, FAILIDS, AOVID, stripestore_dir, node_combinations):
    # 生成节点数组
    numbers = list(range(NAGENTS))
    # add = ECN - FAILNUM
    add = ECN - len(FAILIDS)

    arrays = []
    for _ in range(NSTRIPES):
        # 首先确保每个数组包含FAILIDS中的节点
        array = FAILIDS.copy()

        # 剔除不希望rand选中的节点
        remaining_numbers = numbers.copy()
        remove_elements(remaining_numbers, array)
        remove_elements(remaining_numbers, AOVID)
        random.shuffle(remaining_numbers)
        array.extend(remaining_numbers[:add])

        # 将数组随机排序
        random.shuffle(array)
        arrays.append(array)

    # 生成条带内容
    placement = []  # 用来存储条带数据的列表
    for array in arrays:
        # 将数组中的元素转换为字符串并连接成一行
        line = " ".join(map(str, array))  # 将每个元素转换为字符串并以空格连接
        line += "\n"  # 添加换行符
        placement.append(line)

    # 写入文件
    filepath = f"{stripestore_dir}/simulation_{NAGENTS}_{NSTRIPES}_{ECN}_{ECK}_{FAILNUM}"
    with open(filepath, "w") as f:
        f.writelines(placement)  # 使用 writelines 写入字符串列表

    # 获取统计结果
    return count_stripes_with_nodes(filepath, node_combinations)

def get_average_counts(NAGENTS, NSTRIPES, ECN, ECK, FAILNUM, FAILIDS, AOVID, stripestore_dir, node_combinations, runs=20):
    # 记录所有运行的结果
    all_counts = {tuple(nodes): [] for nodes in node_combinations}

    for _ in range(runs):
        counts = run_script_and_get_counts(NAGENTS, NSTRIPES, ECN, ECK, FAILNUM, FAILIDS, AOVID, stripestore_dir, node_combinations)
        
        # 将结果添加到 all_counts 中
        for nodes in node_combinations:
            all_counts[tuple(nodes)].append(counts.get(tuple(nodes), 0))

    # 计算每个节点组合的平均值
    avg_counts = {tuple(nodes): sum(counts_list) / len(counts_list) for nodes, counts_list in all_counts.items()}

    return avg_counts

if len(sys.argv) < 6:
    usage()
    exit()

NAGENTS = int(sys.argv[1])
NSTRIPES = int(sys.argv[2])
ECN = int(sys.argv[3])
ECK = int(sys.argv[4])
FAILNUM = int(sys.argv[5])

# 拿到传递的命令行参数，处理 `#` 为分隔符
args = sys.argv[6:]

# 找到 `#` 并分隔 `FAILIDS` 和 `AOVID`
if ',' in args:
    # Split based on '#'
    split_idx = args.index(',')
    FAILIDS = [int(arg) for arg in args[:split_idx]]
    AOVID = [int(arg) for arg in args[split_idx + 1:] if arg != ',']
else:
    FAILIDS = [int(arg) for arg in args]

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir = f"{home_dir}/Multiple_ParaRC"
stripestore_dir = f"{proj_dir}/stripeStore"

# 定义需要检查的节点组合
node_combinations_pre = [
    [0],
    [0, 1],
    [0, 1, 2],
    [0, 1, 2, 3]
]

# 根据FAILNUM限制需要检查的组合数量
node_combinations = node_combinations_pre[:min(FAILNUM, 4)]

# 获取平均统计结果
avg_counts = get_average_counts(NAGENTS, NSTRIPES, ECN, ECK, FAILNUM, FAILIDS, AOVID, stripestore_dir, node_combinations)

# 输出平均统计结果
for nodes, avg_count in avg_counts.items():
    print(f"条带中包含节点 {nodes}: 平均数 = {avg_count:.2f} 个")

