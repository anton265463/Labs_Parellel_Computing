#include <iostream>
#include <vector>
#include <algorithm> 
#include <chrono>    
#include <cstdlib>   
#include <ctime>     
#include <iomanip>   
#include <sstream>   
#include <thread>
#include <mutex>
#include <atomic>

using namespace std;
using namespace std::chrono;

struct answer {
    int max_value;
    int count_max;

    answer() : max_value(0), count_max(0) {}
    answer(int max_value, int count_max) : max_value(max_value), count_max(count_max) {}
};

string int_to_string(int value) {
    ostringstream oss;
    oss << value;
    return oss.str();
}

answer find_slow(vector<int>& arr, long long& duration, int num_threads = 1) {
    auto start = high_resolution_clock::now();

    int max_value = *max_element(arr.begin(), arr.end());
    int count_max = count(arr.begin(), arr.end(), max_value);

    auto end = high_resolution_clock::now();
    duration = duration_cast<nanoseconds>(end - start).count();

    return answer(max_value, count_max);
}

answer find_with_mutex(vector<int>& arr, long long& duration, int num_threads) {
    auto start = high_resolution_clock::now();

    if (num_threads == 0) num_threads = thread::hardware_concurrency();

    vector<thread> threads;
    mutex mtx;
    int global_max = 0;
    int global_count = 0;

    auto worker = [&](int start_idx, int end_idx) {
        int local_max = 0;
        int local_count = 0;
        for (int i = start_idx; i < end_idx; ++i) {
            if (arr[i] > local_max) {
                local_max = arr[i];
                local_count = 1;
            }
            else if (arr[i] == local_max) {
                local_count++;
            }
        }
        lock_guard<mutex> lock(mtx);
        if (local_max > global_max) {
            global_max = local_max;
            global_count = local_count;
        }
        else if (local_max == global_max) {
            global_count += local_count;
        }
        };

    int chunk_size = arr.size() / num_threads;
    for (int i = 0; i < num_threads; ++i) {
        int start_idx = i * chunk_size;
        int end_idx = (i == num_threads - 1) ? arr.size() : start_idx + chunk_size;
        threads.emplace_back(worker, start_idx, end_idx);
    }

    for (auto& th : threads) {
        th.join();
    }

    auto end = high_resolution_clock::now();
    duration = duration_cast<nanoseconds>(end - start).count();

    return answer(global_max, global_count);
}

answer find_with_atomic(vector<int>& arr, long long& duration, int num_threads) {
    auto start = high_resolution_clock::now();

    if (num_threads == 0) num_threads = thread::hardware_concurrency();

    vector<thread> threads;
    atomic<int> global_max(0);
    atomic<int> global_count(0);

    auto worker = [&](int start_idx, int end_idx) {
        for (int i = start_idx; i < end_idx; ++i) {
            int val = arr[i];

            while (true) {
                int current_max = global_max.load();
                if (val > current_max) {
                    if (global_max.compare_exchange_weak(current_max, val)) {
                        global_count.store(1);
                        break;
                    }
                }
                else if (val == current_max) {
                    global_count.fetch_add(1);
                    break;
                }
                else {
                    break;
                }
            }
        }
        };

    int chunk_size = arr.size() / num_threads;
    for (int i = 0; i < num_threads; ++i) {
        int start_idx = i * chunk_size;
        int end_idx = (i == num_threads - 1) ? arr.size() : start_idx + chunk_size;
        threads.emplace_back(worker, start_idx, end_idx);
    }

    for (auto& th : threads) {
        th.join();
    }

    auto end = high_resolution_clock::now();
    duration = duration_cast<nanoseconds>(end - start).count();

    return answer(global_max.load(), global_count.load());
}

void print_results(vector<int>& arr, answer(*find_func)(vector<int>&, long long&, int), const string& method_label, int num_threads) {
    long long duration = 0;
    answer result = find_func(arr, duration, num_threads);

    cout << left
        << setw(12) << arr.size()
        << setw(20) << result.max_value
        << setw(20) << result.count_max
        << setw(20) << duration
        << setw(25) << method_label
        << setw(15) << num_threads
        << endl;
}

int main() {
    srand(static_cast<unsigned int>(time(0)));

    const int sizes[] = { 100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000, 10000000, 50000000, 100000000 };
    const int numSizes = sizeof(sizes) / sizeof(sizes[0]);

    cout << left
        << setw(12) << "Size"
        << setw(20) << "Max Value"
        << setw(20) << "Count Max"
        << setw(20) << "Execution Time (ns)"
        << setw(25) << "Method"
        << setw(15) << "Threads"
        << endl;
    cout << string(97, '-') << endl;

    for (int i = 0; i < numSizes; i++) {
        vector<int> arr(sizes[i]);
        for (int j = 0; j < sizes[i]; j++) {
            arr[j] = rand() % 1000 + 1;
        }

        print_results(arr, find_slow, "Single-threaded (slow)", 1);
        print_results(arr, find_with_mutex, "Multi-threaded (mutex)", 2);
        print_results(arr, find_with_mutex, "Multi-threaded (mutex)", 4);
        print_results(arr, find_with_mutex, "Multi-threaded (mutex)", 8);
        print_results(arr, find_with_mutex, "Multi-threaded (mutex)", 16);
        print_results(arr, find_with_mutex, "Multi-threaded (mutex)", 32);
        print_results(arr, find_with_mutex, "Multi-threaded (mutex)", 64);
        print_results(arr, find_with_mutex, "Multi-threaded (mutex)", 128);

        print_results(arr, find_with_atomic, "Multi-threaded (CAS)", 2);
        print_results(arr, find_with_atomic, "Multi-threaded (CAS)", 4);
        print_results(arr, find_with_atomic, "Multi-threaded (CAS)", 8);
        print_results(arr, find_with_atomic, "Multi-threaded (CAS)", 16);
        print_results(arr, find_with_atomic, "Multi-threaded (CAS)", 32);
        print_results(arr, find_with_atomic, "Multi-threaded (CAS)", 64);
        print_results(arr, find_with_atomic, "Multi-threaded (CAS)", 128);
        cout << string(97, '-') << endl;
    }

    return 0;
}
