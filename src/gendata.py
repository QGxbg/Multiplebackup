import random
import os

def generate_cpp_trace(filename="trace.txt", total_days=100, node_count=40):
    """
    生成适配 C++ TraceReader 的故障数据。
    
    参数:
        filename: 输出文件名
        total_days: 模拟总天数
        node_count: 集群节点总数 (0 ~ node_count-1)
    """
    
    # === 1. 场景配置 (Configuration) ===
    # 平静期配置 (80% 概率)
    # 大概率是 0 或 1，系统很安全，适合 Lazy Repair
    calm_prob = 0.8
    calm_range = [0, 1, 2] 
    calm_weights = [0.5, 0.4, 0.1] 
    
    # 突发期配置 (20% 概率)
    # 坏 3-5 个，系统进入危险区，需要算法介入
    burst_range = [3, 4, 5]
    burst_weights = [0.4, 0.4, 0.2]
    
    print(f"🚀 开始生成 Trace: {total_days} 天, {node_count} 节点...")
    
    with open(filename, "w") as f:
        # 写个头部注释 (C++代码会跳过 # 开头的行)
        f.write(f"# Simulation Trace for {node_count} nodes over {total_days} days\n")
        f.write("# Format: <timestamp> <num_failures> <node_id_1> ... <node_id_N>\n")
        
        for day in range(total_days):
            # A. 决定今天是“平静”还是“突发”
            if random.random() < calm_prob:
                # 平静模式
                num_failures = random.choices(calm_range, weights=calm_weights, k=1)[0]
                note = "Calm"
            else:
                # 突发模式
                num_failures = random.choices(burst_range, weights=burst_weights, k=1)[0]
                note = "BURST"
            
            # B. 如果今天有故障
            if num_failures > 0:
                # 1. 生成时间戳 (每天一个随机时刻发生这批故障)
                # 比如：第5天的 14:30:00
                timestamp = day * 86400 + random.randint(0, 86399)
                
                # 2. 随机挑选坏掉的节点 (保证不重复)
                # 从 0 到 39 中选出 num_failures 个
                failed_nodes = random.sample(range(node_count), num_failures)
                
                # 3. 拼接成字符串
                # 格式: timestamp count node1 node2 ...
                nodes_str = " ".join(map(str, failed_nodes))
                line = f"{timestamp} {num_failures} {nodes_str}"
                
                # 4. 写入文件
                f.write(line + "\n")
                
                # (可选) 打印预览，方便你调试
                if day < 5 or note == "BURST":
                    print(f"Day {day} [{note}]: {line}")

    print(f"\n✅ Trace 生成完毕: {filename}")
    print(f"   该文件可直接被你的 C++ TraceReader 读取。")

# ================= 运行脚本 =================
if __name__ == "__main__":
    generate_cpp_trace("final_cpp_trace.txt", total_days=100, node_count=40)