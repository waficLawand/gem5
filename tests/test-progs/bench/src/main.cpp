#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>



const int T1_SIZE = 64*1024;
const int T2_SIZE = 127*1024;
const int T2_ITER = 2;
const int T2_WAIT = 1000*1024;
int arr1[T1_SIZE];
int arr2[T2_SIZE];
volatile int tmp;


std::mutex mtx; // mutex to synchronize access to shared array
std::condition_variable cv;
bool eventTriggered = false;
bool terminate = false;
// Thread 1
void thread1Func() {
    #ifdef FETCH
    std::cout << "Thread 1: prefetching\n";
    for (int i = 0; i < T1_SIZE; i++) {
        arr1[i] = i;
    }
    //
    
    {
        std::lock_guard<std::mutex> lock(mtx);
        eventTriggered = true;
    }
    std::cout << "Notifying Thread 2\n";
    cv.notify_all();
    #endif
    // Copy private array to shared array
    std::cout << "Thread 1: accessing array 1\n";
    while(!terminate) {
        for (int i = 0; i < T1_SIZE; i++) {
            tmp = arr1[i];
            if(terminate)
                break;
        }
    }
    std::cout << "Thread 1: Finished!\n";
}

// Thread 2
void thread2Func() {
    
    int wait_var = 0;
    #ifdef FETCH
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{return eventTriggered;});
    std::cout << "Thread 2: Notified!\n";
    #endif
    for(int j = 0; j< T2_ITER; j++){
        std::cout << "Thread 2: waiting\n";
        while(wait_var<T2_WAIT)
            wait_var++;
        std::cout << "Thread 2: accessing array 2\n";
        for (int i = j*T2_SIZE/T2_ITER; i < (j+1)*T2_SIZE/T2_ITER; i++) {
            arr2[i] = 2*i;
        }
        wait_var = 0;
    }
    std::cout << "Thread 2: Finished!\n";
    terminate = true;
}

int main() {
    // Create Thread 2
    std::thread t1(thread1Func);
    std::thread t2(thread2Func);

    // Wait for Thread 2 to complete
    t1.join();
    t2.join();

    std::cout << "All threads are done." << std::endl;
    return 0;
}