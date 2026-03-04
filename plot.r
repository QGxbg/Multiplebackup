# ==========================================
# 准备工作：如果未安装 ggplot2 包，请先删除下一行的 # 号运行安装
# install.packages("ggplot2")
# ==========================================

# 加载绘图包
library(ggplot2)

# 1. 读取数据
# sep = "" 完美处理单空格或多空格分隔的数据；header = FALSE 表示没有列名表头
data <- read.table("log.txt", header = FALSE, sep = "")

# 2. 为数据列重命名，方便代码调用
colnames(data) <- c("MaxRepairLoad", "RepairBandwidth")

# 3. 创建散点图对象
p <- ggplot(data, aes(x = MaxRepairLoad/4, y = RepairBandwidth/4)) +
  geom_point(color = "#1f77b4", size = 3, alpha = 0.7) +   # 设置点的颜色(经典蓝)、大小和透明度
  
  # 关键设置：强制 X 轴和 Y 轴严格从 0 开始 (第一象限)，最大值上方留出 10% 的空白 (mult = c(0, 0.1))
  scale_x_continuous(expand = expansion(mult = c(0, 0.1)), limits = c(0, NA)) +
  scale_y_continuous(expand = expansion(mult = c(0, 0.1)), limits = c(0, NA)) +
  
  # 设置图表文字标签
  labs(
    # title = "最大修复荷载与修复带宽关系图",
    x = "最大修复荷载 (块)",
    y = "修复带宽 (块)"
  ) +
  
  # 美化图表主题
  theme_minimal() +                                        # 简洁的白色网格背景
  theme(
    plot.title = element_text(hjust = 0.5, face = "bold", size = 15), # 标题居中并加粗
    axis.line = element_line(color = "black", linewidth = 0.5),       # 画出黑色的 X/Y 实线坐标轴
    panel.grid.minor = element_blank()                                # 去除次要网格线，让画面更干净
  )

# 4. 在屏幕/RStudio 绘图区中显示出这张图
print(p)

# 5. 保存图表为高清图片 (保存在与 log.txt 相同的文件夹下)
# dpi = 300 保证了图片插入论文或报告时足够清晰
ggsave("scatter_plot_butterfly.png", plot = p, width = 8, height = 6, dpi = 300)

cat("绘图完成！图表已保存为当前目录下的 'scatter_plot.png' 文件。\n")