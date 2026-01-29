# 读取文件并提取有效信息
valid_lines = []
with open("/home/qgxbg/桌面/mytest/parafullnode-main/your_file", "r") as file:
    for line in file:
        if line.startswith("dst:") and "value:" in line:
            valid_lines.append(line.strip())

# 按照 "dst" 的顺序排列有效信息
sorted_lines = sorted(valid_lines, key=lambda x: int(x.split("dst:")[1].split()[0].replace(",", "")))

# 输出排序后的有效信息
for line in sorted_lines:
    print(line)
