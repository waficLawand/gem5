#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>



const int T1_SIZE = 125*1024;
const int T2_SIZE = 125*1024;
const int T2_ITER = 2;
const int T2_WAIT = 1000*1024;
int arr1[T1_SIZE];
volatile int arr2[T2_SIZE];
volatile int tmp1;
volatile int tmp2;


std::mutex mtx; // mutex to synchronize access to shared array
std::condition_variable cv;
bool eventTriggered = false;
bool terminate = false;
int counter = 0;

// Thread 1
void _thread1Func() {


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
            tmp1 = arr1[i];
            if(terminate)
                break;
        }
        counter++;
    }
    std::cout << "Thread 1: Finished!\n";
}


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
    std::cout << "Thread 1: 1st Access\n";
    for (int i = 0; i < T1_SIZE; i++) {
        arr1[i] = i;
    }


    std::cout << "Thread 1: Computation\n";
    while(!terminate) {
        #ifdef FETCH
        std::cout << "Thread 1: Fake Reads\n";
        for (int i = 0; i < T1_SIZE; i++) {
            arr1[i] = i;
            if(terminate)
                break;
        }
        #endif
    }
    std::cout << "Thread 1: 2nd Access\n";
    for (int i = 0; i < T1_SIZE; i++) {
        tmp1 = arr1[i];
    }
    std::cout << "Thread 1: Finished!\n";
}



// Thread 2
void _thread2Func() {

    int wait_var = 0;
    #ifdef FETCH
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{return eventTriggered;});
    std::cout << "Thread 2: Notified!\n";
    #endif
    for(int j = 0; j< T2_ITER; j++){
        std::cout << "Thread 2: waiting\n";
        // while(wait_var<T2_WAIT)
        //     wait_var++;
        std::cout << "Thread 2: accessing array 2\n";
        for (int i = 0; i < T2_SIZE; i++) {
            tmp2 = arr2[i] + j + i;
        }
        if(j == T2_ITER-1)
            break;
        while(wait_var<T2_WAIT)
            wait_var++;
        wait_var = 0;
    }
    std::cout << "Thread 2: Finished!\n";
    terminate = true;
}

void thread2Func() {

    int wait_var = 0;
    #ifdef FETCH
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{return eventTriggered;});
    std::cout << "Thread 2: Notified!\n";
    #endif
    std::cout << "Thread 2: 1st Access\n";
    for (int i = 0; i < T2_SIZE; i++) {
        arr2[i] = i;
    }

    std::cout << "Thread 2: Computation\n";
    while(wait_var<T2_WAIT)
        wait_var++;
    wait_var = 0;

    std::cout << "Thread 2: 2nd Access\n";
    for (int i = 0; i < T2_SIZE; i++) {
        tmp2 = arr2[i];
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