import shutil
import os

def remove_batch_lines(file_path):
    """
    删除单个文件中以“batch:”开头的行
    """
    # 检查文件是否存在
    if not os.path.exists(file_path):
        print(f"文件 {file_path} 不存在")
        return
    
    print(f"处理文件: {file_path}")
    
    # 创建备份文件
    backup_path = f"{file_path}.bak"
    shutil.copy2(file_path, backup_path)
    print(f"已创建备份: {backup_path}")
    
    # 读取文件并过滤掉以"batch:"开头的行
    new_lines = []
    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            if not line.strip().startswith("  "):
                new_lines.append(line)
    
    # 将过滤后的内容写回文件
    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)
    
    print(f"已删除文件中以'batch:'开头的行")

# 使用示例
file_path = "/home/lx/Multiple_ParaRC/Stripe099.out"  # 替换为实际的文件路径
remove_batch_lines(file_path)