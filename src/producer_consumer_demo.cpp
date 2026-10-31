
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

int main() {
    // 1. 共享资源
    std::queue<int> tasks;
    constexpr std::size_t kCapacity = 3;

    std::mutex mtx;

    std::condition_variable not_empty;
    std::condition_variable not_full;

    bool producer_done = false;

    int consumed_count = 0;
    long long sum = 0;

    // 2. 生产者线程
    auto produce = [&]() {
        for (int value = 1; value <= 10; ++value) {

            std::unique_lock<std::mutex> lock(mtx);

            // 队列满时，等待消费者取走任务
            not_full.wait(lock, [&]() {
                return tasks.size() < kCapacity;
            });

            tasks.push(value);

            // 释放锁后通知消费者
            lock.unlock();
            not_empty.notify_one();
        }

        // 所有任务生产完成
        {
            std::lock_guard<std::mutex> lock(mtx);
            producer_done = true;
        }

        not_empty.notify_all();
    };

    // 3. 消费者线程
    auto consume = [&]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);

            // 队列为空且生产者未完成时，继续等待
            not_empty.wait(lock, [&]() {
                return !tasks.empty() || producer_done;
            });

            // 生产者完成，队列也为空，退出
            if (tasks.empty() && producer_done) {
                break;
            }

            // 取出一个任务
            const int value = tasks.front();
            tasks.pop();

            lock.unlock();

            // 通知生产者：队列出现空位
            not_full.notify_one();

            // 消费任务
            sum += value;
            ++consumed_count;
        }
    };

    // 4. 创建两个线程
    std::thread producer(produce);
    std::thread consumer(consume);

    // 5. 等待两个线程结束
    producer.join();
    consumer.join();

    // 6. 输出最终结果
    std::cout << "Consumed tasks: "
              << consumed_count << '\n';

    std::cout << "Total sum: "
              << sum << '\n';

    if (consumed_count != 10 || sum != 55) {
        std::cerr << "FAIL: unexpected result\n";
        return 1;
    }

    std::cout << "PASS: producer-consumer experiment\n";

    return 0;
}
