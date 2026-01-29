import re
from collections import defaultdict

def count_lines_target_numbers(file_path, target_numbers):
    """统计每行的目标数字数量及各类包含情况"""
    stats = {
        'lines': [],  # 保存(行号, 目标数量, 原始行, 行中数字集合)
    }

    with open(file_path, 'r') as f:
        for line_num, line in enumerate(f, 1):
            numbers = re.findall(r'\b\d+\b', line)
            numbers_set = set(numbers)
            target_count = len(numbers_set.intersection(target_numbers))
            stats['lines'].append((line_num, target_count, line, numbers_set))

    return stats

def generate_pair_optimized_file(file_path, new_file_path, lines_data):
    """生成两两分组差异最大化的文件"""
    lines = lines_data['lines'].copy()
    if not lines:
        return

    result = []
    while lines:
        current_line = lines.pop(0)
        current_set = current_line[3]
        
        if not lines:
            result.append(current_line)
            break
        
        best_idx = 0
        min_common = float('inf')
        
        for i, (_, _, _, num_set) in enumerate(lines):
            common = len(current_set.intersection(num_set))
            if common < min_common:
                min_common = common
                best_idx = i
        
        best_line = lines.pop(best_idx)
        result.extend([current_line, best_line])

    with open(new_file_path, 'w', encoding='utf-8') as f:
        for _, _, line, _ in result:
            f.write(line)

    print(f"已生成两两分组差异最大化文件: {new_file_path}")
    return new_file_path

def analyze_file(file_path, target_numbers, file_desc):
    """分析文件并打印统计结果"""
    stats = count_lines_target_numbers(file_path, target_numbers)
    print(f"\n{file_desc} 的统计结果：")
    
    line_counts = [count for _, count, _, _ in stats['lines']]
    print(f"总行数: {len(line_counts)}")
    print(f"目标数字数量分布: {sorted(line_counts)}")
    
    # 检查两两分组的相似性
    print("\n两两分组的相同数字数量：")
    with open(file_path, 'r') as f:
        lines = f.readlines()
        for i in range(0, len(lines), 2):
            if i + 1 >= len(lines):
                continue
            set1 = set(re.findall(r'\b\d+\b', lines[i]))
            set2 = set(re.findall(r'\b\d+\b', lines[i + 1]))
            common = len(set1.intersection(set2))
            print(f"第{i//2 + 1}组: {common}个相同数字")

def main(file_path, target_numbers={'0', '1', '2', '3'}):
    """主函数：分析原始文件并生成差异最大化文件"""
    print(f"正在分析原始文件: {file_path}")
    stats = count_lines_target_numbers(file_path, target_numbers)
    
    # 生成两两分组差异最大化文件
    pair_optimized_file = file_path + "_pair_optimized"
    generate_pair_optimized_file(file_path, pair_optimized_file, stats)
    
    # 分析新生成的文件
    print("\n\n=== 分析两两分组差异最大化文件 ===")
    analyze_file(pair_optimized_file, target_numbers, "两两分组差异最大化文件")

if __name__ == "__main__":
    file_path = "stripeStore/simulation_50_150_14_10_4"
    main(file_path)