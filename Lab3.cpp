#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <random>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <map>
#include <iostream>

using namespace std;
using namespace chrono;

const int MAX_QUEUE_SIZE = 5;
const int MAX_ORDER_ID = 20;

const vector<string> menu = {
    "Big Mac", "French Fries", "Cheeseburger", "McChicken",
    "Chicken McNuggets", "Filet-O-Fish", "Apple Pie", "McFlurry"
};
const vector<string> cashiers = { "Amy", "Markus", "Jane" };
const vector<string> cooks = { "John", "Marie", "Paul", "Abby" };

struct Order {
    int id;
    string name;
    string cashier;
    string cook;
    string status;
    system_clock::time_point created;
};

queue<Order> orderQueue;
vector<Order> rejectedOrders;
vector<Order> doneOrders;
mutex mtxQueue, mtxDone, mtxCout;
condition_variable cvQueue;
atomic<int> orderId{ 1 };
atomic<bool> simulationDone{ false };

random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> menuDist(0, menu.size() - 1);
uniform_int_distribution<> cookInterval(5, 10);
uniform_int_distribution<> processInterval(5, 10);
uniform_int_distribution<> orderInterval{ 1, 5 };

atomic<int> totalThreadsCreated{ 0 };

vector<steady_clock::time_point> queueFullTimestamps;
atomic<int> maxQueueDuration{ 0 };
atomic<int> minQueueDuration{ 100 };

void printInterface() {
    lock_guard<mutex> lock(mtxCout);
    system("cls");
    // перевірка роботи випадкового присвоєння часових інтервалів
    // cout << "cookInterval =" << orderInterval(gen) << endl;
    // cout << "processInterval =" << processInterval(gen) << endl;
    // cout << "orderInterval =" << orderInterval(gen) << endl;

    // ------------------------ черга замовлень ----------------------
    cout << "=== Queue ===" << endl;
    {
        lock_guard<mutex> lock(mtxQueue);
        queue<Order> temp = orderQueue;
        int i = 1;
        while (!temp.empty()) {
            const auto& o = temp.front();
            cout << i++ << ") " << o.name << " (ID: " << o.id << ", Cashier: " << o.cashier << ")" << endl;
            temp.pop();
        }
        if (orderQueue.empty()) cout << "(Empty)" << endl;
    }
    // -------------------------------------------------------------


    // ------------------------ відкинуті замовлення ---------------
    if (!rejectedOrders.empty()) {
        cout << "\n--- Rejected Orders ---" << endl;
        for (const auto& o : rejectedOrders)
            cout << o.name << " (rejected by " << o.cashier << ")" << endl;
    }
    // -------------------------------------------------------------


    // ---------------------- робота на кухні ----------------------
    cout << "\n=== Kitchen In-Progress ===" << endl;
    for (const auto& cook : cooks) {
        cout << cook << ":\t";
        bool found = false;
        for (const auto& o : doneOrders)
            if (o.cook == cook && o.status == "in process") {
                cout << o.name << " (ID: " << o.id << ")";
                found = true;
                break;
            }
        if (!found) cout << "(free)";
        cout << endl;
    }
    // -------------------------------------------------------------


    // ---------------------- виконані замовлення -------------------
    cout << "\n=== Completed Orders ===" << endl;
    for (const auto& o : doneOrders)
        if (o.status == "done")
            cout << "ID: " << o.id << " | " << o.name << " | Cashier: " << o.cashier << " | Cook: " << o.cook << endl;
    // -------------------------------------------------------------
    cout << endl;
}

