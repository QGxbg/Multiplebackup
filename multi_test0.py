import itertools
import subprocess
import os

def generate_commands(program, fixed_part, failnums, node_range, output_file):
    """生成所有可能的 Multiple_Failure 命令并运行，获取输出的最后一行"""
    nodes = list(range(node_range))
    
    with open(output_file, 'w', encoding='utf-8') as out:
        for failnum in failnums:
            # 生成所有可能的节点组合
            combinations = itertools.combinations(nodes, failnum)
            
            for combo in combinations:
                # 生成命令行，failnum 紧跟在固定部分之后
                command = f"{program} {fixed_part} {failnum} {' '.join(map(str, combo))} 0 scatter 0"
                
                # 运行命令并获取输出的最后一行
                process = subprocess.Popen(command.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                stdout, stderr = process.communicate()
                last_line = stdout.decode('utf-8').strip().split('\n')[-1] if stdout else stderr.decode('utf-8').strip().split('\n')[-1]
                
                # 输出错误节点分布和最后一行输出到文件
                error_distribution = f"错误节点分布: {failnum} 节点 ({', '.join(map(str, combo))})"
                out.write(f"{error_distribution}\n最后一行输出: {last_line}\n\n")
                
                # 输出到控制台以便立即查看
                print(f"{error_distribution}\n最后一行输出: {last_line}\n")

def main():
    # 设置参数
    program = "./Multiple_Failure Clay"
    fixed_part = "14 10 256"  # 固定部分的参数
    failnums = [4]  # 需要生成的错误节点数量
    node_range = 14  # 节点范围从 0 到 13（14 个节点）
    output_file = 'test_1410_4_0_new'  # 输出文件名

    # 生成并打印所有可能的命令
    if os.path.exists(output_file):
        os.remove(output_file)  # 删除旧的输出文件
    generate_commands(program, fixed_part, failnums, node_range, output_file)

if __name__ == "__main__":
    main()