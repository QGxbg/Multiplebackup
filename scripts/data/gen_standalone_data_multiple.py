# usage:
# python gen_simulation_data.py
#   1. cluster [lab]
#   2. number of stripes [100]
#   3. code [Clay]
#   4. ecn [4]
#   5. eck [2]
#   6. ecw [4]
#   7. blkMB [1]
#   8. fail node ids [0,1]


import os
import random
import sys
import subprocess
import time

if len(sys.argv) < 9:
    print('''
    # usage:
    # python gen_standalone_data_multiple.py
    #   1. cluster [lab]
    #   2. number of stripes [100]
    #   3. code [Clay]
    #   4. ecn [4]
    #   5. eck [2]
    #   6. ecw [4]
    #   7. blkMB [1]
    #   8. must-exist node ids [0,1] (ends with , if avoid nodes follow)
    #   9. avoid node ids [2]
    ''')
    exit()

CLUSTER=sys.argv[1]
NSTRIPES=int(sys.argv[2])
CODE=sys.argv[3]
ECN=int(sys.argv[4])
ECK=int(sys.argv[5])
ECW=int(sys.argv[6])
BLKMB=int(sys.argv[7])

# Handle comma separator for MUST_EXIST and AVOID nodes
args = sys.argv[8:]
if ',' in args:
    split_idx = args.index(',')
    MUST_EXIST_IDS = [int(arg) for arg in args[:split_idx]]
    AVOID_IDS = [int(arg) for arg in args[split_idx + 1:] if arg != ',']
else:
    MUST_EXIST_IDS = [int(arg) for arg in args]
    AVOID_IDS = []

BLKBYTES=BLKMB * 1048576
system = "Multiple_ParaRC"
# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
stripestore_dir = "{}/stripeStore".format(proj_dir)
script_dir = "{}/scripts".format(proj_dir)
blk_dir = "{}/blkDir".format(proj_dir)

data_script_dir = "{}/data".format(script_dir)
cluster_dir = "{}/cluster/{}".format(script_dir, CLUSTER)

# read cluster structure
clusternodes=[]
controller=""
agentnodes=[]
repairnodes=[]

# read controller
f=open(cluster_dir+"/controller","r")
for line in f:
    controller=line.strip()
    if controller:
        clusternodes.append(controller)
f.close()

# read agentnodes
f=open(cluster_dir+"/agents","r")
for line in f:
    agent=line.strip()
    if agent:
        clusternodes.append(agent)
        agentnodes.append(agent)
f.close()

# read repairnodes
f=open(cluster_dir+"/newnodes","r")
for line in f:
    node=line.strip()
    if node:
        clusternodes.append(node)
        repairnodes.append(node)
f.close()

must_exist_locs = [agentnodes[i] for i in MUST_EXIST_IDS]
avoid_locs = [agentnodes[i] for i in AVOID_IDS]

print("Must-exist nodes: ", MUST_EXIST_IDS, " (", must_exist_locs, ")")
print("Avoid nodes: ", AVOID_IDS, " (", avoid_locs, ")")

# the goal of this script is to generate placement of NSTRIPES stripes
placement=[]

for stripeid in range(NSTRIPES):
    stripename = "{}-{}{}{}-{}".format(CODE, ECN, ECK, ECW, stripeid)

    blklist=[]
    loclist=must_exist_locs.copy()

    # Create a list of candidate agent nodes excluding must-exist and avoid nodes
    candidates = []
    for loc in agentnodes:
        if loc not in loclist and loc not in avoid_locs:
            candidates.append(loc)

    if len(loclist) + len(candidates) < ECN:
        print("Error: Not enough candidate nodes to satisfy constraints!")
        exit(1)

    random.shuffle(candidates)
    num_to_add = ECN - len(loclist)
    loclist.extend(candidates[:num_to_add])

    random.shuffle(loclist)
    print("stripe",stripeid,"source nodes: ", loclist)
    

    for blkid in range(len(loclist)):
        blkname = "{}-{}{}{}-{}-{}".format(CODE, ECN, ECK, ECW, stripeid, blkid)
        blklist.append(blkname)


    line = stripename + " "
    for i in range(ECN):
        line += blklist[i] + ":" + loclist[i] + " "
    line += "\n"
    placement.append(line)
    # print(line)
    

    # ssh to loclist[i] and generate a blklist[i]
    for i in range(len(blklist)):
       cmd = "ssh {} \"mkdir -p {}; dd if=/dev/urandom of={}/{} bs={} count=1 iflag=fullblock\"".format(loclist[i], blk_dir, blk_dir, blklist[i], BLKBYTES)
       # print(cmd)
       os.system(cmd)


# now we write placement into a file in stripestore
ssfilename="standalone_{}_{}{}{}_{}".format(NSTRIPES, CODE, ECN, ECK, ECW)
filepath="{}/{}".format(stripestore_dir, ssfilename)

f=open(filepath, "w")
for line in placement:
    f.write(line)
f.close()

cmd="cp {} {}/placement".format(filepath, stripestore_dir)
os.system(cmd)
