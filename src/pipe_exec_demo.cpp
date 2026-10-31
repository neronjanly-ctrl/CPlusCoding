#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    int pipefd[2];

    if (::pipe(pipefd) == -1) {
        const int error_number = errno;
        std::cerr << "pipe failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    const pid_t child_pid = ::fork();

    if (child_pid == -1) {
        const int error_number = errno;
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        std::cerr << "fork failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    if (child_pid == 0) {
        ::close(pipefd[0]);

        if (::dup2(pipefd[1], STDOUT_FILENO) == -1) {
            ::_exit(126);
        }

        ::close(pipefd[1]);

        ::execl("/bin/echo", "echo",
                "Hello through pipe and exec",
                static_cast<char*>(nullptr));

        ::_exit(127);
    }

    ::close(pipefd[1]);

    char buffer[128];
    ssize_t bytes_read = 0;

    while ((bytes_read = ::read(pipefd[0], buffer,
                                sizeof(buffer))) > 0) {
        std::cout.write(buffer, bytes_read);
    }

    const bool read_ok = bytes_read == 0;
    if (!read_ok) {
        const int error_number = errno;
        std::cerr << "read failed: "
                  << std::strerror(error_number) << '\n';
    }

    ::close(pipefd[0]);

    int status = 0;
    pid_t wait_result;
    do {
        wait_result = ::waitpid(child_pid, &status, 0);
    } while (wait_result == -1 && errno == EINTR);

    if (wait_result == -1) {
        const int error_number = errno;
        std::cerr << "waitpid failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    if (!read_ok) {
        return 1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        std::cout << "PASS: pipe + dup2 + exec\n";
        return 0;
    }

    std::cerr << "FAIL: child did not exit successfully\n";
    return 1;
}
