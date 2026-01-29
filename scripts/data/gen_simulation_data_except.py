# usage:
# python gen_simulation_data.py
#   1. number of agents [10|20|30|40]
#   2. number of stripes [100]
#   3. ecn [6]
#   4. eck [4]
#   5. fail node id [0]
#   6. not fail node id [1]


# 为了实现1-faliure和2-failure的实验, 1-faliure的条带中需要避免出现多个错误节点

import os
import random
import sys
import subprocess
import time
system = "Multiple_ParaRC"
if len(sys.argv) < 7:
    print('''
        # usage:
        #   1. number of agents [10|20|30|40]
        #   2. number of stripes [100]
        #   3. ecn [6]
        #   4. eck [4]
        #   5. fail node id [0]
        #   6. survive node id [1]
    ''')
    exit()

NAGENTS=int(sys.argv[1])
NSTRIPES=int(sys.argv[2])
ECN=int(sys.argv[3])
ECK=int(sys.argv[4])
FAILID=int(sys.argv[5])
SURVIVE= [int(arg) for arg in sys.argv[6:]]

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
stripestore_dir = "{}/stripeStore".format(proj_dir)

# format of block placement: a NSTRIPES * ECN matrix
# each line includes the placement of a stripe, the ECN integer shows the ECN nodes that stores the blocks
# example of a line: 5 3 0 8 1 4
# meaning:  blk-0 stores in agent-5
#           blk-1 stores in agent-3
#           blk-2 stores in agent-0
#           blk-3 stores in agent-8
#           blk-4 stores in agent-1
#           blk-5 stores in agent4

# the goal of this script is to generate placement of NSTRIPES stripes
placement=[]

print("survive is ", SURVIVE)

for stripeid in range(NSTRIPES):
    nodelist=[]
    for i in range(ECN):
        tmpid = random.randint(0, NAGENTS-1)
        while tmpid in nodelist or tmpid in SURVIVE:
            tmpid = random.randint(0, NAGENTS-1)
        nodelist.append(tmpid)

    if 0 not in nodelist:
        idx = random.randint(0, len(nodelist)-1)
        nodelist[idx] = FAILID

    line = ""
    line_sim = ""
    for i in range(len(nodelist)):
        line += str(nodelist[i]) + " "
    line+="\n"
    placement.append(line)

# now we write placement into a file in stripestore
filepath="{}/simulation_{}_{}_{}_{}".format(stripestore_dir, NAGENTS, NSTRIPES, ECN, ECK)

f=open(filepath, "w")
for line in placement:
    f.write(line)
f.close()
