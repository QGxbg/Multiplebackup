# usage
# python createhdfs.py
#   1. BLKBYTES [1048576]
#   2. OECPKTBYTES [65536]

import os
import sys
import subprocess
system = "Multiple_ParaRC"
CLUSTER="aliyunhdd"
BLKBYTES=sys.argv[1]
PKTBYTES=sys.argv[2]

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()



# pararc source dir
proj_dir="{}/{}".format(home_dir,system)
script_dir = "{}/scripts".format(proj_dir)
config_dir = "{}/conf".format(proj_dir)
config_filename = "sysSetting.xml"
stripeStore_dir = "{}/stripeStore".format(proj_dir)
tradeoffPoint_dir = "{}/tradeoffPoint".format(proj_dir)
blk_dir = "{}/blkDir".format(proj_dir)

# pararc test script dir
test_dir="{}/hdfs".format(script_dir)
cluster_dir="{}/cluster/{}".format(test_dir, CLUSTER)

# hdfs configuration
cmd = r'echo $HADOOP_HOME'
hadoop_home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
hadoop_home_dir = hadoop_home_dir_str.decode().strip()
blk_dir = "{}/dfs/data/current".format(hadoop_home_dir)
hdfsconf_path = "{}/etc/hadoop".format(hadoop_home_dir)
print(hadoop_home_dir)
print(blk_dir)
print(hdfsconf_path)

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

for node in clusternodes:

    content=[]

    line = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    content.append(line)

    line = "<?xml-stylesheet type=\"text/xsl\" href=\"configuration.xsl\"?>\n"
    content.append(line)

    line = "<configuration>\n"
    content.append(line)

    line = "<property><name>dfs.replication</name><value>1</value></property>\n"
    content.append(line)

    line = "<property><name>dfs.blocksize</name><value>"+BLKBYTES+"</value></property>\n"
    content.append(line)

    line = "<property><name>dfs.block.replicator.classname</name><value>org.apache.hadoop.hdfs.server.blockmanagement.BlockPlacementPolicyOEC</value></property>\n"
    content.append(line)

    line = "<property><name>link.oec</name><value>true</value></property>\n"
    content.append(line)

    line = "<property><name>oec.controller.addr</name><value>"+controller+"</value></property>\n"
    content.append(line)

    line = "<property><name>oec.local.addr</name><value>"+node+"</value></property>\n"
    content.append(line)

    line = "<property><name>oec.pktsize</name><value>"+PKTBYTES+"</value></property>\n"
    content.append(line)

    line = "<property><name>dfs.blockreport.intervalMsec</name><value>10</value></property>\n"
    content.append(line)

    line = "<property><name>dfs.datanode.directoryscan.interval</name><value>10s</value></property>\n"
    content.append(line)

    line = "<property><name>dfs.client.max.block.acquire.failures</name><value>0</value></property>\n"
    content.append(line)

    line = "<property><name>dfs.namenode.fs-limits.min-block-size</name><value>1024</value></property>\n"
    content.append(line)

    line = "<property><name>dfs.client-write-packet-size</name><value>131072</value></property>\n"
    content.append(line)

    line = "<property><name>io.file.buffer.size</name><value>131072</value></property>\n"
    content.append(line)

    line = "</configuration>\n"
    content.append(line)

    f = open("hdfs-site.xml", "w")
    for line in content:
        f.write(line)
    f.close()

    targetpath = "{}/hdfs-site.xml".format(hdfsconf_path)

    cmd="scp hdfs-site.xml {}:{}".format(node, hdfsconf_path)
    os.system(cmd)

    cmd="rm hdfs-site.xml"
    os.system(cmd)

    print("finished create hdfs-site.xml on node", node)
