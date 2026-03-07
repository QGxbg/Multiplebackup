# usage
#   python createconf.py
#       1. cluster [lab]
#       2. block_source [standalone|hdfs]
#       3. blockMiB [1]
#       4. pktKB [64]
#       5. code [Clay]
#       6. ecn [4]
#       7. eck [2]
#       8. ecw [4]
#       9. batch [3]

import os
import sys
import subprocess
system = "Multiple_ParaRC"
def usage():
    print("python createconf.py cluster[lab] block_source[standalone|hdfs]")


if len(sys.argv) < 10:
    usage()
    exit()

CLUSTER=sys.argv[1]
block_source=sys.argv[2]
BLKMB=int(sys.argv[3])
PKTKB=int(sys.argv[4])
CODE=sys.argv[5]
ECN=int(sys.argv[6])
ECK=int(sys.argv[7])
ECW=int(sys.argv[8])
BATCHSIZE=int(sys.argv[9])

RECVGROUP=10
SENDGROUP=10
COMPUTEGROUP=10

BLKBYTES=BLKMB*1048576
PKTBYTES=PKTKB*1024

ECCSIZE="10"
RPTHREADS="4"

# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
script_dir = "{}/scripts".format(proj_dir)
config_dir = "{}/conf".format(proj_dir)

gen_conf_dir = "{}/conf".format(script_dir)
cluster_dir = "{}/cluster/{}".format(script_dir, CLUSTER)

config_filename = "sysSetting.xml"
stripeStore_dir = "{}/stripeStore".format(proj_dir)
tradeoffPoint_dir = "{}/offline".format(proj_dir)
blk_dir = "{}/blkDir".format(proj_dir)

if block_source == "hdfs":
   cmd = r'echo $HADOOP_HOME'
   hadoop_home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
   hadoop_home_dir = hadoop_home_dir_str.decode().strip()
   blk_dir = "{}/dfs/data/current".format(hadoop_home_dir)

clusternodes=[]
controller=""
datanodes=[]
repairnodes=[]

# get controller
f=open(cluster_dir+"/controller","r")
for line in f:
    controller=line.strip()
    if controller:
        clusternodes.append(controller)
f.close()

# get datanodes
f=open(cluster_dir+"/agents","r")
for line in f:
    agent=line.strip()
    if agent:
        clusternodes.append(agent)
        datanodes.append(agent)
f.close()

# get clients
f=open(cluster_dir+"/newnodes","r")
for line in f:
    node=line.strip()
    if node:
        clusternodes.append(node)
        repairnodes.append(node)
f.close()

print(controller)
print(datanodes)
print(repairnodes)
print(clusternodes)

# threads
controller_threads = 4
agent_threads = 4
cmddist_threads = 4
if CLUSTER == "aliyunhdd" or CLUSTER == "lab":
    controller_threads = 20
    agent_threads = 20
    cmddist_threads = 10

for node in clusternodes:

    content=[]

    line="<setting>\n"
    content.append(line)

    line="<attribute><name>controller.addr</name><value>"+controller+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>agents.addr</name>\n"
    content.append(line)

    for agent in datanodes:
        line="<value>"+agent+"</value>\n"
        content.append(line)

    line="</attribute>\n"
    content.append(line)

    line="<attribute><name>repairnodes.addr</name>\n"
    content.append(line)

    for client in repairnodes:
        line="<value>"+client+"</value>\n"
        content.append(line)

    line="</attribute>\n"
    content.append(line)

    line="<attribute><name>block.bytes</name><value>"+str(BLKBYTES)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>packet.bytes</name><value>"+str(PKTBYTES)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>code.name</name><value>"+CODE+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>code.ecn</name><value>"+str(ECN)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>code.eck</name><value>"+str(ECK)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>code.ecw</name><value>"+str(ECW)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>batch.size</name><value>"+str(BATCHSIZE)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>recvgroup.size</name><value>"+str(RECVGROUP)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>sendgroup.size</name><value>"+str(SENDGROUP)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>computegroup.size</name><value>"+str(COMPUTEGROUP)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>controller.thread.num</name><value>"+str(controller_threads)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>agent.thread.num</name><value>"+str(agent_threads)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>cmddist.thread.num</name><value>"+str(cmddist_threads)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>local.addr</name><value>"+node+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>block.directory</name><value>"+blk_dir+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>stripestore.directory</name><value>"+stripeStore_dir+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>tradeoffpoint.directory</name><value>"+tradeoffPoint_dir+"</value></attribute>\n"
    content.append(line)

    #line="<attribute><name>eccluster.size</name><value>"+ECCSIZE+"</value></attribute>\n"
    #content.append(line)

    #line="<attribute><name>repair.thread.num</name><value>"+RPTHREADS+"</value></attribute>\n"
    #content.append(line)

    line="</setting>\n"
    content.append(line)

    f=open("sysSetting.xml","w")
    for line in content:
        f.write(line)
    f.close()

    cmd="scp sysSetting.xml {}:{}".format(node, config_dir)
    print(cmd)
    os.system(cmd)

    cmd="rm sysSetting.xml"
    print(cmd)
    os.system(cmd)

    print("finished create conf on node", node)
