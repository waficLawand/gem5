#include <iostream>
#include <thread>

// Function to be executed by each thread
void helloFunction() {
    std::cout << "Hello from thread " << std::this_thread::get_id() << std::endl;
}

int main() {
    // Create two threads
    std::thread thread1(helloFunction);
    std::thread thread2(helloFunction);

    // Join the threads with the main thread
    thread1.join();
    thread2.join();

    // Output from the main thread
    std::cout << "Hello from main thread " << std::this_thread::get_id() << std::endl;

    return 0;
}

