# _*_ coding:utf-8 _*_  

import os
import random
import sys
import subprocess
import time

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


if len(sys.argv) < 6:
    usage()
    exit()

NAGENTS = int(sys.argv[1])
NSTRIPES = int(sys.argv[2])
ECN = int(sys.argv[3])
ECK = int(sys.argv[4])
FAILNUM = int(sys.argv[5])

# 拿到传递的命令行参数，处理 `,` 为分隔符
args = sys.argv[6:]

# 找到 `,` 并分隔 `FAILIDS` 和 `AOVID`
if ',' in args:
    # Split based on '#'
    split_idx = args.index(',')
    FAILIDS = [int(arg) for arg in args[:split_idx]]
    AOVID = [int(arg) for arg in args[split_idx + 1:] if arg != ',']
else:
    FAILIDS = [int(arg) for arg in args]
    AOVID = []   # 未传逗号时默认无需回避节点

print("Fail node ids = ", FAILIDS)
print("Avoid node ids = ", AOVID)

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir = "{}/Multiple_ParaRC".format(home_dir)
stripestore_dir = "{}/stripeStore".format(proj_dir)

# the goal of this script is to generate placement of NSTRIPES stripes
placement = []

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
    
    line = ""
    for i in range(len(array)):
        line += str(array[i]) + " "
    line += "\n"
    placement.append(line)

# now we write placement into a file in stripestore
filepath = "{}/simulation_{}_{}_{}_{}_{}".format(stripestore_dir, NAGENTS, NSTRIPES, ECN, ECK, FAILNUM)

with open(filepath, "w") as f:
    for line in placement:
        f.write(line)
