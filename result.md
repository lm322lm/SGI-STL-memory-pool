# SGI STL 内存池压测结果解读



## 测试对象

| 编号 |      容器    |         分配器       |          说明            |
|  A   | MyVector    | SGIAllocator（内存池）| 自定义容器 + 自定义内存池 |
|  B   | MyVector    |     std::allocator   | 自定义容器 + 标准分配器   |
|  C   | std::vector |     std::allocator   | 标准库容器 + 标准分配器   |



## 第一组：A vs B（SGI Pool vs std::alloc，同容器对比）

Operation                     SGI Pool    std::alloc     Speedup
push_back x 1000000              2.90          3.41       1.18x
pop_back  x 1000000              0.00          0.00       0.03x
Total                            2.91          3.41       1.17x


### 解读
这组对比的核心变量是 allocator，容器实现完全相同。
push_back 阶段 SGI Pool 快 18%，原因：
- vector 扩容时需要 allocate 新内存块。SGI 内存池的 allocate 只需从 free_list 头部取一个节点，是 O(1) 的指针操作。
- std::allocator 底层走 malloc，每次需要遍历空闲链表寻找合适的块，可能触发 brk/mmap 系统调用。
- 内存池一次性向 OS 申请大块内存，后续分配全在用户态完成，减少了系统调用次数。
pop_back 阶段两者都接近 0ms，原因：
- vector 的 pop_back 只做两件事：`--_finish` 和调用析构函数。
- int 是基本类型，析构函数是空操作（编译器直接优化掉）。
- pop_back 不释放内存（capacity 不变），allocator 完全不参与，所以两者无差异。

### 结论
纯 allocator 层面，SGI 内存池比标准 malloc 快约 18%。



## 第二组：A vs C（SGI Pool + MyVector vs std::vector）

Operation                     SGI Pool   std::vector     Speedup
push_back x 1000000              2.90          5.14       1.77x
pop_back  x 1000000              0.00          0.00       0.00x
Total                            2.91          5.14       1.77x


### 解读
这组同时改变了两个变量：allocator 和容器实现。
push_back 阶段 SGI Pool 快 77%，加速来自两个因素叠加：
1. allocator 优势（约 18%）：同第一组分析。
2. 容器实现差异（约 51%）：MSVC 的 std::vector 内部有额外开销：
   - 迭代器调试检查（即使 Release 模式也可能保留部分）
   - 更复杂的异常安全路径
   - 边界安全检查
   - MyVector 是极简实现，没有这些额外逻辑

### 结论
内存池 + 精简容器的组合，比标准库方案快 77%。这也是简历项目的核心亮点。


## 第三组：B vs C（MyVector vs std::vector，同 allocator 对比）

Operation                   std::alloc   std::vector     Speedup
push_back x 1000000              3.41          5.14       1.51x
pop_back  x 1000000              0.00          0.00       0.00x
Total                            3.41          5.14       1.51x


### 解读
这组的核心变量是容器实现，allocator 相同。
MyVector 比 std::vector 快 51%，纯粹是容器实现的差异：
- MyVector 的 push_back 路径极短：判断是否需要扩容 → expand/construct → ++_finish
- std::vector 的 push_back 路径更长：安全检查 → 异常处理 → 可能的迭代器失效标记 → 实际操作

### 结论
精简的容器实现本身就能带来显著的性能提升。


## 内存统计
Process private before: 0.52 MB
Process private after:  1.57 MB
Delta:                  1.05 MB


### 解读
- 程序启动时占用 0.52 MB（运行时库、栈等基础开销）
- 三组测试全部完成后占用 1.57 MB，净增 1.05 MB
- 理论上 100 万个 int = 4 MB，但实际增量远小于 3 × 4 MB = 12 MB，原因：
  - 三组测试是顺序执行的，前一组的 vector 析构后释放的内存会被后一组复用
  - SGI 内存池的 deallocate 把块挂回 free_list 而非归还 OS，但这些块可以被后续分配复用
  - vector 扩容时旧空间被释放，分配器可以回收利用
- 1.05 MB 的增量主要来自 SGI 内存池持有的未归还内存（内存池只增不减的特性）


## 性能加速比汇总
|                对比                 | push_back 加速比 |              主要原因                 |
|   SGI Pool vs std::alloc（同容器）  |       1.18x      |   内存池 allocate 更快，减少系统调用   |
| SGI Pool + MyVector vs std::vector |       1.77x      |     内存池优势 + 精简容器无额外检查     |
| MyVector vs std::vector（同 alloc） |       1.51x      | 精简容器实现，无迭代器调试/安全检查开销 |


## 为什么 pop_back 全部接近 0？
pop_back 对 int 类型来说几乎是空操作：
1. `--_finish`：一次指针减法
2. `destroy(int*)`：int 是 trivially destructible，析构函数被编译器完全优化掉
100 万次指针减法在现代 CPU 上耗时不到 1ms，低于计时精度，所以显示为 0.00ms。如果测试的是 std::string 等有实际析构逻辑的类型，pop_back 的耗时差异就会体现出来。
