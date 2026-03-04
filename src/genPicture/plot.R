# 加载必要的包
library(ggplot2)
library(dplyr)
library(tidyr)

# 1. 读取保存的 txt 文件
# 请确保将你的全部数据保存为了 "stripe_data.txt" 并放在工作目录下
text_data <- readLines("stripe_data.txt")

# 2. 数据清洗与提取
# 使用正则表达式精准提取以 "in :" 和 "out:" 开头的行（避开 inmap 和 outmap）
in_lines <- text_data[grep("\\bin\\s*:", text_data)]
out_lines <- text_data[grep("\\bout\\s*:", text_data)]

# 去掉前面的 "in :" / "out:" 标签，只保留纯数字部分
in_str <- sub(".*in\\s*:\\s*", "", in_lines)
out_str <- sub(".*out\\s*:\\s*", "", out_lines)

# 3. 将字符串转换为矩阵 (100行 x 40列)
parse_lines_to_matrix <- function(lines_str) {
  # 拆分空格并转为数字，按行绑定成矩阵
  do.call(rbind, lapply(strsplit(trimws(lines_str), "\\s+"), as.numeric))
}

in_matrix <- parse_lines_to_matrix(in_str)
out_matrix <- parse_lines_to_matrix(out_str)

# 4. 按节点(列)汇总求和
# colSums 会把 100 个 Stripe 里同一个节点的值全部加起来
total_in <- colSums(in_matrix, na.rm = TRUE)
total_out <- colSums(out_matrix, na.rm = TRUE)

# 5. 构建供 ggplot 绘图使用的数据框 (长数据格式)
node_count <- ncol(in_matrix) # 应该是 40

df <- data.frame(
  Node = factor(rep(0:(node_count - 1), 2), levels = 0:(node_count - 1)),
  Type = factor(rep(c("In (接收)", "Out (发送)"), each = node_count), 
                levels = c("In (接收)", "Out (发送)")),
  Traffic = c(total_in, total_out)
)

# 6. 绘制分布图
p <- ggplot(df, aes(x = Node, y = Traffic, fill = Type)) +
  # 使用 dodge 并排显示 In 和 Out
  geom_bar(stat = "identity", position = position_dodge(width = 0.8), width = 0.7) +
  scale_fill_manual(values = c("In (接收)" = "#4daf4a", "Out (发送)" = "#e41a1c")) +
  labs(
    title = "全局节点 In / Out 流量分布总计 (100 Stripes)",
    x = "节点编号 (Node 0 - 39)",
    y = "累加流量 (Traffic)",
    fill = "流量方向"
  ) +
  theme_minimal() +
  theme(
    plot.title = element_text(hjust = 0.5, face = "bold", size = 16),
    axis.title = element_text(face = "bold", size = 12),
    # 横坐标有40个节点，字体稍微缩小以免拥挤
    axis.text.x = element_text(size = 9, angle = 0), 
    axis.text.y = element_text(size = 10),
    legend.position = "top",
    legend.title = element_blank(),
    legend.text = element_text(size = 11, face = "bold"),
    panel.grid.major.x = element_blank()
  )

# 如果你的屏幕较窄，或者节点文字重叠，可以将横坐标文字倾斜：
# axis.text.x = element_text(size = 9, angle = 45, hjust = 1)

print(p)

# (可选) 将这 40 个节点的具体总和保存为表格查看
# summary_table <- data.frame(Node = 0:(node_count-1), Total_In = total_in, Total_Out = total_out)
# View(summary_table)