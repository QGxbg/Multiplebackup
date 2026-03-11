import os
import subprocess
import argparse

def get_slaves_from_conf(conf_path):
    slavelist = []
    with open(conf_path, 'r') as f:
        concactstr = ""
        for line in f:
            if line.find("setting") == -1:
                line = line[:-1]
            concactstr += line
        
        res = concactstr.split("<attribute>")
        for attr in res:
            if attr.find("agents.addr") != -1 or attr.find("repairnodes.addr") != -1:
                valuestart = attr.find("<value>")
                valueend = attr.find("</attribute>")
                attrtmp = attr[valuestart:valueend]
                slavestmp = attrtmp.split("<value>")
                for slaveentry in slavestmp:
                    if slaveentry.find("</value>") != -1:
                        entrysplit = slaveentry.split("<")
                        slave = entrysplit[0]
                        if slave not in slavelist:
                            slavelist.append(slave)
    return slavelist

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Limit bandwidth on all cluster nodes")
    parser.add_argument("action", choices=["start", "stop", "status"], help="Action to perform")
    parser.add_argument("-i", "--interface", default="eth0", help="Network interface (default: eth0)")
    parser.add_argument("-r", "--rate", default="100mbit", help="Rate limit e.g. 10mbit, 1gbit (default: 100mbit)")
    
    args = parser.parse_args()
    
    system = "Multiple_ParaRC"
    home_dir_str, _ = subprocess.Popen('echo ~', shell=True, stdout=subprocess.PIPE).communicate()
    home_dir = home_dir_str.decode().strip()
    proj_dir = f"{home_dir}/{system}"
    conf_path = f"{proj_dir}/conf/sysSetting.xml"
    script_path = f"{proj_dir}/scripts/limit_bandwidth.sh"
    
    slavelist = get_slaves_from_conf(conf_path)
    print(f"Found {len(slavelist)} nodes in config file.")
    
    for slave in slavelist:
        print(f"--- Processing node: {slave} ---")
        
        # SCP script to remote node to make sure it's fully updated there
        scp_cmd = f"scp {script_path} {slave}:/tmp/limit_bandwidth.sh"
        os.system(scp_cmd)
        
        if args.action == "start":
            cmd = f"ssh {slave} \"chmod +x /tmp/limit_bandwidth.sh && sudo /tmp/limit_bandwidth.sh start -i {args.interface} -r {args.rate}\""
        elif args.action == "stop":
            cmd = f"ssh {slave} \"chmod +x /tmp/limit_bandwidth.sh && sudo /tmp/limit_bandwidth.sh stop -i {args.interface}\""
        else:
            cmd = f"ssh {slave} \"chmod +x /tmp/limit_bandwidth.sh && sudo /tmp/limit_bandwidth.sh status -i {args.interface}\""
            
        os.system(cmd)
