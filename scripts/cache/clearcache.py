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

# clear cache dir
clear_cache_dir=script_dir+"/cache"

# read all agents
agentnodes=[]
f=open(script_dir+"/cluster/"+CLUSTER+"/agents", "r")
for line in f:
    agent=line[:-1]
    agentnodes.append(agent)
f.close()
f=open(script_dir+"/cluster/"+CLUSTER+"/newnodes", "r")
for line in f:
    client=line[:-1]
    agentnodes.append(client)

# ssh to each agent and issue cache clear reqeust
for agent in agentnodes:
    cmd="ssh "+agent+" \"cd {}; bash cacheclear.sh\"".format(clear_cache_dir)
    os.system(cmd)
    print("finished clear cache on node", agent)
