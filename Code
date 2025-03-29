#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <chrono>
#include <thread>
using namespace std;
using namespace std::chrono;

void worker(int* arr, int size, int start, int step, double& local_sum) {
    local_sum = 0;
    for (int i = start; i < size; i += step) {
        local_sum += arr[i] * arr[i];
    }
}

double norm_dynamic_threads(int* arr, int size, int num_threads, double& sum, long long& duration) {
    double* local_sum = new double[num_threads] {0};
    thread* workers = new thread[num_threads];

    auto start = high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        workers[i] = thread(worker, arr, size, i, num_threads, ref(local_sum[i]));
    }

    for (int i = 0; i < num_threads; ++i) {
        workers[i].join();
    }

    sum = 0;
    for (int i = 0; i < num_threads; ++i) {
        sum += local_sum[i];
    }

    double norm = sqrt(sum);

    auto end = high_resolution_clock::now();
    duration = duration_cast<nanoseconds>(end - start).count();

    delete[] local_sum;
    delete[] workers;

    return norm;
}

double norm_slow(int* arr, int size, double& sum, long long& duration) {
    auto start = high_resolution_clock::now();

    sum = 0;
    for (int i = 0; i < size; i++) {
        sum += arr[i] * arr[i];
    }
    double norm = sqrt(sum);

    auto end = high_resolution_clock::now();
    duration = duration_cast<nanoseconds>(end - start).count();

    return norm;
}

void print_results(int* arr, int size, double (*norm_func)(int*, int, int, double&, long long&), int num_threads, const string& label) {
    double sum = 0;
    long long duration = 0;
    double norm;

    if (num_threads == 0) {
        norm = norm_slow(arr, size, sum, duration);
    }
    else {
        norm = norm_func(arr, size, num_threads, sum, duration); 
    }

    cout << left << setw(10) << size
        << setw(15) << sum
        << setw(20) << (size * 2)
        << setw(15) << norm
        << setw(20) << duration
        << setw(25) << label
        << endl;
}

int main() {
    srand(static_cast<unsigned int>(time(0)));

    const int sizes[] = { 10, 100, 200, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000, 10000000, 50000000, 100000000 };
    const int numSizes = sizeof(sizes) / sizeof(sizes[0]);

    int* arrays[numSizes];
    for (int i = 0; i < numSizes; i++) {
        arrays[i] = new int[sizes[i]];
        for (int j = 0; j < sizes[i]; j++) {
            arrays[i][j] = rand() % 1000 + 1;
        }
    }

    cout << left
        << setw(10) << "Size"
        << setw(15) << "Sum"
        << setw(20) << "Total Operations"
        << setw(15) << "Norm"
        << setw(20) << "Execution Time (ns)"
        << setw(20) << "Calculation Method" << endl;
    cout << string(100, '-') << endl;

    for (int i = 0; i < numSizes; i++) {
        print_results(arrays[i], sizes[i], norm_dynamic_threads, 0, "Default");
        print_results(arrays[i], sizes[i], norm_dynamic_threads, 1, "1 Thread");
        print_results(arrays[i], sizes[i], norm_dynamic_threads, 2, "2 Threads");
        print_results(arrays[i], sizes[i], norm_dynamic_threads, 5, "5 Threads");
        print_results(arrays[i], sizes[i], norm_dynamic_threads, 10, "10 Threads");
        print_results(arrays[i], sizes[i], norm_dynamic_threads, 25, "25 Threads");
        print_results(arrays[i], sizes[i], norm_dynamic_threads, 50, "50 Threads");
        print_results(arrays[i], sizes[i], norm_dynamic_threads, 100, "100 Threads");
        cout << string(100, '-') << endl;
    }

    for (int i = 0; i < numSizes; i++) {
        delete[] arrays[i];
    }

    return 0;
}


