# python usage
#   python setbdwt.py
#       1. cluster [lab|aliyun]

import sys
import subprocess
import os

CLUSTER=sys.argv[1]
system = "Multiple_ParaRC"
# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
script_dir = "{}/scripts".format(proj_dir)
config_dir = "{}/conf".format(proj_dir)

CLUSTERDIR = "{}/cluster/{}".format(script_dir, CLUSTER)
WONDERSHAPER="{}/Tools/wondershaper".format(home_dir)

# get hostip and nic
iplist=[]
niclist=[]
ip2nicpath=CLUSTERDIR+"/ip2nic"
f=open(ip2nicpath, "r")
for line in f:
    item=line[:-1].split(":")
    ip = item[0]
    nic = item[1]
    iplist.append(ip)
    niclist.append(nic)

    #print(iplist)
    #print(niclist)

# ssh to host and set the bandwidth
hostnum = len(iplist)
for i in range(hostnum):
    ip = iplist[i]
    nic = niclist[i]
    #print(ip, nic)

    cmd = "ssh "+ip+" \"cd "+WONDERSHAPER+"; sudo ./wondershaper -c -a "+nic+"\""
    print(cmd)
    os.system(cmd)
