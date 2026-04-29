# SGI STL 内存池压测结果解读（Linux / Windows 三轮对比）

## 测试说明

- 操作规模：`100000` 次 `push_back` + `100000` 次 `pop_back`
- 测试类型：`int`
- Linux 编译命令：`g++ -std=c++17 -O2 -o benchmark main.cpp`
- Linux 运行命令：`./benchmark`
- Windows 运行环境：Visual Studio `x64/Debug` 运行结果（由截图中的 `x64\Debug\Project1.exe` 判断）
- 结果来源：`result-Linux (1).png`、`result-Linux (2).png`、`result-Linux (3).png`、`result-Windows (1).png`、`result-Windows (2).png`、`result-Windows (3).png`

> 当前 `Benchmark.h` 只统计 `push_back`、`pop_back` 和 `Total` 耗时，不统计进程内存占用，因此本文档只展示耗时和 speedup。

## 测试对象

| 编号 | 容器 | 分配器 | 说明 |
| --- | --- | --- | --- |
| A | `MyVector` | `SGIAllocator` | 自定义容器 + 自定义内存池 |
| B | `MyVector` | `std::allocator` | 自定义容器 + 标准分配器 |
| C | `std::vector` | `std::allocator` | 标准库容器 + 标准分配器 |

## Speedup 说明

程序输出中的 `Speedup` 计算方式为：第二列耗时 / 第一列耗时。

- `Speedup > 1.00x`：第一列更快。
- `Speedup < 1.00x`：第一列更慢。
- `pop_back` 中出现 `0.00 ms` 时，说明耗时低于当前输出精度，对应的大倍数 speedup 不适合作为主要性能结论。

## 平均结果汇总（按三轮 Total 耗时计算）

| 环境 | 对比 | Run 1 | Run 2 | Run 3 | 平均第一列 | 平均第二列 | 平均 Speedup | 结论 |
| --- | --- | --- | --- | --- | ---: | ---: | ---: | --- |
| Linux/g++ `-O2` | A vs B：`MyVector + SGIAllocator` vs `MyVector + std::allocator` | 1.26 vs 1.10 | 1.25 vs 1.09 | 1.33 vs 0.92 | 1.28 ms | 1.04 ms | 0.81x | 同容器下，`std::allocator` 更快 |
| Linux/g++ `-O2` | A vs C：`MyVector + SGIAllocator` vs `std::vector` | 1.26 vs 1.14 | 1.25 vs 1.09 | 1.33 vs 1.02 | 1.28 ms | 1.08 ms | 0.85x | Linux 下标准库方案略快 |
| Linux/g++ `-O2` | B vs C：`MyVector + std::allocator` vs `std::vector` | 1.10 vs 1.14 | 1.09 vs 1.09 | 0.92 vs 1.02 | 1.04 ms | 1.08 ms | 1.04x | 自定义容器略快，但差距很小 |
| Windows / VS x64 Debug | A vs B：`MyVector + SGIAllocator` vs `MyVector + std::allocator` | 4.42 vs 2.60 | 4.93 vs 3.52 | 3.74 vs 2.45 | 4.36 ms | 2.86 ms | 0.65x | 同容器下，`std::allocator` 更快 |
| Windows / VS x64 Debug | A vs C：`MyVector + SGIAllocator` vs `std::vector` | 4.42 vs 12.01 | 4.93 vs 12.70 | 3.74 vs 12.11 | 4.36 ms | 12.27 ms | 2.81x | 自定义组合明显快于 `std::vector` |
| Windows / VS x64 Debug | B vs C：`MyVector + std::allocator` vs `std::vector` | 2.60 vs 12.01 | 3.52 vs 12.70 | 2.45 vs 12.11 | 2.86 ms | 12.27 ms | 4.30x | 自定义容器路径明显快于 `std::vector` |

## Linux 三轮详细结果

### A vs B：同容器，对比分配器

