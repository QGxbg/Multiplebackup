# usage:
#   python fn_hdfs.py
#       1. cluster [office|aliyunhdd|csencs1]
#       2. CODE [RSPIPE|Clay]
#       3. ECN [4|14]
#       4. ECK [2|10]
#       5. ECW [1|1]
#       6. blkmib [1|64|256]
#       7. pktkib [64]
#       8. num_stripes [20] (default: the number of agent nodes)
#       9. batchsize [29] (default: the number of agent nodes -1)
#       10. gen_files [true|false]
#       11. gen_blocks [true|false]
#       12. gen_meta [true|false]

# This script runs the full node recovery of a specific code

import os
import sys
import subprocess
import time

CLUSTER=sys.argv[1]
CODE=sys.argv[2]
ECN=int(sys.argv[3])
ECK=int(sys.argv[4])
ECW=int(sys.argv[5])
BLKMB=int(sys.argv[6])
PKTKB=int(sys.argv[7])
NUM_STRIPES=int(sys.argv[8])
BATCHSIZE=int(sys.argv[9])
GEN_FILES=sys.argv[10]
GEN_BLOCKS=sys.argv[11]
GEN_META=sys.argv[12]
system = "Multiple_ParaRC"
# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
script_dir = "{}/scripts".format(proj_dir)
stripeStore_dir = "{}/stripeStore".format(proj_dir)

# test dir
test_dir="{}/hdfs".format(script_dir)
data_dir=test_dir+"/data"
cluster_dir="{}/cluster/{}".format(test_dir, CLUSTER)

# 1. stop previous run
cmd="cd {}; python stop.py".format(script_dir)
os.system(cmd)

GEN_DATA="false"
if GEN_FILES == "true":
    GEN_DATA = "true"
if GEN_BLOCKS == "true":
    GEN_DATA = "true"
if GEN_META == "true":
    GEN_DATA = "true"


# 2. gendata
if GEN_DATA == "true":
    cmd="cd "+data_dir+"; python gendata_from_oec_fullnode.py "+CLUSTER+" "+str(NUM_STRIPES-1)+" "+CODE+" "+str(ECN)+" "+str(ECK)+" "+str(ECW)+" "+str(BLKMB)+" "+str(PKTKB)+" "+ GEN_FILES + " " + GEN_BLOCKS + " " + GEN_META + " " + str(BATCHSIZE)
    print(cmd)
    os.system(cmd)

# 3. conf
cmd="cd {}/conf; python3 createconf.py {} hdfs {} {} {} {} {} {} {}".format(script_dir, CLUSTER, BLKMB, PKTKB, CODE, ECN, ECK, ECW, BATCHSIZE)
print(cmd)
os.system(cmd)

# 4. run
cmd="cd {}".format(proj_dir)
print(cmd)
os.system(cmd)

cmd="python {}/pretest/bw.py {} {} {} {} {} parallel scatter {} {} {} {} false 1000 &> output".format(script_dir,CLUSTER, CODE, ECN, ECK, ECW, BLKMB, PKTKB, BATCHSIZE, NUM_STRIPES)
os.system(cmd)
