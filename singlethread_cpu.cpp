#pragma GCC optimize ("O0")

#include <iostream>
#include <chrono>
#include <cstdint>

// Number of operations to do
constexpr uint64_t OP_COUNT = 1E9;

// Template function to perform operations based on data type
template <typename T>
void perform_operations(const char* type_name) {
    volatile T op_counter = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < OP_COUNT; ++i) {
        ++op_counter;  // Increment operation counter (dummy operation)
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Type: " << type_name << "\n";
    std::cout << OP_COUNT << " operations in " << elapsed.count() << " seconds\n";
    std::cout << "Operations per second: " << OP_COUNT / elapsed.count() << '\n';
    std::cout << "-----------------------\n";
}

int main() {
    // Benchmark operations for different data types
    perform_operations<double>("double");
    perform_operations<float>("float");
    perform_operations<int64_t>("int64_t");
    perform_operations<int32_t>("int32_t");
    perform_operations<int8_t>("int8_t");

    return 0;
}
