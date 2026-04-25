#ifndef BENCHMARK_H_
#define BENCHMARK_H_

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace bench {

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// 压测结果
struct BenchResult {
    std::string name;
    double push_ms  = 0.0;   // push_back 耗时
    double pop_ms   = 0.0;   // pop_back 耗时
    std::size_t ops = 0;     // 操作次数
};

// 对任意 vector 类型执行压测
// VecType 需要支持 push_back(int) 和 pop_back()
template <class VecType>
BenchResult run_benchmark(const std::string& name, std::size_t count) {
    BenchResult r;
    r.name = name;
    r.ops  = count;
    VecType vec;

    // push_back 压测
    auto t1 = Clock::now();
    for (std::size_t i = 0; i < count; ++i) {
        vec.push_back(static_cast<int>(i));
    }
    auto t2 = Clock::now();
    r.push_ms = Ms(t2 - t1).count();

    // pop_back 压测
    t1 = Clock::now();
    for (std::size_t i = 0; i < count; ++i) {
        vec.pop_back();
    }
    t2 = Clock::now();
    r.pop_ms = Ms(t2 - t1).count();

    return r;
}

// 格式化输出对比表
inline void print_comparison(const BenchResult& a, const BenchResult& b) {
    auto row = [](const char* op, double ta, double tb) {
        double speedup = (ta > 0.0) ? (tb / ta) : 0.0;
        std::cout << std::left  << std::setw(28) << op
                  << std::right << std::setw(14) << std::fixed << std::setprecision(2) << ta
                  << std::setw(14) << tb
                  << std::setw(13) << speedup << "x\n";
    };

    std::cout << "\n========== Benchmark Results (ms) ==========\n";
    std::cout << std::left  << std::setw(28) << "Operation"
              << std::right << std::setw(14) << a.name
              << std::setw(14) << b.name
              << std::setw(14) << "Speedup" << "\n";
    std::cout << std::string(70, '-') << "\n";

    std::string push_label = "push_back x " + std::to_string(a.ops);
    std::string pop_label  = "pop_back  x " + std::to_string(a.ops);

    row(push_label.c_str(), a.push_ms, b.push_ms);
    row(pop_label.c_str(),  a.pop_ms,  b.pop_ms);

    double total_a = a.push_ms + a.pop_ms;
    double total_b = b.push_ms + b.pop_ms;
    row("Total", total_a, total_b);
}

} // namespace bench

#endif // BENCHMARK_H_