| 轮次 | SGI Pool Total | std::alloc Total | Speedup |
| --- | ---: | ---: | ---: |
| Run 1 | 1.26 ms | 1.10 ms | 0.87x |
| Run 2 | 1.25 ms | 1.09 ms | 0.87x |
| Run 3 | 1.33 ms | 0.92 ms | 0.69x |
| 平均 | 1.28 ms | 1.04 ms | 0.81x |

### A vs C：自定义组合 vs 标准库方案

| 轮次 | SGI Pool Total | std::vector Total | Speedup |
| --- | ---: | ---: | ---: |
| Run 1 | 1.26 ms | 1.14 ms | 0.91x |
| Run 2 | 1.25 ms | 1.09 ms | 0.87x |
| Run 3 | 1.33 ms | 1.02 ms | 0.77x |
| 平均 | 1.28 ms | 1.08 ms | 0.85x |

### B vs C：同分配器，对比容器

| 轮次 | MyVector(std::alloc) Total | std::vector Total | Speedup |
| --- | ---: | ---: | ---: |
| Run 1 | 1.10 ms | 1.14 ms | 1.04x |
| Run 2 | 1.09 ms | 1.09 ms | 1.01x |
| Run 3 | 0.92 ms | 1.02 ms | 1.11x |
| 平均 | 1.04 ms | 1.08 ms | 1.04x |

### Linux 结论

Linux/g++ `-O2` 下，自定义内存池和自定义组合方案没有超过标准库方案。更准确的表述是：Linux 环境下完成了三轮 benchmark，对 allocator 与 container 的影响进行了拆分验证，结果显示标准库实现已经非常接近甚至略优。

## Windows 三轮详细结果

### A vs B：同容器，对比分配器

| 轮次 | SGI Pool Total | std::alloc Total | Speedup |
| --- | ---: | ---: | ---: |
| Run 1 | 4.42 ms | 2.60 ms | 0.59x |
| Run 2 | 4.93 ms | 3.52 ms | 0.71x |
| Run 3 | 3.74 ms | 2.45 ms | 0.66x |
| 平均 | 4.36 ms | 2.86 ms | 0.65x |

### A vs C：自定义组合 vs 标准库方案

| 轮次 | SGI Pool Total | std::vector Total | Speedup |
| --- | ---: | ---: | ---: |
| Run 1 | 4.42 ms | 12.01 ms | 2.72x |
| Run 2 | 4.93 ms | 12.70 ms | 2.57x |
| Run 3 | 3.74 ms | 12.11 ms | 3.23x |
| 平均 | 4.36 ms | 12.27 ms | 2.81x |

### B vs C：同分配器，对比容器

| 轮次 | MyVector(std::alloc) Total | std::vector Total | Speedup |
| --- | ---: | ---: | ---: |
| Run 1 | 2.60 ms | 12.01 ms | 4.62x |
| Run 2 | 3.52 ms | 12.70 ms | 3.61x |
| Run 3 | 2.45 ms | 12.11 ms | 4.94x |
| 平均 | 2.86 ms | 12.27 ms | 4.30x |

### Windows 结论

Windows / Visual Studio `x64/Debug` 下，自定义组合方案相对 `std::vector` 表现更好，三轮平均 speedup 约为 `2.81x`。但同容器对比中，`MyVector + SGIAllocator` 慢于 `MyVector + std::allocator`，说明 Windows 下的主要优势不能直接归因于“内存池本身更快”，更合理的解释是：简化版 `MyVector` 的执行路径更短，并且 `std::vector` 在 Debug 环境下可能存在更多检查和调试开销。

## 综合结论

1. Linux/g++ `-O2` 下，自定义内存池方案没有取得性能优势，标准库实现表现更稳定。
2. Windows / Visual Studio `x64/Debug` 下，自定义组合方案相对 `std::vector` 有明显优势，平均约 `2.81x`。
3. 两个平台结果相反，说明该 benchmark 对编译器、标准库实现、构建模式和 workload 非常敏感。

