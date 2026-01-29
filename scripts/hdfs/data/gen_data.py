# usage:
#   python gendata.py
#       1. cluster [aliyunhdd|office|csencs1]
#       2. number of stripes
#       3. code [Clay|RSPIPE|RDP|HH]
#       4. n
#       5. k
#       6. w
#       7. blockMiB
#       8. subpacketKiB

import os
import random
import sys
import subprocess
import time

if len(sys.argv) != 9:
    exit()

CLUSTER=sys.argv[1]
NSTRIPES=int(sys.argv[2])+1
CODE=sys.argv[3]
ECN=int(sys.argv[4])
ECK=int(sys.argv[5])
ECW=int(sys.argv[6])
BLOCKMB=float(sys.argv[7])
SUBPACKETKB=int(sys.argv[8])
PACKETKB=SUBPACKETKB*ECW
system = "Multiple_ParaRC"
# sizes
block_bytes = int(BLOCKMB * 1048576)
packet_bytes = int(PACKETKB * 1024)
subpacket_bytes = int(packet_bytes / ECW)

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
script_dir = "{}/scripts".format(proj_dir)
block_dir="{}/blkDir".format(proj_dir)
stripestore_dir="{}/stripeStore".format(proj_dir)

# test dir
test_dir="{}/hdfs".format(script_dir)
data_dir="{}/data".format(test_dir)
cluster_dir="{}/cluster/{}".format(test_dir, CLUSTER)

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
clusternodes=[]
controller=""
datanodes=[]
clientnodes=[]

# get controller
f=open(cluster_dir+"/dist_controller","r")
for line in f:
    controller=line[:-1]
    clusternodes.append(controller)
f.close()

# get datanodes
f=open(cluster_dir+"/dist_agents","r")
for line in f:
    agent=line[:-1]
    clusternodes.append(agent)
    datanodes.append(agent)
f.close()

# get clients
f=open(cluster_dir+"/dist_clients","r")
for line in f:
    client=line[:-1]
    clusternodes.append(client)
    clientnodes.append(client)
f.close()


## 1. clean data blocks in each node
#for dn in datanodes:
#    cmd = "ssh {} \"mkdir -p {}; rm {}/*\"".format(dn, block_dir, block_dir)
#    print(cmd)
#    os.system(cmd)
#
## 2. clean metadata
#cmd = "rm {}/*".format(stripestore_dir)
#print(cmd)
#os.system(cmd)

# 2. generate stripe of blocks
for stripeid in range(NSTRIPES):
    blklist = []
    iplist = []
    
    for blkid in range(ECK):
        blkname = CODE+"-"+str(stripeid)+"-"+str(blkid)
        blklist.append(blkname)

        # randomly choose one from datanodes
        tmpid = random.randint(0, len(datanodes)-1)
        tmpip = datanodes[tmpid]

        while tmpip in iplist:
            tmpid = random.randint(0, len(datanodes)-1)
            tmpip = datanodes[tmpid]
        iplist.append(tmpip)

    for blkid in range(ECK, ECN):
        blkname = CODE+"-"+str(stripeid)+"-"+str(blkid)
        blklist.append(blkname)

        # randomly choose one from datanodes
        tmpid = random.randint(0, len(datanodes)-1)
        tmpip = datanodes[tmpid]

        while tmpip in iplist:
            tmpid = random.randint(0, len(datanodes)-1)
            tmpip = datanodes[tmpid]

        iplist.append(tmpip)


    print(blklist)
    print(iplist)

    # now we ssh to each ip to place the blocks
    for blkid in range(len(blklist)):
        blkname = blklist[blkid]
        blkip = iplist[blkid]
        cmd = "ssh {} \"dd if=/dev/urandom of={}/{} bs={} count=1 iflag=fullblock\"".format(blkip, block_dir, blkname, block_bytes)
        print(cmd)
        os.system(cmd)

    # now we generate stripe metadata for the current stripe
    stripe_name ="{}_{}_{}_{}".format(CODE, ECN, ECK, stripeid)
    stripefile_path = "{}/{}.xml".format(stripestore_dir, stripe_name)

    content=[]

    line="<stripe>\n"
    content.append(line)

    line="<attribute><name>code</name><value>{}</value></attribute>\n".format(CODE)
    content.append(line)

    line="<attribute><name>ecn</name><value>{}</value></attribute>\n".format(ECN)
    content.append(line)

    line="<attribute><name>eck</name><value>{}</value></attribute>\n".format(ECK)
    content.append(line)

    line="<attribute><name>ecw</name><value>{}</value></attribute>\n".format(ECW)
    content.append(line)

    line="<attribute><name>stripename</name><value>{}</value></attribute>\n".format(stripe_name)
    content.append(line)

    line="<attribute><name>blocklist</name>\n"
    content.append(line)

    for blkid in range(len(blklist)):
        blkname = blklist[blkid]
        blkip = iplist[blkid]

        line="<value>{}:{}</value>\n".format(blkname, blkip)
        content.append(line)

    line="</attribute>\n"
    content.append(line)

    line="<attribute><name>blockbytes</name><value>{}</value></attribute>".format(block_bytes)
    content.append(line)

    line="<attribute><name>subpktbytes</name><value>{}</value></attribute>\n".format(subpacket_bytes)
    content.append(line)

    line="</stripe>\n"
    content.append(line)

    f=open(stripefile_path, "w")
    for line in content:
        f.write(line)
    f.close()
