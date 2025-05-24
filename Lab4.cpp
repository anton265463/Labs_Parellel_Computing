#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <vector>
#include <thread>
#include <chrono>
#include <numeric>
#include <random>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <mutex>
#include <algorithm>
using namespace std;
using namespace std::chrono;
#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

struct Result {
    float sum;
    float norm;
    long long duration_ns;
};

void handleError(const char* message) {
    cerr << "Error: " << message << " (" << WSAGetLastError() << ")" << endl;
    exit(EXIT_FAILURE);
}

float norm_slow(const vector<int>& arr, float& sum, long long& duration) {
    auto start = high_resolution_clock::now();

    sum = 0;
    for (int val : arr) {
        sum += val * val;
    }
    float norm = static_cast<float>(sqrt(sum));

    auto end = high_resolution_clock::now();
    duration = duration_cast<nanoseconds>(end - start).count();

    return norm;
}

void worker(const vector<int>& arr, int start, int step, float& local_sum) {
    local_sum = 0;
    for (int i = start; i < (int)arr.size(); i += step) {
        local_sum += arr[i] * arr[i];
    }
}

float norm_fast(const vector<int>& arr, int num_threads, float& sum, long long& duration) {
    vector<float> local_sum(num_threads, 0);
    vector<thread> workers(num_threads);

    auto start = high_resolution_clock::now();
    sum = 0;
    for (int i = 0; i < num_threads; ++i) {
        workers[i] = thread(worker, cref(arr), i, num_threads, ref(local_sum[i]));
    }

    for (int i = 0; i < num_threads; ++i) {
        workers[i].join();
    }

    sum = accumulate(local_sum.begin(), local_sum.end(), 0.0f);
    float norm = static_cast<float>(sqrt(sum));

    auto end = high_resolution_clock::now();
    duration = duration_cast<nanoseconds>(end - start).count();

    return norm;
}

void processClient(SOCKET clientSocket) {
    int mode;
    int res = recv(clientSocket, (char*)&mode, sizeof(mode), 0);
    if (res <= 0) {
        closesocket(clientSocket);
        return;
    }
    send(clientSocket, "Mode received", 13, 0);

    vector<int> array;
    int size = 0;

    if (mode == 1) {
        res = recv(clientSocket, (char*)&size, sizeof(size), 0);
        if (res <= 0) {
            closesocket(clientSocket);
            return;
        }
        array.resize(size);
        res = recv(clientSocket, (char*)array.data(), size * sizeof(int), 0);
        if (res <= 0) {
            closesocket(clientSocket);
            return;
        }
    }
    else {
        res = recv(clientSocket, (char*)&size, sizeof(size), 0);
        if (res <= 0) {
            closesocket(clientSocket);
            return;
        }
        array.resize(size);
        default_random_engine eng((unsigned)time(0));
        uniform_int_distribution<int> dist(0, 1000);
        for (int i = 0; i < size; ++i) array[i] = dist(eng);
    }

    int threadCount;
    res = recv(clientSocket, (char*)&threadCount, sizeof(threadCount), 0);
    if (res <= 0) {
        closesocket(clientSocket);
        return;
    }

    float sum = 0;
    float norm = 0;
    long long duration_ns = 0;

    if (threadCount <= 0) {
        norm = norm_slow(array, sum, duration_ns);
    }
    else {
        norm = norm_fast(array, threadCount, sum, duration_ns);
    }

    Result result = { sum, norm, duration_ns };
    send(clientSocket, (char*)&result, sizeof(result), 0);

    cout << "\n--- Server Processed Data ---\n";
    cout << "Array size: " << size << "\n";
    cout << "Thread count: " << threadCount << "\n";
    cout << "Result (sum): " << sum << "\n";
    cout << "Result (norm): " << norm << "\n";
    cout << "Time taken (ns): " << duration_ns << "\n";

    closesocket(clientSocket);
}

void startServer() {
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    sockaddr_in serverAddr, clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) handleError("WSAStartup failed");

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) handleError("Socket creation failed");

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        handleError("Bind failed");

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) handleError("Listen failed");

    cout << "Server listening on port " << SERVER_PORT << "...\n";

    vector<thread> clientThreads;

    while (true) {
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Accept failed: " << WSAGetLastError() << endl;
            continue;
        }
        clientThreads.emplace_back(thread([clientSocket]() {
            processClient(clientSocket);
            }));

        clientThreads.erase(
            remove_if(clientThreads.begin(), clientThreads.end(),
                [](thread& t) {
                    if (t.joinable() && (t.get_id() != this_thread::get_id())) {
                        t.join();
                        return true;
                    }
                    return false;
                }),
            clientThreads.end());
    }

    for (auto& t : clientThreads) {
        if (t.joinable()) t.join();
    }

    closesocket(serverSocket);
    WSACleanup();
}

void startClient() {
    WSADATA wsaData;
    SOCKET clientSocket;
    sockaddr_in serverAddr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) handleError("WSAStartup failed");

    while (true) {
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) handleError("Socket creation failed");

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverAddr.sin_port = htons(SERVER_PORT);

        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Connection failed: " << WSAGetLastError() << endl;
            closesocket(clientSocket);
            continue;
        }

        int mode;
        cout << "\nChoose input type:\n1 - Manual\n0 - Automatic: ";
        cin >> mode;
        send(clientSocket, (char*)&mode, sizeof(mode), 0);

        char buffer[BUFFER_SIZE];
        recv(clientSocket, buffer, BUFFER_SIZE, 0);

        int size;
        cout << "Enter array size (0 to quit): ";
        cin >> size;
        if (size == 0) {
            closesocket(clientSocket);
            break;
        }

        send(clientSocket, (char*)&size, sizeof(size), 0);

        if (mode == 1) {
            vector<int> array(size);
            cout << "Enter " << size << " int values:\n";
            for (int i = 0; i < size; ++i) {
                cin >> array[i];
            }
            send(clientSocket, (char*)array.data(), size * sizeof(int), 0);
        }

        int threadCount;
        cout << "Enter thread count (0 = no threads): ";
        cin >> threadCount;
        send(clientSocket, (char*)&threadCount, sizeof(threadCount), 0);

        cout << "Data sent to server.\n";

        Result res;
        recv(clientSocket, (char*)&res, sizeof(res), 0);

        cout << "\n--- Result from Server ---\n";
        cout << "Sum of squares: " << res.sum << "\n";
        cout << "Vector norm:    " << res.norm << "\n";
        cout << "Time taken:     " << res.duration_ns << " ns\n";

        closesocket(clientSocket);
    }

    WSACleanup();
}

int main() {
    int choice;
    cout << "\nSelect mode: 1 - Server, 2 - Client\n";
    cin >> choice;

    if (choice == 1) {
        startServer();
    }
    else if (choice == 2) {
        startClient();
    }
    else {
        cout << "Wrong choice" << endl;
    }
    return 0;
}
