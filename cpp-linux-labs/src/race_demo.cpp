#include <iostream>
#include <mutex>
#include <thread>

namespace {

constexpr int kIterations = 100000;
int counter = 0;
std::mutex counter_mutex;

void worker() {
    for (int i = 0; i < kIterations; ++i) {
        std::lock_guard<std::mutex> lock(counter_mutex);
        ++counter;
    }
}

}  // namespace

int main() {
    std::thread first(worker);
    std::thread second(worker);

    first.join();
    second.join();

    std::cout << "Final counter: " << counter << '\n';
    if (counter != 2 * kIterations) {
        std::cerr << "FAIL: synchronized counter mismatch\n";
        return 1;
    }

    std::cout << "PASS: synchronized counter\n";
    return 0;
}
