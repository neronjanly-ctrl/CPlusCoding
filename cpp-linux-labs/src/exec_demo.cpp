#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    const pid_t child_pid = ::fork();

    if (child_pid == -1) {
        const int error_number = errno;
        std::cerr << "fork failed: " << std::strerror(error_number) << '\n';
        return 1;
    }

    if (child_pid == 0) {
        // Flush before exec replaces the process image and its C++ streams.
        std::cout << "[Child] Before exec, PID = " << ::getpid() << std::endl;

        ::execl("/bin/echo", "echo", "Hello from exec",
                static_cast<char*>(nullptr));

        // Only an exec failure reaches this branch.
        const int error_number = errno;
        std::cerr << "exec failed: " << std::strerror(error_number) << '\n';
        ::_exit(127);
    }

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

    if (!WIFEXITED(status)) {
        std::cerr << "FAIL: child did not exit normally\n";
        return 1;
    }

    const int exit_code = WEXITSTATUS(status);
    std::cout << "[Parent] Child exit code = " << exit_code << '\n';
    if (exit_code != 0) {
        std::cerr << "FAIL: exec child returned a nonzero status\n";
        return 1;
    }

    std::cout << "PASS: fork and exec experiment\n";
    return 0;
}
