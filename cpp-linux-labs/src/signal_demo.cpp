#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// 信号处理函数与主程序共享的状态标志
volatile std::sig_atomic_t signal_received = 0;

// 收到 SIGUSR1 后，设置标志位
void handle_sigusr1(int signal_number) {
    if (signal_number == SIGUSR1) {
        signal_received = 1;
    }
}

int main() {
    // 1. 注册 SIGUSR1 的处理函数
    struct sigaction action {};

    action.sa_handler = handle_sigusr1;
    ::sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (::sigaction(SIGUSR1, &action, nullptr) == -1) {
        std::cerr << "sigaction failed: "
                  << std::strerror(errno) << '\n';
        return 1;
    }

    // 2. 暂时屏蔽 SIGUSR1，避免等待前发生信号丢失
    sigset_t blocked_set;
    sigset_t original_mask;

    ::sigemptyset(&blocked_set);
    ::sigaddset(&blocked_set, SIGUSR1);

    if (::sigprocmask(SIG_BLOCK, &blocked_set, &original_mask) == -1) {
        std::cerr << "sigprocmask failed: "
                  << std::strerror(errno) << '\n';
        return 1;
    }

    // 等待时临时解除 SIGUSR1 的屏蔽
    sigset_t wait_mask = original_mask;
    ::sigdelset(&wait_mask, SIGUSR1);

    // 3. 创建子进程
    const pid_t parent_pid = ::getpid();
    const pid_t child_pid = ::fork();

    if (child_pid == -1) {
        const int error_number = errno;
        ::sigprocmask(SIG_SETMASK, &original_mask, nullptr);
        std::cerr << "fork failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    if (child_pid == 0) {
        // 4. 子进程向父进程发送 SIGUSR1
        if (::kill(parent_pid, SIGUSR1) == -1) {
            ::_exit(1);
        }

        ::_exit(0);
    }

    // 5. 父进程等待信号
    std::cout << "[Parent] Waiting for SIGUSR1...\n";

    while (signal_received == 0) {
        if (::sigsuspend(&wait_mask) == -1 && errno != EINTR) {
            const int error_number = errno;
            std::cerr << "sigsuspend failed: "
                      << std::strerror(error_number) << '\n';
            break;
        }
    }

    // 6. 恢复原来的信号屏蔽集合
    if (::sigprocmask(SIG_SETMASK, &original_mask, nullptr) == -1) {
        std::cerr << "restore signal mask failed\n";
        return 1;
    }

    if (signal_received == 1) {
        std::cout << "[Parent] Received SIGUSR1\n";
    }

    // 7. 等待并回收子进程
    int status = 0;
    pid_t result;

    do {
        result = ::waitpid(child_pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        std::cerr << "waitpid failed: "
                  << std::strerror(errno) << '\n';
        return 1;
    }

    if (WIFEXITED(status)) {
        std::cout << "[Parent] Child exit code = "
                  << WEXITSTATUS(status) << '\n';
    }

    const bool passed = signal_received == 1 && WIFEXITED(status) &&
                        WEXITSTATUS(status) == 0;
    if (!passed) {
        std::cerr << "FAIL: signal delivery or child cleanup\n";
        return 1;
    }

    std::cout << "PASS: signal delivery and child cleanup\n";
    return 0;
}
