
#include <iostream>
#include <mutex>
#include <thread>

int counter = 0;
std::mutex counter_mutex;

void worker() {
  for (int i = 0; i < 100000; ++i) {
    std::lock_guard<std::mutex> lock(counter_mutex);
    ++counter;
  }
}

int main() {
  std::thread t1(worker);
  std::thread t2(worker);

  t1.join();
  t2.join();

  std::cout << "Final counter: " << counter << '\n';

  return 0;
}
