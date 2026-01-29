# usage:
# python gendata.py
#   1. cluster [aliyunhdd|office|csencs1]
#   2. number of stripes
#   3. code [Clay|RSPIPE|RDP|HH]
#   4. n
#   5. k
#   6. w
#   7. blockMiB
#   8. subpacketKiB
#   9. gen_files [true|false]
#   10. gen_blocks [true|false]
#   11. gen_meta [true|false]
#   12. batchsize

import os
import random
import sys
import subprocess
import time

def usage():
    print("python gendata_from_oec.py cluster[office|aliyunhdd|csencs1] num_stripes code[RSPIPE|Clay] n k w blockMiB packetKiB gen_files[true|false] gen_blocks[true|false] gen_meta[true|false] batchsize")


if len(sys.argv) != 13:
    usage()
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
gen_files=(sys.argv[9] == "true")
gen_blocks=(sys.argv[10] == "true")
gen_meta=(sys.argv[11] == "true")
batchsize=int(sys.argv[12])
system = "Multiple_ParaRC"
# sizes
filesize_MB = int(BLOCKMB * ECK)
filesize_bytes = int(filesize_MB * 1048576)
block_bytes = int(BLOCKMB * 1048576)
packet_bytes = int(PACKETKB * 1024)
subpacket_bytes = int(packet_bytes / ECW)

print("filesize_bytes: "+str(filesize_bytes))
print("block_bytes: "+str(block_bytes))
print("packet_bytes: "+str(packet_bytes))
print("subpacket_bytes: "+str(subpacket_bytes))

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

# generate n files  filesize_MB = int(BLOCKMB * ECK)
if gen_files == True:
    for i in range(NSTRIPES):
        # go to the first client node
        cmd = "ssh {} \"cd {}; dd if=/dev/urandom of=input_{}_{} bs={} count={} iflag=fullblock\"".format(
            datanodes[0], oec_proj_dir,
            str(filesize_bytes), str(i),
            str(block_bytes), str(ECK),
        )
        print(cmd)  
        os.system(cmd)

if gen_blocks == True:
    # set OEC and Hadoop configs (block and packet sizes only)

    # stop hdfs
    cmd = "stop-dfs.sh"
    os.system(cmd)

    cmd = "cd {}/hdfs/{}; bash clean.sh".format(test_dir, CLUSTER)
    os.system(cmd)

    # set hdfs configuration files
    hdfs_conf_script = "{}/hdfs/{}/createhdfs.py".format(test_dir,CLUSTER)
    cmd="python {} {} {}".format(hdfs_conf_script, str(block_bytes), str(packet_bytes))
    print(cmd)
    os.system(cmd)

    # start dfs
    cmd = "hdfs namenode -format"
    os.system(cmd)
    cmd = "start-dfs.sh"
    os.system(cmd)

    time.sleep(2)

    # stop openec
    cmd = "cd {}; python script/stop.py".format(oec_proj_dir)
    os.system(cmd)

    cmd = "cd {}; rm entryStore poolStore".format(oec_proj_dir)
    os.system(cmd)

    # set openec configuration files
    oec_conf_script = "{}/openec/createconf.py".format(test_dir)
    cmd = "python {} {} {}".format(oec_conf_script, CLUSTER, str(packet_bytes))
    os.system(cmd)

    # start openec
    cmd = "cd {}; python script/start.py".format(oec_proj_dir)
    os.system(cmd)

    time.sleep(2)

    # generate NSTRIPES
    for i in range(NSTRIPES):
        # go to the first client node
        inputfile = "input_{}_{}".format(str(filesize_bytes), str(i))
        saveas = "/{}_{}_{}_{}".format(CODE,ECN,ECK,str(i))
        ecid = "{}_{}_{}".format(CODE, str(ECN), str(ECK))
        cmd = "ssh {} \"cd {}; ./OECClient write {} {} {} online {}\"".format(datanodes[0], oec_proj_dir,inputfile,saveas,ecid,str(filesize_MB))
        print(cmd)
        os.system(cmd)

        time.sleep(2)

        # run redis flushall between each two runs
        print("flush redis for {}-th stripe".format(str(i)))
        for node_ip in clusternodes:
            cmd = "ssh " + node_ip + " \"redis-cli flushall\""
            # print(cmd)
            os.system(cmd)

        time.sleep(2)

    # stop OpenEC
    cmd = "cd {}; python script/stop.py".format(oec_proj_dir)
    print(cmd)
    os.system(cmd)

if gen_meta == True:
    # generate NSTRIPES metadata for ParaRC
    cmd = "rm {}/*".format(stripestore_dir)
    os.system(cmd)

    for i in range(NSTRIPES):
        cmd = "cd {}; python3 genmeta_from_hdfs_fullnode.py -code {} -n {} -k {} -w {} -bs {} -ps {} -filename /{}_{}_{}_{} -stripename {}_{}{}{}_{}".format(
            data_dir, CODE, ECN, ECK, ECW,
            str(block_bytes), str(subpacket_bytes),
            CODE, ECN, ECK, i,
            CODE, ECN, ECK, ECW, i
        )
        print(cmd)
        os.system(cmd)

    # stop hdfs
    cmd="stop-dfs.sh"
    os.system(cmd)
