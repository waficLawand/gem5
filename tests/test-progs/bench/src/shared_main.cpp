#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

const int ARRAY_SIZE = 10000;
int my_array[ARRAY_SIZE];
std::mutex my_mutex;

// Function to be executed by each thread
void thread_function(int core_id) {
    for (int i = 0; i < ARRAY_SIZE; i++) {
        my_mutex.lock();
        // Access the array here
        std::cout << "Simple Thread " << core_id << " accessed element " << i << "\n";
        my_array[i]++;
        my_mutex.unlock();
    }
}

int main() {
    // Create two threads
    std::thread thread1(thread_function, 1);
    std::thread thread2(thread_function, 2);

    // Wait for threads to finish
    thread1.join();
    thread2.join();

    return 0;
}
