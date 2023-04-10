#include <iostream>
#include <thread>
#include <mutex>

const int PV_ARR_SIZE = 48*1024;
const int PB_ARR_SIZE = 48*1024;
int privateArr[PV_ARR_SIZE];
int sharedArr[PB_ARR_SIZE];
std::mutex mtx; // mutex to synchronize access to shared array

// Thread 1
void thread1Func() {
    for (int i = 0; i < PV_ARR_SIZE; i++) {
        privateArr[i] = i * 2;
        //std::cout << "Thread 1: privateArr[" << i << "] = " << privateArr[i] << std::endl;
    }

    // Copy private array to shared array
    mtx.lock();
    for (int i = 0; i < PB_ARR_SIZE; i++) {
        sharedArr[i] = privateArr[i];
    }
    mtx.unlock();
}

// Thread 2
void thread2Func() {
    std::cout << "Thread 2: waiting for Thread 1 to join..." << std::endl;
    // Create Thread 1 and wait for it to complete
    std::thread t1(thread1Func);
    t1.detach();

    // Access shared array
    mtx.lock();
    for (int i = 0; i < PB_ARR_SIZE; i++) {
        sharedArr[i] *= 2;
        std::cout << "Thread 2: sharedArr[" << i << "] = " << sharedArr[i] << std::endl;
    }
    mtx.unlock();
}

int main() {
    // Create Thread 2
    std::thread t2(thread2Func);

    // Wait for Thread 2 to complete
    t2.join();

    std::cout << "All threads are done." << std::endl;
    return 0;
}