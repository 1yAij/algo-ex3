#!/usr/bin/env bash
# 一键：编译 C++ 求解器 → 跑全部实验写 CSV → 用 Python 画所有图到 cpp/output/figures
set -euo pipefail
cd "$(dirname "$0")"
echo "==> 编译"
g++ -std=c++17 -O2 -I cpp -o cpp/graph_coloring cpp/main.cpp cpp/src/graph_coloring.cpp
echo "==> 跑实验 (请在项目根目录，保证 地图数据 可访问)"
./cpp/graph_coloring
echo "==> 画图"
if command -v conda >/dev/null 2>&1; then
  conda run -n base python plot_results.py
else
  python3 plot_results.py
fi
echo "完成: CSV = cpp/output/graph_coloring_results.csv, 图 = cpp/output/figures/"
