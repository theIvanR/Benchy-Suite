#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <immintrin.h>

using namespace std;
using namespace chrono;

//------------------------//
// Scalar versions
//------------------------//

void write_memory_chunk_scalar(char* memory, size_t chunk_size) {
    long long value = 0x0101010101010101LL;
    for (size_t i = 0; i < chunk_size; i += sizeof(long long))
        *(reinterpret_cast<long long*>(memory + i)) = value;
}

void read_memory_chunk_scalar(volatile char* memory, size_t chunk_size, volatile char* sum) {
    volatile long long local_sum = 0;
    for (size_t i = 0; i < chunk_size; i += sizeof(long long))
        local_sum += *(reinterpret_cast<volatile long long*>(memory + i));
    *sum += static_cast<char>(local_sum);
}

//------------------------//
// SSE versions (128-bit)
//------------------------//

void write_memory_chunk_sse(char* memory, size_t chunk_size) {
    __m128i value = _mm_set1_epi64x(0x0101010101010101LL);
    for (size_t i = 0; i < chunk_size; i += sizeof(__m128i))
        _mm_storeu_si128(reinterpret_cast<__m128i*>(memory + i), value);
}

void read_memory_chunk_sse(volatile char* memory, size_t chunk_size, volatile char* sum) {
    const char* mem_ptr = const_cast<const char*>(memory);
    __m128i local_sum = _mm_setzero_si128();
    for (size_t i = 0; i < chunk_size; i += sizeof(__m128i)) {
        __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(mem_ptr + i));
        local_sum = _mm_add_epi8(local_sum, data);
    }
    alignas(16) char tmp[16];
    _mm_store_si128(reinterpret_cast<__m128i*>(tmp), local_sum);
    for (int i = 0; i < 16; i++)
        *sum += tmp[i];
}

//------------------------//
// AVX versions (256-bit)
//------------------------//

void write_memory_chunk_avx(char* memory, size_t chunk_size) {
    __m256i value = _mm256_set1_epi64x(0x0101010101010101LL);
    for (size_t i = 0; i < chunk_size; i += sizeof(__m256i))
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(memory + i), value);
}

// AVX read: we avoid the AVX2-only _mm256_extracti128_si256 by extracting via floating‑point conversion.
void read_memory_chunk_avx(volatile char* memory, size_t chunk_size, volatile char* sum) {
    const char* mem_ptr = const_cast<const char*>(memory);
    __m128i local_sum = _mm_setzero_si128();
    // Process 32 bytes per iteration.
    for (size_t i = 0; i < chunk_size; i += 32) {
        // Load 256 bits
        __m256i data256 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(mem_ptr + i));
        // Lower 128 bits:
        __m128i lower = _mm256_castsi256_si128(data256);
        // Extract upper 128 bits via float conversion (available in AVX):
        __m256  data_ps = _mm256_castsi256_ps(data256);
        __m128   upper_ps = _mm256_extractf128_ps(data_ps, 1);
        __m128i upper = _mm_castps_si128(upper_ps);
        // Add the two 128-bit halves
        __m128i sum128 = _mm_add_epi8(lower, upper);
        local_sum = _mm_add_epi8(local_sum, sum128);
    }
    alignas(16) char tmp[16]; // Use 16-byte alignment for 128-bit store.
    _mm_store_si128(reinterpret_cast<__m128i*>(tmp), local_sum);
    for (int i = 0; i < 16; i++)
        *sum += tmp[i];
}

//------------------------//
// Benchmark wrappers
//------------------------//

template<typename WriteFunc>
double measure_write_bandwidth(char* memory, size_t size, int thread_count, WriteFunc write_func) {
    vector<thread> threads;
    size_t chunk_size = size / thread_count;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < thread_count; i++)
        threads.push_back(thread(write_func, memory + i * chunk_size, chunk_size));
    for (auto& t : threads)
        t.join();
    auto end = high_resolution_clock::now();
    double seconds = duration_cast<nanoseconds>(end - start).count() * 1e-9;
    return (size / (1024.0 * 1024.0 * 1024.0)) / seconds;
}

template<typename ReadFunc>
double measure_read_bandwidth(char* memory, size_t size, int thread_count, ReadFunc read_func) {
    vector<thread> threads;
    size_t chunk_size = size / thread_count;
    volatile char sum = 0;
    auto start = high_resolution_clock::now();
    for (int i = 0; i < thread_count; i++)
        threads.push_back(thread(read_func, memory + i * chunk_size, chunk_size, &sum));
    for (auto& t : threads)
        t.join();
    auto end = high_resolution_clock::now();
    double seconds = duration_cast<nanoseconds>(end - start).count() * 1e-9;
    return (size / (1024.0 * 1024.0 * 1024.0)) / seconds;
}

int main() {
    size_t size = 1ULL * 1024 * 1024 * 1024; // 1 GiB
    char* memory = new char[size];

    const int iterations = 5;
    int max_threads = thread::hardware_concurrency();

    cout << "1 GB test on " << max_threads << " threads\n";
    cout << "-----------------------\n";

    auto scalar_write = write_memory_chunk_scalar;
    auto sse_write = write_memory_chunk_sse;
    auto avx_write = write_memory_chunk_avx;
    auto scalar_read = read_memory_chunk_scalar;
    auto sse_read = read_memory_chunk_sse;
    auto avx_read = read_memory_chunk_avx;

    for (int threads = 1; threads <= max_threads; threads++) {
        double total_scalar_write = 0.0, total_scalar_read = 0.0;
        double total_sse_write = 0.0, total_sse_read = 0.0;
        double total_avx_write = 0.0, total_avx_read = 0.0;
        for (int i = 0; i < iterations; i++) {
            total_scalar_write += measure_write_bandwidth(memory, size, threads, scalar_write);
            total_scalar_read  += measure_read_bandwidth(memory, size, threads, scalar_read);
            total_sse_write    += measure_write_bandwidth(memory, size, threads, sse_write);
            total_sse_read     += measure_read_bandwidth(memory, size, threads, sse_read);
            total_avx_write    += measure_write_bandwidth(memory, size, threads, avx_write);
            total_avx_read     += measure_read_bandwidth(memory, size, threads, avx_read);
        }
        cout << "Threads: " << threads << "\n";
        cout << "Scalar: Read = " << total_scalar_read / iterations << " GB/s, Write = " 
             << total_scalar_write / iterations << " GB/s\n";
        cout << "SSE   : Read = " << total_sse_read / iterations << " GB/s, Write = " 
             << total_sse_write / iterations << " GB/s\n";
        cout << "AVX   : Read = " << total_avx_read / iterations << " GB/s, Write = " 
             << total_avx_write / iterations << " GB/s\n";
        cout << "-----------------------\n";
    }

    cout << "Done.\n";
    delete[] memory;
    cin.get(); // Pause before exit
    return 0;
}
