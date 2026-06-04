# DPBF vs Test4 随机对拍

默认生成 `n=12`、连通稀疏整数权图，查询数 `q=1`、组数 `g=6`，并比较 `DPBF` 与 `Test4` 的输出权值。

先确保已编译：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

运行默认对拍：

```powershell
python .\hacktests\hack_dpbf_vs_test4.py
```

常用参数：

```powershell
python .\hacktests\hack_dpbf_vs_test4.py --max-iters 20000 --seed 1
python .\hacktests\hack_dpbf_vs_test4.py --n 12 --extra-edges 10 --groups 6 --group-size 3
python .\hacktests\hack_dpbf_vs_test4.py --dpbf-exe .\build\Release\gst_dpbf_main.exe --test4-exe .\build\Release\gst_test4_main.exe
```

脚本会反复覆盖 `data/hack_dpbf_vs_test4/Graph.txt` 和 `data/hack_dpbf_vs_test4/Query.txt`，结果写入 `hacktests/work/result/`。发现差异时会停止，并把触发差异的数据复制到 `hacktests/cases/diff_.../`。
