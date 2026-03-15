#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

using namespace std;

// Clay(14,10) 负载数据
double get_repair_load(int f) {
  if (f <= 0)
    return 0.0;
  if (f <= 2)
    return 1.0;
  if (f == 3)
    return 1.3;
  if (f == 4)
    return 1.7;
  return 2.0;
}

struct Stripe {
  vector<int> node_indices;
  int current_faults = 0;
};

int main(int argc, char *argv[]) {
  // 参数：mode (0:Eager, 1:Lazy, 2:Proposed), p, auc
  int mode = (argc >= 2) ? atoi(argv[1]) : 2;
  double p_fault = (argc >= 3) ? atof(argv[2]) : 0.01;
  double auc = (argc >= 4) ? atof(argv[3]) : 0.85;

  const int TOTAL_NODES = 100;
  const int STRIPES_COUNT = 100;
  const int N = 14;
  const int TOLERANCE = 4;

  // 初始化 Placement (此处建议载入你的真实数据)
  vector<vector<int>> placement(STRIPES_COUNT, vector<int>(N));
  mt19937 gen(42);
  for (int i = 0; i < STRIPES_COUNT; ++i) {
    vector<int> all_nodes(TOTAL_NODES);
    iota(all_nodes.begin(), all_nodes.end(), 0);
    shuffle(all_nodes.begin(), all_nodes.end(), gen);
    for (int j = 0; j < N; ++j)
      placement[i][j] = all_nodes[j];
  }

  vector<bool> node_health(TOTAL_NODES, true);
  double total_load = 0;
  int loss_events = 0;
  int sim_days = 365;

  for (int d = 0; d < sim_days; ++d) {
    bernoulli_distribution dist(p_fault);
    for (int i = 0; i < TOTAL_NODES; ++i)
      if (dist(gen))
        node_health[i] = false;

    int max_f = 0;
    for (int i = 0; i < STRIPES_COUNT; ++i) {
      int f = 0;
      for (int idx : placement[i])
        if (!node_health[idx])
          f++;
      max_f = max(max_f, f);
      if (f > TOLERANCE)
        loss_events++;
    }

    bool trigger = false;
    if (mode == 0) { // Eager
      if (max_f >= 1)
        trigger = true;
    } else if (mode == 1) { // Lazy
      if (max_f >= 4)
        trigger = true;
    } else { // Proposed (ML-Aided)
      if (max_f >= 4)
        trigger = true;
      else if (max_f == 3) {
        // 基于 AUC 的简化预测判定
        uniform_real_distribution<double> d_score(0, 1);
        if (d_score(gen) < auc)
          trigger = true;
      }
    }

    if (trigger) {
      total_load += get_repair_load(max_f);
      fill(node_health.begin(), node_health.end(), true);
    }
  }

  // 输出：Mode, Load, Loss
  cout << mode << "," << total_load << "," << loss_events << endl;
  return 0;
}