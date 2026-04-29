# SGI STL Memory Pool

## 🚀 项目在线演示
点击下方链接查看性能测试结果展示页：
[项目演示地址](https://lm322lm.github.io/SGI-STL-memory-pool/)

一个参考 SGI STL 二级空间配置器思想实现的 C++ 内存池实验项目，包含自定义 allocator、简化版 vector 容器，以及 Linux / Windows 双环境 benchmark 对比。

## 项目简介

本项目参考 SGI STL 的经典二级空间配置器设计，实现了一个面向小块内存的内存池分配器，并将其封装为 STL 风格的 `SGIAllocator<T>`。在此基础上，项目实现了一个支持自定义 allocator 的简化版 `MyVector<T, Alloc>`，用于验证“分配器 - 容器 - benchmark”之间的完整调用链。

这个项目的重点是把以下内容放在同一个实验框架里进行观察和验证：

1. 内存分配策略：一级配置器、二级配置器、free list、小块复用。
2. 容器实现方式：三指针模型、扩容策略、对象构造与析构。
3. Benchmark 对比方法：拆分 allocator-only、container-only 和组合方案的性能影响。

## 核心实现

### 1. 一级配置器 `MallocAlloc`

- 对大于 `128` 字节的内存请求直接走 `malloc/free`。
- 支持 OOM handler 机制。
- 内存不足时循环调用用户设置的处理函数，直到分配成功或抛出 `std::bad_alloc`。
- 主要负责大块内存和兜底分配，不把碎片优化直接归因到一级配置器本身。

### 2. 二级配置器 `PoolAlloc`

- 负责管理 `<= 128` 字节的小块内存分配。
- 使用 `8` 字节对齐。
- 通过 `16` 条 free list 管理 `8, 16, 24, ..., 128` 字节规格的小块。
- `allocate/deallocate` 在命中 free list 时为 `O(1)`。
- 通过 `refill + chunk_alloc` 批量向内存池补充节点，减少频繁向系统申请小块内存的开销。

### 3. STL 适配器 `SGIAllocator<T>`

- 将 `PoolAlloc` 封装成 STL 风格 allocator。
- 提供 `allocate`、`deallocate`、`construct`、`destroy`、`rebind` 等接口。
- 可作为 `MyVector<T, Alloc>` 的 allocator 参数，验证自定义分配器与容器之间的协作流程。

### 4. 自定义容器 `MyVector<T, Alloc>`

- 使用 `_start / _finish / _end_of_storage` 三指针模型管理连续内存。
- 采用 `2` 倍扩容策略。
- 支持 `push_back` 左值/右值重载。
- 支持 `emplace_back`。
- 支持 `reserve`。
- 实现拷贝构造、移动构造、拷贝赋值、移动赋值和析构函数。

## Benchmark 设计

当前 benchmark 主要测试 `int` 类型下的十万级 `push_back/pop_back` 操作，并对比三组方案：

| 编号 | 容器 | 分配器 | 说明 |
| --- | --- | --- | --- |
| A | `MyVector` | `SGIAllocator` | 自定义容器 + 自定义内存池 |
| B | `MyVector` | `std::allocator` | 自定义容器 + 标准分配器 |
| C | `std::vector` | `std::allocator` | 标准库容器 + 标准分配器 |

程序输出中的 `Speedup` 计算方式为：

```text
Speedup = 第二列耗时 / 第一列耗时
```

因此：

- `Speedup > 1.00x` 表示第一列更快。
- `Speedup < 1.00x` 表示第一列更慢。

当前代码只统计 `push_back`、`pop_back` 和 `Total` 耗时，不统计进程内存占用。

## 测试环境与结果

### Linux/g++ `-O2`

Linux 环境下使用如下命令编译运行：

```bash
cd src
g++ -std=c++17 -O2 -o benchmark main.cpp
./benchmark
```

三轮 Total 平均结果如下：

| 对比 | 平均第一列 | 平均第二列 | 平均 Speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `MyVector + SGIAllocator` vs `MyVector + std::allocator` | 1.28 ms | 1.04 ms | 0.81x | 同容器下，`std::allocator` 更快 |
| `MyVector + SGIAllocator` vs `std::vector` | 1.28 ms | 1.08 ms | 0.85x | 标准库方案略快 |
| `MyVector + std::allocator` vs `std::vector` | 1.04 ms | 1.08 ms | 1.04x | 自定义容器略快，但差距很小 |

Linux/g++ `-O2` 下，自定义内存池和自定义组合方案没有超过标准库方案。这个结果说明，在该平台和该 workload 下，标准库实现已经非常接近甚至略优。

### Windows / Visual Studio x64 Debug

Windows 环境下的结果来自 Visual Studio `x64/Debug` 运行截图。三轮 Total 平均结果如下：

| 对比 | 平均第一列 | 平均第二列 | 平均 Speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| `MyVector + SGIAllocator` vs `MyVector + std::allocator` | 4.36 ms | 2.86 ms | 0.65x | 同容器下，`std::allocator` 更快 |
| `MyVector + SGIAllocator` vs `std::vector` | 4.36 ms | 12.27 ms | 2.81x | 自定义组合明显快于 `std::vector` |
| `MyVector + std::allocator` vs `std::vector` | 2.86 ms | 12.27 ms | 4.30x | 自定义容器路径明显快于 `std::vector` |

Windows Debug 环境下，自定义组合方案相对 `std::vector` 表现更好，三轮平均 speedup 约为 `2.81x`。但同容器对比中，`SGIAllocator` 慢于 `std::allocator`，因此不能把 Windows 下的整体优势简单归因为“内存池本身更快”。更合理的解释是：简化版 `MyVector` 的执行路径更短，并且 `std::vector` 在 Debug 构建下可能存在更多检查和调试开销。

## 结果解读

综合 Linux 和 Windows 两组结果，可以得到更谨慎的结论：

- 自定义内存池复现了 SGI STL 二级空间配置器的核心机制。
- `SGIAllocator<T>` 与 `MyVector<T, Alloc>` 打通了自定义 allocator 和容器之间的调用链。
- Windows Debug 环境下，自定义组合方案相对 `std::vector` 有明显优势。
- Linux/g++ `-O2` 环境下，标准库方案更接近甚至略优。
- 性能结果对编译器、标准库实现、构建模式和测试 workload 比较敏感。
- 通过双环境三轮 benchmark，拆分分析 allocator 与 container 对性能的影响。


## 已知局限

- 当前内存池为非线程安全版本，不能直接用于多线程并发分配场景。
- 二级配置器仅优化 `<= 128` 字节的小块内存分配，更大内存请求会回退到一级配置器。
- 内存池采用“复用优先”的策略，已申请的小块内存会挂回 free list 供后续使用，不会在进程生命周期内主动归还给操作系统。
- `MyVector` 是教学与实验目的的简化实现，不等同于工业级 `std::vector` 的完整功能、异常安全和标准兼容性。
- 当前 benchmark 主要覆盖单线程、`int` 类型和十万级 `push_back/pop_back` workload，后续可扩展到 `std::string`、自定义对象、多轮统计、中位数统计和 Release/Debug 多构建模式对比。
- Windows 结果来自 Visual Studio `x64/Debug`，Debug 构建可能包含额外检查，不能和 Linux/g++ `-O2` 做绝对等价比较。

## 项目结构

```text
.
├── src
│   ├── SGIAllocator.h       # 一级配置器 + 二级配置器 + STL allocator 适配器
│   ├── MyVector.h           # 简化版 vector 容器
│   ├── Benchmark.h          # benchmark 计时与结果输出
│   └── main.cpp             # 主程序
├── benchmark-result
│   ├── result.md            # Linux / Windows 三轮压测结果说明
│   ├── result-Linux (1).png
│   ├── result-Linux (2).png
│   ├── result-Linux (3).png
│   ├── result-Windows (1).png
│   ├── result-Windows (2).png
│   └── result-Windows (3).png
├── Architecture
│   └── Architecture.md      # 项目架构文档
└── website
    └── index.html           # benchmark 可视化展示页
```

## 编译与运行

### Linux

```bash
cd src
g++ -std=c++17 -O2 -o benchmark main.cpp
./benchmark
```

### Windows

可使用 Visual Studio 打开项目文件，并在 `x64/Debug` 或 `x64/Release` 下运行。不同构建模式下的 benchmark 结果可能差异较大。

