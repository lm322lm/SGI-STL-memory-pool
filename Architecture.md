# SGI STL 内存池与空间配置器 - Linux，Windows双环境 Benchmark

## 一、项目目标

参考 SGI STL 二级空间配置器思想，实现一个面向小块内存的内存池分配器，并通过简化版 `MyVector` 验证自定义 allocator 与容器之间的完整调用链。

项目重点是拆解并复现以下核心机制：

- 一级配置器：封装 `malloc/free`，并提供 OOM handler。
- 二级配置器：对 `<= 128B` 小块内存按 `8B` 对齐，维护 `16` 条 `free_list`。
- STL allocator 适配层：实现 `SGIAllocator<T>`，供容器通过 `allocator_traits` 调用。
- 自定义容器：实现基于 `_start/_finish/_end_of_storage` 三指针模型的简化版 `MyVector`。
- Benchmark：在 Linux/g++ 与 Windows/Visual Studio 环境下进行三轮十万级 `push_back/pop_back` 对比。

## 二、整体框架

```text
main.cpp
  |
  +-- Benchmark.h
  |     负责计时、运行 push_back/pop_back、打印三组对比结果
  |
  +-- MyVector.h
        |
        +-- SGIAllocator<T>
              |
              +-- PoolAlloc（二级配置器）
              |     free_list + chunk_alloc/refill
              |     管理 8B ~ 128B 小块内存
              |
              +-- MallocAlloc（一级配置器）
                    malloc/free + OOM handler
                    处理大块内存与内存不足场景
```

## 三、各模块作用

| 模块 | 文件 | 作用 |
| --- | --- | --- |
| 一级配置器 | `SGIAllocator.h` | 封装 `malloc/free`，提供 OOM handler |
| 二级配置器 | `SGIAllocator.h` | 使用 `16` 条 `free_list` 管理 `<= 128B` 小块内存 |
| STL 适配器 | `SGIAllocator.h` | 实现 `SGIAllocator<T>`，适配 STL Allocator 模型 |
| 自定义容器 | `MyVector.h` | 基于三指针模型实现简化版 vector，并支持自定义 allocator |
| 压测模块 | `Benchmark.h` | 统计 `push_back`、`pop_back` 和总耗时 |
| 主程序 | `main.cpp` | 组装三组 benchmark 并输出对比表 |

## 四、压测流程

1. 使用 `MyVector<int, SGIAllocator<int>>` 执行 `100000` 次 `push_back` 与 `100000` 次 `pop_back`。
2. 使用 `MyVector<int, std::allocator<int>>` 执行同样测试。
3. 使用 `std::vector<int>` 执行同样测试。
4. 每个环境运行三次，分别记录 `push_back`、`pop_back`、`Total` 和 speedup。

当前项目不统计进程私有内存

## 五、性能指标

- `push_back` 耗时，单位 ms。
- `pop_back` 耗时，单位 ms。
- `Total` 总耗时，单位 ms。
- `Speedup = 第二列耗时 / 第一列耗时`。

当 speedup 大于 `1.00x` 时，第一列更快；当 speedup 小于 `1.00x` 时，第一列更慢。

## 六、Linux 编译运行

```bash
cd ~/Desktop/src
g++ -std=c++17 -O2 -o benchmark main.cpp
./benchmark
```

## 七、Windows 运行说明

Windows 结果来自 Visual Studio `x64/Debug` 运行截图。由于 Debug 构建可能引入额外检查和调试开销，Windows 数据适合说明“不同平台/构建模式下性能表现差异明显”，不应和 Linux/g++ `-O2` 做绝对等价比较。

## 八、三轮平均结果摘要

| 环境 | 对比 | 平均第一列 | 平均第二列 | 平均 Speedup | 结论 |
| --- | --- | ---: | ---: | ---: | --- |
| Linux/g++ `-O2` | `MyVector + SGIAllocator` vs `MyVector + std::allocator` | 1.28 ms | 1.04 ms | 0.81x | 同容器下，标准分配器更快 |
| Linux/g++ `-O2` | `MyVector + SGIAllocator` vs `std::vector` | 1.28 ms | 1.08 ms | 0.85x | 标准库方案略快 |
| Linux/g++ `-O2` | `MyVector + std::allocator` vs `std::vector` | 1.04 ms | 1.08 ms | 1.04x | 自定义容器略快，差距很小 |
| Windows / VS x64 Debug | `MyVector + SGIAllocator` vs `MyVector + std::allocator` | 4.36 ms | 2.86 ms | 0.65x | 同容器下，标准分配器更快 |
| Windows / VS x64 Debug | `MyVector + SGIAllocator` vs `std::vector` | 4.36 ms | 12.27 ms | 2.81x | 自定义组合明显快于 `std::vector` |
| Windows / VS x64 Debug | `MyVector + std::allocator` vs `std::vector` | 2.86 ms | 12.27 ms | 4.30x | 自定义容器路径明显快于 `std::vector` |

## 九、结论边界

当前双环境 benchmark 证明项目实现了自定义分配器、容器和测试链路，并能通过多组对照拆分 allocator 与 container 的影响。

需要注意的是，Windows 下自定义组合相对 `std::vector` 更快，但同容器对比中 `SGIAllocator` 慢于 `std::allocator`，因此得到的准确的结论应该是：自定义内存池和简化版容器在不同平台、标准库实现和构建模式下表现差异明显，性能结论必须绑定测试环境。
