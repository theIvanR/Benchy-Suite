#pragma GCC optimize ("O0")

#include <iostream>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>
#include <algorithm>

// Number of operations per thread and threads
    constexpr uint64_t OP_COUNT = 1E9; // Adjust as needed
    unsigned NUM_THREADS = std::thread::hardware_concurrency();

// Template function to perform operations based on data type
template <typename T>
void perform_operations(const char* type_name, double& thread_time, uint64_t ops) {
    volatile T op_counter = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < ops; ++i) {
        ++op_counter;  // Increment operation counter (dummy operation)
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    thread_time = elapsed.count();
}

int main() {
    std::vector<std::thread> threads;
    std::vector<double> thread_times(NUM_THREADS); // Store individual thread times

    auto benchmark = [&](const char* type_name, auto data_type) {
        // Single-threaded benchmark
        double single_thread_time;
        perform_operations<decltype(data_type)>(type_name, single_thread_time, OP_COUNT);
        double single_thread_ops_per_sec = OP_COUNT / single_thread_time;

        // Multi-threaded benchmark
        for (unsigned i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&, i] {
                perform_operations<decltype(data_type)>(type_name, thread_times[i], OP_COUNT);
            });
        }

        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        threads.clear(); // Clear threads vector for the next benchmark run

        // Calculate the max time taken by any thread for this data type
        double max_time = *std::max_element(thread_times.begin(), thread_times.end());
        double multi_thread_ops_per_sec = (OP_COUNT * NUM_THREADS) / max_time;

        // Calculate the single to multi-thread ratio
        double ratio = multi_thread_ops_per_sec / single_thread_ops_per_sec;

        // Print results
        std::cout << "Type: " << type_name << "\n";
        std::cout << "Single Thread: " << single_thread_ops_per_sec << " GOPs\n";
        std::cout << "Multi Thread: " << multi_thread_ops_per_sec << " GOPs\n";
        std::cout << "Single to Multi Ratio: " << ratio << "\n";
        std::cout << "-----------------------\n";
    };

    // Benchmark operations for different data types

    std::cout << "G Operations: " << OP_COUNT/(1E9) << "\n";
    std::cout << "-----------------------\n";
    benchmark("double", double());
    benchmark("float", float());
    benchmark("int64_t", int64_t());
    benchmark("int32_t", int32_t());
    benchmark("int8_t", int8_t());

    return 0;
}
