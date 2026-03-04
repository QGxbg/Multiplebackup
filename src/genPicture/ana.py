import numpy as np
import matplotlib.pyplot as plt

# === 参数设置 ===
num_nodes = 40        # 节点总数
num_stripes = 40     # 批次大小 (Batch size)
n = 14                # Clay(14,10) -> n=14
p = n / num_nodes     # 每个节点被选中的概率 (14/40 = 0.35)

# === 1. 模拟全随机分布 ===
# 初始化节点计数器
node_counts = np.zeros(num_nodes)

for _ in range(num_stripes):
    # 从 40 个节点中随机选择 14 个不同的节点放置数据块
    chosen_nodes = np.random.choice(np.arange(num_nodes), size=n, replace=False)
    node_counts[chosen_nodes] += 1

# === 2. 绘制图 A：CDF 分布 (模仿论文 Figure 3a) ===
# 排序数据以计算 CDF
sorted_data = np.sort(node_counts)
yvals = np.arange(1, len(sorted_data) + 1) / len(sorted_data)

plt.figure(figsize=(12, 5))

# --- 子图 1: CDF ---
plt.subplot(1, 2, 1)
# 绘制阶梯图
plt.step(sorted_data, yvals, where='post', label='Random Layout CDF', color='blue', linewidth=2)

# 标记平均值
mean_val = np.mean(node_counts)
plt.axvline(mean_val, color='red', linestyle='--', label=f'Mean ({mean_val:.1f})')

# 标记最大值 (瓶颈节点)
max_val = np.max(node_counts)
plt.axvline(max_val, color='orange', linestyle='--', label=f'Max Load ({max_val})')

# 标注图例和轴标签
plt.title(f'Figure 3(a) Replica: CDF of Blocks per Node\n(Clay 14,10, 100 Stripes)')
plt.xlabel('# of blocks in nodes (Load)')
plt.ylabel('CDF (Cumulative Probability)')
plt.legend()
plt.grid(True, alpha=0.3)

# --- 子图 2: CV 分析 (模仿论文 Figure 3b 的概念) ---
plt.subplot(1, 2, 2)

# 计算实际模拟的 CV
std_dev = np.std(node_counts)
actual_cv = std_dev / mean_val

# 计算论文公式给出的理论 CV
# 论文公式: CV ≈ sqrt(1 / (k+m)) = sqrt(1/14)
theoretical_cv = np.sqrt(1 / n) 

# 绘制对比柱状图
bars = plt.bar(['Theoretical CV', 'Simulated CV'], [theoretical_cv, actual_cv], 
               color=['gray', 'green'], alpha=0.7)

# 在柱子上标数值
for bar in bars:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2, yval + 0.01, round(yval, 4), ha='center', va='bottom')

# 绘制 "严重偏斜" 阈值线 (论文提到 CV > 0.15 就算严重 )
plt.axhline(y=0.15, color='red', linestyle=':', linewidth=2, label='Skewed Threshold (0.15)')

plt.title(f'Figure 3(b) Concept: Coefficient of Variation\n(Imbalance Metric)')
plt.ylabel('Coefficient of Variation (CV)')
plt.legend()

plt.tight_layout()
plt.show()

# === 打印文本分析结果 ===
print(f"=== 模拟结果分析 ===")
print(f"最忙的节点持有块数: {max_val} (平均值的 {max_val/mean_val:.2f} 倍)")
print(f"最闲的节点持有块数: {np.min(node_counts)}")
print(f"变异系数 (CV): {actual_cv:.4f}")
print(f"论文认为 CV > 0.15 即为严重偏斜，当前结果是否严重? {'是' if actual_cv > 0.15 else '否'}")