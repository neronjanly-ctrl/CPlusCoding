#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool write_all(int fd, const char* data, std::size_t size) {
    std::size_t total = 0;
    while (total < size) {
        const ssize_t result = ::write(fd, data + total, size - total);
        if (result > 0) {
            total += static_cast<std::size_t>(result);
            continue;
        }
        if (result == -1 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

int main() {
    int pipefd[2];
    if (::pipe(pipefd) == -1) {
        const int error_number = errno;
        std::cerr << "pipe failed: " << std::strerror(error_number) << '\n';
        return 1;
    }

    const pid_t child_pid = ::fork();
    if (child_pid == -1) {
        const int error_number = errno;
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        std::cerr << "fork failed: " << std::strerror(error_number) << '\n';
        return 1;
    }

    if (child_pid == 0) {
        ::close(pipefd[0]);

        constexpr char message[] = "Hello from child process\n";
        const bool write_ok = write_all(pipefd[1], message, sizeof(message) - 1);
        ::close(pipefd[1]);
        ::_exit(write_ok ? 0 : 1);
    }

    ::close(pipefd[1]);

    bool read_ok = true;
    char buffer[128];
    while (true) {
        const ssize_t bytes_read = ::read(pipefd[0], buffer, sizeof(buffer));
        if (bytes_read > 0) {
            std::cout.write(buffer, bytes_read);
            continue;
        }
        if (bytes_read == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }

        const int error_number = errno;
        std::cerr << "read failed: " << std::strerror(error_number) << '\n';
        read_ok = false;
        break;
    }
    ::close(pipefd[0]);

    int status = 0;
    pid_t wait_result = -1;
    do {
        wait_result = ::waitpid(child_pid, &status, 0);
    } while (wait_result == -1 && errno == EINTR);

    if (wait_result == -1) {
        const int error_number = errno;
        std::cerr << "waitpid failed: " << std::strerror(error_number) << '\n';
        return 1;
    }

    if (read_ok && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        std::cout << "PASS: pipe communication\n";
        return 0;
    }

    std::cerr << "FAIL: pipe communication did not complete successfully\n";
    return 1;
}
