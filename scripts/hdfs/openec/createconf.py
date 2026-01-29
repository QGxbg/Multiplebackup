# usage
#   python createconf.py
#       1. cluster [office]
#       2. pktbytes [65536]

import os
import sys
import subprocess

CLUSTER=sys.argv[1]
PKTBYTES=sys.argv[2]
system = "Multiple_ParaRC"
# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()



# openec dir
proj_dir="{}/openec-et".format(home_dir)
script_dir = "{}/script".format(proj_dir)
conf_dir = "{}/conf".format(proj_dir)
config_filename = "sysSetting.xml"
stripeStore_dir = "{}/stripeStore".format(proj_dir)

# test dir
test_dir="/home/pararc/$system/scripts/hdfs"
data_dir=test_dir+"/data"
cluster_dir="{}/cluster/{}".format(test_dir, CLUSTER)
openec_genconf_dir="{}/openec".format(test_dir)
print(openec_genconf_dir)

clusternodes=[]
controller=""
datanodes=[]

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

print(controller)
print(datanodes)
print(clusternodes)

# threads
controller_threads = 4
agent_threads = 4
cmddist_threads = 4

if CLUSTER == "aliyunhdd" or CLUSTER == "csencs1":
    controller_threads = 20
    agent_threads = 20
    cmddist_threads = 10

for node in clusternodes:
    content = []

    line="<setting>\n"
    content.append(line)

    line="<attribute><name>controller.addr</name><value>"+controller+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>agents.addr</name>\n"
    content.append(line)

    for dn in datanodes:
        line="<value>/default/"+dn+"</value>\n"
        content.append(line)

    line="</attribute>\n"
    content.append(line)

    line="<attribute><name>oec.controller.thread.num</name><value>"+str(controller_threads)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>oec.agent.thread.num</name><value>"+str(agent_threads)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>oec.cmddist.thread.num</name><value>"+str(cmddist_threads)+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>local.addr</name><value>"+node+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>packet.size</name><value>"+PKTBYTES+"</value></attribute>\n"
    content.append(line)

    line="<attribute><name>dss.type</name><value>HDFS3</value></attribute>\n"
    content.append(line)

    line="<attribute><name>dss.parameter</name><value>"+controller+",9000</value></attribute>\n"
    content.append(line)

    line="<attribute><name>ec.concurrent.num</name><value>15</value></attribute>\n"
    content.append(line)

    line="<attribute><name>ec.policy</name>\n"
    content.append(line)

    line="<value><ecid>Clay_4_2</ecid><class>Clay</class><n>4</n><k>2</k><w>4</w><opt>-1</opt><param>3</param></value>\n"
    content.append(line)

    line="<value><ecid>Clay_5_3</ecid><class>Clay</class><n>5</n><k>3</k><w>8</w><opt>-1</opt><param>4</param></value>\n"
    content.append(line)

    line="<value><ecid>Clay_12_8</ecid><class>Clay</class><n>12</n><k>8</k><w>64</w><opt>-1</opt><param>11</param></value>\n"
    content.append(line)

    line="<value><ecid>Clay_14_10</ecid><class>Clay</class><n>14</n><k>10</k><w>256</w><opt>-1</opt><param>13</param></value>\n"
    content.append(line)

    line="<value><ecid>RSPIPE_4_2</ecid><class>RSPIPE</class><n>4</n><k>2</k><w>1</w><opt>-1</opt></value>\n"
    content.append(line)

    line="<value><ecid>RSPIPE_14_10</ecid><class>RSPIPE</class><n>14</n><k>10</k><w>1</w><opt>-1</opt></value>\n"
    content.append(line)

    line="<value><ecid>RDP_12_10</ecid><class>RDP</class><n>12</n><k>10</k><w>10</w><opt>-1</opt></value>\n"
    content.append(line)

    line="<value><ecid>HHXORPlus_14_10</ecid><class>HHXORPlus</class><n>14</n><k>10</k><w>2</w><opt>-1</opt></value>\n"
    content.append(line)

    line="</attribute>\n"
    content.append(line)

    line="<attribute><name>offline.pool</name>\n"
    content.append(line)

    line="<value><poolid>Clay_4_2_pool</poolid><ecid>Clay_4_2</ecid><base>1</base></value>\n"
    content.append(line)

    line="<value><poolid>Clay_5_3_pool</poolid><ecid>Clay_5_3</ecid><base>1</base></value>\n"
    content.append(line)

    line="<value><poolid>Clay_12_8_pool</poolid><ecid>Clay_12_8</ecid><base>1</base></value>\n"
    content.append(line)

    line="<value><poolid>Clay_14_10_pool</poolid><ecid>Clay_14_10</ecid><base>1</base></value>\n"
    content.append(line)

    line="<value><poolid>RSPIPE_14_10_pool</poolid><ecid>RSPIPE_14_10</ecid><base>1</base></value>\n"
    content.append(line)

    line="<value><poolid>RSPIPE_4_2_pool</poolid><ecid>RSPIPE_4_2</ecid><base>1</base></value>\n"
    content.append(line)

    line="<value><poolid>RDP_12_10_pool</poolid><ecid>RDP_12_10</ecid><base>1</base></value>\n"
    content.append(line)

    line="<value><poolid>HHXORPlus_14_10_pool</poolid><ecid>HHXORPlus_14_10</ecid><base>1</base></value>\n"
    content.append(line)

    line="</attribute>\n"
    content.append(line)

    line="</setting>\n"
    content.append(line)

    f=open("sysSetting.xml", "w")
    for line in content:
        f.write(line)
    f.close()

    cmd="scp sysSetting.xml {}:{}".format(node, conf_dir)
    os.system(cmd)

    cmd="rm sysSetting.xml"
    os.system(cmd)

    print("finished create conf on node for openec-et", node)


