# SGI STL Memory Pool

一个基于 SGI STL 二级空间配置器思想实现的 C++ 项目，包含：
- 一级配置器：封装 `malloc/free`，支持 OOM 处理函数
- 二级配置器：针对 `<= 128` 字节小对象的内存池分配
- `SGIAllocator<T>`：可适配 STL 风格容器的自定义分配器
- `MyVector<T, Alloc>`：基于三指针模型实现的简化版 Vector
- 百万级 `push_back/pop_back` 性能基准测试与内存占用统计

## 项目简介
本项目参考 SGI STL 的经典二级空间配置器设计，重点解决频繁小块内存分配带来的系统调用开销和内存复用问题。在此基础上，我实现了一个支持自定义 allocator 的简化版 `MyVector`，并与 `std::allocator`、`std::vector` 做了百万级数据量下的性能对比测试。

这个项目的重点不只是“写一个内存池”，而是把：
1. 内存分配策略
2. 容器实现方式
3. benchmark 对比方法
放在同一个实验框架里，观察它们各自对性能的影响。

## 核心实现

### 1. 一级配置器 `MallocAlloc`
- 对大于 `128` 字节的内存请求直接走 `malloc/free`
- 支持 OOM handler 机制
- 内存不足时循环调用用户设置的处理函数，直到分配成功或抛出 `std::bad_alloc`
- 显著降低外部内存碎片，降低碎片和提升复用主要是整个 PoolAlloc/内存池机制的效果

### 2. 二级配置器 `PoolAlloc`
- 负责管理 `<= 128` 字节的小对象分配
- 使用 `8` 字节对齐
- 通过 `16` 条 free list 管理不同规格的小块内存
- `allocate/deallocate` 在命中 free list 时为 O(1)
- 通过 `refill + chunk_alloc` 批量向内存池补充节点，减少频繁系统分配

### 3. STL 适配器 `SGIAllocator<T>`
- 将内存池封装成 STL 风格 allocator
- 可直接用于自定义容器或标准容器的 allocator 参数

### 4. 自定义容器 `MyVector<T, Alloc>`
- 使用 `_start / _finish / _end_of_storage` 三指针管理内存
- 采用 2 倍扩容策略
- 实现了拷贝构造、移动构造、拷贝赋值、移动赋值和析构函数
- 支持 `push_back` 左值/右值重载
- 支持 `emplace_back`

### 5. 性能说明
- SGI内存池的收益主要来自减少系统调用与低碎片
- MyVector更快部分源于极简实现（无迭代器调试/安全检查），并非全由内存池贡献

### 6. 已知局限
- 当前内存统计依赖 Windows PSAPI，Linux/macOS 需替换对应的进程内存统计实现
- 内存池为非线程安全版本，不能直接用于多线程并发分配场景
- 二级配置器仅优化 <= 128 字节的小块内存分配，更大内存请求会直接回退到 malloc
- 内存池采用“只复用、不回收给操作系统”的策略，已申请内存不会在进程生命周期内主动释放
- MyVector 为教学与实验目的的简化实现，不等同于工业级 std::vector 的完整功能与异常安全保证

## 项目结构
```text
.
├── src
│   ├── SGIAllocator.h       ← 实现一级 + 二级配置器 + STL 适配器
│   ├── MyVector.h           ← 自定义 vector
│   ├── Benchmark.h          ← 压测引擎
│   └── main.cpp             ← 主程序
├── benchmark-result
│   ├── result.md            ← 压测结果说明
│   └── result-picture.png   ← 压测截图展示
├── Architecture
│   └── Architecture.md      ← 项目架构文档
└── website
    └── index.html           ← 可视化展示，结果对比
```

## 编译与运行

```bash
cd src
g++ -std=c++17 -O2 -o benchmark main.cpp -lpsapi
./benchmark
