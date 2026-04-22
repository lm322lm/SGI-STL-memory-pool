#include "SGIAllocator.h"
#include "MyVector.h"
#include "Benchmark.h"

#include <vector>
#include <iostream>

int main() {
    constexpr std::size_t N = 100'000;

    std::cout << "===== SGI STL Memory Pool Benchmark =====\n";
    std::cout << "Operations: " << N << " (push_back + pop_back)\n\n";

    std::size_t mem_before = bench::get_private_bytes();

    // 1. MyVector + SGIAllocator（自定义内存池）
    auto r1 = bench::run_benchmark<MyVector<int, sgi_stl::SGIAllocator<int>>>(
        "SGI Pool", N);

    // 2. MyVector + std::allocator（标准分配器）
    auto r2 = bench::run_benchmark<MyVector<int, std::allocator<int>>>(
        "std::alloc", N);

    // 3. std::vector + std::allocator（标准库参照）
    auto r3 = bench::run_benchmark<std::vector<int>>(
        "std::vector", N);

    std::size_t mem_after = bench::get_private_bytes();

    // 输出对比：SGI Pool vs std::allocator
    std::cout << "\n[ MyVector: SGI Pool vs std::allocator ]\n";
    bench::print_comparison(r1, r2);

    // 输出对比：SGI Pool vs std::vector
    std::cout << "\n[ MyVector(SGI Pool) vs std::vector ]\n";
    bench::print_comparison(r1, r3);

    // 输出对比：MyVector(std::alloc) vs std::vector
    std::cout << "\n[ MyVector(std::alloc) vs std::vector ]\n";
    bench::print_comparison(r2, r3);

    // 内存信息
    bench::print_memory(mem_before, mem_after);

    std::cout << "\nDone.\n";
    return 0;
}
