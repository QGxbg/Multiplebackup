import sys
import os

# 项目配置
SYSTEM = "Multiple_ParaRC"

if len(sys.argv) < 2:
    print("Usage: python setbdwt.py [cluster_name] [optional_Mbps_all_nodes]")
    print("   If [optional_Mbps_all_nodes] is not provided, reads from bandwidth.txt (1.0 = 1000MiB/s = 8000mbit)")
    sys.exit(1)

CLUSTER = sys.argv[1]

# 获取主目录路径
HOME = os.path.expanduser('~')
IP2NIC_PATH = f"{HOME}/{SYSTEM}/scripts/cluster/{CLUSTER}/ip2nic"
BANDWIDTH_PATH = f"{HOME}/{SYSTEM}/bandwidth.txt"

def set_bandwidth():
    if not os.path.exists(IP2NIC_PATH):
        print(f"Error: Config {IP2NIC_PATH} not found.")
        return

    use_txt = False
    global_bdwt = 0
    bdwts = []
    
    if len(sys.argv) == 3:
        global_bdwt = int(sys.argv[2])
    else:
        use_txt = True
        if not os.path.exists(BANDWIDTH_PATH):
            print(f"Error: {BANDWIDTH_PATH} not found.")
            return
            
        with open(BANDWIDTH_PATH, "r") as f:
            for l in f:
                if l.strip():
                    bdwts.append(float(l.strip()))

    with open(IP2NIC_PATH, "r") as f:
        ip2nic_lines = f.readlines()

    idx = 0
    for line in ip2nic_lines:
        if ":" not in line: continue
        ip, nic = line.strip().split(":")
        
        if use_txt:
            if idx >= len(bdwts):
                print(f"Warning: Not enough entries in {BANDWIDTH_PATH} for all IPs. Using default 1.0 (8000mbit).")
                val = 1.0
            else:
                val = bdwts[idx]
            # 1.0 means 1000MiB/s, which is 8000 Mbit/s
            node_bdwt = int(val * 8000)
        else:
            node_bdwt = global_bdwt

        remote_cmd = (
            f"sudo tc qdisc del dev {nic} root >/dev/null 2>&1; "
            f"sudo tc qdisc add dev {nic} root handle 1: htb default 1 r2q 100; "
            f"sudo tc class add dev {nic} parent 1: classid 1:1 htb "
            f"rate {node_bdwt}mbit ceil {node_bdwt}mbit quantum 1500; "
            f"sudo tc qdisc add dev {nic} parent 1:1 handle 10: sfq perturb 10"
        )

        print(f"[*] Node {ip}: Setting bandwidth to {node_bdwt}mbit on {nic}...")
        os.system(f"ssh {ip} \"{remote_cmd}\"")
        idx += 1

if __name__ == "__main__":
    set_bandwidth()
    print("\n[+] Bandwidth configuration applied via native tc.")