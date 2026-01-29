import itertools
import subprocess
import os

def generate_specific_node_failure(program, fixed_part, specific_node, node_range, output_file, exclude_nodes):
    """生成指定节点始终出现在错误分布中的命令，且不包含已经计算过的节点"""
    nodes = list(range(node_range))
    
    # 剔除特定节点和排除的节点
    nodes = [node for node in nodes if node > specific_node and node not in exclude_nodes]

    with open(output_file, 'a', encoding='utf-8') as out:  # 改为 'a' 模式
        # 生成所有可能的节点组合
        combinations = itertools.combinations(nodes, 3)  # 这里的 3 可以调整为你希望的其他数量

        for combo in combinations:
            # 将 specific_node 添加到组合的前面
            combo_with_specific_node = (specific_node,) + combo

            # 生成命令行，failnum 紧跟在固定部分之后
            command = f"{program} {fixed_part} {len(combo_with_specific_node)} {' '.join(map(str, combo_with_specific_node))} 0"

            # 运行命令并获取输出的最后一行
            process = subprocess.Popen(command.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = process.communicate()
            last_line = stdout.decode('utf-8').strip().split('\n')[-1] if stdout else stderr.decode('utf-8').strip().split('\n')[-1]

            # 输出错误节点分布和最后一行输出到文件
            error_distribution = f"错误节点分布: {len(combo_with_specific_node)} 节点 ({', '.join(map(str, combo_with_specific_node))})"
            out.write(f"{error_distribution}\n最后一行输出: {last_line}\n\n")

            # 输出到控制台以便立即查看
            print(f"{error_distribution}\n最后一行输出: {last_line}\n")

def main():
    # 设置参数
    program = "./Multiple_Failure Clay"
    fixed_part = "16 12 256"  # 固定部分的参数
    specific_node = 12 # 固定坏掉的节点编号，可以调整
    node_range = 16  # 节点范围从 0 到 13（14 个节点）
    exclude_nodes = [0,1,2,3,4,5,6,7,8,9,10,11]  # 排除的节点列表，如之前已经计算过的节点
    output_file = f'test_1612_4_0'  # 输出文件名

    # 生成并打印指定节点故障下的所有可能的命令
    generate_specific_node_failure(program, fixed_part, specific_node, node_range, output_file, exclude_nodes)

if __name__ == "__main__":
    main()
