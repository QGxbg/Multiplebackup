# usage:
# python: simTOR.py
#   1. cluster [office2|aliyunhdd]
#   2. bandwidth [Mbps] ?

import os
from random import seed
from random import randint
from random import random
import sys
import subprocess
import time
import threading
system = "Multiple_ParaRC"


if len(sys.argv) != 3:
    print("parameter error!")
    exit()

CLUSTER=sys.argv[1]
BANDWIDTH=int(sys.argv[2])

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# test dir
test_dir="{}/hdfs".format(script_dir)
data_dir="{}/data".format(test_dir)
cluster_dir="{}/cluster/{}".format(test_dir, CLUSTER)

# proj dir
proj_dir="{}/{}".format(home_dir,system)
script_dir = "{}/scripts".format(proj_dir)
block_dir="{}/blkDir".format(proj_dir)
stripestore_dir="{}/stripeStore".format(proj_dir)

# oec and hadoop dir
oec_proj_dir="{}/openec-et".format(home_dir)
oec_script_dir = "{}/script".format(oec_proj_dir)
stop_oec_filename = "stop.py"
oec_scripts_exp_dir = "{}/scripts_exp".format(oec_proj_dir)
update_configs_filename = "update_configs.sh"
restart_oec_filename = "restart_oec.sh"
oec_config_path = "{}/conf/sysSetting.xml".format(oec_proj_dir)
oec_hdfs_config_path = "{}/hdfs3.3.4-integration/conf/hdfs-site.xml".format(oec_proj_dir)

# read ips
node2nic={}
rack2nodes={}
allnodes=[]

# get node2nic
f=open(cluster_dir+"/node2nic")
for line in f:
    item=line[:-1].split(":")
    node=item[0]
    nic=item[1]
    node2nic[node]=nic

# get rack2nodes, include rack2agents and rack2clients
f=open(cluster_dir+"/rackarch")
for line in f:
    item=line[:-1].split(":")
    rack=item[0]
    nodelist=item[1].split(",")
    rack2nodes[rack]=nodelist
    allnodes.append(rack)
    for node in nodelist:
        allnodes.append(node)

# get node to gw
node2gw={}
for rack in rack2nodes:
    nodelist=rack2nodes[rack]
    for node in nodelist:
        node2gw[node] = rack

# 1. construct routing rules in each node
for node in node2gw:
    srcip = node
    srcgw = node2gw[node]

    for node in node2gw:
        dstip = node
        dstgw = node2gw[node]

        if srcgw != dstgw:
            # data sent from srcip to dst ip should go from srcip-srcgw-dstgw-dstip
            cmd = "ssh {} \"sudo route add -net {} netmask 255.255.255.255 gw {}\"".format(srcip, dstip, srcgw)
            print(cmd)
            os.system(cmd)

            cmd = "ssh {} \"sudo route add -net {} netmask 255.255.255.255 gw {}\"".format(srcgw, dstip, dstgw)
            print(cmd)
            os.system(cmd)
        else:
            # data sent from srcip to dstip does not need to go through gw
            continue;

# 2. construct bandwidth limiting in each gw
for rack in rack2nodes:
    nic = node2nic[rack]
    
    cmd = "ssh {} \"sudo tc qdisc del dev {} root\"".format(rack, nic)
    print(cmd)
    os.system(cmd)

    cmd = "ssh {} \"sudo tc qdisc add dev {} root handle 1: htb default 20\"".format(rack, nic)
    print(cmd)
    os.system(cmd)

    cmd = "ssh {} \"sudo tc class add dev {} parent 1: classid 1:1 htb rate {}mbit ceil {}mbit burst 3k\"".format(rack, nic, BANDWIDTH, BANDWIDTH)
    print(cmd)
    os.system(cmd)

    cmd = "ssh {} \"sudo tc filter add dev {} protocol ip parent 1: prio 7 u32 match ip dst 192.168.7.0/24 flowid 1:1\"".format(rack, nic)
    print(cmd)
    os.system(cmd)

