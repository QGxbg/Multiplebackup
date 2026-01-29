import subprocess
import os
import re

# 输出文件路径
output_file = "output_new2.txt"

# 1. 生成数据的指令
gen_data_cmd = "python3 scripts/data/gen_simulation_data_multiple_failure.py 30 100 14 10 2 0 , "
subprocess.run(gen_data_cmd, shell=True, check=True)

# 2. 运行程序的指令并获取实验结果
run_program_cmd = "./MulSim_parallel stripeStore/simulation_30_100_14_10_2 30 100 Clay 14 10 256 scatter 3 2 0 1"
result = subprocess.run(run_program_cmd, shell=True, capture_output=True, text=True)

# 提取最后三行结果
last_three_lines = result.stdout.strip().split('\n')[-3:]
with open(output_file, "w") as f:
    f.write("第一次实验结果（最后三行）：\n")
    f.write("\n".join(last_three_lines) + "\n")

# 3. 修改 stripeStore/simulation_20_50_14_10_2 文件
input_file = "stripeStore/simulation_30_100_14_10_2"
output_file_modified = "stripeStore/simulation_30_100_14_10_2_modified"

# 检查文件是否存在
if not os.path.exists(input_file):
    print(f"文件 {input_file} 不存在！")
    exit(1)

# 读取文件内容
with open(input_file, "r") as f:
    lines = f.readlines()

print(f"读取文件：{input_file}")
print(f"文件行数：{len(lines)}")

# 将包含单独数字 1 的行放到最后
lines_with_single_1 = [line for line in lines if re.search(r'\b1\b', line)]
lines_without_single_1 = [line for line in lines if not re.search(r'\b1\b', line)]

print(f"包含单独 '1' 的行数：{len(lines_with_single_1)}")
print(f"不包含单独 '1' 的行数：{len(lines_without_single_1)}")

# 合并行并写入新文件
modified_lines = lines_without_single_1 + lines_with_single_1
with open(output_file_modified, "w") as f:
    f.writelines(modified_lines)

# 替换原文件
os.replace(output_file_modified, input_file)

# 4. 再次运行程序并获取实验结果
result_modified = subprocess.run(run_program_cmd, shell=True, capture_output=True, text=True)

# 提取修改后的最后三行结果
last_three_lines_modified = result_modified.stdout.strip().split('\n')[-3:]
with open(output_file, "a") as f:
    f.write("\n修改文件后的实验结果（最后三行）：\n")
    f.write("\n".join(last_three_lines_modified) + "\n")

print("实验结果已保存到 output.txt 文件中。")