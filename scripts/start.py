import os
import subprocess
system="Multiple_ParaRC"
# home dir
cmd = r'echo ~'
home_dir_str, stderr = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()
home_dir = home_dir_str.decode().strip()

# proj dir
proj_dir="{}/{}".format(home_dir,system)
config_dir="{}/conf".format(proj_dir)
CONF = config_dir+"/sysSetting.xml"
script_dir = "{}/scripts".format(proj_dir)
stripestore_dir="{}/stripeStore".format(proj_dir)


f=open(CONF)
start = False
concactstr = ""
for line in f:
    if line.find("setting") == -1:
        line = line[:-1]
    concactstr += line
res=concactstr.split("<attribute>")

slavelist=[]
fstype=""
for attr in res:
    #if attr.find("dss.type") != -1:
    #    attrtmp=attr.split("<value>")[1]
    #    fstype=attrtmp.split("</value>")[0]
    if attr.find("agents.addr") != -1:
        valuestart=attr.find("<value>")
        valueend=attr.find("</attribute>")
        attrtmp=attr[valuestart:valueend]
        slavestmp=attrtmp.split("<value>")
        for slaveentry in slavestmp:
            if slaveentry.find("</value>") != -1:
                #entrysplit=slaveentry.split("/")
                #slave=entrysplit[2][0:-1]
                entrysplit=slaveentry.split("<")
                slave=entrysplit[0]
                slavelist.append(slave)

    if attr.find("repairnodes.addr") != -1:
        valuestart=attr.find("<value>")
        valueend=attr.find("</attribute>")
        attrtmp=attr[valuestart:valueend]
        slavestmp=attrtmp.split("<value>")
        for slaveentry in slavestmp:
            if slaveentry.find("</value>") != -1:
                #entrysplit=slaveentry.split("/")
                #slave=entrysplit[2][0:-1]
                entrysplit=slaveentry.split("<")
                slave=entrysplit[0]
                slavelist.append(slave)

print("Starting controller redis...")
# Stop any existing redis (including system service running as root)
os.system("sudo systemctl stop redis 2>/dev/null ; sudo systemctl stop redis-server 2>/dev/null ; sudo killall redis-server 2>/dev/null || true")
os.system("sleep 1")
# Start redis bound to all interfaces so agents on other nodes can connect back
os.system("redis-server --daemonize yes --protected-mode no --bind 0.0.0.0")
os.system("sleep 1")
os.system("redis-cli -h 127.0.0.1 -p 6379 flushall")

## start
#print("start coordinator")
#os.system("redis-cli flushall")
#os.system("killall DistCoordinator")
#os.system("sudo service redis_6379 restart")
#
## create stripestore dir
#command="mkdir -p " + stripestore_dir
#subprocess.Popen(['/bin/bash', '-c', command])
#
## open controller
#command="cd "+proj_dir+"; ./build/DistCoordinator &> "+proj_dir+"/coor_output &"
#subprocess.Popen(['/bin/bash', '-c', command])
for slave in slavelist:
    cmd="ssh "+ slave +" \"ps aux|grep redis\""
    res=subprocess.Popen(['/bin/bash','-c',cmd], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = res.communicate()

    #pid=-1
    #out = out.split("\n")
    #for line in out:
    #    if line.find("redis-server") == -1:
    #        continue
    #    item = line.split(" ")

    #    for i in range(1,7):
    #        if (item[i] != ''):
    #            pid = item[i]
    #            break

    #cmd="ssh "+slave+" \"sudo kill -KILL "+str(pid)+"\""
    #print(cmd)
    #os.system(cmd)

    #cmd="ssh "+slave+" \"sudo rm /var/run/redis_6379.pid\""
    #print(cmd)
    #os.system(cmd)

    #cmd="ssh "+slave+" \"sudo service redis_6379 start\""
    #print(cmd)
    #os.system(cmd)

    # cmd="ssh "+slave+" \"echo 'hustdlmm1037' | sudo -S service redis restart\""
    # cmd="ssh "+slave+" \"sudo service redis restart\""
    # print(cmd)
    # os.system(cmd)
    
    # Attempt to start redis if it's not already running
    os.system("ssh " + slave + " \"redis-server --daemonize yes 2>/dev/null || sudo service redis-server start 2>/dev/null || sudo service redis start 2>/dev/null\"")

    os.system("ssh " + slave + " \"redis-cli flushall \"")

    os.system("ssh " + slave + " \"killall -9 ParaAgent \"")
    
    # Try to remove the file to avoid 'Text file busy' during scp
    os.system("ssh " + slave + " \"rm -f "+proj_dir+"/ParaAgent\"")
    
    command="scp "+proj_dir+"/ParaAgent "+slave+":"+proj_dir+"/"
    #command="scp "+proj_dir+"/build/ParaAgent "+slave+":"+proj_dir+"/build/"
    os.system(command)

    command="ssh "+slave+" \"cd "+proj_dir+"; ./ParaAgent &> "+proj_dir+"/agent_output &\""
    os.system(command)
    
    os.system("sleep 1")
    check_cmd = "ssh " + slave + " \"ps aux | grep '[P]araAgent'\""
    res = subprocess.Popen(['/bin/bash', '-c', check_cmd], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = res.communicate()
    if not out.strip():
        print("ParaAgent failed to start on " + slave + ", restarting...")
        os.system(command)
