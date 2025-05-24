#pragma GCC optimize ("O0")

#include <iostream>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>
#include <algorithm>
#include <immintrin.h>
#include <malloc.h>

constexpr uint64_t OP_COUNT = 1000000000;
unsigned NUM_THREADS = std::thread::hardware_concurrency();

// Scalar implementations
template <typename T>
void scalar_operations(const char* type_name, double& thread_time, uint64_t ops) {
    T* buffer = new T[32];
    T counter = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < ops; ++i) {
        counter += static_cast<T>(1);
    }
    buffer[0] = counter;
    auto end = std::chrono::high_resolution_clock::now();
    
    thread_time = std::chrono::duration<double>(end - start).count();
    delete[] buffer;
}

// SSE implementations
template <typename T>
void sse_operations(const char* type_name, double& thread_time, uint64_t ops) {
    const size_t elements_per_op = sizeof(__m128) / sizeof(T);
    T* buffer = static_cast<T*>(_mm_malloc(32 * sizeof(T), 16));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    if (std::is_floating_point<T>::value) {
        __m128 counter = _mm_setzero_ps();
        __m128 one = _mm_set1_ps(1.0f);
        for (uint64_t i = 0; i < ops / elements_per_op; ++i) {
            counter = _mm_add_ps(counter, one);
        }
        _mm_store_ps(reinterpret_cast<float*>(buffer), counter);
    } else {
        __m128i counter = _mm_setzero_si128();
        __m128i one = _mm_set1_epi32(1);
        for (uint64_t i = 0; i < ops / elements_per_op; ++i) {
            counter = _mm_add_epi32(counter, one);
        }
        _mm_store_si128(reinterpret_cast<__m128i*>(buffer), counter);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    thread_time = std::chrono::duration<double>(end - start).count();
    _mm_free(buffer);
}

// AVX implementations
template <typename T>
void avx_operations(const char* type_name, double& thread_time, uint64_t ops) {
    const size_t elements_per_op = sizeof(__m256d) / sizeof(T);
    T* buffer = static_cast<T*>(_mm_malloc(32 * sizeof(T), 32));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    if (std::is_floating_point<T>::value) {
        __m256d counter = _mm256_setzero_pd();
        __m256d one = _mm256_set1_pd(1.0);
        for (uint64_t i = 0; i < ops / elements_per_op; ++i) {
            counter = _mm256_add_pd(counter, one);
        }
        _mm256_store_pd(reinterpret_cast<double*>(buffer), counter);
    } else {
        // Fallback to SSE for integers
        __m128i counter = _mm_setzero_si128();
        __m128i one = _mm_set1_epi32(1);
        for (uint64_t i = 0; i < ops / (elements_per_op/2); ++i) {
            counter = _mm_add_epi32(counter, one);
        }
        _mm_store_si128(reinterpret_cast<__m128i*>(buffer), counter);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    thread_time = std::chrono::duration<double>(end - start).count();
    _mm_free(buffer);
}

template <typename T, typename Func>
double run_benchmark(Func func, const char* isa_name, const char* type_name) {
    double single_thread_time;
    func(type_name, single_thread_time, OP_COUNT);
    double single_thread_ops = OP_COUNT / single_thread_time;

    std::vector<std::thread> threads(NUM_THREADS);
    std::vector<double> thread_times(NUM_THREADS);

    for (unsigned i = 0; i < NUM_THREADS; ++i) {
        threads[i] = std::thread([&, i]() {
            func(type_name, thread_times[i], OP_COUNT);
        });
    }

    for (auto& t : threads) t.join();

    double max_time = *std::max_element(thread_times.begin(), thread_times.end());
    double multi_thread_ops = (OP_COUNT * NUM_THREADS) / max_time;

    std::cout << "ISA: " << isa_name << " | Type: " << type_name << "\n"
              << "Single: " << single_thread_ops/1e9 << " GOP/s\n"
              << "Multi (" << NUM_THREADS << "): " << multi_thread_ops/1e9 << " GOP/s\n"
              << "Scaling: " << multi_thread_ops/single_thread_ops << "\n"
              << "------------------------\n";

    return multi_thread_ops/single_thread_ops;
}

int main() {
    // C++11-compatible benchmarking
    std::cout << "\n=== BENCHMARKING double ===\n";
    run_benchmark<double>(scalar_operations<double>, "x86", "double");
    run_benchmark<double>(sse_operations<double>, "SSE", "double");
    run_benchmark<double>(avx_operations<double>, "AVX", "double");

    std::cout << "\n=== BENCHMARKING float ===\n";
    run_benchmark<float>(scalar_operations<float>, "x86", "float");
    run_benchmark<float>(sse_operations<float>, "SSE", "float");
    run_benchmark<float>(avx_operations<float>, "AVX", "float");

    std::cout << "\n=== BENCHMARKING int64 ===\n";
    run_benchmark<int64_t>(scalar_operations<int64_t>, "x86", "int64");
    run_benchmark<int64_t>(sse_operations<int64_t>, "SSE", "int64");
    run_benchmark<int64_t>(avx_operations<int64_t>, "AVX", "int64");

    std::cout << "\n=== BENCHMARKING int32 ===\n";
    run_benchmark<int32_t>(scalar_operations<int32_t>, "x86", "int32");
    run_benchmark<int32_t>(sse_operations<int32_t>, "SSE", "int32");
    run_benchmark<int32_t>(avx_operations<int32_t>, "AVX", "int32");

    std::cout << "\n=== BENCHMARKING int8 ===\n";
    run_benchmark<int8_t>(scalar_operations<int8_t>, "x86", "int8");
    run_benchmark<int8_t>(sse_operations<int8_t>, "SSE", "int8");
    run_benchmark<int8_t>(avx_operations<int8_t>, "AVX", "int8");

    return 0;
}