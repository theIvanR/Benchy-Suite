#pragma GCC optimize ("O0")

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring> // for memset

using namespace std;
using namespace chrono;

// Function to measure write bandwidth in a specific memory chunk
void write_memory_chunk(char* memory, size_t chunk_size) {
    long long value = 0x0101010101010101LL; // A 64-bit pattern to write
    for (size_t i = 0; i < chunk_size; i += sizeof(long long)) {
        *(reinterpret_cast<long long*>(memory + i)) = value;
    }
}

// Function to measure write bandwidth with multiple threads
double measure_write_bandwidth(char* memory, size_t size, int thread_count) {
    vector<thread> threads;
    size_t chunk_size = size / thread_count;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < thread_count; ++i) {
        threads.push_back(thread(write_memory_chunk, memory + i * chunk_size, chunk_size));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();

    double seconds = duration * 1e-9;
    double bandwidth = (size / (1024.0 * 1024.0 * 1024.0)) / seconds; // GB/s

    return bandwidth;
}

// Function to measure read bandwidth in a specific memory chunk
void read_memory_chunk(volatile char* memory, size_t chunk_size, volatile char* sum) {
    volatile long long local_sum = 0; // Use a wider type for summation
    for (size_t i = 0; i < chunk_size; i += sizeof(long long)) {
        local_sum += *(reinterpret_cast<volatile long long*>(memory + i));
    }
    *sum += static_cast<char>(local_sum);
}

// Function to measure read bandwidth with multiple threads
double measure_read_bandwidth(char* memory, size_t size, int thread_count) {
    vector<thread> threads;
    size_t chunk_size = size / thread_count;
    volatile char sum = 0;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < thread_count; ++i) {
        threads.push_back(thread(read_memory_chunk, memory + i * chunk_size, chunk_size, &sum));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();

    double seconds = duration * 1e-9;
    double bandwidth = (size / (1024.0 * 1024.0 * 1024.0)) / seconds; // GB/s

    return bandwidth;
}

int main() {
    size_t size = 1 * 1024 * 1024 * 1024; // 1 GiB
    char* memory = new char[size];

    const int iterations = 10; //run 10x

    cout << iterations << " X " << size/(1024*1024*1024) << " GB" << "\n";
    cout << "-----------------------\n";

    // Measure read and write bandwidth with multiple threads
    for (int thread_count = 1; thread_count <= thread::hardware_concurrency(); thread_count++) {
        double total_write_bandwidth = 0.0;
        double total_read_bandwidth = 0.0;

        // Run the test 10 times for each thread count
        for (int i = 0; i < iterations; i++) {
            double write_bandwidth = measure_write_bandwidth(memory, size, thread_count);
            double read_bandwidth = measure_read_bandwidth(memory, size, thread_count);

            total_write_bandwidth += write_bandwidth;
            total_read_bandwidth += read_bandwidth;
        }

        // Calculate averages and ratios
        double avg_write_bandwidth = total_write_bandwidth / iterations;
        double avg_read_bandwidth = total_read_bandwidth / iterations;
        double ratio = avg_read_bandwidth/avg_write_bandwidth;

        // Print the results
        cout << "Threads: " << thread_count << endl;
        cout << "Read Bandwidth: " << avg_read_bandwidth << " GB/s" << endl;
        cout << "Write Bandwidth: " << avg_write_bandwidth << " GB/s" << endl;
        cout << "Read to Write: " << ratio << " x" << endl;
        cout << "-----------------------\n";

    }

    delete[] memory;
    return 0;
}
