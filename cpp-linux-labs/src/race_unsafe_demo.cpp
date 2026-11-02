#include <iostream>
#include <thread>

// This global is intentionally unsynchronized. Build and run this program only
// with the opt-in TSan preset to observe a real data-race report.
int counter = 0;

void worker() {
    for (int i = 0; i < 100000; ++i) {
        ++counter;
    }
}

int main() {
    std::thread first(worker);
    std::thread second(worker);

    first.join();
    second.join();

    std::cout << "Unsafe final counter: " << counter << '\n';
    return 0;
}
