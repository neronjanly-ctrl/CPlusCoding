
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    int pipefd[2];

    // 1. 创建管道
    if (::pipe(pipefd) == -1) {
        const int error_number = errno;
        std::cerr << "pipe failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    std::cout << "[Parent] pipe created: read_fd="
              << pipefd[0] << ", write_fd="
              << pipefd[1] << '\n';

    // 2. 创建第一个子进程：echo
    const pid_t child1 = ::fork();

    if (child1 == -1) {
        const int error_number = errno;
        std::cerr << "first fork failed: "
                  << std::strerror(error_number) << '\n';
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return 1;
    }

    if (child1 == 0) {
        // 子进程 A：echo

        ::close(pipefd[0]);

        if (::dup2(pipefd[1], STDOUT_FILENO) == -1) {
            ::_exit(126);
        }

        ::close(pipefd[1]);

        ::execl("/bin/echo", "echo",
                "Hello", static_cast<char*>(nullptr));

        ::_exit(127);
    }

    std::cout << "[Parent] echo child PID = "
              << child1 << '\n';

    // 3. 创建第二个子进程：wc
    const pid_t child2 = ::fork();

    if (child2 == -1) {
        const int error_number = errno;
        std::cerr << "second fork failed: "
                  << std::strerror(error_number) << '\n';

        ::close(pipefd[0]);
        ::close(pipefd[1]);

        int status = 0;
        while (::waitpid(child1, &status, 0) == -1) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "waitpid first child failed\n";
            return 1;
        }
        return 1;
    }

    if (child2 == 0) {
        // 子进程 B：wc

        ::close(pipefd[1]);

        if (::dup2(pipefd[0], STDIN_FILENO) == -1) {
            ::_exit(126);
        }

        ::close(pipefd[0]);

        ::execl("/usr/bin/wc", "wc", "-c",
                static_cast<char*>(nullptr));

        ::_exit(127);
    }

    std::cout << "[Parent] wc child PID = "
              << child2 << '\n';

    // 4. 父进程不参与数据传输，关闭管道两端
    ::close(pipefd[0]);
    ::close(pipefd[1]);

    // 5. 等待两个子进程结束
    int status1 = 0;
    int status2 = 0;

    pid_t result1;
    do {
        result1 = ::waitpid(child1, &status1, 0);
    } while (result1 == -1 && errno == EINTR);

    pid_t result2;
    do {
        result2 = ::waitpid(child2, &status2, 0);
    } while (result2 == -1 && errno == EINTR);

    if (result1 == -1 || result2 == -1) {
        std::cerr << "waitpid failed\n";
        return 1;
    }

    // 6. 检查子进程退出状态
    if (!WIFEXITED(status1) ||
        WEXITSTATUS(status1) != 0) {
        std::cerr << "echo child failed\n";
        return 1;
    }

    if (!WIFEXITED(status2) ||
        WEXITSTATUS(status2) != 0) {
        std::cerr << "wc child failed\n";
        return 1;
    }

    std::cout << "[Parent] Both children exited successfully\n";
    return 0;
}
