#!/usr/bin/env python3
"""
preprocess_trace.py - 从Backblaze CSV数据中提取节点故障trace

Backblaze CSV格式: date, serial_number, model, capacity_bytes, failure, smart_*...
其中 failure=1 表示硬盘在该天故障

用法:
    python3 preprocess_trace.py <input_csv_dir> <output_trace_file> [num_nodes]

参数:
    input_csv_dir    - 包含Backblaze CSV文件的目录
    output_trace_file - 输出的trace文件路径
    num_nodes        - 选取的节点数量(默认40)

输出格式 (每行):
    day  num_failures  node_id1 node_id2 ...
"""

import os
import sys
import csv
import glob
from datetime import datetime
from collections import defaultdict

def main():
    if len(sys.argv) < 3:
        print("用法: python3 preprocess_trace.py <input_csv_dir_or_file> <output_trace_file> [num_nodes]")
        print("示例: python3 preprocess_trace.py ./backblaze_data/ ./conf/trace_40nodes.txt 40")
        sys.exit(1)

    input_path = sys.argv[1]
    output_file = sys.argv[2]
    num_nodes = int(sys.argv[3]) if len(sys.argv) > 3 else 40

    # 收集所有CSV文件
    if os.path.isdir(input_path):
        csv_files = sorted(glob.glob(os.path.join(input_path, "*.csv")))
    else:
        csv_files = [input_path]

    if not csv_files:
        print(f"错误: 在 {input_path} 中未找到CSV文件")
        sys.exit(1)

    print(f"找到 {len(csv_files)} 个CSV文件")

    # 第一遍: 收集所有serial_number, 找到有故障记录的硬盘
    all_serials = set()
    failure_records = []  # (date, serial_number)

    for csv_file in csv_files:
        print(f"处理: {csv_file}")
        with open(csv_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                serial = row.get('serial_number', '').strip()
                date_str = row.get('date', '').strip()
                failure = row.get('failure', '0').strip()

                if not serial or not date_str:
                    continue

                all_serials.add(serial)

                if failure == '1':
                    failure_records.append((date_str, serial))

    print(f"总硬盘数: {len(all_serials)}")
    print(f"总故障记录: {len(failure_records)}")

    if not failure_records:
        print("警告: 未找到故障记录(failure=1)")
        sys.exit(1)

    # 选取num_nodes个硬盘
    # 优先选有故障记录的硬盘, 再补充无故障的
    failed_serials = set(s for _, s in failure_records)
    selected_serials = list(failed_serials)[:num_nodes]

    if len(selected_serials) < num_nodes:
        remaining = list(all_serials - failed_serials)[:num_nodes - len(selected_serials)]
        selected_serials.extend(remaining)

    selected_serials = selected_serials[:num_nodes]
    serial_to_nodeid = {s: i for i, s in enumerate(selected_serials)}

    print(f"选取节点数: {len(selected_serials)}")
    print(f"其中有故障记录的: {len(set(selected_serials) & failed_serials)}")

    # 按天聚合故障事件(只保留选中节点的故障)
    daily_failures = defaultdict(list)  # date -> [node_ids]

    for date_str, serial in failure_records:
        if serial in serial_to_nodeid:
            daily_failures[date_str].append(serial_to_nodeid[serial])

    if not daily_failures:
        print("警告: 选中的节点中没有故障记录")
        sys.exit(1)

    # 排序并计算相对天数
    sorted_dates = sorted(daily_failures.keys())
    base_date = datetime.strptime(sorted_dates[0], "%Y-%m-%d")

    # 输出trace文件
    with open(output_file, 'w') as f:
        f.write("# Trace generated from Backblaze data\n")
        f.write(f"# Nodes: {num_nodes}, Source: {input_path}\n")
        f.write(f"# Date range: {sorted_dates[0]} to {sorted_dates[-1]}\n")
        f.write("# Format: day  num_failures  node_id1 node_id2 ...\n")
        f.write("#\n")

        event_count = 0
        for date_str in sorted_dates:
            node_ids = sorted(set(daily_failures[date_str]))  # 去重并排序
            day = (datetime.strptime(date_str, "%Y-%m-%d") - base_date).days
            f.write(f"{day}    {len(node_ids)}    {' '.join(map(str, node_ids))}\n")
            event_count += 1

    print(f"\n输出trace文件: {output_file}")
    print(f"总事件数: {event_count}")
    print(f"时间跨度: {sorted_dates[0]} ~ {sorted_dates[-1]}")

    # 打印node映射关系
    mapping_file = output_file + ".mapping"
    with open(mapping_file, 'w') as f:
        f.write("# node_id -> serial_number mapping\n")
        for serial, nid in sorted(serial_to_nodeid.items(), key=lambda x: x[1]):
            f.write(f"{nid}\t{serial}\n")
    print(f"节点映射: {mapping_file}")


if __name__ == "__main__":
    main()