void cashierThread(const string& cashierName) {
    totalThreadsCreated++;
    while (!simulationDone) {
        this_thread::sleep_for(seconds(orderInterval(gen)));

        Order order;
        order.name = menu[menuDist(gen)];
        order.cashier = cashierName;
        order.created = system_clock::now();

        int newId = orderId++;
        if (newId > MAX_ORDER_ID) {
            simulationDone = true;
            cvQueue.notify_all();
            break;
        }

        order.id = newId;
        order.status = "pending";

        {
            lock_guard<mutex> lock(mtxQueue);
            if (orderQueue.size() >= MAX_QUEUE_SIZE) {
                rejectedOrders.push_back(order);
                auto now = steady_clock::now();
                queueFullTimestamps.push_back(now);

                if (queueFullTimestamps.size() > 1) {
                    int duration = duration_cast<milliseconds>(now - queueFullTimestamps[queueFullTimestamps.size() - 2]).count();
                    maxQueueDuration = max(maxQueueDuration.load(), duration);
                    minQueueDuration = min(minQueueDuration.load(), duration);
                }
            }
            else {
                orderQueue.push(order);
                cvQueue.notify_one();
            }
        }
        printInterface();
    }
}

void cookThread(const string& cookName) {
    totalThreadsCreated++;
    while (true) {
        unique_lock<mutex> lock(mtxQueue);
        cvQueue.wait(lock, [] { return !orderQueue.empty() || simulationDone; });

        if (simulationDone && orderQueue.empty())
            return;

        Order order = orderQueue.front();
        orderQueue.pop();
        order.cook = cookName;
        order.status = "in process";
        {
            lock_guard<mutex> doneLock(mtxDone);
            doneOrders.push_back(order);
        }

        lock.unlock();
        printInterface();
        this_thread::sleep_for(seconds(cookInterval(gen)));
        {
            lock_guard<mutex> doneLock(mtxDone);
            for (auto& o : doneOrders)
                if (o.id == order.id)
                    o.status = "done";
        }
        printInterface();
    }
}

int main() {
    srand(time(nullptr));
    int cashiersNum = 3;
    int cookNum = 4;

    thread cashierThreads[3], cookThreads[4]; // створення масву потоків робітників
    // ------------------------- Статистика опрацьованих засовлень робітниками ------------------


    for (int i = 0; i < cashiersNum; ++i)
        cashierThreads[i] = thread(cashierThread, cashiers[i]); // створення потоків касирів

    for (int i = 0; i < cookNum; ++i)
        cookThreads[i] = thread(cookThread, cooks[i]); // створення потоків кухарів

    for (auto& t : cashierThreads) t.join(); // очікування завершення роботи касирів
    for (auto& t : cookThreads) t.join(); // очікування завершення роботи кухарів

    cout << "\n=== Simulation completed. Final statistics ===" << endl;
    int totalDone = count_if(doneOrders.begin(), doneOrders.end(), [](const Order& o) {
        return o.status == "done";
        });
    cout << "Total completed orders: " << totalDone << endl;
    cout << "Rejected orders: " << rejectedOrders.size() << endl;

    // ------------------------- Статистика опрацьованих засовлень робітниками ------------------
    cout << "\n=== Cashier Statistics ===" << endl;
    map<string, int> cashierStats;
    for (const auto& o : doneOrders) // перебір усіх завершених замовлень
        if (o.status == "done")
            cashierStats[o.cashier]++; // збільшуємо лічильник замовлень для відповідного касира
    for (const auto& p : cashierStats) // виводимо на екран ім’я касира та кількість замовлень
        cout << p.first << ": " << p.second << " orders" << endl;

    cout << "\n=== Cook Statistics ===" << endl;
    map<string, int> cookStats;
    for (const auto& o : doneOrders)
        if (o.status == "done")
            cookStats[o.cook]++;
    for (const auto& p : cookStats)
        cout << p.first << ": " << p.second << " orders" << endl;
    // ---------------------------------------------------------------------------------------


    // ---------------------- Статистика заповнення черги ------------------------------------
    if (maxQueueDuration == 0) minQueueDuration = 0;
    cout << "\n=== Queue Fill Time Statistics ===" << endl;
    cout << "Max duration queue was full: " << maxQueueDuration.load() << " milliseconds" << endl;
    cout << "Min duration queue was full: " << (minQueueDuration.load() == INT_MAX ? 0 : minQueueDuration.load()) << " milliseconds" << endl;

    // ----------------------------------------------------------------------------------------
    cout << "\nTotal threads created: " << totalThreadsCreated.load() +2 << endl;
    return 0;
}