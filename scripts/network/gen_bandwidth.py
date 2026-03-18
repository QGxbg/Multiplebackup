import sys
import os
import random

def main():
    if len(sys.argv) < 2:
        print("Usage: python gen_bandwidth.py <num_nodes> [min_bw=0.5] [max_bw=2.0] [discrete=1]")
        print("       discrete: 1 for specific limits (e.g. 0.5, 0.8, 1.0, 1.5, 2.0)")
        print("                 0 for uniform random")
        sys.exit(1)

    num_nodes = int(sys.argv[1])
    min_bw = float(sys.argv[2]) if len(sys.argv) > 2 else 0.5
    max_bw = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
    discrete = int(sys.argv[4]) if len(sys.argv) > 4 else 1

    # 获取带宽文件输出路径
    SYSTEM = "Multiple_ParaRC"
    HOME = os.path.expanduser('~')
    output_path = f"{HOME}/{SYSTEM}/bandwidth.txt"

    print(f"Generating bandwidth for {num_nodes} nodes.")
    print(f"Range: {min_bw} ~ {max_bw}, Mode: {'Discrete' if discrete else 'Uniform'}")
    
    # 定义常见的离散带宽档位，使随机出来的网络环境更符合实际情况
    discrete_choices = [0.5, 0.8, 1.0, 1.25, 1.5, 2.0]
    # 如果指定了上下界，过滤掉超出范围的档位
    valid_choices = [x for x in discrete_choices if min_bw <= x <= max_bw]
    if not valid_choices:
        valid_choices = [1.0]

    with open(output_path, "w") as f:
        for _ in range(num_nodes):
            if discrete == 1:
                val = random.choice(valid_choices)
            else:
                # 连续随机，保留一位小数
                val = round(random.uniform(min_bw, max_bw), 1)
            
            f.write(f"{val}\n")
    
    print(f"Success! Output written to {output_path}")

if __name__ == "__main__":
    main()
