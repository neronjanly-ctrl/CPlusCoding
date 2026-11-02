#include <cerrno>
#include <cstring>
#include <iostream>

#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

struct SharedData {
    pthread_mutex_t mutex;
    int counter;
};

constexpr int kIterations = 100000;

int increment_counter(SharedData* shared) {
    for (int i = 0; i < kIterations; ++i) {
        int result = ::pthread_mutex_lock(&shared->mutex);
        if (result != 0) {
            return result;
        }

        ++shared->counter;

        result = ::pthread_mutex_unlock(&shared->mutex);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

void print_pthread_error(const char* operation, int error_number) {
    std::cerr << operation << " failed: " << std::strerror(error_number) << '\n';
}

}  // namespace

int main() {
    void* memory = ::mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        const int error_number = errno;
        std::cerr << "mmap failed: " << std::strerror(error_number) << '\n';
        return 1;
    }

    auto* shared = static_cast<SharedData*>(memory);
    shared->counter = 0;

    pthread_mutexattr_t attributes;
    int result = ::pthread_mutexattr_init(&attributes);
    if (result != 0) {
        print_pthread_error("pthread_mutexattr_init", result);
        ::munmap(memory, sizeof(SharedData));
        return 1;
    }

    result = ::pthread_mutexattr_setpshared(&attributes, PTHREAD_PROCESS_SHARED);
    if (result != 0) {
        print_pthread_error("pthread_mutexattr_setpshared", result);
        ::pthread_mutexattr_destroy(&attributes);
        ::munmap(memory, sizeof(SharedData));
        return 1;
    }

    result = ::pthread_mutex_init(&shared->mutex, &attributes);
    if (result != 0) {
        print_pthread_error("pthread_mutex_init", result);
        ::pthread_mutexattr_destroy(&attributes);
        ::munmap(memory, sizeof(SharedData));
        return 1;
    }

    result = ::pthread_mutexattr_destroy(&attributes);
    if (result != 0) {
        print_pthread_error("pthread_mutexattr_destroy", result);
        ::pthread_mutex_destroy(&shared->mutex);
        ::munmap(memory, sizeof(SharedData));
        return 1;
    }

    const pid_t child_pid = ::fork();
    if (child_pid == -1) {
        const int error_number = errno;
        std::cerr << "fork failed: " << std::strerror(error_number) << '\n';
        ::pthread_mutex_destroy(&shared->mutex);
        ::munmap(memory, sizeof(SharedData));
        return 1;
    }

    if (child_pid == 0) {
        const int child_result = increment_counter(shared);
        ::_exit(child_result == 0 ? 0 : 2);
    }

    const int parent_result = increment_counter(shared);
    if (parent_result != 0) {
        print_pthread_error("parent increment", parent_result);
    }

    int status = 0;
    pid_t wait_result = -1;
    do {
        wait_result = ::waitpid(child_pid, &status, 0);
    } while (wait_result == -1 && errno == EINTR);

    if (wait_result == -1) {
        const int error_number = errno;
        std::cerr << "waitpid failed: " << std::strerror(error_number) << '\n';
        ::munmap(memory, sizeof(SharedData));
        return 1;
    }

    const bool child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!child_ok) {
        std::cerr << "child process failed\n";
    }

    std::cout << "Parent increments: " << kIterations << '\n';
    std::cout << "Child increments: " << kIterations << '\n';
    std::cout << "Final counter: " << shared->counter << '\n';

    bool passed = parent_result == 0 && child_ok &&
                  shared->counter == 2 * kIterations;

    result = ::pthread_mutex_destroy(&shared->mutex);
    if (result != 0) {
        print_pthread_error("pthread_mutex_destroy", result);
        passed = false;
    }

    if (::munmap(memory, sizeof(SharedData)) == -1) {
        const int error_number = errno;
        std::cerr << "munmap failed: " << std::strerror(error_number) << '\n';
        passed = false;
    }

    if (!passed) {
        std::cerr << "FAIL: shared memory synchronization\n";
        return 1;
    }

    std::cout << "PASS: shared memory synchronization\n";
    return 0;
}
