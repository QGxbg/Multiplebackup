import subprocess
import re

# 生成数据的指令
# def generate_data(failnum):
#     command = f"python3 ../scripts/data/gen_simulation_data_multiple_failure.py 50 1000 14 10 {failnum}  , "
#     subprocess.run(command, shell=True, check=True)

# # 根据 failnum 设置目标数字
# def get_target_numbers(failnum):
#     if failnum == 4:
#         return {'0', '1', '2', '3'}
#     elif failnum == 3:
#         return {'0', '1', '2'}
#     elif failnum == 2:
#         return {'0', '1'}
#     else:
#         raise ValueError("failnum 必须是 2、3 或 4")

# 读取文件并统计行数
def count_lines(file_path, target_numbers):
    # 初始化统计结果
    stats = {
        'contains_0': 0,  # 包含节点 0 的行数
        'only_0': 0,      # 仅包含节点 0 的行数（不包含其他目标数字）
        'contains_1': 0,  # 包含节点 1 的行数
        'only_1': 0,      # 仅包含节点 1 的行数（不包含其他目标数字）
        'contains_2': 0,  # 包含节点 2 的行数
        'only_2': 0,      # 仅包含节点 2 的行数（不包含其他目标数字）
        'contains_3': 0,  # 包含节点 3 的行数
        'only_3': 0,      # 仅包含节点 3 的行数（不包含其他目标数字）
        'only_1_target': 0,  # 仅包含 1 个目标数字的行数
        'only_2_target': 0,  # 仅包含 2 个目标数字的行数
        'only_3_target': 0,  # 仅包含 3 个目标数字的行数
        'only_4_target': 0   # 仅包含 4 个目标数字的行数
    }

    # 读取文件
    with open(file_path, 'r') as f:
        for line in f:
            # 提取行中的数字
            numbers = re.findall(r'\b\d+\b', line)  # 使用正则表达式匹配单独的数字
            numbers_set = set(numbers)  # 转换为集合去重

            # 统计包含节点 0 的行数
            if '0' in target_numbers and '0' in numbers_set:
                stats['contains_0'] += 1
                # 判断是否仅包含节点 0（不包含其他目标数字）
                if not (target_numbers - {'0'}).intersection(numbers_set):
                    stats['only_0'] += 1

            # 统计包含节点 1 的行数
            if '1' in target_numbers and '1' in numbers_set:
                stats['contains_1'] += 1
                # 判断是否仅包含节点 1（不包含其他目标数字）
                if not (target_numbers - {'1'}).intersection(numbers_set):
                    stats['only_1'] += 1

            # 统计包含节点 2 的行数
            if '2' in target_numbers and '2' in numbers_set:
                stats['contains_2'] += 1
                # 判断是否仅包含节点 2（不包含其他目标数字）
                if not (target_numbers - {'2'}).intersection(numbers_set):
                    stats['only_2'] += 1

            # 统计包含节点 3 的行数
            if '3' in target_numbers and '3' in numbers_set:
                stats['contains_3'] += 1
                # 判断是否仅包含节点 3（不包含其他目标数字）
                if not (target_numbers - {'3'}).intersection(numbers_set):
                    stats['only_3'] += 1

            # 统计仅包含目标数字的行数
            intersection = numbers_set.intersection(target_numbers)
            count = len(intersection)
            if count == 1:
                stats['only_1_target'] += 1
            elif count == 2:
                stats['only_2_target'] += 1
            elif count == 3:
                stats['only_3_target'] += 1
            elif count == 4:
                stats['only_4_target'] += 1

    return stats

# 主函数
def main(failnum, runs=1):
    # 根据 failnum 设置目标数字
    # target_numbers = get_target_numbers(failnum)
    target_numbers = {'0', '1', '2', '3'}

    # 初始化累加器
    total_stats = {
        'contains_0': 0,
        'only_0': 0,
        'contains_1': 0,
        'only_1': 0,
        'contains_2': 0,
        'only_2': 0,
        'contains_3': 0,
        'only_3': 0,
        'only_1_target': 0,
        'only_2_target': 0,
        'only_3_target': 0,
        'only_4_target': 0
    }

    # 运行多次并累加统计结果
    for i in range(runs):
        print(f"正在运行第 {i + 1} 次...")
        # 生成数据
        # generate_data(failnum)

        # 文件路径
        file_path = "stripeStore/simulation_50_100_14_10_4_pair_optimized"  # 根据实际生成的文件路径修改

        # 统计行数
        stats = count_lines(file_path, target_numbers)

        # 累加统计结果
        for key in total_stats:
            total_stats[key] += stats[key]

    # 计算平均值
    avg_stats = {key: total_stats[key] / runs for key in total_stats}

    # 输出平均结果
    print("\n平均统计结果：")
    if '0' in target_numbers:
        print(f"包含节点 0 的行数: {avg_stats['contains_0']}")
        print(f"仅包含节点 0 的行数: {avg_stats['only_0']}")
    if '1' in target_numbers:
        print(f"包含节点 1 的行数: {avg_stats['contains_1']}")
        print(f"仅包含节点 1 的行数: {avg_stats['only_1']}")
    if '2' in target_numbers:
        print(f"包含节点 2 的行数: {avg_stats['contains_2']}")
        print(f"仅包含节点 2 的行数: {avg_stats['only_2']}")
    if '3' in target_numbers:
        print(f"包含节点 3 的行数: {avg_stats['contains_3']}")
        print(f"仅包含节点 3 的行数: {avg_stats['only_3']}")
    print(f"仅包含 1 个目标数字的行数: {avg_stats['only_1_target']}")
    print(f"仅包含 2 个目标数字的行数: {avg_stats['only_2_target']}")
    if failnum >= 3:
        print(f"仅包含 3 个目标数字的行数: {avg_stats['only_3_target']}")
    if failnum >= 4:
        print(f"仅包含 4 个目标数字的行数: {avg_stats['only_4_target']}")

# 运行主函数
if __name__ == "__main__":
    failnum = 4  # 根据需求设置 failnum
    main(failnum)