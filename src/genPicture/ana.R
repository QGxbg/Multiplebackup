# 安装并加载必要的包
if (!require("ggplot2")) install.packages("ggplot2")
if (!require("dplyr")) install.packages("dplyr")
library(ggplot2)
library(dplyr)

# ================= 配置参数 =================
set.seed(42)  # 设置随机种子
num_nodes <- 40
num_stripes <- 80   # 批次大小
stripe_width <- 14  # Clay(14,10)
target_group <- c(0, 1, 2, 3) # 限制策略的目标组
fat_node_id <- 0    # 胖节点策略的特定节点 ID

# ================= 模拟函数定义 =================

# 1. 限制性放置策略 (Constrained: Must include one of 0-3)
simulate_constrained <- function() {
  # R索引修正 (0->1)
  r_target_group <- target_group + 1
  node_counts <- rep(0, num_nodes)
  
  for (i in 1:num_stripes) {
    # 步骤A: 从目标组 {0,1,2,3} 中随机强制选 1 个
    guaranteed <- sample(r_target_group, 1)
    
    # 步骤B: 从剩余节点中选 13 个
    pool <- setdiff(1:num_nodes, guaranteed)
    others <- sample(pool, stripe_width - 1, replace = FALSE)
    
    stripe <- c(guaranteed, others)
    node_counts[stripe] <- node_counts[stripe] + 1
  }
  return(node_counts)
}

# 2. 胖节点策略 (Fat Node: Must include Node 0)
simulate_fat_node <- function() {
  # R索引修正 (0->1)
  r_fat_node <- fat_node_id + 1
  node_counts <- rep(0, num_nodes)
  
  for (i in 1:num_stripes) {
    # 步骤A: 强制包含 Node 0
    guaranteed <- r_fat_node
    
    # 步骤B: 从剩余 39 个节点中选 13 个
    pool <- setdiff(1:num_nodes, guaranteed)
    others <- sample(pool, stripe_width - 1, replace = FALSE)
    
    stripe <- c(guaranteed, others)
    node_counts[stripe] <- node_counts[stripe] + 1
  }
  return(node_counts)
}

# 3. 纯随机策略 (Pure Random)
simulate_random <- function() {
  node_counts <- rep(0, num_nodes)
  for (i in 1:num_stripes) {
    stripe <- sample(1:num_nodes, stripe_width, replace = FALSE)
    node_counts[stripe] <- node_counts[stripe] + 1
  }
  return(node_counts)
}

# ================= 执行模拟 =================

counts_constrained <- simulate_constrained()
counts_fat <- simulate_fat_node()
counts_random <- simulate_random()

# ================= 统计分析 =================

calc_stats <- function(counts, name) {
  mean_val <- mean(counts)
  sd_val <- sd(counts)
  cv_val <- sd_val / mean_val
  
  cat(sprintf("--- %s ---\n", name))
  cat(sprintf("平均负载: %.2f\n", mean_val))
  cat(sprintf("变异系数 (CV): %.4f\n", cv_val))
  cat(sprintf("最大负载: %d\n", max(counts)))
  # 打印 Node 0 的负载
  cat(sprintf("Node 0 负载: %d\n", counts[1]))
  cat(sprintf("Top 5 节点负载: %s\n\n", paste(sort(counts, decreasing=T)[1:5], collapse=", ")))
}

calc_stats(counts_random, "纯随机策略 (Random)")
calc_stats(counts_constrained, "限制策略 (Group 0-3)")
calc_stats(counts_fat, "胖节点策略 (Fat Node 0)")

# ================= 绘图 (ggplot2) =================

# 1. 准备绘图数据 (合并三种策略)
df_all <- rbind(
  data.frame(Load = counts_random, Strategy = "1. Pure Random"),
  data.frame(Load = counts_constrained, Strategy = "2. Constrained (Group 0-3)"),
  data.frame(Load = counts_fat, Strategy = "3. Fat Node (Node 0)")
)

# 2. 绘制 CDF 图 (累积分布函数) - 对比不均衡度
p_cdf <- ggplot(df_all, aes(x = Load, color = Strategy, linetype = Strategy)) +
  stat_ecdf(geom = "step", linewidth = 1.2) +
  scale_color_manual(values = c("1. Pure Random" = "#377EB8", 
                                "2. Constrained (Group 0-3)" = "#4DAF4A",
                                "3. Fat Node (Node 0)" = "#E41A1C")) +
  scale_linetype_manual(values = c("solid", "longdash", "solid")) +
  labs(
    title = "Load Distribution CDF Comparison",
    subtitle = "Comparing Random, Group Constrained, and Fat Node scenarios",
    x = "Blocks per Node (Load)",
    y = "CDF"
  ) +
  theme_minimal() +
  theme(legend.position = "bottom")

print(p_cdf)

# 3. 绘制 分面柱状图 (直观展示 Node 0 的极端情况)
# 构造用于柱状图的数据，增加是否为 Target 的标记
df_bar <- df_all %>%
  group_by(Strategy) %>%
  mutate(NodeID = 0:(num_nodes-1)) %>%
  mutate(Highlight = case_when(
    Strategy == "3. Fat Node (Node 0)" & NodeID == 0 ~ "Fat Node (0)",
    Strategy == "2. Constrained (Group 0-3)" & NodeID %in% 0:3 ~ "Group (0-3)",
    TRUE ~ "Normal"
  ))

p_bar <- ggplot(df_bar, aes(x = NodeID, y = Load, fill = Highlight)) +
  geom_bar(stat = "identity", width = 0.8) +
  facet_wrap(~Strategy, ncol = 1) +  # 上下分面排列
  scale_fill_manual(values = c("Normal" = "gray80", 
                               "Group (0-3)" = "#4DAF4A", 
                               "Fat Node (0)" = "#E41A1C")) +
  labs(
    title = "Node Load Balance per Strategy",
    subtitle = "Notice the extreme load on Node 0 in the Fat Node strategy",
    x = "Node ID",
    y = "Block Count"
  ) +
  theme_bw()

print(p_bar)