#usage:
#   python fullnode.py
#       1.  cluster [lab]
#       2.  CODE [Clay|RDP|HHXORPlus|BUTTERFLY]
#       3.  ECN [4]
#       4.  ECK [2]
#       5.  ECW [4]
#       6.  method [centralize|offline|parallel]
#       7.  scenario [standby|scatter]
#       8.  blkMiB [1|256]
#       9.  pktKiB [64]
#       10. batchsize [3]
#       11. num stripes [20]
#       12. gendata [true|false]
#       13. BDWTMpbs [1000]

import os
import sys
import subprocess
import time

CLUSTER=sys.argv[1]
CODE=sys.argv[2]
ECN=int(sys.argv[3])
ECK=int(sys.argv[4])
ECW=int(sys.argv[5])
METHOD=sys.argv[6]
SCENARIO=sys.argv[7]
BLKMB=int(sys.argv[8])
PKTKB=int(sys.argv[9])
BATCHSIZE=int(sys.argv[10])
NSTRIPE=int(sys.argv[11])
GENDATASTR=sys.argv[12]
BDWT=int(sys.argv[13])

gendata=False
if GENDATASTR == "true":
    gendata = True

BLOCKSOURCE="hdfs"
FAILNODEID=0
system = "Multiple_ParaRC"

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
script_dir="{}/scripts".format(proj_dir)
data_dir="{}/data".format(script_dir)
conf_dir="{}/conf".format(script_dir)
cache_dir="{}/cache".format(script_dir)
network_dir="{}/network".format(script_dir)

exe="{}/build/ParaCoordinator".format(proj_dir)

# 0. stop $system
cmd="cd {}; python stop.py".format(script_dir)
os.system(cmd)

# 1. gen data
if gendata:
    cmd="cd {}; python clean_standalone_data.py {}".format(data_dir, CLUSTER)
    os.system(cmd)

    cmd="cd {}; python gen_standalone_data.py {} {} {} {} {} {} {} {}".format(data_dir, CLUSTER, NSTRIPE, CODE, ECN, ECK, ECW, BLKMB, FAILNODEID)
    os.system(cmd)

# 2. create configuration file for $system
cmd="cd {}; python createconf.py {} {} {} {} {} {} {} {} {}".format(conf_dir, CLUSTER, BLOCKSOURCE, BLKMB, PKTKB, CODE, ECN, ECK, ECW, BATCHSIZE)
os.system(cmd)

# 3. clear cache
cmd="cd {}; python clearcache.py {}".format(cache_dir, CLUSTER)
os.system(cmd)

# 4. set bdwt
cmd="cd {}; python setbdwt.py {} {}".format(network_dir, CLUSTER, BDWT)
os.system(cmd)
time.sleep(2)

# 5. start parafullnode
cmd="cd {}; python start.py".format(script_dir)
os.system(cmd)

# 6 repair
cmd="cd {}; ./build/ParaCoordinator {} {} {} > curr_output".format(proj_dir, METHOD, SCENARIO, FAILNODEID)
res=subprocess.Popen(['/bin/bash','-c',cmd], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
out, err = res.communicate()
print(out)

# 7. unset bdwt
cmd="cd {}; python clearbdwt.py {}".format(network_dir, CLUSTER)
os.system(cmd)

# 5. stop parafullnode
cmd="cd {}; python stop.py".format(script_dir)
os.system(cmd)
