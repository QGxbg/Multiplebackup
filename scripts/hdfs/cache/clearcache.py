# usage:
#   python clearcache.py
#       1. cluster[office|aliyunhdd|csencs1]

import os
import sys
import subprocess

def usage():
    print("python clearcache.py cluster[office|aliyunhdd|csencs1]")

if len(sys.argv) != 2:
    usage()
    exit()


CLUSTER=sys.argv[1]
system = "Multiple_ParaRC"

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
script_dir = "{}/scripts".format(proj_dir)

# test dir
test_dir="{}/hdfs".format(script_dir)
data_dir=test_dir+"/data"

# clear cache dir
clear_cache_dir=script_dir

# read all agents
agentnodes=[]
f=open(test_dir+"/cluster/"+CLUSTER+"/dist_agents", "r")
for line in f:
    agent=line[:-1]
    agentnodes.append(agent)
f.close()
f=open(test_dir+"/cluster/"+CLUSTER+"/dist_clients", "r")
for line in f:
    client=line[:-1]
    agentnodes.append(client)

# ssh to each agent and issue cache clear reqeust
for agent in agentnodes:
    cmd="ssh "+agent+" \"cd {}; bash cacheclear.sh\"".format(clear_cache_dir)
    os.system(cmd)
    print("finished clear cache on node", agent)
