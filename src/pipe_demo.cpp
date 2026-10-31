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
        // The child writes to the pipe.
        ::close(pipefd[0]);

        constexpr char message[] = "Hello from child process\n";
        const ssize_t bytes_written =
            ::write(pipefd[1], message, sizeof(message) - 1);

        if (bytes_written != static_cast<ssize_t>(sizeof(message) - 1)) {
            ::close(pipefd[1]);
            ::_exit(1);
        }

        ::close(pipefd[1]);
        ::_exit(0);
    }

    // The parent reads from the pipe.
    ::close(pipefd[1]);

    char buffer[128];
    ssize_t bytes_read = 0;

    while ((bytes_read = ::read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        std::cout.write(buffer, bytes_read);
    }

    if (bytes_read == -1) {
        const int error_number = errno;
        std::cerr << "read failed: "
                  << std::strerror(error_number) << '\n';
        ::close(pipefd[0]);
        return 1;
    }

    ::close(pipefd[0]);

    int status = 0;
    if (::waitpid(child_pid, &status, 0) == -1) {
        const int error_number = errno;
        std::cerr << "waitpid failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        std::cout << "PASS: pipe communication\n";
        return 0;
    }

    std::cerr << "FAIL: child process did not exit successfully\n";
    return 1;
}
